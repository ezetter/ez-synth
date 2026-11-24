#include "EffectRack.h"

EffectRack::EffectRack()
{
    formats.addFormat (std::make_unique<juce::VST3PluginFormat>());
   #if JUCE_PLUGINHOST_AU && JUCE_MAC
    formats.addFormat (std::make_unique<juce::AudioUnitPluginFormat>());
   #endif
    knownPlugins.addChangeListener (this);
}

EffectRack::~EffectRack()
{
    knownPlugins.removeChangeListener (this);
    clear();
}

void EffectRack::keepKnownPluginsIn (const juce::File& file)
{
    knownPluginsFile = juce::File();

    if (const auto xml = juce::XmlDocument::parse (file))
        knownPlugins.recreateFromXml (*xml);

    knownPluginsFile = file;
}

void EffectRack::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (knownPluginsFile == juce::File())
        return;

    if (const auto xml = knownPlugins.createXml())
        if (knownPluginsFile.getParentDirectory().createDirectory())
            xml->writeTo (knownPluginsFile);
}

juce::File EffectRack::getCrashedPluginFile() const
{
    return (knownPluginsFile != juce::File() ? knownPluginsFile.getParentDirectory()
                                             : juce::File::getSpecialLocation (juce::File::tempDirectory))
        .getChildFile ("Effect plugin being looked at.txt");
}

juce::Array<juce::PluginDescription> EffectRack::getEffects() const
{
    juce::Array<juce::PluginDescription> effects;

    for (const auto& description : knownPlugins.getTypes())
        if (! description.isInstrument)
            effects.add (description);

    std::sort (effects.begin(), effects.end(), [] (const juce::PluginDescription& a, const juce::PluginDescription& b)
    {
        if (const auto byMaker = a.manufacturerName.compareIgnoreCase (b.manufacturerName); byMaker != 0)
            return byMaker < 0;

        if (const auto byName = a.name.compareIgnoreCase (b.name); byName != 0)
            return byName < 0;

        return a.pluginFormatName < b.pluginFormatName;
    });

    return effects;
}

EffectRack::SlotInfo EffectRack::getSlot (int bus, int slot) const
{
    const auto& held = at (bus, slot);

    if (! held.description.has_value())
        return {};

    return { held.description->name, held.plugin != nullptr, held.plugin == nullptr, held.bypassed };
}

juce::AudioPluginInstance* EffectRack::getPlugin (int bus, int slot) const
{
    return at (bus, slot).plugin.get();
}

void EffectRack::prepareSlot (Slot& slot) const
{
    auto& plugin = *slot.plugin;

    // Its main input and output in stereo, if it can have them, or as it comes if not.
    auto layout = plugin.getBusesLayout();

    if (! layout.inputBuses.isEmpty())
        layout.inputBuses.getReference (0) = juce::AudioChannelSet::stereo();

    if (! layout.outputBuses.isEmpty())
        layout.outputBuses.getReference (0) = juce::AudioChannelSet::stereo();

    plugin.setBusesLayout (layout);

    const auto rate = sampleRate.load();
    const auto size = std::max (1, blockSize.load());

    if (rate > 0.0)
    {
        plugin.setRateAndBufferSizeDetails (rate, size);
        plugin.prepareToPlay (rate, size);
    }

    slot.scratch.setSize (std::max ({ 2, plugin.getTotalNumInputChannels(), plugin.getTotalNumOutputChannels() }), size);
}

void EffectRack::replace (int bus, int slot, Slot&& replacement)
{
    auto& held = at (bus, slot);

    if (held.plugin != nullptr && beforeRemoving != nullptr)
        beforeRemoving (*held.plugin);

    // Swapped while the audio thread isn't playing it, and the old one let go of afterwards.
    {
        const juce::ScopedLock sl (lock);
        std::swap (held, replacement);
    }

    if (replacement.plugin != nullptr)
        replacement.plugin->releaseResources();

    ++version;
}

juce::String EffectRack::load (int bus, int slot, const juce::PluginDescription& description)
{
    juce::String error;
    const auto rate = sampleRate.load() > 0.0 ? sampleRate.load() : 44100.0;
    auto plugin = formats.createPluginInstance (description, rate, std::max (512, blockSize.load()), error);

    if (plugin == nullptr)
        return error.isNotEmpty() ? error : "It couldn't be opened.";

    Slot loaded;
    loaded.plugin = std::move (plugin);
    loaded.description = description;
    prepareSlot (loaded);
    replace (bus, slot, std::move (loaded));
    return {};
}

void EffectRack::remove (int bus, int slot)
{
    replace (bus, slot, {});
}

void EffectRack::setBypassed (int bus, int slot, bool bypassed)
{
    {
        const juce::ScopedLock sl (lock);
        at (bus, slot).bypassed = bypassed;
    }

    ++version;
}

void EffectRack::clear()
{
    for (int bus = 0; bus < numBuses; ++bus)
        for (int slot = 0; slot < slotsPerBus; ++slot)
            if (at (bus, slot).description.has_value())
                remove (bus, slot);
}

