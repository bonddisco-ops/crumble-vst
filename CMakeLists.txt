cmake_minimum_required(VERSION 3.22)

# Universal macOS build that also runs on older macOS versions
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
# Static MSVC runtime: the plugin does not depend on the host's C++ runtime DLLs
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Crumble VERSION 3.3.0)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)


if(APPLE)
    set(CRUMBLE_FORMATS VST3 AU Standalone)
else()
    set(CRUMBLE_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Crumble
    COMPANY_NAME "Primal Rage Lab"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Dlyc
    PLUGIN_CODE Crmb
    FORMATS ${CRUMBLE_FORMATS}
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE
    PRODUCT_NAME "Crumble")

target_sources(Crumble PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Components.cpp
    Source/PresetManager.cpp)

target_compile_definitions(Crumble PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# Russian help texts are UTF-8 string literals
target_compile_options(Crumble PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(Crumble
    PRIVATE juce::juce_audio_utils
            juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_warning_flags)
