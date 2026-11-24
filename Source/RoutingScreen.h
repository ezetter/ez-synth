#pragma once

#include "ChoiceButtons.h"
#include "SynthProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** A screen of its own, over the window but for the keyboard, setting where an oscillator's sound goes, on two pages.

    Filters: which filters it's sent through, a switch for each, all on to begin with. The filters run in a chain,
    from the first to the last, and an oscillator that isn't sent through one skips it, going straight on to the next.

    Effects, in the standalone app: which effect buses it's sent through, a switch for each, all off to begin with,
    beside each bus's chain of effect plugins. Each slot's button loads a plugin found on the Mac, shows its window,
    bypasses it or removes it. An oscillator sent through no bus plays straight out; one sent through two plays
    through both, side by side. Find Plugins looks for the VST3 and Audio Unit effects installed.

    Buttons along the top choose the oscillator. Done, or Escape, goes back to the synth.
*/
class RoutingScreen final : public juce::Component
{
public:
    explicit RoutingScreen (SynthProcessor&);
    ~RoutingScreen() override;

    enum class Page { filters, effects };

    /** Shows the page for the filters, or the effects, which only the standalone app has. */
    void showPage (Page);
    Page getPage() const noexcept   { return page; }

    /** Called when the screen's closed, and when another oscillator's chosen on it. */
    std::function<void()> onClose;
    std::function<void (int oscillator)> onOscillatorChosen;

    /** Shows the routing of an oscillator, numbered from 0. */
    void showOscillator (int oscillator);
    int getOscillator() const noexcept   { return oscillator; }

    /** Brings what's shown of the filters, their types, cutoffs and switches, up to date with the synth. */
    void refresh();

    /** The switch for whether the oscillator shown is sent through a filter, numbered from 0. */
    juce::ToggleButton& getSendSwitch (int filter)   { return rows[(size_t) filter].sendSwitch; }

    /** The switch for whether the oscillator shown is sent through an effect bus, and a slot's button, numbered from 0. */
    juce::ToggleButton& getBusSwitch (int bus)          { return busRows[(size_t) bus].sendSwitch; }
    juce::TextButton& getSlotButton (int bus, int slot) { return busRows[(size_t) bus].slots[(size_t) slot]; }

    /** Shows the window of the plugin in a slot, if it has one, and whether one's shown. */
    void showPluginWindow (int bus, int slot);
    bool isPluginWindowShown (int bus, int slot) const;

    /** The path the oscillator shown takes, such as "Osc 2 → Filter 1 → Bus 1 → Out". */
    juce::String describePath() const;

    juce::TextButton doneButton { "Done" };

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    struct Row
    {
        juce::ToggleButton sendSwitch;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
        juce::Rectangle<int> area;
        juce::String shownSettings;
    };

    struct BusRow
    {
        juce::ToggleButton sendSwitch;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
        std::array<juce::TextButton, EffectRack::slotsPerBus> slots;
        juce::Rectangle<int> area, titleArea;
    };

    class PluginWindow;

    juce::String describeFilter (int filter) const;
    void showSlotMenu (int bus, int slot);
    void showSlots();
    void findPlugins();
    void closePluginWindow (juce::AudioPluginInstance&);

    SynthProcessor& synthProcessor;
    ChoiceButtons oscillatorButtons, pageButtons { { "Filters", "Effects" } };
    std::array<Row, modulation::numFilters> rows;
    std::array<BusRow, EffectRack::numBuses> busRows;
    juce::TextButton findPluginsButton { "Find Plugins..." };
    std::vector<std::unique_ptr<PluginWindow>> pluginWindows;
    juce::Component::SafePointer<juce::DialogWindow> findPluginsWindow;
    Page page = Page::filters;
    int oscillator = 0;
    int shownRackVersion = -1;
    juce::Rectangle<int> panelArea, titleArea, subtitleArea, oscillatorCaptionArea, filtersCaptionArea, chainArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingScreen)
};
