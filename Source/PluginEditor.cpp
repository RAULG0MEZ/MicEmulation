#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
constexpr int editorWidth = 470;
constexpr int editorHeight = 836;

const auto panelAccent = juce::Colour (0xffc49a78);
const auto panelGold = juce::Colour (0xffd6b18f);
const auto panelDark = juce::Colour (0xff080706);

juce::Rectangle<int> scaleRect (float x, float y, float width, float height)
{
    return { juce::roundToInt (x * editorWidth),
             juce::roundToInt (y * editorHeight),
             juce::roundToInt (width * editorWidth),
             juce::roundToInt (height * editorHeight) };
}
}

EqCurveComponent::EqCurveComponent (RodeM2ToSlateML1AudioProcessor& processorToUse)
    : processor (processorToUse)
{
    setInterceptsMouseClicks (false, false);
}

void EqCurveComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (6.0f);

    g.setColour (juce::Colours::black.withAlpha (0.46f));
    g.fillRoundedRectangle (bounds, 7.0f);

    g.setColour (panelAccent.withAlpha (0.18f));
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);

    const auto plot = bounds.reduced (10.0f, 8.0f);

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    for (auto normalisedX : { 0.18f, 0.36f, 0.54f, 0.72f, 0.90f })
        g.drawVerticalLine (juce::roundToInt (plot.getX() + plot.getWidth() * normalisedX),
                            plot.getY(),
                            plot.getBottom());

    g.setColour (panelGold.withAlpha (0.22f));
    g.drawHorizontalLine (juce::roundToInt (plot.getCentreY()), plot.getX(), plot.getRight());

    juce::Path response;
    const auto sampleRate = juce::jmax (processor.getAnalysisSampleRate(), 44100.0);
    const auto blend = processor.getBlend01();
    const auto profileIndex = processor.getSourceMicIndex();
    const auto minFrequency = 45.0;
    const auto maxFrequency = 20000.0;
    const auto minDb = -14.0;
    const auto maxDb = 14.0;

    for (int x = 0; x < juce::roundToInt (plot.getWidth()); ++x)
    {
        const auto normalisedX = static_cast<double> (x) / juce::jmax (1.0f, plot.getWidth() - 1.0f);
        const auto frequency = minFrequency * std::pow (maxFrequency / minFrequency, normalisedX);
        const auto db = juce::jlimit (minDb, maxDb, EQModel::magnitudeDbAt (frequency, sampleRate, blend, profileIndex));
        const auto normalisedY = juce::jmap (db, minDb, maxDb, 1.0, 0.0);
        const auto px = plot.getX() + static_cast<float> (x);
        const auto py = plot.getY() + static_cast<float> (normalisedY) * plot.getHeight();

        if (x == 0)
            response.startNewSubPath (px, py);
        else
            response.lineTo (px, py);
    }

    g.setColour (panelAccent.withAlpha (0.24f));
    auto glow = response;
    g.strokePath (glow, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (panelGold);
    g.strokePath (response, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

RodeM2ToSlateML1AudioProcessorEditor::RodeM2ToSlateML1AudioProcessorEditor (RodeM2ToSlateML1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      backgroundImage (juce::ImageCache::getFromMemory (BinaryData::plugin_background_png,
                                                        BinaryData::plugin_background_pngSize)),
      eqCurve (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    setResizable (false, false);

    blendSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    blendSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    blendSlider.setRange (0.0, 200.0, 0.01);
    blendSlider.setDoubleClickReturnValue (true, 100.0);
    blendSlider.setMouseDragSensitivity (220);
    blendSlider.setColour (juce::Slider::thumbColourId, panelGold);
    blendSlider.setColour (juce::Slider::trackColourId, panelAccent);
    addAndMakeVisible (blendSlider);

    sourceMicBox.addItemList (EQModel::getProfileNames(), 1);
    sourceMicBox.setJustificationType (juce::Justification::centred);
    sourceMicBox.setTextWhenNothingSelected ("RODE M2");
    sourceMicBox.setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    sourceMicBox.setColour (juce::ComboBox::outlineColourId, panelAccent.withAlpha (0.42f));
    sourceMicBox.setColour (juce::ComboBox::textColourId, panelGold.withAlpha (0.88f));
    sourceMicBox.setColour (juce::ComboBox::arrowColourId, panelGold.withAlpha (0.72f));
    addAndMakeVisible (sourceMicBox);

    addAndMakeVisible (eqCurve);

    blendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.parameters,
                                                                                              "blend",
                                                                                              blendSlider);
    sourceMicAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (audioProcessor.parameters,
                                                                                                    "sourceMic",
                                                                                                    sourceMicBox);

    setSize (editorWidth, editorHeight);
    startTimerHz (30);
}

RodeM2ToSlateML1AudioProcessorEditor::~RodeM2ToSlateML1AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void RodeM2ToSlateML1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    if (backgroundImage.isValid())
        g.drawImage (backgroundImage, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
}

void RodeM2ToSlateML1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    const auto value = juce::roundToInt (audioProcessor.getBlendPercent());
    const auto valueText = juce::String (value) + "%";

    g.setColour (panelGold.withAlpha (0.84f));
    g.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
    g.drawFittedText (valueText, scaleRect (0.425f, 0.846f, 0.15f, 0.030f), juce::Justification::centred, 1);

}

void RodeM2ToSlateML1AudioProcessorEditor::resized()
{
    eqCurve.setBounds (scaleRect (0.090f, 0.685f, 0.820f, 0.075f));
    blendSlider.setBounds (scaleRect (0.198f, 0.872f, 0.604f, 0.026f));
    sourceMicBox.setBounds (scaleRect (0.315f, 0.116f, 0.370f, 0.040f));
}

void RodeM2ToSlateML1AudioProcessorEditor::timerCallback()
{
    repaint();
    eqCurve.repaint();
}

RodeM2ToSlateML1AudioProcessorEditor::EmuLookAndFeel::EmuLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, panelDark);
    setColour (juce::PopupMenu::textColourId, panelGold.withAlpha (0.88f));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, panelAccent.withAlpha (0.92f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
}

