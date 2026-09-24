cmake_minimum_required(VERSION 3.22.1)

project(magicpodscore VERSION 2.0.9 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
if(NOT MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
endif()
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

configure_file(config/Config.h.in src/Config.h)

add_executable(magicpodscore
    src/main.cpp

    src/tests/TestsSgb.cpp
    src/tests/TestsAapBle.cpp
    src/tests/TestsAapAudio.cpp
    src/tests/TestsZik.cpp

    src/client/Client.cpp
    src/audio/AudioEffects.cpp

    src/sdk/aap/Aes.cpp
    src/sdk/aap/AapHelper.cpp
    src/sdk/aap/setters/AapEnableNotifications.cpp
    src/sdk/aap/setters/AapInit.cpp
    src/sdk/aap/setters/AapInitExt.cpp
    src/sdk/aap/setters/AapRequest.cpp
    src/sdk/aap/setters/AapSetAnc.cpp
    src/sdk/aap/setters/AapSetConversationAwarenessStatus.cpp
    src/sdk/aap/setters/AapSetNoiseCancellationOneAirPod.cpp
    src/sdk/aap/setters/AapSetPressAndHoldDuration.cpp
    src/sdk/aap/setters/AapSetPressSpeed.cpp
    src/sdk/aap/setters/AapSetVolumeSwipe.cpp
    src/sdk/aap/setters/AapSetVolumeSwipeLength.cpp
    src/sdk/aap/setters/AapSetPersonalizedVolume.cpp
    src/sdk/aap/setters/AapSetToneVolume.cpp
    src/sdk/aap/setters/AapSetMuteMicrophoneEndCall.cpp
    src/sdk/aap/setters/AapSetAdaptiveAudioNoise.cpp
    src/sdk/aap/setters/AapPrivateKeys.cpp

    src/sdk/aap/watchers/AapAncWatcher.cpp
    src/sdk/aap/watchers/AapBatteryWatcher.cpp
    src/sdk/aap/watchers/AapWatcher.cpp
    src/sdk/aap/watchers/AapConversationAwarenessWatcher.cpp
    src/sdk/aap/watchers/AapConversationAwarenessStateWatcher.cpp
    src/sdk/aap/watchers/AapNoiseCancellationOneAirPodWatcher.cpp
    src/sdk/aap/watchers/AapPressAndHoldDurationWatcher.cpp
    src/sdk/aap/watchers/AapPressSpeedWatcher.cpp
    src/sdk/aap/watchers/AapVolumeSwipeWatcher.cpp
    src/sdk/aap/watchers/AapVolumeSwipeLengthWatcher.cpp
    src/sdk/aap/watchers/AapPersonalizedVolumeWatcher.cpp
    src/sdk/aap/watchers/AapToneVolumeWatcher.cpp
    src/sdk/aap/watchers/AapMuteMicrophoneEndCallWatcher.cpp
    src/sdk/aap/watchers/AapAdaptiveAudioNoiseWatcher.cpp
    src/sdk/aap/watchers/AapPrivateKeysWatcher.cpp

    src/sdk/sgb/watchers/GalaxyBudsAncWatcher.cpp
    src/sdk/sgb/watchers/GalaxyBudsBatteryWatcher.cpp
    src/sdk/sgb/watchers/GalaxyBudsEarDetectionWatcher.cpp
    src/sdk/sgb/Crc16.cpp
    src/sdk/sgb/GalaxyBudsHelper.cpp
    src/sdk/sgb/GalaxyBudsPacket.cpp

    src/device/capabilities/aap/AapCapability.cpp
    src/device/capabilities/aap/AapAncCapability.cpp
    src/device/capabilities/aap/AapBatteryCapability.cpp
    src/device/capabilities/aap/AapConversationAwarenessCapability.cpp
    src/device/capabilities/aap/AapConversationAwarenessStateCapability.cpp
    src/device/capabilities/aap/AapNoiseCancellationOneAirPodModeCapability.cpp
    src/device/capabilities/aap/AapPressAndHoldDurationCapability.cpp
    src/device/capabilities/aap/AapPressSpeedCapability.cpp
    src/device/capabilities/aap/AapVolumeSwipeCapability.cpp
    src/device/capabilities/aap/AapVolumeSwipeLengthCapability.cpp
    src/device/capabilities/aap/AapPersonalizedVolumeCapability.cpp
    src/device/capabilities/aap/AapToneVolumeCapability.cpp
    src/device/capabilities/aap/AapMuteMicrophoneEndCallCapability.cpp
    src/device/capabilities/aap/AapAdaptiveAudioNoiseCapability.cpp
    src/device/capabilities/aap/AppAnimationCapability.cpp
    src/device/capabilities/aap/AapEarDetectionCapability.cpp
    src/device/capabilities/zik/ZikCapabilities.cpp
    src/device/capabilities/aap/AapAudioSwitchCapability.cpp
    src/device/capabilities/aap/AapAudioEffectsCapabilities.cpp

    src/device/capabilities/sgb/GalaxyBudsCapability.cpp
    src/device/capabilities/sgb/GalaxyBudsAncCapability.cpp
    src/device/capabilities/sgb/GalaxyBudsBatteryCapability.cpp

    src/device/capabilities/bhf/BhfBatteryCapability.cpp
    src/device/capabilities/cmn/CmnBluetoothCodecCapability.cpp

    src/device/capabilities/Capability.cpp
    src/device/AapDevice.cpp
    src/device/GalaxyBudsDevice.cpp
    src/device/BhfDevice.cpp
    src/device/ZikDevice.cpp
    src/device/DeviceBattery.cpp
    src/device/Device.cpp

    src/DevicesInfoFetcher.cpp

    src/settings/SettingsService.cpp
    src/settings/JsonToTomlConverter.cpp
)
add_subdirectory(dependencies)
find_package(OpenSSL REQUIRED)
target_include_directories(magicpodscore PRIVATE
    src
    ${PROJECT_BINARY_DIR}/src
)
target_link_libraries(magicpodscore PRIVATE
    uWebSockets
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
    OpenSSL::Crypto
)

# The platform layer: Bluetooth, sound server and media players. Everything above it is shared.
if(WIN32)
    # WinRT (Bluetooth, radios, media sessions), Winsock RFCOMM, Core Audio
    target_sources(magicpodscore PRIVATE
        src/client/Client_win.cpp
        src/dbus/DBusDeviceInfo_win.cpp
        src/dbus/DBusService_win.cpp
        src/ble_ads/WinRtBleAdvertisingService.cpp
        src/pulseaudio/PulseAudioClient_win.cpp
        src/media/MprisClient_win.cpp
    )
    target_compile_definitions(magicpodscore PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
    target_compile_options(magicpodscore PRIVATE /utf-8 /bigobj)
    # mypodsaap.h: interface of the optional AAP driver
    target_include_directories(magicpodscore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../driver/windows)
    target_link_libraries(magicpodscore PRIVATE windowsapp ws2_32 ole32 cfgmgr32)
else()
    # BlueZ (D-Bus, L2CAP/RFCOMM sockets), PulseAudio/PipeWire, MPRIS
    target_sources(magicpodscore PRIVATE
        src/tests/EmulateAirPods.cpp
        src/pulseaudio/PulseAudioClient.cpp
        src/media/MprisClient.cpp
        src/dbus/DBusDeviceInfo.cpp
        src/dbus/DBusService.cpp
        src/ble_ads/DBusBasedBleAdvertisingService.cpp
        src/ble_ads/PassiveHciBasedBleAdvertisingService.cpp
    )
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PULSEAUDIO REQUIRED libpulse)
    target_include_directories(magicpodscore PRIVATE ${PULSEAUDIO_INCLUDE_DIRS})
    target_link_libraries(magicpodscore PRIVATE sdbus-c++ bluetooth ${PULSEAUDIO_LIBRARIES})
endif()
