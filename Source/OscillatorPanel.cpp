#include "OscillatorPanel.h"

#include "Modulation.h"
#include "SynthProcessor.h"
#include "SynthLookAndFeel.h"

namespace
{
    const juce::String dot (juce::CharPointer_UTF8 ("\xc2\xb7"));

    /** How near a whole semitone, on screen, dragging the tune snaps to it, however far it's zoomed in. */
    constexpr double snapPixels = 3.0;
}

OscillatorPanel::OscillatorPanel()
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::trackColourId, colours::accent);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.setMouseClickGrabsKeyboardFocus (false);
    slider.setWantsKeyboardFocus (false);
    slider.fillOrigin = 0.0;
    slider.snap = [this] (double semitones) { return tuning::snap (semitones, getSnapDistance()); };
    slider.positionToProportion = [this] (float position)
    {
        return (tuning::fromPosition (position) - window.low) / (window.high - window.low);
    };

    // The slider shows only part of the tune's range, so it sets the tune itself, and only when the user moves it.
    // Showing the tune never sets it, even if the tune lies beyond the part shown.
    slider.onDragStart = [this]
    {
        dragging = true;

        if (attachment != nullptr)
            attachment->beginGesture();
    };

    slider.onDragEnd = [this]
    {
        dragging = false;

        if (attachment != nullptr)
            attachment->endGesture();
    };

    slider.onValueChange = [this]
    {
        if (attachment == nullptr)
            return;

        if (dragging)
            attachment->setValueAsPartOfGesture ((float) slider.getValue());
        else
            attachment->setValueAsCompleteGesture ((float) slider.getValue());
    };

    addAndMakeVisible (slider);

    panSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    panSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    panSlider.setRange (0.0, 1.0);
    panSlider.setColour (juce::Slider::trackColourId, colours::dimText);
    panSlider.setDoubleClickReturnValue (true, 0.5);
    panSlider.setMouseClickGrabsKeyboardFocus (false);
    panSlider.setWantsKeyboardFocus (false);
    panSlider.setTitle ("Pan");
    panSlider.onValueChange = [this] { repaint (panCaptionArea); };
    panSlider.onLivePositionChange = [this] { repaint (panCaptionArea); };
    addAndMakeVisible (panSlider);

    // The unison's sliders, the number of voices in whole steps. The detune and spread are dimmed while there's only
    // one voice, when they do nothing unless something moves the number up.
    for (auto* unisonSlider : { &voicesSlider, &detuneSlider, &spreadSlider })
    {
        unisonSlider->setSliderStyle (juce::Slider::LinearHorizontal);
        unisonSlider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        unisonSlider->setColour (juce::Slider::trackColourId, colours::dimText);
        unisonSlider->setMouseClickGrabsKeyboardFocus (false);
        unisonSlider->setWantsKeyboardFocus (false);
        unisonSlider->onValueChange = [this] { showUnison(); };
        unisonSlider->onLivePositionChange = [this] { showUnison(); };
        addAndMakeVisible (*unisonSlider);
    }

    zoomSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    zoomSlider.setRange (0.0, 1.0);
    zoomSlider.setColour (juce::Slider::trackColourId, colours::dimText);
    zoomSlider.setDoubleClickReturnValue (true, 0.0);
    zoomSlider.setMouseClickGrabsKeyboardFocus (false);
    zoomSlider.setWantsKeyboardFocus (false);
    zoomSlider.setTitle ("Tune zoom");
    zoomSlider.onValueChange = [this] { moveWindow(); };
    addAndMakeVisible (zoomSlider);

    glideMode.onChoose = [this] (int index)
    {
        if (glideModeAttachment != nullptr)
            glideModeAttachment->setValueAsCompleteGesture ((float) index);
    };
    addAndMakeVisible (glideMode);

    glideTimeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    glideTimeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    glideTimeSlider.setColour (juce::Slider::trackColourId, colours::dimText);
    glideTimeSlider.setMouseClickGrabsKeyboardFocus (false);
    glideTimeSlider.setWantsKeyboardFocus (false);
    glideTimeSlider.onValueChange = [this] { repaint (glideValueArea); };
    addAndMakeVisible (glideTimeSlider);

    filtersButton.setTooltip ("Choose which filters this oscillator is sent through");
    filtersButton.setMouseClickGrabsKeyboardFocus (false);
    filtersButton.setWantsKeyboardFocus (false);
    filtersButton.onClick = [this]
    {
        if (onShowFilterRouting != nullptr)
            onShowFilterRouting();
    };
    addAndMakeVisible (filtersButton);

    moveWindow();
}

