cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
project(VoltaResonance VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
)
FetchContent_MakeAvailable(JUCE)

# Platform-specific settings
if(APPLE)
    set(VOLTA_FORMATS AU VST3)
    set(VOLTA_COPY_AFTER_BUILD TRUE)
else()
    set(VOLTA_FORMATS VST3)
    set(VOLTA_COPY_AFTER_BUILD FALSE)   # installer handles deployment
endif()

juce_add_plugin(VoltaResonance
    COMPANY_NAME                "Volta Labs"
    PLUGIN_MANUFACTURER_CODE    Volt
    PLUGIN_CODE                 Vrs1
    FORMATS                     ${VOLTA_FORMATS}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT             TRUE
    NEEDS_MIDI_OUTPUT            FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS  FALSE
    COPY_PLUGIN_AFTER_BUILD      ${VOLTA_COPY_AFTER_BUILD}
    PRODUCT_NAME                "VOLTA Resonance"
    BUNDLE_ID                    "com.voltalabs.voltaresonance"
    VST3_CATEGORIES              "Instrument|Synth"
    AU_MAIN_TYPE                 "kAudioUnitType_MusicDevice"
)

# Generate JuceHeader.h (required in JUCE 8 — no longer auto-generated)
juce_generate_juce_header(VoltaResonance)

# Network entitlement for VoltaSync (allows outbound TCP connections)
set_target_properties(VoltaResonance PROPERTIES
    XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/VoltaResonance.entitlements"
)

target_sources(VoltaResonance
    PRIVATE
        Source/DrumSynth.cpp
        Source/StepSequencer.cpp
        Source/SamplePlayer.cpp
        Source/VoltaSync.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_include_directories(VoltaResonance
    PRIVATE
        Source
)

target_compile_definitions(VoltaResonance
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(VoltaResonance
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)
