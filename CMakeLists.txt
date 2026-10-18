cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

project(EZSynth VERSION 0.4.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.2.tar.gz
    URL_HASH SHA256=16d01c27e8327f3644306cc5c223aa6e21af1cc1ec62faa709fc0dced57d208a
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(JUCE)

# One codebase, three products: a standalone Mac app, a VST3 instrument and an Audio Unit instrument.
juce_add_plugin(EZSynth
    PRODUCT_NAME "EZ Synth"
    COMPANY_NAME "EZ Synth"
    BUNDLE_ID "com.ez-synth.synth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE Ezsy
    PLUGIN_CODE Ezs1
    FORMATS Standalone VST3 AU
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(EZSynth
    PRIVATE
        Source/ChoiceButtons.cpp
        Source/FilterPanel.cpp
        Source/FilterStage.cpp
        Source/HarmonicSelector.cpp
        Source/HarmonicSliders.cpp
        Source/HarmonicVoice.cpp
        Source/Keyboard.cpp
        Source/LfoPanel.cpp
        Source/ModulatableControls.cpp
        Source/Modulation.cpp
        Source/ModulationEditor.cpp
        Source/SourceCard.cpp
        Source/SynthEditor.cpp
        Source/SynthLookAndFeel.cpp
        Source/SynthProcessor.cpp
        Source/WaveGrid.cpp
        Source/Waveform.cpp)

target_compile_definitions(EZSynth
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(EZSynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Building installs the plugins where hosts find them: the VST3 in the system-wide folder, beside other VST3
# plugins, and the Audio Unit in the user's own. The system folder needs a one-time step by an administrator,
# which the build explains; see cmake/InstallPlugin.cmake.
set(EZSYNTH_VST3_FOLDER "/Library/Audio/Plug-Ins/VST3" CACHE PATH "Where building installs the VST3 plugin")
set(EZSYNTH_AU_FOLDER "$ENV{HOME}/Library/Audio/Plug-Ins/Components" CACHE PATH "Where building installs the Audio Unit")

foreach(format VST3 AU)
    add_custom_command(TARGET EZSynth_${format} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" "-Dbundle=$<TARGET_BUNDLE_DIR:EZSynth_${format}>" "-Dfolder=${EZSYNTH_${format}_FOLDER}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/InstallPlugin.cmake"
        VERBATIM)
endforeach()

# The linker signs only the standalone app's executable. Sign the whole app too, so macOS sees an intact bundle.
add_custom_command(TARGET EZSynth_Standalone POST_BUILD
    COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:EZSynth_Standalone>"
    VERBATIM)

# Checks the harmonic analysis and the synth's output, and can save pictures of the editor. Like JUCE's own
# plugin wrappers, it borrows the shared code's include paths rather than linking JUCE a second time.
add_executable(EZSynthTests Tests/Tests.cpp)
target_include_directories(EZSynthTests PRIVATE $<TARGET_PROPERTY:EZSynth,INCLUDE_DIRECTORIES>)
target_link_libraries(EZSynthTests PRIVATE EZSynth)