void OscillatorPanel::setOscillator (juce::AudioProcessorValueTreeState& parameters, int oscillator)
{
    const auto name = "Osc " + juce::String (oscillator + 1);
    slider.setTitle (name + " tune");
    panSlider.setTitle (name + " pan");
    modulatable::setTarget (panSlider, modulation::oscillatorPanTarget (oscillator));
    voicesSlider.setTitle (name + " unison voices");
    modulatable::setTarget (voicesSlider, modulation::unisonVoicesTarget (oscillator));
    detuneSlider.setTitle (name + " unison detune");
    modulatable::setTarget (detuneSlider, modulation::unisonDetuneTarget (oscillator));
    spreadSlider.setTitle (name + " unison spread");
    modulatable::setTarget (spreadSlider, modulation::unisonSpreadTarget (oscillator));
    zoomSlider.setTitle (name + " tune zoom");
    glideMode.setTitle (name + " glide");
    glideTimeSlider.setTitle (name + " glide time");
    modulatable::setTarget (slider, modulation::oscillatorTuneTarget (oscillator));

    attachment.reset();
    panAttachment.reset();
    voicesAttachment.reset();
    detuneAttachment.reset();
    spreadAttachment.reset();
    zoomAttachment.reset();
    glideModeAttachment.reset();
    glideTimeAttachment.reset();

    for (auto& sendAttachment : sendAttachments)
        sendAttachment.reset();

    attachment = std::make_unique<juce::ParameterAttachment> (*parameters.getParameter (SynthProcessor::tuneId (oscillator)),
                                                              [this] (float semitones) { showTune (semitones); });
    attachment->sendInitialUpdate();
    panAttachment = std::make_unique<juce::SliderParameterAttachment> (*parameters.getParameter (SynthProcessor::panId (oscillator)),
                                                                       panSlider);
    panSlider.setDoubleClickReturnValue (true, 0.5);
    voicesAttachment = std::make_unique<juce::SliderParameterAttachment> (*parameters.getParameter (SynthProcessor::unisonVoicesId (oscillator)),
                                                                          voicesSlider);
    voicesSlider.setDoubleClickReturnValue (true, 1.0);
    detuneAttachment = std::make_unique<juce::SliderParameterAttachment> (*parameters.getParameter (SynthProcessor::unisonDetuneId (oscillator)),
                                                                          detuneSlider);
    detuneSlider.setDoubleClickReturnValue (true, 20.0);
    spreadAttachment = std::make_unique<juce::SliderParameterAttachment> (*parameters.getParameter (SynthProcessor::unisonSpreadId (oscillator)),
                                                                          spreadSlider);
    spreadSlider.setDoubleClickReturnValue (true, 0.5);
    showUnison();
    zoomAttachment = std::make_unique<juce::SliderParameterAttachment> (*parameters.getParameter (SynthProcessor::tuneZoomId (oscillator)),
                                                                        zoomSlider);

    // The glide's time is greyed out while it's off.
    glideModeAttachment = std::make_unique<juce::ParameterAttachment> (*parameters.getParameter (SynthProcessor::glideId (oscillator)),
                                                                       [this] (float value)
                                                                       {
                                                                           glideMode.setChosen (juce::roundToInt (value));
                                                                           glideTimeSlider.setEnabled (juce::roundToInt (value) != 0);
                                                                           glideTimeSlider.setAlpha (glideTimeSlider.isEnabled() ? 1.0f : 0.4f);
                                                                           repaint();
                                                                       });
    glideModeAttachment->sendInitialUpdate();
    glideTimeAttachment = std::make_unique<juce::SliderParameterAttachment> (*parameters.getParameter (SynthProcessor::glideTimeId (oscillator)),
                                                                             glideTimeSlider);
    glideTimeSlider.setDoubleClickReturnValue (true, 0.2);

    // Which filters it's sent through, shown beside the Filters button.
    filtersButton.setTitle (name + " filters");

    for (size_t filter = 0; filter < sendAttachments.size(); ++filter)
    {
        sendAttachments[filter] = std::make_unique<juce::ParameterAttachment> (
            *parameters.getParameter (SynthProcessor::filterSendId (oscillator, (int) filter)),
            [this, filter] (float value)
            {
                sends[filter] = value >= 0.5f;
                repaint (routingValueArea);
            });
        sendAttachments[filter]->sendInitialUpdate();
    }

    moveWindow();
}

