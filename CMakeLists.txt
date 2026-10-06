cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel) and a broadly compatible macOS target.
# Must be set before project().
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
endif()

# Static MSVC runtime so Windows users don't need the VC++ redistributable.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

project(MUTEGHIRA VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MUTEGHIRA_BUILD_TESTS "Build the logic tests / UI snapshot tool" OFF)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.9
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(MUTEGHIRA_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

juce_add_plugin(MUTEGHIRA
    COMPANY_NAME                Muteghira
    PLUGIN_MANUFACTURER_CODE    Mtgh
    PLUGIN_CODE                 Mgh1
    FORMATS                     VST3
    PRODUCT_NAME                "MUTEGHIRA"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    VST3_CATEGORIES             Fx Tools
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(MUTEGHIRA PRIVATE ${MUTEGHIRA_SOURCES})

target_compile_definitions(MUTEGHIRA PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MUTEGHIRA
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(MUTEGHIRA_BUILD_TESTS)
    juce_add_console_app(MuteghiraTests PRODUCT_NAME "MuteghiraTests")
    target_sources(MuteghiraTests PRIVATE
        tests/Tests.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(MuteghiraTests PRIVATE Source)
    target_compile_definitions(MuteghiraTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(MuteghiraTests PRIVATE
        juce::juce_audio_processors juce::juce_gui_basics
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
