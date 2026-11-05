cmake_minimum_required(VERSION 3.22)
project(Ubermensch VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

set(UBERMENSCH_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND UBERMENSCH_FORMATS AU)
endif()

juce_add_plugin(Ubermensch
    COMPANY_NAME                "Ubermensch Audio"
    PLUGIN_MANUFACTURER_CODE    Ubma
    PLUGIN_CODE                 Ubmn
    FORMATS                     ${UBERMENSCH_FORMATS}
    PRODUCT_NAME                "Ubermensch"
    PLUGIN_NAME                 "Übermensch"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    VST3_CATEGORIES             Fx Modulation
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(Ubermensch PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    UbermenschEngine.cpp)

target_compile_definitions(Ubermensch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(Ubermensch PRIVATE /utf-8)
endif()

target_link_libraries(Ubermensch
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
