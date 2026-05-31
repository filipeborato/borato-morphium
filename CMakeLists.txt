cmake_minimum_required(VERSION 3.22)

project(BoratoMorphium VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----------------------------------------------------------------------------
# Locate JUCE
# ----------------------------------------------------------------------------
option(MORPHIUM_FETCH_JUCE "Download JUCE if it is not found locally" OFF)
option(MORPHIUM_COPY_PLUGIN_AFTER_BUILD "Install the plugin to the system folder after build" OFF)
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
    FORMATS VST3 Standalone
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
        Source/Core/MorphiumSound.h
        Source/Core/MorphiumVoice.cpp
        Source/Core/MorphiumVoice.h
        Source/Core/ExcitationCore.cpp
        Source/Core/ExcitationCore.h
        Source/DSP/BasicMatterProcessor.cpp
        Source/DSP/BasicMatterProcessor.h
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
