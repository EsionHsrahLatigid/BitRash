cmake_minimum_required(VERSION 3.22)

project(BitRash VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BITRASH_BUILD_PLUGIN "Build the JUCE plug-in targets" ON)
option(BITRASH_BUILD_TESTS "Build the test targets" ON)
set(BITRASH_ARTIFACT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/artifacts" CACHE PATH "Stable product staging root")
set(EHL_JUCE_SOURCE_DIR "" CACHE PATH "Existing JUCE source checkout; avoids network when set")

add_library(bitrash_dsp STATIC
    Source/dsp/BitRashDSP.cpp)
target_include_directories(bitrash_dsp PUBLIC Source)
target_compile_features(bitrash_dsp PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(bitrash_dsp PRIVATE /W4)
else()
    target_compile_options(bitrash_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BITRASH_BUILD_TESTS)
    enable_testing()
    add_executable(bitrash_dsp_tests Tests/DSPTests.cpp)
    target_link_libraries(bitrash_dsp_tests PRIVATE bitrash_dsp)
    add_test(NAME bitrash_dsp_tests COMMAND bitrash_dsp_tests)
endif()

if(BITRASH_BUILD_PLUGIN)
    include(FetchContent)
    if(EHL_JUCE_SOURCE_DIR)
        if(NOT EXISTS "${EHL_JUCE_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR "EHL_JUCE_SOURCE_DIR does not contain JUCE: ${EHL_JUCE_SOURCE_DIR}")
        endif()
        add_subdirectory("${EHL_JUCE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        add_subdirectory(JUCE)
    else()
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 91ad83ae34a81e0833b1a2b0866f54846370ae53
            GIT_SHALLOW FALSE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    set(BITRASH_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND BITRASH_FORMATS AU)
    endif()

    juce_add_plugin(BitRash
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "EsionHsrahLatigid"
        BUNDLE_ID "jp.ehl.bitrash"
        PRODUCT_NAME "BitRash"
        DESCRIPTION "deterministic digital harsh noise bitcrusher"
        PLUGIN_MANUFACTURER_CODE EHL_
        PLUGIN_CODE BtRh
        FORMATS ${BITRASH_FORMATS}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "BitRash processes live audio input."
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(BitRash PRIVATE
        Source/ParameterIDs.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h)
    target_include_directories(BitRash PRIVATE Source)
    target_compile_definitions(BitRash PUBLIC
        JUCE_REPORT_APP_USAGE=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0)
    target_link_libraries(BitRash
        PRIVATE
            bitrash_dsp
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_sources(BitRash_Standalone PRIVATE Source/StandaloneApp.cpp)
    target_compile_definitions(BitRash_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)

    set(ehl_stage_platform "linux-x64")
    if(APPLE)
        set(ehl_stage_platform "macos-arm64")
    elseif(WIN32)
        set(ehl_stage_platform "windows-x64")
    endif()
    set(ehl_juce_products "${CMAKE_CURRENT_BINARY_DIR}/BitRash_artefacts/$<CONFIG>")
    set(ehl_staged_products "${BITRASH_ARTIFACT_ROOT}/plugin-release/${ehl_stage_platform}")

    set(ehl_stage_dependencies BitRash_VST3 BitRash_Standalone)
    set(ehl_stage_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ehl_staged_products}/vst3"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ehl_staged_products}/standalone"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ehl_staged_products}/au")

    if(APPLE)
        list(APPEND ehl_stage_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${ehl_staged_products}/vst3/bitrash_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${ehl_staged_products}/standalone/bitrash_standalone_plugin.app"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${ehl_staged_products}/au/bitrash_au_plugin.component"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_juce_products}/VST3/BitRash.vst3" "${ehl_staged_products}/vst3/bitrash_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_juce_products}/Standalone/BitRash.app" "${ehl_staged_products}/standalone/bitrash_standalone_plugin.app"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_juce_products}/AU/BitRash.component" "${ehl_staged_products}/au/bitrash_au_plugin.component"
            COMMAND codesign --force --deep --sign - "${ehl_staged_products}/vst3/bitrash_vst3_plugin.vst3"
            COMMAND codesign --force --deep --sign - "${ehl_staged_products}/standalone/bitrash_standalone_plugin.app"
            COMMAND codesign --force --deep --sign - "${ehl_staged_products}/au/bitrash_au_plugin.component")
        list(APPEND ehl_stage_dependencies BitRash_AU)
    elseif(WIN32)
        list(APPEND ehl_stage_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${ehl_staged_products}/vst3/bitrash_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_juce_products}/VST3/BitRash.vst3" "${ehl_staged_products}/vst3/bitrash_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy "${ehl_juce_products}/Standalone/BitRash.exe" "${ehl_staged_products}/standalone/bitrash_standalone_plugin.exe")
    else()
        list(APPEND ehl_stage_commands
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_juce_products}/VST3/BitRash.vst3" "${ehl_staged_products}/vst3/bitrash_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy "${ehl_juce_products}/Standalone/BitRash" "${ehl_staged_products}/standalone/bitrash_standalone_plugin")
    endif()

    list(APPEND ehl_stage_commands
        COMMAND ${CMAKE_COMMAND}
            -DPRODUCT_NAME=BitRash
            -DSLUG=bitrash
            -DBUNDLE_ID=jp.ehl.bitrash
            -DPLUGIN_CODE=BtRh
            -DSTAGE_DIR=${ehl_staged_products}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteArtifactsManifest.cmake")

    add_custom_target(ehl_stage_products
        ${ehl_stage_commands}
        DEPENDS ${ehl_stage_dependencies}
        COMMENT "Staging BitRash products under ${BITRASH_ARTIFACT_ROOT}/plugin-release")

    if(BITRASH_BUILD_TESTS)
        add_executable(bitrash_plugin_tests Tests/PluginTests.cpp)
        target_include_directories(bitrash_plugin_tests PRIVATE Source)
        target_link_libraries(bitrash_plugin_tests PRIVATE BitRash juce::juce_audio_utils)
        add_test(NAME bitrash_plugin_tests COMMAND bitrash_plugin_tests)

        add_executable(bitrash_editor_tests Tests/EditorTests.cpp)
        target_include_directories(bitrash_editor_tests PRIVATE Source)
        target_link_libraries(bitrash_editor_tests PRIVATE BitRash juce::juce_audio_utils)
        add_test(NAME bitrash_editor_tests COMMAND bitrash_editor_tests)

        add_test(
            NAME bitrash_artifact_contract
            COMMAND ${CMAKE_COMMAND}
                -DSTAGE_DIR=${ehl_staged_products}
                -DSLUG=bitrash
                -DEXPECT_AU=$<BOOL:${APPLE}>
                -P "${CMAKE_CURRENT_SOURCE_DIR}/Tests/VerifyArtifacts.cmake")
    endif()
endif()
