#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include <functional>
#include <vector>

/** The MIDI devices the standalone app plays from, which the window shows and lets you switch on and off. A plugin
    takes its MIDI from its host, so it has none of these.
*/
class MidiInputs
{
public:
    virtual ~MidiInputs() = default;

    struct Input
    {
        juce::String identifier, name;
        bool on = false;

        bool operator== (const Input&) const = default;
    };

    /** The MIDI devices plugged in, and whether each one is played from. */
    virtual std::vector<Input> getInputs() const = 0;

    /** Switches playing from a device on or off. */
    virtual void setOn (const juce::String& identifier, bool on) = 0;

    /** Opens the window for choosing the audio and MIDI devices. */
    virtual void showSettings() = 0;

    /** The standalone app's MIDI inputs, or nothing in a plugin. */
    static MidiInputs* get() noexcept;

    /** The standalone app gives its MIDI inputs as it starts, and takes them away, with nothing, as it quits. */
    static void set (MidiInputs*) noexcept;
};

/** Plays from every MIDI device that's plugged in, as soon as it's plugged in, except any that have been switched off.
    It remembers those, so one switched off stays off, through being unplugged and plugged in again and through the
    app quitting, until it's switched on again, here or in the audio and MIDI settings.
*/
class AutoMidiInputs final : public MidiInputs,
                             private juce::Timer,
                             private juce::ChangeListener
{
public:
    /** What it needs from the app's devices. Tests can stand in for them. */
    struct Devices
    {
        std::function<juce::Array<juce::MidiDeviceInfo>()> list = [] { return juce::MidiInput::getAvailableDevices(); };
        std::function<bool (const juce::String& identifier)> isOn;
        std::function<void (const juce::String& identifier, bool on)> setOn;
        std::function<void()> showSettings;

        /** Says when devices are switched on or off elsewhere, such as in the audio and MIDI settings, if anything. */
        juce::ChangeBroadcaster* changes = nullptr;
    };

    /** Plays from the devices in an app's device manager. */
    static Devices getDevices (juce::AudioDeviceManager&, std::function<void()> showSettings);

    /** Starts playing from the devices plugged in now, and checks for more every half second. The devices switched
        off are kept in the settings.
    */
    AutoMidiInputs (Devices, juce::PropertySet& settings);
    ~AutoMidiInputs() override;

    std::vector<Input> getInputs() const override;
    void setOn (const juce::String& identifier, bool on) override;
    void showSettings() override;

    /** Plays from any device plugged in since last time, unless it's been switched off. The timer calls it. */
    void update();

    /** Notes which devices are switched on and off, after they've been switched elsewhere. */
    void devicesSwitched();

    static constexpr const char* switchedOffKey = "midiInputsSwitchedOff";

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void remember (const juce::String& identifier, bool on);

    Devices devices;
    juce::PropertySet& settings;
    juce::Array<juce::MidiDeviceInfo> pluggedIn;
    juce::StringArray switchedOff;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoMidiInputs)
};