void OscillatorPanel::setPanOnly (bool shouldBePanOnly)
{
    if (panOnly == shouldBePanOnly)
        return;

    panOnly = shouldBePanOnly;

    for (auto* component : std::initializer_list<juce::Component*> { &slider, &zoomSlider, &glideMode, &glideTimeSlider,
                                                                     &voicesSlider, &detuneSlider, &spreadSlider })
        component->setVisible (! panOnly);

    resized();
    repaint();
}

int OscillatorPanel::getGlideMode() const noexcept
{
    return glideMode.getChosen();
}

void OscillatorPanel::setZoom (float zoom)
{
    zoomSlider.setValue (zoom, juce::sendNotificationSync);
    moveWindow();
}

float OscillatorPanel::getZoom() const
{
    return (float) zoomSlider.getValue();
}

void OscillatorPanel::showTune (float semitones)
{
    tune = semitones;

    // A tune set from elsewhere, such as a preset, that lies beyond the part of the scale shown moves it there.
    if (! dragging && ! window.contains (tune))
        moveWindow();
    else
        slider.setValue (tune, juce::dontSendNotification);

    repaint();
}

void OscillatorPanel::moveWindow()
{
    window = tuning::getWindow (tune, tuning::zoomToReach (getZoom()));
    slider.setRange (window.low, window.high);
    slider.setValue (tune, juce::dontSendNotification);
    repaint();
}

double OscillatorPanel::getSnapDistance() const
{
    return snapPixels * (window.high - window.low) / std::max (1, slider.getWidth());
}

void OscillatorPanel::showUnison()
{
    const auto voices = juce::roundToInt (voicesSlider.getValue());
    const auto moved = modulatable::getLivePosition (voicesSlider).has_value();

    for (auto* unisonSlider : { &detuneSlider, &spreadSlider })
        unisonSlider->setAlpha (voices > 1 || moved ? 1.0f : 0.4f);

    repaint();
}