void RodeM2ToSlateML1AudioProcessorEditor::EmuLookAndFeel::drawLinearSlider (juce::Graphics& g,
                                                                              int x,
                                                                              int y,
                                                                              int width,
                                                                              int height,
                                                                              float sliderPos,
                                                                              float minSliderPos,
                                                                              float maxSliderPos,
                                                                              const juce::Slider::SliderStyle,
                                                                              juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                               static_cast<float> (y),
                                               static_cast<float> (width),
                                               static_cast<float> (height));
    juce::ignoreUnused (sliderPos, minSliderPos, maxSliderPos);

    const auto track = juce::Rectangle<float> (bounds.getX() + 4.0f,
                                              bounds.getCentreY() - 3.0f,
                                              bounds.getWidth() - 8.0f,
                                              6.0f);

    g.setColour (juce::Colours::black.withAlpha (0.72f));
    g.fillRoundedRectangle (track.expanded (2.0f, 2.0f), 7.0f);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (track.expanded (2.0f, 2.0f), 7.0f, 1.0f);

    const auto min = slider.getMinimum();
    const auto max = slider.getMaximum();
    const auto proportion = max > min ? juce::jlimit (0.0, 1.0, (slider.getValue() - min) / (max - min)) : 0.0;
    const auto fillRight = track.getX() + track.getWidth() * static_cast<float> (proportion);
    const auto fill = juce::Rectangle<float> (track.getX(),
                                             track.getY(),
                                             fillRight - track.getX(),
                                             track.getHeight());

    g.setColour (panelAccent.withAlpha (slider.isEnabled() ? 0.78f : 0.32f));
    g.fillRoundedRectangle (fill, 5.0f);

    const auto thumbBounds = juce::Rectangle<float> (fillRight - 6.0f,
                                                    bounds.getCentreY() - 9.0f,
                                                    12.0f,
                                                    18.0f);

    g.setColour (juce::Colours::black.withAlpha (0.58f));
    g.fillRoundedRectangle (thumbBounds.translated (0.0f, 2.0f), 7.0f);

    g.setGradientFill (juce::ColourGradient (panelGold.brighter (0.15f),
                                             thumbBounds.getX(),
                                             thumbBounds.getY(),
                                             panelAccent.darker (0.20f),
                                             thumbBounds.getRight(),
                                             thumbBounds.getBottom(),
                                             false));
    g.fillRoundedRectangle (thumbBounds, 7.0f);
}

