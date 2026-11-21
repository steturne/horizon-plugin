# HORIZON as a DAW plugin (AU and VST3, and a stand-alone app), built with JUCE 8 by GitHub Actions (.github/workflows/build.yml).
#   core/    the sound engine, plain C++ (no JUCE): also meant for WebAssembly later (Mixtape 2, the web)
#   plugin/  the JUCE wrapper: parameters, MIDI, the sample loader, the window (a WebBrowserComponent)
#   face/    the window's web page (built into the plugin)
#   samples/ the piano (Salamander Grand, four layers), copied into each plugin's Resources folder
cmake_minimum_required(VERSION 3.22)

# set before project(): the Mac build runs on Intel and Apple Silicon, macOS 11 and later; Windows needs no Visual C++ runtime installed
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Oldest macOS")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Mac processors")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(HORIZON VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.17-security
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(juce)

juce_add_plugin(Horizon
    PRODUCT_NAME "HORIZON"
    COMPANY_NAME "Steve Turner"
    BUNDLE_ID com.steveturner.horizon
    PLUGIN_MANUFACTURER_CODE Stvt
    PLUGIN_CODE Hrzn
    FORMATS AU VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEBVIEW2 TRUE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    DESCRIPTION "Atmospheric piano under a live aurora")

target_sources(Horizon PRIVATE
    core/HorizonEngine.cpp
    core/HaloPad.cpp
    plugin/PluginProcessor.cpp
    plugin/PluginEditor.cpp
    plugin/SampleLibrary.cpp)

# the face's files, and JUCE's own little script that joins the page's knobs to the plugin's parameters
set(JUCE_JS "${juce_SOURCE_DIR}/modules/juce_gui_extra/native/javascript")
juce_add_binary_data(HorizonFace SOURCES
    face/horizon.html
    face/face.js
    face/face.css
    face/sky.js
    face/picture.js
    face/mountains.jpg
    "${JUCE_JS}/index.js"
    "${JUCE_JS}/check_native_interop.js")

target_compile_definitions(Horizon PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)

target_link_libraries(Horizon
    PRIVATE
        HorizonFace
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# the piano goes inside each plugin (Contents/Resources/Salamander Grand), so there is one thing to install;
# the Windows stand-alone HORIZON.exe is not a bundle: there the folder goes beside it
set(HORIZON_SAMPLES "${CMAKE_CURRENT_SOURCE_DIR}/samples/Salamander Grand")
foreach(fmt AU VST3 Standalone)
    if(TARGET Horizon_${fmt})
        if(fmt STREQUAL "Standalone" AND NOT APPLE)
            set(dest "$<TARGET_FILE_DIR:Horizon_${fmt}>/Salamander Grand")
        else()
            set(dest "$<TARGET_FILE_DIR:Horizon_${fmt}>/../Resources/Salamander Grand")
        endif()
        add_custom_command(TARGET Horizon_${fmt} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${HORIZON_SAMPLES}" "${dest}"
            VERBATIM)
    endif()
endforeach()
