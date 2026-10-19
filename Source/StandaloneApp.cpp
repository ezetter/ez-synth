// EZ Synth's standalone app, which only the standalone build includes. It's JUCE's own standalone app, but it plays
// from every MIDI device that's plugged in, as soon as it's plugged in, rather than waiting for each to be chosen in
// the audio and MIDI settings. The plugins take their MIDI from their hosts.

#include "MidiInputs.h"

#include <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>
#include <juce_audio_plugin_client/detail/juce_IncludeSystemHeaders.h>
#include <juce_audio_plugin_client/detail/juce_IncludeModuleHeaders.h>
#include <juce_audio_plugin_client/detail/juce_PluginUtilities.h>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace
{
    class StandaloneApp final : public juce::JUCEApplication
    {
    public:
        StandaloneApp()
        {
            juce::PropertiesFile::Options options;
            options.applicationName = juce::CharPointer_UTF8 (JucePlugin_Name);
            options.filenameSuffix = ".settings";
            options.osxLibrarySubFolder = "Application Support";
            appProperties.setStorageParameters (options);
        }

        const juce::String getApplicationName() override              { return juce::CharPointer_UTF8 (JucePlugin_Name); }
        const juce::String getApplicationVersion() override           { return JucePlugin_VersionString; }
        bool moreThanOneInstanceAllowed() override                    { return true; }
        void anotherInstanceStarted (const juce::String&) override    {}

        void initialise (const juce::String&) override
        {
            // The MIDI inputs come before the window, so the window finds them as it opens.
            auto holder = std::make_unique<juce::StandalonePluginHolder> (appProperties.getUserSettings(), false);
            auto& heldHolder = *holder;
            midiInputs = std::make_unique<AutoMidiInputs> (AutoMidiInputs::getDevices (heldHolder.deviceManager,
                                                                                        [&heldHolder] { heldHolder.showAudioSettingsDialog(); }),
                                                           *appProperties.getUserSettings());
            MidiInputs::set (midiInputs.get());

            if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
            {
                pluginHolder = std::move (holder);
                return;
            }

            const auto background = juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
            mainWindow = std::make_unique<juce::StandaloneFilterWindow> (getApplicationName(), background, std::move (holder));
            mainWindow->setVisible (true);
        }

        void shutdown() override
        {
            // The MIDI inputs go first, as they use the devices the window's plugin holder looks after.
            MidiInputs::set (nullptr);
            midiInputs = nullptr;
            pluginHolder = nullptr;
            mainWindow = nullptr;
            appProperties.saveIfNeeded();
        }

        void systemRequestedQuit() override
        {
            if (pluginHolder != nullptr)
                pluginHolder->savePluginState();

            if (mainWindow != nullptr)
                mainWindow->pluginHolder->savePluginState();

            if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
            {
                juce::Timer::callAfterDelay (100, []
                {
                    if (auto* app = juce::JUCEApplicationBase::getInstance())
                        app->systemRequestedQuit();
                });
            }
            else
            {
                quit();
            }
        }

    private:
        juce::ApplicationProperties appProperties;
        std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
        std::unique_ptr<AutoMidiInputs> midiInputs;
        std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
    };
}

juce::JUCEApplicationBase* juce_CreateApplication();

juce::JUCEApplicationBase* juce_CreateApplication()
{
    return new StandaloneApp();
}