void OscillatorPanel::resized()
{
    auto area = getLocalBounds();
    dividers.clear();

    constexpr auto captionHeight = 14, sliderHeight = 16, rowGap = 4;

    // A caption, with its figure at the right, over a slider.
    const auto captionedSlider = [&area] (juce::Rectangle<int>& caption, juce::Slider& sliderUnder)
    {
        caption = area.removeFromTop (captionHeight);
        area.removeFromTop (1);
        sliderUnder.setBounds (area.removeFromTop (sliderHeight));
    };

    // The sections fit in the least height, and spread apart in more, with a line between each two.
    const auto numSections = panOnly ? 2 : 5;
    constexpr auto panHeight = captionHeight + 1 + sliderHeight;
    constexpr auto unisonHeight = captionHeight + 3 * (panHeight + rowGap);
    constexpr auto tuneHeight = captionHeight + 2 + panHeight + 2 + 20 + 16;
    constexpr auto glideHeight = 22 + rowGap + sliderHeight;
    constexpr auto routingHeight = 22;
    const auto content = panOnly ? panHeight + routingHeight : panHeight + unisonHeight + tuneHeight + glideHeight + routingHeight;
    const auto gap = juce::jlimit (12, 36, (area.getHeight() - content) / (numSections - 1));

    const auto divide = [&area, gap, this]
    {
        area.removeFromTop (gap / 2);
        dividers.push_back (area.removeFromTop (1));
        area.removeFromTop (gap - gap / 2 - 1);
    };

    captionedSlider (panCaptionArea, panSlider);
    divide();

    // The routing comes straight under the pan when that's all there is, as for an oscillator playing noise.
    const auto layOutRouting = [this, &area]
    {
        auto routingRow = area.removeFromTop (22);
        routingCaptionArea = routingRow.removeFromLeft (70);
        filtersButton.setBounds (routingRow.removeFromLeft (76));
        routingRow.removeFromLeft (8);
        routingValueArea = routingRow;
    };

    if (panOnly)
    {
        layOutRouting();
        return;
    }

    unisonCaptionArea = area.removeFromTop (captionHeight);

    for (auto [caption, unisonSlider] : { std::pair (&voicesCaptionArea, &voicesSlider), std::pair (&detuneCaptionArea, &detuneSlider),
                                          std::pair (&spreadCaptionArea, &spreadSlider) })
    {
        area.removeFromTop (rowGap);
        auto row = area.removeFromTop (panHeight);
        *caption = row.removeFromTop (captionHeight);
        row.removeFromTop (1);
        unisonSlider->setBounds (row.withTrimmedLeft (12));
        caption->setLeft (caption->getX() + 12);
    }

    divide();

    captionArea = area.removeFromTop (captionHeight);
    area.removeFromTop (2);
    captionedSlider (zoomCaptionArea, zoomSlider);
    area.removeFromTop (2);
    slider.setBounds (area.removeFromTop (20));
    tickArea = area.removeFromTop (16);
    divide();

    auto glideRow = area.removeFromTop (22);
    glideCaptionArea = glideRow.removeFromLeft (52);
    glideMode.setBounds (glideRow.removeFromLeft (juce::jmin (156, glideRow.getWidth())));
    area.removeFromTop (rowGap);
    auto timeRow = area.removeFromTop (sliderHeight);
    glideValueArea = timeRow.removeFromRight (64);
    timeRow.removeFromRight (8);
    glideTimeSlider.setBounds (timeRow);
    divide();

    layOutRouting();
}

