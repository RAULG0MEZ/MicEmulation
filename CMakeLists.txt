cmake_minimum_required(VERSION 3.22)

project(MicEmulation
    VERSION 0.1.7
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.13
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(MicEmulationAssets
    SOURCES
        Assets/plugin_background.png)

juce_add_plugin(MicEmulation
    COMPANY_NAME "Raul Gomez"
    PRODUCT_NAME "MicEmulation"
    BUNDLE_ID "com.raulgomez.micemulation"
    PLUGIN_MANUFACTURER_CODE Rglm
    PLUGIN_CODE MicE
    FORMATS AU VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect)

juce_generate_juce_header(MicEmulation)

target_sources(MicEmulation
    PRIVATE
        Source/EQModel.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h)

target_compile_features(MicEmulation
    PRIVATE
        cxx_std_20)

target_compile_definitions(MicEmulation
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MicEmulation
    PRIVATE
        MicEmulationAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
