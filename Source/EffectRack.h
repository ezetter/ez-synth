#pragma once

#include "Patch.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** The standalone app's effect buses: each a chain of slots, played in turn, that VST3 and Audio Unit effect plugins
    load into. Oscillators sent through a bus are played through its chain, and the bus then plays out with the
    oscillators sent through none. An oscillator sent through two buses plays through each, side by side.

    Plugins load, and are removed, on the message thread, and play on the audio thread, which leaves the buses as
    they are for a block while a slot's changing rather than waiting. A plugin that's saved with a sound but can't be
    found here stays in its slot, missing, with its settings, so saving the sound again keeps it.
*/
class EffectRack final : private juce::ChangeListener
{
public:
    static constexpr int numBuses = Patch::numEffectBuses;
    static constexpr int slotsPerBus = 4;

    EffectRack();
    ~EffectRack() override;

    /** The plugin formats it hosts, VST3 and Audio Unit, and the plugins found, which are kept in a file and saved
        to it whenever they change.
    */
    juce::AudioPluginFormatManager& getFormats() noexcept   { return formats; }
    juce::KnownPluginList& getKnownPlugins() noexcept       { return knownPlugins; }
    void keepKnownPluginsIn (const juce::File&);

    /** The file naming a plugin that crashed while being looked for, so looking again skips it. */
    juce::File getCrashedPluginFile() const;

    /** The effect plugins found, by manufacturer and then name. */
    juce::Array<juce::PluginDescription> getEffects() const;

    struct SlotInfo
    {
        juce::String name;
        bool loaded = false, missing = false, bypassed = false;
    };

    SlotInfo getSlot (int bus, int slot) const;
    juce::AudioPluginInstance* getPlugin (int bus, int slot) const;

    /** Loads a plugin into a slot, replacing what's there. Gives an error, if it can't, and leaves the slot as it was. */
    juce::String load (int bus, int slot, const juce::PluginDescription&);

    void remove (int bus, int slot);
    void setBypassed (int bus, int slot, bool);
    void clear();

    /** Called just before a plugin's removed, so its window can close first. */
    std::function<void (juce::AudioPluginInstance&)> beforeRemoving;

    /** Every slot's plugin, its settings and whether it's bypassed, and putting them back, for saving with a sound. */
    juce::ValueTree save() const;
    void restore (const juce::ValueTree&);
    static constexpr const char* stateType = "EFFECTS";

    /** Goes up whenever a slot changes, for the window to follow. */
    int getVersion() const noexcept   { return version.load(); }

    /** Whether any slot of any bus has a plugin that plays. */
    bool hasEffects() const;

    void prepare (double sampleRate, int maximumBlockSize);
    void release();

    /** Plays each bus's pair of channels, after the pair played straight out, through its chain, in place. */
    void process (juce::AudioBuffer<float>& routed, int numSamples);

private:
    struct Slot
    {
        std::unique_ptr<juce::AudioPluginInstance> plugin;
        std::optional<juce::PluginDescription> description;
        juce::String missingState;
        bool bypassed = false;
        juce::AudioBuffer<float> scratch;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void prepareSlot (Slot&) const;
    void replace (int bus, int slot, Slot&& replacement);
    Slot& at (int bus, int slot)               { return slots[(size_t) bus][(size_t) slot]; }
    const Slot& at (int bus, int slot) const   { return slots[(size_t) bus][(size_t) slot]; }

    juce::AudioPluginFormatManager formats;
    juce::KnownPluginList knownPlugins;
    juce::File knownPluginsFile;

    std::array<std::array<Slot, slotsPerBus>, numBuses> slots;
    juce::CriticalSection lock;
    std::atomic<double> sampleRate { 0.0 };
    std::atomic<int> blockSize { 0 };
    std::atomic<int> version { 0 };
    juce::MidiBuffer noMidi;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectRack)
};
