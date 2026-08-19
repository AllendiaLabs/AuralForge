cmake_minimum_required(VERSION 3.25)

project(AuralForge VERSION 1.0.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

set(AURALFORGE_JUCE_PATH "" CACHE PATH "Path to a JUCE 9.x source tree")
if(AURALFORGE_JUCE_PATH)
    add_subdirectory("${AURALFORGE_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.1
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

find_package(Torch REQUIRED)

# LibTorch's CMake config injects GNU-ld-only flags (--no-as-needed) that Apple's linker rejects.
if(APPLE)
    list(FILTER TORCH_LIBRARIES EXCLUDE REGEX "(^-Wl,|^--no-as-needed|^--as-needed)")

    foreach(_torch_tgt torch_cpu_library torch_library)
        if(TARGET ${_torch_tgt})
            get_target_property(_ill ${_torch_tgt} INTERFACE_LINK_LIBRARIES)
            if(_ill)
                string(REPLACE "-Wl,--no-as-needed," "" _ill "${_ill}")
                string(REPLACE " -Wl,--as-needed" "" _ill "${_ill}")
                set_target_properties(${_torch_tgt} PROPERTIES INTERFACE_LINK_LIBRARIES "${_ill}")
            endif()
        endif()
    endforeach()
endif()

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE)
FetchContent_Declare(
    imgui_node_editor
    GIT_REPOSITORY https://github.com/thedmd/imgui-node-editor.git
    GIT_TAG 021aa0ea4da13fed864bafb2a92d4c5205076866
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui imgui_node_editor)

add_library(auralforge_imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    AuralForge/Source/ui/ImGuiOpenGLBackend.cpp
    "${imgui_node_editor_SOURCE_DIR}/crude_json.cpp"
    "${imgui_node_editor_SOURCE_DIR}/imgui_canvas.cpp"
    "${imgui_node_editor_SOURCE_DIR}/imgui_node_editor.cpp"
    "${imgui_node_editor_SOURCE_DIR}/imgui_node_editor_api.cpp")
target_include_directories(auralforge_imgui PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
    "${imgui_node_editor_SOURCE_DIR}")
target_link_libraries(auralforge_imgui PUBLIC juce::juce_opengl)

juce_add_plugin(AuralForge
    COMPANY_NAME "Allendia"
    BUNDLE_ID "com.allendia.auralforge"
    PLUGIN_MANUFACTURER_CODE Alnd
    PLUGIN_CODE AuFg
    FORMATS AU VST3
    PRODUCT_NAME "AuralForge"
    DESCRIPTION "Live modular temporal convolution audio effect"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion)

juce_generate_juce_header(AuralForge)

target_sources(AuralForge PRIVATE
    AuralForge/Source/PluginEditor.cpp
    AuralForge/Source/PluginProcessor.cpp
    AuralForge/Source/dsp/LookbackBuffer.cpp
    AuralForge/Source/dsp/TCNModel.cpp
    AuralForge/Source/dsp/WeightRandomizer.cpp
    AuralForge/Source/graph/NodeGraph.cpp
    AuralForge/Source/graph/NodeRenderer.cpp
    AuralForge/Source/params/ParamLayout.cpp
    AuralForge/Source/ui/ImGuiHost.cpp
    AuralForge/Source/ui/InfoPanel.cpp
    AuralForge/Source/ui/RandomizeButton.cpp)

target_include_directories(AuralForge PRIVATE
    AuralForge/Source
    "${TORCH_INCLUDE_DIRS}")

target_compile_definitions(AuralForge PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_options(AuralForge PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang>:-Wall -Wextra -Wpedantic>)

target_link_libraries(AuralForge PRIVATE
    auralforge_imgui
    "${TORCH_LIBRARIES}"
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_opengl
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(APPLE)
    set(AURALFORGE_USER_VST3_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
    set_target_properties(AuralForge PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    add_custom_command(TARGET AuralForge_VST3 POST_BUILD
        COMMAND codesign --force --deep --sign -
            "$<TARGET_BUNDLE_DIR:AuralForge_VST3>"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${AURALFORGE_USER_VST3_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "$<TARGET_BUNDLE_DIR:AuralForge_VST3>"
            "${AURALFORGE_USER_VST3_DIR}/AuralForge.vst3"
        VERBATIM)
    add_custom_command(TARGET AuralForge_AU POST_BUILD
        COMMAND codesign --force --deep --sign -
            "$<TARGET_BUNDLE_DIR:AuralForge_AU>"
        VERBATIM)
endif()

include(CTest)
if(BUILD_TESTING)
    juce_add_console_app(AuralForgeTests PRODUCT_NAME "AuralForge Tests")
    target_sources(AuralForgeTests PRIVATE
        Tests/TCNModelTests.cpp
        AuralForge/Source/dsp/TCNModel.cpp)
    target_include_directories(AuralForgeTests PRIVATE
        AuralForge/Source
        "${TORCH_INCLUDE_DIRS}")
    target_link_libraries(AuralForgeTests PRIVATE
        "${TORCH_LIBRARIES}"
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    add_test(NAME AuralForgeTCNModel COMMAND AuralForgeTests)

    juce_add_console_app(AuralForgeProcessorTests PRODUCT_NAME "AuralForge Processor Tests")
    target_sources(AuralForgeProcessorTests PRIVATE Tests/ProcessorIntegrationTests.cpp)
    target_include_directories(AuralForgeProcessorTests BEFORE PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/AuralForge_artefacts/JuceLibraryCode"
        AuralForge/Source
        "${TORCH_INCLUDE_DIRS}")
    target_link_libraries(AuralForgeProcessorTests PRIVATE
        AuralForge
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    add_test(NAME AuralForgeProcessor COMMAND AuralForgeProcessorTests)
endif()
