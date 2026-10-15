# Closer: build with CMake (used for macOS builds on GitHub Actions).
# On Windows the project is still built from imager.jucer with the Projucer.
#
#   git clone --depth 1 --branch 9.0.3 https://github.com/juce-framework/JUCE.git JUCE
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release

cmake_minimum_required (VERSION 3.22)

# Universal binary (Apple Silicon + Intel), runs on macOS 11 or newer
set (CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set (CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")

project (Closer VERSION 0.4.0)

set (JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Path to the JUCE source folder")
add_subdirectory ("${JUCE_PATH}" JUCE)

# Same identity as imager.jucer, so hosts see the same plugin on every platform
juce_add_plugin (Closer
    PRODUCT_NAME              "Closer"
    COMPANY_NAME              "Rave Factory"
    BUNDLE_ID                 "com.ravefactory.closer"
    PLUGIN_MANUFACTURER_CODE  Rvfc
    PLUGIN_CODE               Clsr
    FORMATS                   VST3 AU
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES           "Fx" "Spatial"
    AU_MAIN_TYPE              kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD   FALSE)

# The sources include <JuceHeader.h>, like the Projucer project
juce_generate_juce_header (Closer)

target_sources (Closer
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions (Closer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries (Closer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