void OscillatorPanel::paint (juce::Graphics& g)
{
    const auto whole = tuning::toWholeSemitones (tune);

    g.setColour (colours::outline);

    for (const auto& divider : dividers)
        g.fillRect (divider);

    // A caption, with a figure at the right, brighter when something's moved the control it's for.
    const auto drawCaption = [&g] (const juce::String& caption, const juce::String& figure, juce::Rectangle<int> area, bool moved)
    {
        g.setFont (fonts::caption());
        g.setColour (colours::dimText);
        g.drawText (caption, area, juce::Justification::centredLeft);
        g.setFont (fonts::body (12.0f));
        g.setColour (moved ? colours::text : colours::dimText);
        g.drawText (figure, area, juce::Justification::centredRight);
    };

    const auto movedPan = modulatable::getLivePosition (panSlider);
    drawCaption ("PAN", SynthProcessor::panToText (movedPan.value_or ((float) panSlider.getValue())), panCaptionArea, movedPan.has_value());

    // The routing: which filters the oscillator's sent through.
    g.setColour (colours::dimText);
    g.setFont (fonts::caption());
    g.drawText ("ROUTING", routingCaptionArea, juce::Justification::centredLeft);

    juce::StringArray sentTo;

    for (size_t filter = 0; filter < sends.size(); ++filter)
        if (sends[filter])
            sentTo.add (juce::String ((int) filter + 1));

    g.setFont (fonts::body (12.0f));
    g.drawText (sentTo.size() == (int) sends.size() ? juce::String ("All filters")
                : sentTo.isEmpty()                  ? juce::String ("No filters")
                : (sentTo.size() == 1 ? "Filter " : "Filters ") + sentTo.joinIntoString (", "),
                routingValueArea, juce::Justification::centredRight);

    if (panOnly)
        return;

    // The unison, its figures following where something moves them.
    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("UNISON", unisonCaptionArea, juce::Justification::centredLeft);

    const auto movedVoices = modulatable::getLivePosition (voicesSlider);
    const auto voices = movedVoices.has_value() ? 1 + juce::roundToInt (*movedVoices * (float) (Patch::maxUnisonVoices - 1))
                                                : juce::roundToInt (voicesSlider.getValue());
    const auto movedDetune = modulatable::getLivePosition (detuneSlider);
    const auto movedSpread = modulatable::getLivePosition (spreadSlider);
    drawCaption ("VOICES", SynthProcessor::unisonVoicesToText (voices), voicesCaptionArea, movedVoices.has_value());
    drawCaption ("DETUNE", SynthProcessor::unisonDetuneToText (movedDetune.has_value() ? *movedDetune * Patch::mostUnisonDetune
                                                                                       : (float) detuneSlider.getValue()),
                 detuneCaptionArea, movedDetune.has_value());
    drawCaption ("SPREAD", SynthProcessor::unisonSpreadToText (movedSpread.value_or ((float) spreadSlider.getValue())),
                 spreadCaptionArea, movedSpread.has_value());

    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("TUNE", captionArea, juce::Justification::centredLeft);
    g.drawText ("GLIDE", glideCaptionArea, juce::Justification::centredLeft);

    g.setFont (fonts::body (13.0f));
    g.setColour (colours::text);
    g.drawText (tuning::toText (tune) + (whole.has_value() ? "  " + dot + "  " + tuning::getIntervalName (*whole) : juce::String()),
                captionArea, juce::Justification::centredRight);

    // How far either side the scale reaches.
    drawCaption ("ZOOM", tuning::reachToText ((window.high - window.low) / 2.0f), zoomCaptionArea, false);
    g.setColour (colours::dimText.withAlpha (glideTimeSlider.isEnabled() ? 1.0f : 0.4f));
    g.drawText (SynthProcessor::glideTimeToText ((float) glideTimeSlider.getValue()), glideValueArea, juce::Justification::centredRight);

    const auto ticksTop = (float) tickArea.getY();
    const auto xOf = [this] (float semitones) { return (float) slider.getX() + (float) slider.getPositionOfValue (semitones); };
    const auto pixelsPerSemitone = (float) slider.getWidth() / (window.high - window.low);

    // Zoomed in, the finest steps between semitones that fit, as faint ticks.
    auto step = 0.0f;

    for (const auto candidate : { 0.5f, 0.25f, 0.1f })
        if (candidate * pixelsPerSemitone >= 7.0f)
            step = candidate;

    if (step > 0.0f)
    {
        g.setColour (colours::outline.brighter (0.25f));

        for (auto i = (int) std::ceil (window.low / step - 0.001f); (float) i * step <= window.high + 0.001f; ++i)
            if (! tuning::toWholeSemitones ((float) i * step).has_value())
                g.fillRect (juce::Rectangle<float> (1.0f, 2.0f).withPosition (xOf ((float) i * step) - 0.5f, ticksTop));
    }

    // A tick at each semitone: a longer, bright one at the note itself, and longer, numbered ones at the intervals
    // marked, above the note and below it. The interval the tune is on is picked out. Zoomed out too far for every
    // interval's number to fit, only the 5ths and octaves are numbered.
    g.setFont (fonts::body (11.5f));
    const auto numberedAll = pixelsPerSemitone >= 14.0f;

    for (auto semitone = (int) std::ceil (window.low); semitone <= (int) std::floor (window.high); ++semitone)
    {
        const auto x = xOf ((float) semitone);
        const auto marked = std::find_if (tuning::markedIntervals.begin(), tuning::markedIntervals.end(),
                                          [semitone] (const tuning::Interval& interval) { return interval.semitones == std::abs (semitone); });

        if (semitone == 0)
        {
            g.setColour (whole == 0 ? colours::accent : colours::text.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (1.5f, 9.0f).withPosition (x - 0.75f, ticksTop));
        }
        else if (marked == tuning::markedIntervals.end())
        {
            g.setColour (colours::outline.brighter (0.45f));
            g.fillRect (juce::Rectangle<float> (1.0f, 3.0f).withPosition (x - 0.5f, ticksTop));
        }
        else
        {
            g.setColour (whole == semitone ? colours::accent : colours::dimText);
            g.fillRect (juce::Rectangle<float> (1.0f, 6.0f).withPosition (x - 0.5f, ticksTop));
            if (numberedAll || marked->number == 5 || marked->number == 8 || marked->number == 12 || marked->number == 15)
                g.drawText (juce::String (marked->number), juce::Rectangle<float> (20.0f, 10.0f).withCentre ({ x, ticksTop + 11.0f }),
                            juce::Justification::centred);
        }
    }
}