juce::ValueTree EffectRack::save() const
{
    juce::ValueTree state (stateType);

    for (int bus = 0; bus < numBuses; ++bus)
    {
        for (int slot = 0; slot < slotsPerBus; ++slot)
        {
            const auto& held = at (bus, slot);

            if (! held.description.has_value())
                continue;

            juce::ValueTree entry ("SLOT");
            entry.setProperty ("bus", bus + 1, nullptr);
            entry.setProperty ("slot", slot + 1, nullptr);
            entry.setProperty ("bypassed", held.bypassed, nullptr);

            if (held.plugin != nullptr)
            {
                juce::MemoryBlock settings;
                held.plugin->getStateInformation (settings);
                entry.setProperty ("settings", settings.toBase64Encoding(), nullptr);
            }
            else
            {
                entry.setProperty ("settings", held.missingState, nullptr);
            }

            if (const auto xml = held.description->createXml())
                entry.appendChild (juce::ValueTree::fromXml (*xml), nullptr);

            state.appendChild (entry, nullptr);
        }
    }

    return state;
}

void EffectRack::restore (const juce::ValueTree& state)
{
    clear();

    for (const auto& entry : state)
    {
        const auto bus = (int) entry["bus"] - 1, slot = (int) entry["slot"] - 1;

        if (! entry.hasType ("SLOT") || ! juce::isPositiveAndBelow (bus, numBuses) || ! juce::isPositiveAndBelow (slot, slotsPerBus))
            continue;

        juce::PluginDescription description;
        const auto xml = entry.getChild (0).createXml();

        if (xml == nullptr || ! description.loadFromXml (*xml))
            continue;

        // The plugin as it's found here, if it is, which may be a newer version than the one saved.
        if (const auto found = knownPlugins.getTypeForIdentifierString (description.createIdentifierString()))
            description = *found;

        const auto settings = entry["settings"].toString();

        if (load (bus, slot, description).isEmpty())
        {
            if (juce::MemoryBlock block; settings.isNotEmpty() && block.fromBase64Encoding (settings))
                getPlugin (bus, slot)->setStateInformation (block.getData(), (int) block.getSize());
        }
        else
        {
            Slot missing;
            missing.description = description;
            missing.missingState = settings;
            replace (bus, slot, std::move (missing));
        }

        setBypassed (bus, slot, (bool) entry["bypassed"]);
    }
}

bool EffectRack::hasEffects() const
{
    for (const auto& bus : slots)
        for (const auto& slot : bus)
            if (slot.plugin != nullptr && ! slot.bypassed)
                return true;

    return false;
}

void EffectRack::prepare (double newSampleRate, int maximumBlockSize)
{
    const juce::ScopedLock sl (lock);
    sampleRate = newSampleRate;
    blockSize = std::max (1, maximumBlockSize);

    for (auto& bus : slots)
        for (auto& slot : bus)
            if (slot.plugin != nullptr)
                prepareSlot (slot);
}

void EffectRack::release()
{
    const juce::ScopedLock sl (lock);

    for (auto& bus : slots)
        for (auto& slot : bus)
            if (slot.plugin != nullptr)
                slot.plugin->releaseResources();
}

void EffectRack::process (juce::AudioBuffer<float>& routed, int numSamples)
{
    const juce::ScopedTryLock sl (lock);

    if (! sl.isLocked())
        return;

    for (int bus = 0; bus < numBuses; ++bus)
    {
        const auto leftChannel = 2 * (bus + 1);

        if (routed.getNumChannels() < leftChannel + 2)
            return;

        for (auto& slot : slots[(size_t) bus])
        {
            if (slot.plugin == nullptr || slot.bypassed)
                continue;

            auto& plugin = *slot.plugin;
            const auto ins = plugin.getTotalNumInputChannels(), outs = plugin.getTotalNumOutputChannels();

            // In pieces no longer than it was prepared for.
            for (int start = 0; start < numSamples; start += slot.scratch.getNumSamples())
            {
                const auto length = std::min (slot.scratch.getNumSamples(), numSamples - start);
                auto* left = routed.getWritePointer (leftChannel, start);
                auto* right = routed.getWritePointer (leftChannel + 1, start);
                juce::AudioBuffer<float> piece (slot.scratch.getArrayOfWritePointers(), slot.scratch.getNumChannels(), length);
                piece.clear();

                if (ins >= 2)
                {
                    piece.copyFrom (0, 0, left, length);
                    piece.copyFrom (1, 0, right, length);
                }
                else if (ins == 1)
                {
                    piece.copyFrom (0, 0, left, length, 0.5f);
                    piece.addFrom (0, 0, right, length, 0.5f);
                }

                noMidi.clear();
                plugin.processBlock (piece, noMidi);

                if (outs >= 2)
                {
                    juce::FloatVectorOperations::copy (left, piece.getReadPointer (0), length);
                    juce::FloatVectorOperations::copy (right, piece.getReadPointer (1), length);
                }
                else if (outs == 1)
                {
                    juce::FloatVectorOperations::copy (left, piece.getReadPointer (0), length);
                    juce::FloatVectorOperations::copy (right, piece.getReadPointer (0), length);
                }
            }
        }
    }
}
