cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
# Product identity. Decide these BEFORE using the plugin in real projects:
# changing the manufacturer/plugin codes later makes hosts treat it as a
# different plugin, and saved Ableton sets will no longer find it.
# ---------------------------------------------------------------------------
set(BLEEP_COMPANY_NAME      "BLEEP Audio")
set(BLEEP_MANUFACTURER_CODE "Blpa")   # 4 chars, at least one uppercase
set(BLEEP_PLUGIN_CODE       "Bl01")   # 4 chars, unique per product
set(BLEEP_BUNDLE_ID         "com.bleepaudio.bleep001")

if(APPLE)
    # JUCE 9 / Xcode 27 require macOS 12 or newer.
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()

project(BLEEP001 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BLEEP_BUILD_PLUGIN "Build the JUCE plugin" ON)
option(BLEEP_BUILD_TESTS  "Build the JUCE-free DSP tests" ON)

# ---------------------------------------------------------------------------
# DSP core: pure C++17, no JUCE. Shared by the plugin and the tests, and by
# future products.
# ---------------------------------------------------------------------------
add_library(bleep_dsp STATIC
    Source/dsp/SynthEngine.cpp
    Source/seq/Pattern.cpp
    Source/seq/Sequencer.cpp)
target_include_directories(bleep_dsp PUBLIC Source)
target_compile_features(bleep_dsp PUBLIC cxx_std_17)
set_target_properties(bleep_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
if(BLEEP_BUILD_PLUGIN)
    set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE" CACHE PATH "Path to JUCE")
    if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "JUCE not found at ${JUCE_DIR}. Run ./build.sh (it fetches JUCE) "
                            "or pass -DJUCE_DIR=/path/to/JUCE")
    endif()
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

    set(BLEEP_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND BLEEP_FORMATS AU)
    endif()

    juce_add_plugin(BLEEP001
        PRODUCT_NAME              "BLEEP 001"
        COMPANY_NAME              "${BLEEP_COMPANY_NAME}"
        BUNDLE_ID                 "${BLEEP_BUNDLE_ID}"
        PLUGIN_MANUFACTURER_CODE  "${BLEEP_MANUFACTURER_CODE}"
        PLUGIN_CODE               "${BLEEP_PLUGIN_CODE}"
        FORMATS                   ${BLEEP_FORMATS}
        IS_SYNTH                  TRUE
        NEEDS_MIDI_INPUT          TRUE
        NEEDS_MIDI_OUTPUT         FALSE
        IS_MIDI_EFFECT            FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES           Instrument Synth
        AU_MAIN_TYPE              kAudioUnitType_MusicDevice
        COPY_PLUGIN_AFTER_BUILD   TRUE)

    target_sources(BLEEP001 PRIVATE
        Source/plugin/Parameters.cpp
        Source/plugin/PluginProcessor.cpp
        Source/plugin/PluginEditor.cpp
        Source/plugin/StepGrid.cpp)

    target_compile_definitions(BLEEP001 PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(BLEEP001
        PRIVATE
            bleep_dsp
            juce::juce_audio_utils
            juce::juce_audio_processors
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BLEEP_BUILD_TESTS)
    enable_testing()
    add_executable(bleep_tests tests/EngineTests.cpp)
    target_link_libraries(bleep_tests PRIVATE bleep_dsp)
    add_test(NAME engine COMMAND bleep_tests)

    add_executable(bleep_seq_tests tests/SequencerTests.cpp)
    target_link_libraries(bleep_seq_tests PRIVATE bleep_dsp)
    add_test(NAME sequencer COMMAND bleep_seq_tests)
endif()

option(BLEEP_BUILD_PLUGIN_TESTS "Build headless tests of the JUCE processor" OFF)
if(BLEEP_BUILD_PLUGIN AND BLEEP_BUILD_PLUGIN_TESTS)
    add_executable(bleep_plugin_tests tests/PluginTests.cpp)
    # Use the plugin's already-compiled JUCE code; only borrow its headers and defines.
    target_include_directories(bleep_plugin_tests PRIVATE Source "${JUCE_DIR}/modules"
        $<TARGET_PROPERTY:BLEEP001,INCLUDE_DIRECTORIES>)
    target_compile_definitions(bleep_plugin_tests PRIVATE
        $<TARGET_PROPERTY:BLEEP001,COMPILE_DEFINITIONS>)
    target_link_libraries(bleep_plugin_tests PRIVATE BLEEP001)
    add_test(NAME plugin COMMAND bleep_plugin_tests)
endif()
