#include "MidiInputs.h"

namespace
{
    MidiInputs* standaloneInputs = nullptr;
}

MidiInputs* MidiInputs::get() noexcept
{
    return standaloneInputs;
}

void MidiInputs::set (MidiInputs* inputs) noexcept
{
    standaloneInputs = inputs;
}

//==============================================================================
AutoMidiInputs::Devices AutoMidiInputs::getDevices (juce::AudioDeviceManager& deviceManager, std::function<void()> showSettings)
{
    Devices result;
    result.isOn = [&deviceManager] (const juce::String& identifier) { return deviceManager.isMidiInputDeviceEnabled (identifier); };
    result.setOn = [&deviceManager] (const juce::String& identifier, bool on) { deviceManager.setMidiInputDeviceEnabled (identifier, on); };
    result.showSettings = std::move (showSettings);
    result.changes = &deviceManager;
    return result;
}

AutoMidiInputs::AutoMidiInputs (Devices devicesToUse, juce::PropertySet& settingsToUse)
    : devices (std::move (devicesToUse)),
      settings (settingsToUse),
      switchedOff (juce::StringArray::fromLines (settings.getValue (switchedOffKey)))
{
    switchedOff.removeEmptyStrings();

    // The devices plugged in now are new to it, so it plays from them, before it starts noting what's switched
    // elsewhere. Otherwise those it hasn't switched on yet would count as switched off.
    update();

    if (devices.changes != nullptr)
        devices.changes->addChangeListener (this);

    startTimer (500);
}

AutoMidiInputs::~AutoMidiInputs()
{
    stopTimer();

    if (devices.changes != nullptr)
        devices.changes->removeChangeListener (this);
}

std::vector<MidiInputs::Input> AutoMidiInputs::getInputs() const
{
    std::vector<Input> inputs;

    for (const auto& device : pluggedIn)
        inputs.push_back ({ device.identifier, device.name, devices.isOn (device.identifier) });

    return inputs;
}

void AutoMidiInputs::setOn (const juce::String& identifier, bool on)
{
    remember (identifier, on);
    devices.setOn (identifier, on);
}

void AutoMidiInputs::showSettings()
{
    if (devices.showSettings != nullptr)
        devices.showSettings();
}

void AutoMidiInputs::update()
{
    const auto now = devices.list();

    if (now == pluggedIn)
        return;

    // A device that's been unplugged is let go, so it can be opened afresh when it's plugged in again.
    for (const auto& device : pluggedIn)
        if (! now.contains (device))
            devices.setOn (device.identifier, false);

    for (const auto& device : now)
        if (! pluggedIn.contains (device) && ! switchedOff.contains (device.identifier))
            devices.setOn (device.identifier, true);

    pluggedIn = now;
}

void AutoMidiInputs::devicesSwitched()
{
    for (const auto& device : pluggedIn)
        remember (device.identifier, devices.isOn (device.identifier));
}

void AutoMidiInputs::timerCallback()
{
    update();
}

void AutoMidiInputs::changeListenerCallback (juce::ChangeBroadcaster*)
{
    devicesSwitched();
}

void AutoMidiInputs::remember (const juce::String& identifier, bool on)
{
    const auto wasOff = switchedOff.contains (identifier);

    if (on == ! wasOff)
        return;

    if (on)
        switchedOff.removeString (identifier);
    else
        switchedOff.add (identifier);

    settings.setValue (switchedOffKey, switchedOff.joinIntoString ("\n"));
}
