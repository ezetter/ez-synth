// Draws EZ Synth's icon: three sine waves, the 1st, 2nd and 3rd harmonics over one wavelength, in the colours the
// grid gives them, on a dark tile shaped like a Mac app icon. It saves it as Resources/Icon.png, in the current
// folder, unless another file is named. Run it from the repository's folder after changing the drawing, then build
// again, to build the new icon into EZ Synth:
//
//     ./build/EZSynthMakeIcon

#include "../Source/SynthLookAndFeel.h"

#include <iostream>

namespace
{
    constexpr int size = 1024;

    /** Each harmonic's height, as a share of the fundamental's, smaller the higher it is, so each stays in sight. */
    constexpr std::array<float, 3> amplitudes { 1.0f, 0.7f, 0.45f };

    juce::Image drawIcon()
    {
        juce::Image image (juce::Image::ARGB, size, size, true);
        juce::Graphics g (image);

        // The tile is the size and shape of Apple's app icon template: 824 points square, with rounded corners,
        // leaving room for its shadow.
        const juce::Rectangle<float> tile (100.0f, 100.0f, 824.0f, 824.0f);
        juce::Path tileShape;
        tileShape.addRoundedRectangle (tile, 185.0f);

        juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 36, { 0, 12 }).drawForPath (g, tileShape);
        g.setGradientFill (juce::ColourGradient (colours::panel.brighter (0.12f), tile.getTopLeft(),
                                                 colours::plot, tile.getBottomLeft(), false));
        g.fillPath (tileShape);

        const juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (tileShape);

        // One wavelength across the tile, on a faint grid: its middle line, and a line every quarter wavelength.
        const auto plot = tile.reduced (118.0f, 0.0f);
        const auto middle = tile.getCentreY();
        const auto height = 250.0f;

        g.setColour (colours::gridLineStrong.withAlpha (0.7f));

        for (int quarter = 0; quarter <= 4; ++quarter)
            g.fillRect (juce::Rectangle<float> (4.0f, tile.getHeight()).withCentre ({ plot.getX() + plot.getWidth() * (float) quarter / 4.0f, middle }));

        g.setColour (colours::gridLineStrong.brighter (0.25f));
        g.fillRect (juce::Rectangle<float> (tile.getWidth(), 5.0f).withCentre ({ tile.getCentreX(), middle }));

        // The fundamental is drawn last, over the higher harmonics. Each has a dark edge, so it stands clear of any it
        // crosses.
        for (auto i = amplitudes.size(); i-- > 0;)
        {
            const auto harmonic = (double) (i + 1);
            juce::Path wave;

            for (int point = 0; point <= 600; ++point)
            {
                const auto t = point / 600.0;
                const juce::Point<float> at { plot.getX() + (float) t * plot.getWidth(),
                                              middle - height * amplitudes[i] * (float) std::sin (juce::MathConstants<double>::twoPi * harmonic * t) };

                if (point == 0)
                    wave.startNewSubPath (at);
                else
                    wave.lineTo (at);
            }

            g.setColour (colours::plot);
            g.strokePath (wave, juce::PathStrokeType (46.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (colours::forPartial ((int) i));
            g.strokePath (wave, juce::PathStrokeType (28.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // A fine light edge, as if lit from above.
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.14f), tile.getTopLeft(),
                                                 juce::Colours::white.withAlpha (0.02f), tile.getBottomLeft(), false));
        g.strokePath (tileShape, juce::PathStrokeType (6.0f));

        return image;
    }
}

int main (int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI initialiser;
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "Resources/Icon.png");
    file.getParentDirectory().createDirectory();
    file.deleteFile();

    juce::FileOutputStream stream (file);
    const auto saved = stream.openedOk() && juce::PNGImageFormat().writeImageToStream (drawIcon(), stream);

    std::cout << (saved ? "Saved " : "Couldn't save ") << file.getFullPathName() << std::endl;
    return saved ? 0 : 1;
}
