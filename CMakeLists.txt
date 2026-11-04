cmake_minimum_required(VERSION 3.22)

# Must be set before project() so it applies to every target, including JUCE.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(Echoform
    VERSION 0.2.0
    DESCRIPTION "ECHOFORM stereo delay and reverb processor"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Keep JUCE helper targets (juceaide etc.) out of the IDE root folder.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(ECHOFORM_BUILD_TESTS "Build the GoogleTest suites" ON)
option(ECHOFORM_WARNINGS_AS_ERRORS "Treat warnings in ECHOFORM's own sources as errors" OFF)
option(ECHOFORM_COPY_PLUGIN_AFTER_BUILD "Copy built plug-ins into the user's plug-in folders" OFF)

# Provisional identity. These values are persisted by hosts in sessions; they
# MUST be finalised before the first public release and never changed after.
set(ECHOFORM_COMPANY_NAME "Echoform" CACHE STRING "Company name shown in hosts")
set(ECHOFORM_BUNDLE_ID "com.echoform.echoform" CACHE STRING "Reverse-DNS bundle identifier")
set(ECHOFORM_MANUFACTURER_CODE "Ecfm" CACHE STRING "4-char manufacturer code (>=1 upper-case letter)")
set(ECHOFORM_PLUGIN_CODE "Efrm" CACHE STRING "4-char plug-in code (exactly 1 upper-case letter for AU)")

include(cmake/EchoformCompilerSettings.cmake)

# ---------------------------------------------------------------------------
# JUCE (git submodule pinned to tag 8.0.17-security, see docs/DEPENDENCIES_AND_LICENSING.md)
# ---------------------------------------------------------------------------
set(ECHOFORM_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE" CACHE PATH "Path to the JUCE source tree")
if(NOT EXISTS "${ECHOFORM_JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE was not found at '${ECHOFORM_JUCE_DIR}'.\n"
        "Run: git submodule update --init --depth 1 libs/JUCE")
endif()
add_subdirectory("${ECHOFORM_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# Pure C++ DSP + parameter-model library (no JUCE dependency, unit-testable)
# ---------------------------------------------------------------------------
add_subdirectory(source/core)

# ---------------------------------------------------------------------------
# Plug-in
# ---------------------------------------------------------------------------
set(ECHOFORM_FORMATS VST3)
if(APPLE)
    list(APPEND ECHOFORM_FORMATS AU)
endif()

juce_add_plugin(Echoform
    PRODUCT_NAME "Echoform"
    PLUGIN_NAME "Echoform"
    COMPANY_NAME "${ECHOFORM_COMPANY_NAME}"
    BUNDLE_ID "${ECHOFORM_BUNDLE_ID}"
    DESCRIPTION "Stereo delay and reverb processor"
    VERSION "${PROJECT_VERSION}"
    PLUGIN_MANUFACTURER_CODE "${ECHOFORM_MANUFACTURER_CODE}"
    PLUGIN_CODE "${ECHOFORM_PLUGIN_CODE}"
    FORMATS ${ECHOFORM_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${ECHOFORM_COPY_PLUGIN_AFTER_BUILD}
    VST3_CATEGORIES Fx Delay Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(Echoform PRIVATE
    source/plugin/PluginProcessor.cpp
    source/plugin/PluginProcessor.h
    source/plugin/PluginEditor.cpp
    source/plugin/PluginEditor.h
    source/plugin/ParameterLayout.cpp
    source/plugin/ParameterLayout.h
    source/plugin/ParameterBridge.cpp
    source/plugin/ParameterBridge.h
    source/plugin/StateSerializer.cpp
    source/plugin/StateSerializer.h
    source/ui/Theme.h)

target_include_directories(Echoform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/source")

# PUBLIC so the definitions also reach the format wrapper targets (Echoform_VST3, ...).
target_compile_definitions(Echoform PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
    DONT_SET_USING_JUCE_NAMESPACE=1)

target_link_libraries(Echoform
    PRIVATE
        echoform_core
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

echoform_apply_warning_policy(Echoform
    source/plugin/PluginProcessor.cpp
    source/plugin/PluginEditor.cpp
    source/plugin/ParameterLayout.cpp
    source/plugin/ParameterBridge.cpp
    source/plugin/StateSerializer.cpp)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(ECHOFORM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
