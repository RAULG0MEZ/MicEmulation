#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
constexpr int editorWidth = 470;
constexpr int editorHeight = 836;
constexpr int minEditorWidth = 320;
constexpr int minEditorHeight = 570;
constexpr int maxEditorWidth = 940;
constexpr int maxEditorHeight = 1672;

const auto panelAccent = juce::Colour (0xffc49a78);
const auto panelGold = juce::Colour (0xffd6b18f);
const auto panelDark = juce::Colour (0xff080706);

juce::Rectangle<int> scaleRect (juce::Rectangle<int> bounds, float x, float y, float width, float height)
{
    return { bounds.getX() + juce::roundToInt (x * static_cast<float> (bounds.getWidth())),
             bounds.getY() + juce::roundToInt (y * static_cast<float> (bounds.getHeight())),
             juce::roundToInt (width * static_cast<float> (bounds.getWidth())),
             juce::roundToInt (height * static_cast<float> (bounds.getHeight())) };
}

float getEditorScale (juce::Rectangle<int> bounds)
{
    return static_cast<float> (bounds.getWidth()) / static_cast<float> (editorWidth);
}
}

EqCurveComponent::EqCurveComponent (RodeM2ToSlateML1AudioProcessor& processorToUse)
    : processor (processorToUse)
{
    setInterceptsMouseClicks (false, false);
}

void EqCurveComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto scale = juce::jlimit (0.70f, 2.0f, static_cast<float> (getHeight()) / (static_cast<float> (editorHeight) * 0.092f));

    g.setColour (juce::Colours::black.withAlpha (0.08f));
    g.fillRoundedRectangle (bounds.reduced (0.5f * scale), 8.0f * scale);

    g.setColour (panelAccent.withAlpha (0.12f));
    g.drawRoundedRectangle (bounds.reduced (0.5f * scale), 8.0f * scale, 0.8f * scale);

    const auto plot = bounds.reduced (7.0f * scale, 6.0f * scale);

    g.setColour (juce::Colours::white.withAlpha (0.045f));
    for (auto normalisedX : { 0.18f, 0.36f, 0.54f, 0.72f, 0.90f })
        g.drawVerticalLine (juce::roundToInt (plot.getX() + plot.getWidth() * normalisedX),
                            plot.getY(),
                            plot.getBottom());

    g.setColour (panelGold.withAlpha (0.17f));
    g.drawHorizontalLine (juce::roundToInt (plot.getCentreY()), plot.getX(), plot.getRight());

    juce::Path response;
    const auto sampleRate = juce::jmax (processor.getAnalysisSampleRate(), 44100.0);
    const auto blend = processor.getBlend01();
    const auto profileIndex = processor.getSourceMicIndex();
    const auto minFrequency = 45.0;
    const auto maxFrequency = 20000.0;
    const auto minDb = -24.0;
    const auto maxDb = 24.0;

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
    g.strokePath (glow, juce::PathStrokeType (6.0f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (panelGold);
    g.strokePath (response, juce::PathStrokeType (2.0f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

SourceMicSelector::SourceMicSelector (RodeM2ToSlateML1AudioProcessor& processorToUse)
    : processor (processorToUse)
{
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
}

int SourceMicSelector::getButtonHeight() const
{
    return juce::roundToInt (juce::jlimit (24.0f, 68.0f, static_cast<float> (getHeight()) * 0.26f));
}

float SourceMicSelector::getMenuGap() const
{
    return juce::jlimit (6.0f, 20.0f, static_cast<float> (getHeight()) * 0.077f);
}

juce::Rectangle<float> SourceMicSelector::getButtonArea() const
{
    auto bounds = getLocalBounds().toFloat();
    return bounds.removeFromBottom (static_cast<float> (getButtonHeight())).reduced (0.5f);
}

juce::Rectangle<float> SourceMicSelector::getMenuArea() const
{
    const auto buttonArea = getButtonArea();
    const auto menuBottom = buttonArea.getY() - getMenuGap();
    const auto desiredHeight = juce::jlimit (52.0f, 156.0f, static_cast<float> (getHeight()) * 0.60f);
    const auto menuTop = juce::jmax (0.5f, menuBottom - desiredHeight);

    return { buttonArea.getX(), menuTop, buttonArea.getWidth(), juce::jmax (0.0f, menuBottom - menuTop) };
}

int SourceMicSelector::getMenuIndexAt (juce::Point<float> position) const
{
    if (! menuOpen)
        return -1;

    const auto menuArea = getMenuArea().reduced (0.0f, 4.0f);

    if (! menuArea.contains (position))
        return -1;

    const auto names = EQModel::getProfileNames();
    const auto rowHeight = menuArea.getHeight() / static_cast<float> (juce::jmax (1, names.size()));
    return juce::jlimit (0, names.size() - 1, static_cast<int> ((position.y - menuArea.getY()) / rowHeight));
}

void SourceMicSelector::updateHoverState (bool shouldHoverButton, int newHoveredIndex)
{
    if (buttonHovered == shouldHoverButton && hoveredIndex == newHoveredIndex)
        return;

    buttonHovered = shouldHoverButton;
    hoveredIndex = newHoveredIndex;
    repaint();
}

bool SourceMicSelector::hitTest (int, int y)
{
    const auto buttonHeight = getButtonHeight();
    return menuOpen || y >= getHeight() - buttonHeight;
}

void SourceMicSelector::paint (juce::Graphics& g)
{
    const auto buttonArea = getButtonArea();
    const auto selectedIndex = processor.getSourceMicIndex();
    const auto scale = static_cast<float> (getButtonHeight()) / 34.0f;
    const auto cornerRadius = 7.0f * scale;

    g.setColour (juce::Colours::black.withAlpha (buttonHovered ? 0.88f : 0.78f));
    g.fillRoundedRectangle (buttonArea, cornerRadius);
    g.setColour (panelGold.withAlpha (buttonHovered ? 0.22f : 0.08f));
    g.fillRoundedRectangle (buttonArea.reduced (1.0f * scale), 6.0f * scale);
    g.setColour (panelAccent.withAlpha (buttonHovered ? 0.78f : 0.58f));
    g.drawRoundedRectangle (buttonArea, cornerRadius, (buttonHovered ? 1.25f : 1.0f) * scale);

    g.setFont (juce::FontOptions (12.0f * scale).withStyle ("Bold"));
    g.setColour (panelGold.withAlpha (0.92f));
    g.drawFittedText (EQModel::getProfile (selectedIndex).name,
                      buttonArea.toNearestInt()
                                .reduced (juce::roundToInt (16.0f * scale), 0)
                                .withTrimmedRight (juce::roundToInt (28.0f * scale)),
                      juce::Justification::centred,
                      1);

    const auto arrowCentre = juce::Point<float> (buttonArea.getRight() - 17.0f * scale,
                                                buttonArea.getCentreY() + 0.5f * scale);
    juce::Path arrow;
    arrow.addTriangle (arrowCentre.x - 4.5f * scale,
                       arrowCentre.y - 2.2f * scale,
                       arrowCentre.x + 4.5f * scale,
                       arrowCentre.y - 2.2f * scale,
                       arrowCentre.x,
                       arrowCentre.y + 3.7f * scale);
    g.setColour (panelGold.withAlpha (0.84f));
    g.fillPath (arrow);

    if (! menuOpen)
        return;

    const auto menuArea = getMenuArea();

    g.setColour (juce::Colours::black.withAlpha (0.38f));
    g.fillRoundedRectangle (menuArea.translated (0.0f, 3.0f * scale), 8.0f * scale);
    g.setColour (juce::Colours::black.withAlpha (0.94f));
    g.fillRoundedRectangle (menuArea, cornerRadius);
    g.setColour (panelGold.withAlpha (0.08f));
    g.fillRoundedRectangle (menuArea.reduced (1.0f * scale), 6.0f * scale);
    g.setColour (panelAccent.withAlpha (0.52f));
    g.drawRoundedRectangle (menuArea, cornerRadius, 1.1f * scale);

    const auto names = EQModel::getProfileNames();
    const auto rowsArea = menuArea.reduced (0.0f, 4.0f * scale);
    const auto rowHeight = rowsArea.getHeight() / static_cast<float> (juce::jmax (1, names.size()));

    for (int i = 0; i < names.size(); ++i)
    {
        const auto row = juce::Rectangle<float> (rowsArea.getX(),
                                                rowsArea.getY() + rowHeight * static_cast<float> (i),
                                                rowsArea.getWidth(),
                                                rowHeight).reduced (6.0f * scale, 3.0f * scale);
        const auto active = i == selectedIndex;
        const auto hovered = i == hoveredIndex;

        if (hovered)
        {
            g.setColour (panelGold.withAlpha (0.18f));
            g.fillRoundedRectangle (row, 5.0f * scale);
        }
        else if (active)
        {
            g.setColour (panelAccent.withAlpha (0.14f));
            g.fillRoundedRectangle (row, 5.0f * scale);
        }

        g.setFont (juce::FontOptions (12.0f * scale).withStyle ("Bold"));
        g.setColour (panelGold.withAlpha (hovered || active ? 0.98f : 0.78f));
        g.drawFittedText (names[i],
                          row.toNearestInt().reduced (juce::roundToInt (8.0f * scale), 0),
                          juce::Justification::centred,
                          1);
    }
}

void SourceMicSelector::mouseDown (const juce::MouseEvent& event)
{
    const auto buttonArea = getButtonArea();

    if (buttonArea.contains (event.position))
    {
        menuOpen = ! menuOpen;
        hoveredIndex = -1;
        repaint();
        return;
    }

    if (! menuOpen)
        return;

    const auto index = getMenuIndexAt (event.position);

    if (index >= 0)
        selectProfile (index);

    menuOpen = false;
    hoveredIndex = -1;
    repaint();
}

void SourceMicSelector::mouseMove (const juce::MouseEvent& event)
{
    updateHoverState (getButtonArea().contains (event.position), getMenuIndexAt (event.position));
}

void SourceMicSelector::mouseExit (const juce::MouseEvent&)
{
    updateHoverState (false, -1);
}

void SourceMicSelector::hideMenu()
{
    if (! menuOpen)
        return;

    menuOpen = false;
    hoveredIndex = -1;
    repaint();
}

void SourceMicSelector::selectProfile (int index)
{
    if (auto* parameter = processor.parameters.getParameter ("sourceMic"))
    {
        const auto safeIndex = EQModel::getValidProfileIndex (index);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (safeIndex)));
        parameter->endChangeGesture();
    }
}

RodeM2ToSlateML1AudioProcessorEditor::RodeM2ToSlateML1AudioProcessorEditor (RodeM2ToSlateML1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      backgroundImage (juce::ImageCache::getFromMemory (BinaryData::plugin_background_png,
                                                        BinaryData::plugin_background_pngSize)),
      eqCurve (p),
      sourceMicSelector (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (minEditorWidth, minEditorHeight, maxEditorWidth, maxEditorHeight);

    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (static_cast<double> (editorWidth) / static_cast<double> (editorHeight));

    blendSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    blendSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    blendSlider.setRange (0.0, 200.0, 0.01);
    blendSlider.setDoubleClickReturnValue (true, 100.0);
    blendSlider.setMouseDragSensitivity (220);
    blendSlider.setScrollWheelEnabled (true);
    blendSlider.setWantsKeyboardFocus (false);
    blendSlider.setMouseClickGrabsKeyboardFocus (false);
    blendSlider.setColour (juce::Slider::thumbColourId, panelGold);
    blendSlider.setColour (juce::Slider::trackColourId, panelAccent);
    addAndMakeVisible (blendSlider);

    addAndMakeVisible (eqCurve);
    addAndMakeVisible (sourceMicSelector);
    addMouseListener (this, true);

    blendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.parameters,
                                                                                              "blend",
                                                                                              blendSlider);

    setSize (editorWidth, editorHeight);
    startTimerHz (30);
}

RodeM2ToSlateML1AudioProcessorEditor::~RodeM2ToSlateML1AudioProcessorEditor()
{
    removeMouseListener (this);
    setLookAndFeel (nullptr);
}

void RodeM2ToSlateML1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    if (backgroundImage.isValid())
        g.drawImage (backgroundImage, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);

    const auto bounds = getLocalBounds();
    const auto scale = getEditorScale (bounds);

    g.setColour (panelGold.withAlpha (0.84f));

    g.setFont (juce::FontOptions (12.0f * scale).withStyle ("Bold"));
    g.drawFittedText ("Intensity", scaleRect (bounds, 0.390f, 0.887f, 0.220f, 0.026f), juce::Justification::centred, 1);

    g.setFont (juce::FontOptions (14.0f * scale));
    g.drawFittedText ("0", scaleRect (bounds, 0.090f, 0.918f, 0.070f, 0.030f), juce::Justification::centred, 1);
    g.drawFittedText ("200%", scaleRect (bounds, 0.840f, 0.918f, 0.105f, 0.030f), juce::Justification::centred, 1);
}

void RodeM2ToSlateML1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void RodeM2ToSlateML1AudioProcessorEditor::resized()
{
    const auto bounds = getLocalBounds();

    eqCurve.setBounds (scaleRect (bounds, 0.031f, 0.779f, 0.938f, 0.092f));
    blendSlider.setBounds (scaleRect (bounds, 0.160f, 0.917f, 0.680f, 0.038f));
    sourceMicSelector.setBounds (scaleRect (bounds, 0.285f, 0.580f, 0.430f, 0.156f));
}

void RodeM2ToSlateML1AudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    const auto eventInEditor = event.getEventRelativeTo (this);

    if (! sourceMicSelector.getBounds().toFloat().contains (eventInEditor.position))
        sourceMicSelector.hideMenu();
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
    const auto scale = juce::jlimit (0.70f, 2.0f, bounds.getHeight() / (static_cast<float> (editorHeight) * 0.038f));

    const auto track = juce::Rectangle<float> (bounds.getX() + 2.0f * scale,
                                              bounds.getCentreY() - 1.0f * scale,
                                              bounds.getWidth() - 4.0f * scale,
                                              2.0f * scale);

    const auto min = slider.getMinimum();
    const auto max = slider.getMaximum();
    const auto proportion = max > min ? juce::jlimit (0.0, 1.0, (slider.getValue() - min) / (max - min)) : 0.0;
    const auto thumbX = track.getX() + track.getWidth() * static_cast<float> (proportion);
    const auto centreY = track.getCentreY();

    g.setColour (panelGold.withAlpha (0.42f));
    g.drawLine (track.getX(), centreY, track.getRight(), centreY, 1.0f * scale);

    if (slider.isEnabled())
    {
        g.setColour (panelGold.withAlpha (0.72f));
        g.drawLine (track.getX(), centreY, thumbX, centreY, 1.4f * scale);
    }

    const auto glowBounds = juce::Rectangle<float> (thumbX - 9.0f * scale,
                                                   centreY - 9.0f * scale,
                                                   18.0f * scale,
                                                   18.0f * scale);
    const auto thumbBounds = juce::Rectangle<float> (thumbX - 3.5f * scale,
                                                    centreY - 3.5f * scale,
                                                    7.0f * scale,
                                                    7.0f * scale);

    g.setColour (panelGold.withAlpha (0.24f));
    g.fillEllipse (glowBounds);
    g.setColour (panelGold.withAlpha (0.96f));
    g.fillEllipse (thumbBounds);
    g.setColour (juce::Colours::white.withAlpha (0.72f));
    g.fillEllipse (thumbBounds.reduced (1.8f * scale));
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
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour ((active ? juce::Colours::black : panelGold).withAlpha (0.65f));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
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
    auto fill = juce::Colours::black.withAlpha (0.78f);

    if (box.isMouseOverOrDragging() || isButtonDown)
        fill = fill.brighter (0.05f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (panelAccent.withAlpha (0.78f));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    const auto arrowArea = bounds.removeFromRight (26.0f).reduced (8.0f, 11.0f);
    const auto arrowCentre = arrowArea.getCentre();
    juce::Path arrow;
    arrow.addTriangle (arrowCentre.x - 4.2f,
                       arrowCentre.y - 2.0f,
                       arrowCentre.x + 4.2f,
                       arrowCentre.y - 2.0f,
                       arrowCentre.x,
                       arrowCentre.y + 3.4f);

    g.setColour (panelGold.withAlpha (0.78f));
    g.fillPath (arrow);
}

void RodeM2ToSlateML1AudioProcessorEditor::EmuLookAndFeel::positionComboBoxText (juce::ComboBox& box,
                                                                                  juce::Label& label)
{
    label.setBounds (box.getLocalBounds().reduced (10, 1).withTrimmedRight (20));
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
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
    g.fillRoundedRectangle (bounds.reduced (1.0f), 6.0f);

    g.setColour (panelAccent.withAlpha (0.58f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), 6.0f, 1.0f);
}

void RodeM2ToSlateML1AudioProcessorEditor::EmuLookAndFeel::drawPopupMenuBackgroundWithOptions (juce::Graphics& g,
                                                                                                int width,
                                                                                                int height,
                                                                                                const juce::PopupMenu::Options&)
{
    drawPopupMenuBackground (g, width, height);
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

void RodeM2ToSlateML1AudioProcessorEditor::EmuLookAndFeel::drawPopupMenuItemWithOptions (juce::Graphics& g,
                                                                                          const juce::Rectangle<int>& area,
                                                                                          bool isHighlighted,
                                                                                          const juce::PopupMenu::Item& item,
                                                                                          const juce::PopupMenu::Options&)
{
    const auto* textColour = item.colour.isTransparent() ? nullptr : &item.colour;

    drawPopupMenuItem (g,
                       area,
                       item.isSeparator,
                       item.isEnabled,
                       isHighlighted,
                       item.isTicked,
                       item.subMenu != nullptr,
                       item.text,
                       item.shortcutKeyDescription,
                       item.image.get(),
                       textColour);
}
