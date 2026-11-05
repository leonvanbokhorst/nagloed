cmake_minimum_required(VERSION 3.22)

project(NAGLOED VERSION 1.0.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# -----------------------------------------------------------------------------
# JUCE. Prefer a local checkout (.deps/JUCE via scripts/fetch-juce.sh) so builds
# are reproducible and network-free after the first fetch.
# -----------------------------------------------------------------------------
set(NAGLOED_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")
if (NAGLOED_JUCE_DIR AND EXISTS "${NAGLOED_JUCE_DIR}/CMakeLists.txt")
    message(STATUS "NAGLOED: using local JUCE at ${NAGLOED_JUCE_DIR}")
    add_subdirectory("${NAGLOED_JUCE_DIR}" juce EXCLUDE_FROM_ALL)
else()
    message(STATUS "NAGLOED: fetching JUCE 9.0.5")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.5
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# -----------------------------------------------------------------------------
# Sources (globbed so workstreams can add files without touching this file).
# -----------------------------------------------------------------------------
file(GLOB_RECURSE NAGLOED_CORE   CONFIGURE_DEPENDS src/core/*.cpp)
file(GLOB_RECURSE NAGLOED_DSP    CONFIGURE_DEPENDS src/dsp/*.cpp)
file(GLOB_RECURSE NAGLOED_UI     CONFIGURE_DEPENDS src/ui/*.cpp)
file(GLOB_RECURSE NAGLOED_PLUGIN CONFIGURE_DEPENDS src/plugin/*.cpp)
file(GLOB_RECURSE NAGLOED_TESTS  CONFIGURE_DEPENDS tests/*.cpp)

set(NAGLOED_COMMON_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# -----------------------------------------------------------------------------
# Presets are compiled into the binary from presets/*.json.
# -----------------------------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter REQUIRED)
file(GLOB PRESET_JSONS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/presets/*.json")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(PRESETS_CPP "${GENERATED_DIR}/PresetsData.cpp")
set(PRESETS_H   "${GENERATED_DIR}/PresetsData.h")

add_custom_command(
    OUTPUT "${PRESETS_CPP}" "${PRESETS_H}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_DIR}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_presets.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/presets" "${GENERATED_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/core/ParamIDs.h"
    DEPENDS ${PRESET_JSONS} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_presets.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/core/ParamIDs.h"
    COMMENT "Embedding NAGLOED presets")

add_custom_target(nagloed_presets DEPENDS "${PRESETS_CPP}" "${PRESETS_H}")

# -----------------------------------------------------------------------------
# The plugin.
# -----------------------------------------------------------------------------
juce_add_plugin(NAGLOED
    PRODUCT_NAME                "NAGLOED"
    COMPANY_NAME                "Master Lonn & Little Wan"
    BUNDLE_ID                   "com.masterlonn.littlewan.nagloed"
    PLUGIN_MANUFACTURER_CODE    MLLW
    PLUGIN_CODE                 Nagl
    FORMATS                     AU VST3 Standalone
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    PLUGIN_CATEGORY             "Fx"
    VST3_CATEGORIES             "Fx"
    COMPANY_COPYRIGHT           "Copyright 2026 Master Lonn & Little Wan"
    VERSION                     ${PROJECT_VERSION})

juce_generate_juce_header(NAGLOED)

target_sources(NAGLOED PRIVATE
    ${NAGLOED_CORE} ${NAGLOED_DSP} ${NAGLOED_UI} ${NAGLOED_PLUGIN}
    "${PRESETS_CPP}" "${PRESETS_H}")

target_include_directories(NAGLOED PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${GENERATED_DIR}")

target_compile_definitions(NAGLOED PRIVATE ${NAGLOED_COMMON_DEFS})
target_link_libraries(NAGLOED PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
add_dependencies(NAGLOED nagloed_presets)

# -----------------------------------------------------------------------------
# Developer tools (console apps). They share exactly the same sources as the
# plugin so what you hear/see is what ships.
# -----------------------------------------------------------------------------
option(NAGLOED_BUILD_TOOLS "Build the headless render/test tools" ON)

if (NAGLOED_BUILD_TOOLS)
    juce_add_console_app(nagl_tests PRODUCT_NAME "nagl-tests")
    target_sources(nagl_tests PRIVATE
        ${NAGLOED_TESTS}
        ${NAGLOED_CORE} ${NAGLOED_DSP} ${NAGLOED_PLUGIN}
        "${PRESETS_CPP}" "${PRESETS_H}")
    target_include_directories(nagl_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${GENERATED_DIR}")
    target_compile_definitions(nagl_tests PRIVATE ${NAGLOED_COMMON_DEFS} NAGLOED_HEADLESS=1)
    target_link_libraries(nagl_tests PRIVATE juce::juce_audio_utils juce::juce_dsp)
    add_dependencies(nagl_tests nagloed_presets)

    juce_add_console_app(nagl_render PRODUCT_NAME "nagl-render")
    target_sources(nagl_render PRIVATE
        tools/render_main.cpp tools/DemoScore.cpp
        ${NAGLOED_CORE} ${NAGLOED_DSP} ${NAGLOED_PLUGIN}
        "${PRESETS_CPP}" "${PRESETS_H}")
    target_include_directories(nagl_render PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${GENERATED_DIR}")
    target_compile_definitions(nagl_render PRIVATE ${NAGLOED_COMMON_DEFS} NAGLOED_HEADLESS=1)
    target_link_libraries(nagl_render PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp)
    add_dependencies(nagl_render nagloed_presets)

    juce_add_console_app(nagl_preview PRODUCT_NAME "nagl-preview")
    target_sources(nagl_preview PRIVATE
        tools/preview_main.cpp
        ${NAGLOED_CORE} ${NAGLOED_DSP} ${NAGLOED_UI} ${NAGLOED_PLUGIN}
        "${PRESETS_CPP}" "${PRESETS_H}")
    target_include_directories(nagl_preview PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${GENERATED_DIR}")
    target_compile_definitions(nagl_preview PRIVATE ${NAGLOED_COMMON_DEFS}
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(nagl_preview PRIVATE juce::juce_audio_utils juce::juce_dsp)
    add_dependencies(nagl_preview nagloed_presets)
endif()
