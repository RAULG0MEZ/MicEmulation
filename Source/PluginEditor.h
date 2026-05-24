#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class EqCurveComponent final : public juce::Component
{
public:
    explicit EqCurveComponent (RodeM2ToSlateML1AudioProcessor& processorToUse);
    void paint (juce::Graphics& g) override;

private:
    RodeM2ToSlateML1AudioProcessor& processor;
};

class SourceMicSelector final : public juce::Component
{
public:
    explicit SourceMicSelector (RodeM2ToSlateML1AudioProcessor& processorToUse);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    bool keyPressed (const juce::KeyPress& key) override;
    bool hitTest (int x, int y) override;
    void hideMenu();

private:
    void selectProfile (int index);

    RodeM2ToSlateML1AudioProcessor& processor;
    bool menuOpen = false;
};

class RodeM2ToSlateML1AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                   private juce::Timer
{
public:
    explicit RodeM2ToSlateML1AudioProcessorEditor (RodeM2ToSlateML1AudioProcessor&);
    ~RodeM2ToSlateML1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    class EmuLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        EmuLookAndFeel();

        void drawLinearSlider (juce::Graphics&,
                               int x,
                               int y,
                               int width,
                               int height,
                               float sliderPos,
                               float minSliderPos,
                               float maxSliderPos,
                               const juce::Slider::SliderStyle,
                               juce::Slider&) override;

        void drawButtonBackground (juce::Graphics&,
                                   juce::Button&,
                                   const juce::Colour& backgroundColour,
                                   bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown) override;

        void drawButtonText (juce::Graphics&,
                             juce::TextButton&,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override;

        void drawComboBox (juce::Graphics&,
                           int width,
                           int height,
                           bool isButtonDown,
                           int buttonX,
                           int buttonY,
                           int buttonW,
                           int buttonH,
                           juce::ComboBox&) override;

        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

        void drawPopupMenuBackgroundWithOptions (juce::Graphics&,
                                                 int width,
                                                 int height,
                                                 const juce::PopupMenu::Options&) override;

        void drawPopupMenuItem (juce::Graphics&,
                                const juce::Rectangle<int>& area,
                                bool isSeparator,
                                bool isActive,
                                bool isHighlighted,
                                bool isTicked,
                                bool hasSubMenu,
                                const juce::String& text,
                                const juce::String& shortcutKeyText,
                                const juce::Drawable* icon,
                                const juce::Colour* textColour) override;

        void drawPopupMenuItemWithOptions (juce::Graphics&,
                                           const juce::Rectangle<int>& area,
                                           bool isHighlighted,
                                           const juce::PopupMenu::Item& item,
                                           const juce::PopupMenu::Options&) override;
    };

    void timerCallback() override;

    RodeM2ToSlateML1AudioProcessor& audioProcessor;

    EmuLookAndFeel lookAndFeel;
    juce::Image backgroundImage;
    EqCurveComponent eqCurve;
    SourceMicSelector sourceMicSelector;
    juce::Slider blendSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> blendAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RodeM2ToSlateML1AudioProcessorEditor)
};
