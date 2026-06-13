cmake_minimum_required(VERSION 3.22)

project(BoratoMorphium VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----------------------------------------------------------------------------
# Locate JUCE
# ----------------------------------------------------------------------------
option(MORPHIUM_FETCH_JUCE "Download JUCE if it is not found locally" OFF)
option(MORPHIUM_COPY_PLUGIN_AFTER_BUILD "Install the plugin to the system folder after build" OFF)
option(MORPHIUM_BUILD_CLAP "Build the CLAP plugin through clap-juce-extensions" OFF)
option(MORPHIUM_BUILD_TESTS "Build the offline spectral test suite (Catch2 + CTest)" OFF)
set(MORPHIUM_JUCE_SOURCE_DIR "" CACHE PATH "Path to a JUCE source checkout")

if(NOT MORPHIUM_JUCE_SOURCE_DIR AND EXISTS "C:/JUCE/CMakeLists.txt")
    set(MORPHIUM_JUCE_SOURCE_DIR "C:/JUCE")
endif()

if(MORPHIUM_JUCE_SOURCE_DIR AND EXISTS "${MORPHIUM_JUCE_SOURCE_DIR}/CMakeLists.txt")
    set(JUCE_BUILD_HELPER_TOOLS OFF CACHE BOOL "Bootstrap JUCE helper tools at configure time" FORCE)
    add_subdirectory("${MORPHIUM_JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
else()
    find_package(JUCE CONFIG QUIET)
    if(NOT JUCE_FOUND)
        if(NOT MORPHIUM_FETCH_JUCE)
            message(FATAL_ERROR "JUCE not found. Set MORPHIUM_JUCE_SOURCE_DIR/JUCE_DIR/CMAKE_PREFIX_PATH or enable MORPHIUM_FETCH_JUCE.")
        endif()
        include(FetchContent)
        FetchContent_Declare(
            juce
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.13
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(juce)
    endif()
endif()

# ----------------------------------------------------------------------------
# Windows resource (.rc) helper workaround.
# JUCE 8's Windows resource helper may be emitted as the literal command
# "juce::juceaide" by some VS generators in this environment. Redefine the
# helper locally so the generated project uses the resolved executable path.
# ----------------------------------------------------------------------------
if(WIN32)
    function(_juce_add_resources_rc source_target dest_target)
        if(NOT TARGET ${source_target}_rc_lib)
            get_target_property(juce_library_code ${source_target} JUCE_GENERATED_SOURCES_DIRECTORY)
            get_target_property(input_info_file ${source_target} JUCE_INFO_FILE)
            get_target_property(generated_icon ${source_target} JUCE_ICON_FILE)

            set(dependency)
            if(generated_icon)
                set(dependency DEPENDS "${generated_icon}")
            endif()

            set(resource_rc_file "${juce_library_code}/${source_target}_resources.rc")

            add_custom_command(OUTPUT "${resource_rc_file}"
                COMMAND "$<TARGET_FILE:juce::juceaide>" rcfile "${input_info_file}" "${resource_rc_file}"
                ${dependency}
                VERBATIM)

            add_library(${source_target}_rc_lib OBJECT "${resource_rc_file}")
        endif()

        target_link_libraries(${dest_target} PRIVATE ${source_target}_rc_lib)
    endfunction()
endif()

# ----------------------------------------------------------------------------
# Plugin target
# ----------------------------------------------------------------------------
juce_add_plugin(BoratoMorphium
    COMPANY_NAME "Borato Company"
    BUNDLE_ID com.boratoaudio.morphium
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${MORPHIUM_COPY_PLUGIN_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE Bora
    PLUGIN_CODE Mph1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Borato Morphium"
)

juce_generate_juce_header(BoratoMorphium)

juce_add_binary_data(MorphiumAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Resources/morphium_panel.svg
)

target_sources(BoratoMorphium
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/Presets.h
        Source/Parameters/ParameterIDs.h
        Source/Parameters/ParameterLayout.cpp
        Source/Parameters/ParameterLayout.h
        Source/Core/PresetManager.cpp
        Source/Core/PresetManager.h
        Source/Core/MorphiumSound.h
        Source/Core/MorphiumVoice.cpp
        Source/Core/MorphiumVoice.h
        Source/Core/ExcitationCore.cpp
        Source/Core/ExcitationCore.h
        Source/DSP/BasicMatterProcessor.cpp
        Source/DSP/BasicMatterProcessor.h
        Source/DSP/PolyBlep.h
        Source/DSP/WaveguideResonator.cpp
        Source/DSP/WaveguideResonator.h
        Source/UI/MorphiumLookAndFeel.cpp
        Source/UI/MorphiumLookAndFeel.h
)

target_include_directories(BoratoMorphium
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(BoratoMorphium
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        MorphiumAssets
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(BoratoMorphium
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# ASIO low-latency audio for the Standalone (Windows). JUCE 8 ships a bundled
# ASIO header, so no external Steinberg SDK download is required. By enabling
# this the binary depends on the ASIO SDK source terms from Steinberg.
if(WIN32)
    target_compile_definitions(BoratoMorphium PUBLIC JUCE_ASIO=1)
endif()

if(MSVC)
    target_compile_options(BoratoMorphium PRIVATE /W4 /permissive- /wd4244 /wd4267)
else()
    target_compile_options(BoratoMorphium PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ----------------------------------------------------------------------------
# CLAP (via clap-juce-extensions). OFF by default for local development;
# CI enables it with -DMORPHIUM_BUILD_CLAP=ON. Produces BoratoMorphium_CLAP.
# ----------------------------------------------------------------------------
# ----------------------------------------------------------------------------
# Offline spectral test suite. A JUCE console app that recompiles the plugin
# sources (they use per-module includes and no JucePlugin_* macros, so they
# build cleanly outside juce_add_plugin) and drives MorphiumAudioProcessor
# headlessly: render MIDI offline, FFT the output, assert on the spectrum.
# ----------------------------------------------------------------------------
if(MORPHIUM_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    juce_add_console_app(MorphiumTests PRODUCT_NAME "MorphiumTests")

    target_sources(MorphiumTests
        PRIVATE
            tests/main.cpp
            tests/SpectralUtils.cpp
            tests/SpectralUtils.h
            tests/test_excitation.cpp
            tests/test_matter.cpp
            tests/test_voice.cpp
            tests/test_waveguide.cpp
            tests/test_presets.cpp
            tests/test_integration.cpp
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp
            Source/Parameters/ParameterLayout.cpp
            Source/Core/PresetManager.cpp
            Source/Core/MorphiumVoice.cpp
            Source/Core/ExcitationCore.cpp
            Source/DSP/BasicMatterProcessor.cpp
            Source/DSP/WaveguideResonator.cpp
            Source/UI/MorphiumLookAndFeel.cpp
    )

    target_include_directories(MorphiumTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/Source
    )

    target_link_libraries(MorphiumTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
            MorphiumAssets
            Catch2::Catch2
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    target_compile_definitions(MorphiumTests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_MODAL_LOOPS_PERMITTED=0
            # Lets the tests locate tests/golden and tests/artifacts from any CWD.
            MORPHIUM_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )

    if(MSVC)
        target_compile_options(MorphiumTests PRIVATE /W4 /permissive- /wd4244 /wd4267)
    endif()

    enable_testing()
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(Catch)
    # The test executable initialises JUCE's MessageManager. On Linux that can
    # need X11, so defer Catch2 discovery to ctest where CI wraps it in Xvfb.
    catch_discover_tests(MorphiumTests DISCOVERY_MODE PRE_TEST)
endif()

if(MORPHIUM_BUILD_CLAP)
    include(FetchContent)
    FetchContent_Declare(
        clap-juce-extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG main
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(clap-juce-extensions)

    clap_juce_extensions_plugin(TARGET BoratoMorphium
        CLAP_ID "com.boratoaudio.morphium"
        CLAP_FEATURES instrument synthesizer stereo)
endif()
