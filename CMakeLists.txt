cmake_minimum_required(VERSION 3.22)

project(Chorus VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CHORUS_BUILD_CLAP  "Build a CLAP version alongside the VST3" ON)
option(CHORUS_BUILD_TESTS "Build the DSP unit tests"                ON)

# JUCE and the CLAP wrapper are fetched rather than vendored, so a fresh clone
# needs nothing but CMake and a compiler.
include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

if(CHORUS_BUILD_CLAP)
    FetchContent_Declare(clap-juce-extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(clap-juce-extensions)
endif()

# ---------------------------------------------------------------------------
# The DSP core is a plain C++ library with no JUCE dependency, so it can be
# unit tested on its own and reused in a different plugin shell.
# ---------------------------------------------------------------------------
add_library(chorus_dsp STATIC
    source/dsp/ChorusEngine.cpp)

target_include_directories(chorus_dsp PUBLIC source)
target_compile_features(chorus_dsp PUBLIC cxx_std_17)

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
juce_add_plugin(Chorus
    COMPANY_NAME            "Independent"
    PLUGIN_MANUFACTURER_CODE Idpt
    PLUGIN_CODE              Chr1
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "Chorus"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          Fx Modulation)

target_sources(Chorus PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp)

target_compile_definitions(Chorus PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Chorus
    PRIVATE
        chorus_dsp
    PUBLIC
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(CHORUS_BUILD_CLAP)
    clap_juce_extensions_plugin(TARGET Chorus
        CLAP_ID       "com.independent.chorus"
        CLAP_FEATURES audio-effect chorus stereo)
endif()

# ---------------------------------------------------------------------------
# Tests: no framework, just a binary that returns non-zero on failure.
# ---------------------------------------------------------------------------
if(CHORUS_BUILD_TESTS)
    enable_testing()

    add_executable(chorus_dsp_test tests/dsp_test.cpp)
    target_link_libraries(chorus_dsp_test PRIVATE chorus_dsp)

    add_test(NAME chorus_dsp_test COMMAND chorus_dsp_test)

    # Links the plugin's shared-code target so the test sees the same sources
    # and JucePlugin_* defines the real plugin is built with.
    add_executable(chorus_processor_test tests/processor_test.cpp)
    target_link_libraries(chorus_processor_test PRIVATE Chorus)

    add_test(NAME chorus_processor_test COMMAND chorus_processor_test)
endif()