void RodeM2ToSlateML1AudioProcessorEditor::EmuLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                                                  juce::Button& button,
                                                                                  const juce::Colour&,
                                                                                  bool highlighted,
                                                                                  bool down)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto active = button.getToggleState();

    auto fill = active ? panelGold : juce::Colours::black.withAlpha (0.44f);
    if (highlighted || down)
        fill = fill.brighter (0.10f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 7.0f);

    g.setColour ((active ? juce::Colours::black : panelGold).withAlpha (0.65f));
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);
}

void RodeM2ToSlateML1AudioProcessorEditor::EmuLookAndFeel::drawButtonText (juce::Graphics& g,
                                                                            juce::TextButton& button,
                                                                            bool,
                                                                            bool)
{
    g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
    g.setColour (button.getToggleState() ? juce::Colours::black : panelGold.withAlpha (0.76f));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (4, 1),
                      juce::Justification::centred, 1);
}

void RodeM2ToSlateML1AudioProcessorEditor::EmuLookAndFeel::drawComboBox (juce::Graphics& g,
                                                                          int width,
                                                                          int height,
                                                                          bool isButtonDown,
                                                                          int,
                                                                          int,
                                                                          int,
                                                                          int,
                                                                          juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.5f, 0.5f,
                                          static_cast<float> (width) - 1.0f,
                                          static_cast<float> (height) - 1.0f);
    auto fill = juce::Colours::black.withAlpha (0.70f);

    if (box.isMouseOverOrDragging() || isButtonDown)
        fill = fill.brighter (0.05f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 7.0f);

    g.setColour (panelAccent.withAlpha (0.70f));
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);

    const auto arrowArea = bounds.removeFromRight (24.0f).reduced (7.0f, 9.0f);
    juce::Path arrow;
    arrow.startNewSubPath (arrowArea.getX(), arrowArea.getY());
    arrow.lineTo (arrowArea.getCentreX(), arrowArea.getBottom());
    arrow.lineTo (arrowArea.getRight(), arrowArea.getY());

    g.setColour (panelGold.withAlpha (0.78f));
    g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void RodeM2ToSlateML1AudioProcessorEditor::EmuLookAndFeel::positionComboBoxText (juce::ComboBox& box,
                                                                                  juce::Label& label)
{
    label.setBounds (box.getLocalBounds().reduced (10, 1).withTrimmedRight (20));
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.5f).withStyle ("Bold"));
    label.setColour (juce::Label::textColourId, panelGold.withAlpha (0.88f));
}

void RodeM2ToSlateML1AudioProcessorEditor::EmuLookAndFeel::drawPopupMenuBackground (juce::Graphics& g,
                                                                                     int width,
                                                                                     int height)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f,
                                               static_cast<float> (width),
                                               static_cast<float> (height));

    g.setColour (juce::Colours::black.withAlpha (0.94f));
    g.fillRoundedRectangle (bounds.reduced (1.0f), 8.0f);

    g.setColour (panelAccent.withAlpha (0.72f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), 8.0f, 1.0f);
}

void RodeM2ToSlateML1AudioProcessorEditor::EmuLookAndFeel::drawPopupMenuItem (juce::Graphics& g,
                                                                               const juce::Rectangle<int>& area,
                                                                               bool isSeparator,
                                                                               bool isActive,
                                                                               bool isHighlighted,
                                                                               bool isTicked,
                                                                               bool,
                                                                               const juce::String& text,
                                                                               const juce::String&,
                                                                               const juce::Drawable*,
                                                                               const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (panelAccent.withAlpha (0.20f));
        g.drawHorizontalLine (area.getCentreY(), static_cast<float> (area.getX() + 8), static_cast<float> (area.getRight() - 8));
        return;
    }

    auto itemArea = area.reduced (5, 2).toFloat();

    if (isHighlighted && isActive)
    {
        g.setColour (panelAccent.withAlpha (0.88f));
        g.fillRoundedRectangle (itemArea, 5.0f);
    }

    g.setFont (juce::FontOptions (11.5f).withStyle ("Bold"));
    g.setColour ((isHighlighted && isActive ? juce::Colours::black : panelGold).withAlpha (isActive ? 0.92f : 0.38f));

    if (isTicked)
        g.setColour ((isHighlighted && isActive ? juce::Colours::black : panelGold).withAlpha (1.0f));

    g.drawFittedText (text, area.reduced (16, 0), juce::Justification::centred, 1);
}
