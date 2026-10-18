cmake_minimum_required(VERSION 3.22)

project(Rebanada VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Versión mínima de macOS")
endif()

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG 7adee3a1bd4684d4caa5601100e364abccff4b4d
    GIT_SUBMODULES_RECURSE TRUE)
FetchContent_MakeAvailable(clap-juce-extensions)

set(REBANADA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND REBANADA_FORMATS AU)
endif()

juce_add_plugin(Rebanada
    PRODUCT_NAME "Rebanada"
    COMPANY_NAME "Lautaro Martin Vieyra"
    BUNDLE_ID "ar.lautarovieyra.rebanada"
    PLUGIN_MANUFACTURER_CODE Lmvy
    PLUGIN_CODE Rbnd
    FORMATS ${REBANADA_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

clap_juce_extensions_plugin(TARGET Rebanada
    CLAP_ID "ar.lautarovieyra.rebanada"
    CLAP_FEATURES instrument sampler stereo)

juce_add_binary_data(RebanadaData
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Resources/fonts/Archivo-Regular.ttf
        Resources/fonts/Archivo-SemiBold.ttf
        Resources/fonts/Archivo-ExtraBold.ttf
        Resources/fonts/IBMPlexMono-Regular.ttf
        Resources/fonts/IBMPlexMono-Medium.ttf)

target_sources(Rebanada PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Rebanada PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Rebanada
    PRIVATE
        RebanadaData
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Prueba del motor sin JUCE (se ejecuta en la compilación automática)
add_executable(rebanada_engine_test tests/EngineTest.cpp)
target_include_directories(rebanada_engine_test PRIVATE Source)
enable_testing()
add_test(NAME motor COMMAND rebanada_engine_test)

# Captura de la interfaz y prueba como la carga un DAW (solo desarrollo y compilación automática)
option(REBANADA_SNAPSHOT "Compilar la herramienta de captura y la prueba de anfitrión" OFF)
if(REBANADA_SNAPSHOT)
    juce_add_console_app(rebanada_snapshot PRODUCT_NAME "rebanada_snapshot")
    target_sources(rebanada_snapshot PRIVATE tests/Snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(rebanada_snapshot PRIVATE Source)
    target_compile_definitions(rebanada_snapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_VersionString="0.1.0" JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(rebanada_snapshot PRIVATE RebanadaData juce::juce_audio_utils juce::juce_audio_formats juce::juce_recommended_config_flags)

    juce_add_console_app(rebanada_hosttest PRODUCT_NAME "rebanada_hosttest")
    target_sources(rebanada_hosttest PRIVATE tests/HostTest.cpp)
    target_compile_definitions(rebanada_hosttest PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(rebanada_hosttest PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_recommended_config_flags)
endif()
