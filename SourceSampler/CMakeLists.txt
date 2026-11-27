cmake_minimum_required(VERSION 3.22)

project(SourceSampler VERSION 0.9.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(SOURCE_IOS TRUE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "16.0" CACHE STRING "Minimum iOS version")
elseif(APPLE)
    set(SOURCE_IOS FALSE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
else()
    set(SOURCE_IOS FALSE)
endif()

# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------

option(SOURCE_WEB_UI "Use the legacy browser-based (WebBrowserComponent) editor instead of the native JUCE editor" OFF)
option(SOURCE_INCLUDE_SEQUENCER "Build the SourceShepherd variant that embeds the Shepherd sequencer" OFF)
option(SOURCE_BUILD_TESTS "Build the unit test runner" ON)
option(SOURCE_BUILD_AUV3 "Also build the AUv3 format (needs code signing / app extension setup)" OFF)
set(SOURCE_IOS_APP_GROUP "group.ritaiaurora.source" CACHE STRING
    "iOS app group shared by the standalone app and the AUv3 (holds sounds and presets). Must belong to your development team")
set(SOURCE_FREESOUND_API_KEY "" CACHE STRING
    "Freesound API key. If empty, Source/api_key.h is used (copy it from Source/api_key.example.h)")

# ------------------------------------------------------------------------------
# JUCE
# ------------------------------------------------------------------------------

add_subdirectory(3rdParty/JUCE EXCLUDE_FROM_ALL)

# ------------------------------------------------------------------------------
# Shared configuration
# ------------------------------------------------------------------------------

set(SOURCE_3RDPARTY ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty)

# Backend sources (everything except the plugin wrapper and the editors). These are compiled both into
# the plugin and into the test runner.
set(SOURCE_CORE_SOURCES
    Source/FreesoundAPI.cpp
    Source/SourceSampler.cpp
    Source/SourceSamplerSound.cpp
    Source/SourceSamplerSynthesiser.cpp
    Source/SourceSamplerVoice.cpp)

set(SOURCE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/Source)

# Third party headers are added as SYSTEM includes so their warnings don't flood the build output
set(SOURCE_3RDPARTY_INCLUDE_DIRS
    ${SOURCE_3RDPARTY}/signalsmith-stretch)
# NOTE: the native build only needs the JUCE and signalsmith-stretch submodules (ff_meters is no longer used). Simple-Web-Server,
# Simple-WebSocket-Server and asio are only used by SOURCE_WEB_UI / ELK builds, and shepherd only by SOURCE_INCLUDE_SEQUENCER.

set(SOURCE_COMMON_DEFINITIONS
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    ASIO_STANDALONE=1
    # The plugin talks to its editor in-process (no WebSockets server, no OpenSSL dependency)
    USING_DIRECT_COMMUNICATION_METHOD=1
    USE_WS_SERVER=0)

if(SOURCE_FREESOUND_API_KEY)
    list(APPEND SOURCE_COMMON_DEFINITIONS FREESOUND_API_KEY="${SOURCE_FREESOUND_API_KEY}")
elseif(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Source/api_key.h)
    message(WARNING "No Freesound API key configured: copy Source/api_key.example.h to Source/api_key.h "
                    "or pass -DSOURCE_FREESOUND_API_KEY=... (falling back to the placeholder key)")
    list(APPEND SOURCE_COMMON_DEFINITIONS FREESOUND_API_KEY="YOUR_API_KEY_HERE")
endif()

if(APPLE)
    list(APPEND SOURCE_COMMON_DEFINITIONS JUCE_USE_CURL=0)
else()
    list(APPEND SOURCE_COMMON_DEFINITIONS JUCE_USE_CURL=1)
endif()

# ------------------------------------------------------------------------------
# Plugin
# ------------------------------------------------------------------------------

if(SOURCE_IOS)
    # On iOS/iPadOS: standalone app + AUv3 (the AUv3 extension is embedded in the app)
    set(SOURCE_PLUGIN_FORMATS Standalone AUv3)
    set(SOURCE_BUILD_TESTS OFF CACHE BOOL "" FORCE)  # Tests are run on macOS
    if(SOURCE_WEB_UI)
        message(FATAL_ERROR "The web UI is not supported on iOS, use the native UI")
    endif()
else()
    set(SOURCE_PLUGIN_FORMATS AU VST3 Standalone)
    if(SOURCE_BUILD_AUV3)
        list(APPEND SOURCE_PLUGIN_FORMATS AUv3)
    endif()
endif()

set(SOURCE_IOS_PLUGIN_OPTIONS)
if(SOURCE_IOS)
    set(SOURCE_IOS_PLUGIN_OPTIONS
        TARGETED_DEVICE_FAMILY 2  # iPad
        IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationPortrait UIInterfaceOrientationPortraitUpsideDown
                                 UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
        BACKGROUND_AUDIO_ENABLED TRUE
        BACKGROUND_BLE_ENABLED TRUE
        BLUETOOTH_PERMISSION_ENABLED TRUE
        BLUETOOTH_PERMISSION_TEXT "SOURCE uses Bluetooth to connect to Bluetooth MIDI controllers"
        FILE_SHARING_ENABLED TRUE      # Sounds and presets are visible in the Files app
        DOCUMENT_BROWSER_ENABLED TRUE
        STATUS_BAR_HIDDEN TRUE
        REQUIRES_FULL_SCREEN FALSE)
    if(SOURCE_IOS_APP_GROUP)
        list(APPEND SOURCE_IOS_PLUGIN_OPTIONS APP_GROUPS_ENABLED TRUE APP_GROUP_IDS ${SOURCE_IOS_APP_GROUP})
    endif()
endif()

if(SOURCE_INCLUDE_SEQUENCER)
    set(SOURCE_PRODUCT_NAME "SourceShepherd")
else()
    set(SOURCE_PRODUCT_NAME "SourceSampler")
endif()

juce_add_plugin(SourceSampler
    PRODUCT_NAME ${SOURCE_PRODUCT_NAME}
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Music Technology Group"
    COMPANY_WEBSITE "http://mtg.upf.edu"
    BUNDLE_ID com.RitaAurora.SourceSampler
    PLUGIN_MANUFACTURER_CODE RaAu
    PLUGIN_CODE SoSa
    FORMATS ${SOURCE_PLUGIN_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/../docs/icon.png
    ICON_SMALL ${CMAKE_CURRENT_SOURCE_DIR}/../docs/icon.png
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Sampler
    ${SOURCE_IOS_PLUGIN_OPTIONS}
    PLIST_TO_MERGE [[
<plist>
<dict>
  <key>NSAppTransportSecurity</key>
  <dict>
    <key>NSAllowsLocalNetworking</key>
    <true/>
  </dict>
</dict>
</plist>]])

juce_generate_juce_header(SourceSampler)

target_sources(SourceSampler PRIVATE
    ${SOURCE_CORE_SOURCES}
    Source/PluginProcessor.cpp)

if(SOURCE_WEB_UI)
    target_sources(SourceSampler PRIVATE Source/PluginEditor.cpp)
    target_compile_definitions(SourceSampler PRIVATE SOURCE_WEB_UI=1 JUCE_WEB_BROWSER=1)
    # The web UI is a single HTML file bundled as binary data
    juce_add_binary_data(SourceSamplerBinaryData
        HEADER_NAME BinaryData.h
        NAMESPACE BinaryData
        SOURCES
            Resources/ui_plugin_ws.html
            Resources/ui_plugin_ws_lazy.html)
    target_link_libraries(SourceSampler PRIVATE SourceSamplerBinaryData)
    target_include_directories(SourceSampler SYSTEM PRIVATE
        ${SOURCE_3RDPARTY}/Simple-Web-Server
        ${SOURCE_3RDPARTY}/asio/asio/include)
else()
    target_sources(SourceSampler PRIVATE
        Source/UI/NativeEditor.cpp
        Source/UI/SoundListComponent.cpp
        Source/UI/SoundEditorComponent.cpp)
    target_compile_definitions(SourceSampler PRIVATE
        SOURCE_WEB_UI=0
        JUCE_WEB_BROWSER=0
        SOURCE_DISABLE_HTTP_SERVER=1)  # The embedded HTTP server only serves waveforms to the web UI
endif()

target_include_directories(SourceSampler PRIVATE ${SOURCE_INCLUDE_DIRS})
target_include_directories(SourceSampler SYSTEM PRIVATE ${SOURCE_3RDPARTY_INCLUDE_DIRS})
target_compile_definitions(SourceSampler PUBLIC ${SOURCE_COMMON_DEFINITIONS})
if(SOURCE_IOS AND SOURCE_IOS_APP_GROUP)
    target_compile_definitions(SourceSampler PUBLIC APP_GROUP_ID="${SOURCE_IOS_APP_GROUP}")
endif()

if(SOURCE_INCLUDE_SEQUENCER)
    set(SHEPHERD_SRC ${SOURCE_3RDPARTY}/shepherd/Shepherd/Source)
    target_sources(SourceSampler PRIVATE
        ${SHEPHERD_SRC}/Sequencer.cpp
        ${SHEPHERD_SRC}/HardwareDevice.cpp
        ${SHEPHERD_SRC}/MusicalContext.cpp
        ${SHEPHERD_SRC}/Playhead.cpp
        ${SHEPHERD_SRC}/Track.cpp
        ${SHEPHERD_SRC}/Clip.cpp)
    target_include_directories(SourceSampler SYSTEM PRIVATE
        ${SHEPHERD_SRC}
        ${SHEPHERD_SRC}/common
        ${SOURCE_3RDPARTY}/Simple-WebSocket-Server
        ${SOURCE_3RDPARTY}/asio/asio/include)
    target_compile_definitions(SourceSampler PRIVATE
        INCLUDE_SEQUENCER=1
        SOURCE_APP_DIRECTORY_NAME="SourceShepherd"
        SHEPHERD_DATA_DIRECTORY_NAME="SourceShepherd/sequencer"
        CREATE_INTERNAL_HW_OUTPUT_DEVICES=1)
endif()

target_link_libraries(SourceSampler
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_osc
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------------------

if(SOURCE_BUILD_TESTS)
    enable_testing()

    juce_add_console_app(SourceSamplerTests
        PRODUCT_NAME "SourceSamplerTests"
        VERSION ${PROJECT_VERSION})

    juce_generate_juce_header(SourceSamplerTests)

    target_sources(SourceSamplerTests PRIVATE
        ${SOURCE_CORE_SOURCES}
        Tests/TestMain.cpp
        Tests/HelpersTests.cpp
        Tests/SamplerRenderTests.cpp
        Tests/SourceSamplerTests.cpp)

    target_include_directories(SourceSamplerTests PRIVATE ${SOURCE_INCLUDE_DIRS} Tests)
    target_include_directories(SourceSamplerTests SYSTEM PRIVATE ${SOURCE_3RDPARTY_INCLUDE_DIRS})
    target_compile_definitions(SourceSamplerTests PRIVATE
        ${SOURCE_COMMON_DEFINITIONS}
        SOURCE_DISABLE_HTTP_SERVER=1
        JUCE_UNIT_TESTS=1
        JUCE_MODAL_LOOPS_PERMITTED=1  # Tests run the message loop while waiting for async work
        JUCE_WEB_BROWSER=0)

    target_link_libraries(SourceSamplerTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_osc
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_test(NAME SourceSamplerTests COMMAND SourceSamplerTests)
endif()
