cmake_minimum_required(VERSION 3.22)
project(minihost VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")

if(NOT JUCE_DIR AND DEFINED ENV{JUCE_DIR} AND NOT "$ENV{JUCE_DIR}" STREQUAL "")
    set(JUCE_DIR "$ENV{JUCE_DIR}" CACHE PATH "Path to a local JUCE checkout" FORCE)
endif()

if(NOT JUCE_DIR)
    message(FATAL_ERROR
        "JUCE_DIR is not set. Pass -DJUCE_DIR=/path/to/JUCE or set the JUCE_DIR environment variable.")
endif()

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE_DIR does not point to a JUCE checkout: ${JUCE_DIR}")
endif()

add_subdirectory("${JUCE_DIR}" juce_build)

juce_add_gui_app(minihost)
juce_generate_juce_header(minihost)

target_sources(minihost PRIVATE
    Source/Main.cpp
    Source/HostApp.cpp
    Source/HostApp.h
)

target_include_directories(minihost PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_compile_definitions(minihost PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_PLUGINHOST_LV2=0
    JUCE_STANDALONE_APPLICATION=1
)


target_link_libraries(minihost PRIVATE
    juce::juce_core
    juce::juce_events
    juce::juce_data_structures
    juce::juce_gui_basics
    juce::juce_gui_extra

    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
