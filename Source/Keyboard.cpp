#include "Keyboard.h"

#include "SynthLookAndFeel.h"

Keyboard::Keyboard (juce::MidiKeyboardState& state)
    : MidiKeyboardComponent (state, horizontalKeyboard)
{
    setAvailableRange (36, 96);
    setOctaveForMiddleC (4);
    setKeyPressBaseOctave (5);
    setVelocity (1.0f, false);
    setScrollButtonsVisible (false);
    setBlackNoteLengthProportion (0.62f);
    setTitle ("Piano keyboard");

    // The background shows through the gaps between the keys.
    setColour (whiteNoteColourId, colours::background);
    setColour (keySeparatorLineColourId, juce::Colours::transparentBlack);
    setColour (shadowColourId, juce::Colours::black.withAlpha (0.4f));
}

void Keyboard::resized()
{
    // Stretch the keys to fill the width.
    auto numWhiteKeys = 0;

    for (auto note = getRangeStart(); note <= getRangeEnd(); ++note)
        if (! juce::MidiMessage::isMidiNoteBlack (note))
            ++numWhiteKeys;

    if (getWidth() > 0)
        setKeyWidth ((float) getWidth() / (float) numWhiteKeys);

    MidiKeyboardComponent::resized();
}

void Keyboard::drawWhiteNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                              bool isDown, bool isOver, juce::Colour, juce::Colour)
{
    const auto key = area.reduced (1.0f, 0.0f).withTrimmedBottom (1.0f);
    auto top = juce::Colour (0xfff6f7f9);
    auto bottom = juce::Colour (0xffd3d7df);

    if (isDown)
    {
        top = colours::accent.brighter (0.35f);
        bottom = colours::accent;
    }
    else if (isOver)
    {
        top = top.darker (0.04f);
        bottom = bottom.darker (0.08f);
    }

    juce::Path outline;
    outline.addRoundedRectangle (key.getX(), key.getY(), key.getWidth(), key.getHeight(), 4.0f, 4.0f,
                                 false, false, true, true);
    g.setGradientFill (juce::ColourGradient (top, key.getTopLeft(), bottom, key.getBottomLeft(), false));
    g.fillPath (outline);

    if (const auto text = getWhiteNoteText (midiNoteNumber); text.isNotEmpty())
    {
        g.setColour (isDown ? colours::background : juce::Colour (0xff8a90a0));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (text, key.withTrimmedBottom (6.0f), juce::Justification::centredBottom);
    }
}

void Keyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    auto top = juce::Colour (0xff3a3f4b);
    auto bottom = juce::Colour (0xff131519);

    if (isDown)
    {
        top = colours::accent;
        bottom = colours::accent.darker (0.4f);
    }
    else if (isOver)
    {
        top = top.brighter (0.2f);
    }

    juce::Path outline;
    outline.addRoundedRectangle (area.getX(), area.getY(), area.getWidth(), area.getHeight(), 3.0f, 3.0f,
                                 false, false, true, true);
    g.setGradientFill (juce::ColourGradient (top, area.getTopLeft(), bottom, area.getBottomLeft(), false));
    g.fillPath (outline);

    if (! isDown)
    {
        // A lighter top face makes the key look raised.
        const auto face = area.reduced (area.getWidth() * 0.15f, 0.0f).withTrimmedBottom (area.getHeight() * 0.1f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRoundedRectangle (face, 2.0f);
    }
}
