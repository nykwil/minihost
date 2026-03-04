cmake_minimum_required(VERSION 3.22)
project(minihost VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

# Add JUCE
add_subdirectory("D:/Nykwil/Audio/JUCE" juce_build)

juce_add_gui_app(minihost)
juce_generate_juce_header(minihost)

target_sources(minihost PRIVATE
    Source/Main.cpp
    Source/HostApp.cpp
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
