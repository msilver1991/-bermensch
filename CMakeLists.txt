cmake_minimum_required(VERSION 3.22)
project(Ubermensch VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# --- JUCE ----------------------------------------------------------------------------------
# Either point at a local checkout:   cmake -B build -DUBERMENSCH_JUCE_DIR=/path/to/JUCE
# or let CMake download JUCE 8.0.8 (needs git + internet on the first configure).
set(UBERMENSCH_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(UBERMENSCH_JUCE_DIR)
    add_subdirectory(${UBERMENSCH_JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Plugin formats --------------------------------------------------------------------------
set(UBERMENSCH_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND UBERMENSCH_FORMATS AU)
endif()

juce_add_plugin(Ubermensch
    COMPANY_NAME                "Ubermensch Audio"
    PLUGIN_MANUFACTURER_CODE    Ubma
    PLUGIN_CODE                 Ubmn
    FORMATS                     ${UBERMENSCH_FORMATS}
    PRODUCT_NAME                "Ubermensch"      # ASCII file name (portable)
    PLUGIN_NAME                 "Übermensch"      # name shown in the host
    DESCRIPTION                 "Freeze / sustain / synth engine"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Modulation
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(Ubermensch PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/UbermenschEngine.cpp
    Source/UbermenschEngine.h)

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

# --- Offline DSP tests (no JUCE needed):  cmake -B build -DUBERMENSCH_BUILD_TESTS=ON ----------
option(UBERMENSCH_BUILD_TESTS "Build the offline engine test" OFF)
if(UBERMENSCH_BUILD_TESTS)
    add_executable(ubermensch_engine_test Tests/engine_test.cpp Source/UbermenschEngine.cpp)
    target_include_directories(ubermensch_engine_test PRIVATE Source)
endif()
