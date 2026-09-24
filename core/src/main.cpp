// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2025 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include <iostream>
#include <atomic>
#include <nlohmann/json.hpp>
#include <App.h>

#include "DevicesInfoFetcher.h"
#include "device/DeviceBattery.h"
#include "device/AapDevice.h"
#include "device/enums/DeviceAncModes.h"
#include "tests/TestsSgb.h"
#include "tests/TestsAapBle.h"
#include "tests/TestsAapAudio.h"
#include "tests/TestsZik.h"
#include "tests/TestsCore.h"
#include "Logger.h"
#include "Config.h"
#include "settings/SettingsService.h"
#include "settings/JsonToTomlConverter.h"
#include "audio/AudioEffects.h"
#include <set>
#include <thread>
#include <cstdlib>
#ifdef _WIN32
#include <winrt/base.h>
#else
#include <csignal>
#include <pthread.h>
#endif

using namespace MagicPodsCore;

int EmulateAirPods(); // tests/EmulateAirPods.cpp

//Do not forget to change version when API changes
constexpr int API_VERSION = 0;
constexpr int WEBSOCKET_PORT = 2020;

// Sockets that are open right now. Only touched on the uWS loop thread (open, close, deferred callbacks):
// a reply that arrives after its client went away (BlueZ can take seconds to connect) is dropped
// instead of being written to a freed socket.
static std::set<void*> openSockets;

// The AirPods keys identify and track the headphones. The UI never needs them, so the API doesn't hand
// them to every local process; they stay in config.toml (which the Windows import section points to).
static bool IsSecretSetting(std::string_view name) {
    return name == "irk" || name == "enc";
}

static void StripSecrets(nlohmann::json& container) {
    if (!container.is_object())
        return;
    for (auto name : {"irk", "enc"})
        container.erase(name);
}

nlohmann::json MakeGetDeviceResponse(DevicesInfoFetcher& devicesInfoFetcher) {
    auto rootObject = nlohmann::json::object();
    auto jsonArray = nlohmann::json::array();
    for (const auto& device : devicesInfoFetcher.GetDevices()) {
        auto deviceJson = device->GetAsJson();
        deviceJson.erase("capabilities"); // we do not need capabilities in GetDevices
        jsonArray.push_back(deviceJson);
    }
    rootObject["headphones"] = jsonArray;

    return rootObject;
}

nlohmann::json MakeGetActiveDeviceInfoResponse(DevicesInfoFetcher& devicesInfoFetcher) {
    auto activeDevice = devicesInfoFetcher.GetActiveDevice();
    if (activeDevice)
        return nlohmann::json{{"info", activeDevice->GetAsJson()}};

    return nlohmann::json{{"info", nlohmann::json::object()}};
}

nlohmann::json MakeGetDefaultBluetoothAdapterResponse(DevicesInfoFetcher& devicesInfoFetcher) {
    auto responseJson = nlohmann::json::object();
    auto defaultBluetoothJson = nlohmann::json::object();
    defaultBluetoothJson["enabled"] = devicesInfoFetcher.IsBluetoothAdapterPowered();
    responseJson["defaultbluetooth"] = defaultBluetoothJson;
    return responseJson;
}

nlohmann::json MakeGetSettingsAllResponse(SettingsService& settingsService) {
    auto wrappedSettings = settingsService.GetSettingsAllWrapped();
    auto json = TomlToJson(wrappedSettings);
    if (json.contains("settings") && json["settings"].is_object())
        for (auto& [container, values] : json["settings"].items())
            StripSecrets(values);
    return json;
}

nlohmann::json MakeGetSettingsResponse(SettingsService& settingsService, const std::string& container) {
    auto containerSettings = settingsService.GetSettings(container);
    auto wrappedJson = nlohmann::json::object();
    wrappedJson["settings"] = nlohmann::json::object();
    wrappedJson["settings"][container] = TomlToJson(containerSettings);
    StripSecrets(wrappedJson["settings"][container]);
    return wrappedJson;
}

nlohmann::json MakeGetSettingResponse(SettingsService& settingsService, const std::string& container, const std::string& setting) {
    auto containerSettings = settingsService.GetSettings(container); // a copy, so the view below stays valid

    auto wrappedJson = nlohmann::json::object();
    wrappedJson["settings"] = nlohmann::json::object();
    wrappedJson["settings"][container] = nlohmann::json::object();
    wrappedJson["settings"][container][setting] = IsSecretSetting(setting) ? nlohmann::json{} : TomlNodeViewToJson(containerSettings[setting]);

    return wrappedJson;
}

void InitHandshake(auto *ws) {
    Logger::Info("InitHandshake");
    auto wrappedJson = nlohmann::json::object();
    wrappedJson["init"]["api"] = API_VERSION;
    wrappedJson["init"]["version"] = CMAKE_PROJECT_VERSION;
    auto response = wrappedJson.dump();
    ws->send(response, uWS::OpCode::TEXT, response.length() < 16 * 1024);
}

void HandleGetDevicesRequest(auto *ws, const nlohmann::json& json, uWS::OpCode opCode, DevicesInfoFetcher& devicesInfoFetcher) {
    Logger::Info("HandleGetDevicesRequest");

    auto response = MakeGetDeviceResponse(devicesInfoFetcher).dump();
    ws->send(response, opCode, response.length() < 16 * 1024);
}

void HandleConnectDeviceRequest(auto *ws, const nlohmann::json& json, uWS::OpCode opCode, uWS::App& app, DevicesInfoFetcher& devicesInfoFetcher) {
    Logger::Info("HandleConnectDeviceRequest");

    auto deviceAddress = json.at("arguments").at("address").template get<std::string>();
    auto device = devicesInfoFetcher.GetDevice(deviceAddress);

    if (device == nullptr || device->GetConnected()) {
        auto response = MakeGetDeviceResponse(devicesInfoFetcher).dump();
        ws->send(response, opCode, response.length() < 16 * 1024);
        return;
    }

    device->ConnectAsync([device, ws, opCode, &app, &devicesInfoFetcher](const std::string* error) {
        app.getLoop()->defer([device, ws, opCode, &devicesInfoFetcher]() {
            if (!openSockets.contains(ws))
                return;
            auto response = MakeGetDeviceResponse(devicesInfoFetcher).dump();
            ws->send(response, opCode, response.length() < 16 * 1024);
        });
    });
}

void HandleDisconnectDeviceRequest(auto *ws, const nlohmann::json& json, uWS::OpCode opCode, uWS::App& app, DevicesInfoFetcher& devicesInfoFetcher) {
    Logger::Info("HandleDisconnectDeviceRequest");

    auto deviceAddress = json.at("arguments").at("address").template get<std::string>();
    auto device = devicesInfoFetcher.GetDevice(deviceAddress);

    if (device == nullptr || !device->GetConnected()) {
        auto response = MakeGetDeviceResponse(devicesInfoFetcher).dump();
        ws->send(response, opCode, response.length() < 16 * 1024);
        return;
    }

    device->DisconnectAsync([device, ws, opCode, &app, &devicesInfoFetcher](const std::string* error) {
        app.getLoop()->defer([device, ws, opCode, &devicesInfoFetcher]() {
            if (!openSockets.contains(ws))
                return;
            auto response = MakeGetDeviceResponse(devicesInfoFetcher).dump();
            ws->send(response, opCode, response.length() < 16 * 1024);
        });
    });
}

void HandleSetCapabilitiesRequest(auto *ws, const nlohmann::json& json, uWS::OpCode opCode, DevicesInfoFetcher& devicesInfoFetcher) {
    Logger::Info("HandleSetAncRequest");
    devicesInfoFetcher.SetCapabilities(json);
}


void HandleGetDefaultBluetoothAdapterRequest(auto *ws, const nlohmann::json& json, uWS::OpCode opCode, DevicesInfoFetcher& devicesInfoFetcher) {
    Logger::Info("HandleGetDefaultBluetoothAdapterRequest");

    auto response = MakeGetDefaultBluetoothAdapterResponse(devicesInfoFetcher).dump();
    ws->send(response, opCode, response.length() < 16 * 1024);
}

void HandleEnableDefaultBluetoothAdapter(auto *ws, const nlohmann::json& json, uWS::OpCode opCode, uWS::App& app, DevicesInfoFetcher& devicesInfoFetcher) {
    Logger::Info("HandleEnableDefaultBluetoothAdapter");

    devicesInfoFetcher.EnableBluetoothAdapterAsync([ws, &app, &devicesInfoFetcher, opCode](const std::string* error) {
        app.getLoop()->defer([ws, &devicesInfoFetcher, opCode](){
            if (!openSockets.contains(ws))
                return;
            auto response = MakeGetDefaultBluetoothAdapterResponse(devicesInfoFetcher).dump();
            ws->send(response, opCode, response.length() < 16 * 1024);
        });
    });
}

void HandleDisableDefaultBluetoothAdapter(auto *ws, const nlohmann::json& json, uWS::OpCode opCode, uWS::App& app, DevicesInfoFetcher& devicesInfoFetcher) {
    Logger::Info("HandleDisableDefaultBluetoothAdapter");

    devicesInfoFetcher.DisableBluetoothAdapterAsync([ws, &app, &devicesInfoFetcher, opCode](const std::string* error) {
        app.getLoop()->defer([ws, &devicesInfoFetcher, opCode](){
            if (!openSockets.contains(ws))
                return;
            auto response = MakeGetDefaultBluetoothAdapterResponse(devicesInfoFetcher).dump();
            ws->send(response, opCode, response.length() < 16 * 1024);
        });
    });
}

void HandleGetActiveDeviceInfoRequest(auto *ws, const nlohmann::json& json, uWS::OpCode opCode, DevicesInfoFetcher& devicesInfoFetcher) {
    Logger::Info("HandleGetActiveDeviceInfoRequest");

    auto response = MakeGetActiveDeviceInfoResponse(devicesInfoFetcher).dump();
    ws->send(response, opCode, response.length() < 16 * 1024);
}

void HandleGetSettingsAllRequest(auto *ws, const nlohmann::json& json, uWS::OpCode opCode, SettingsService& settingsService) {
    Logger::Info("HandleGetSettingsAllRequest");

    auto response = MakeGetSettingsAllResponse(settingsService).dump();
    ws->send(response, opCode, response.length() < 16 * 1024);
}

void HandleGetSettingsRequest(auto *ws, const nlohmann::json& json, uWS::OpCode opCode, SettingsService& settingsService) {
    Logger::Info("HandleGetSettingsRequest");

    auto container = json.at("arguments").at("container").template get<std::string>();
    auto response = MakeGetSettingsResponse(settingsService, container).dump();
    ws->send(response, opCode, response.length() < 16 * 1024);
}

void HandleGetSettingRequest(auto *ws, const nlohmann::json& json, uWS::OpCode opCode, SettingsService& settingsService) {
    Logger::Info("HandleGetSettingRequest");

    auto container = json.at("arguments").at("container").template get<std::string>();
    auto setting = json.at("arguments").at("setting").template get<std::string>();
    auto response = MakeGetSettingResponse(settingsService, container, setting).dump();
    ws->send(response, opCode, response.length() < 16 * 1024);
}

void HandleSaveSettingRequest(auto *ws, const nlohmann::json& json, uWS::OpCode opCode, SettingsService& settingsService) {
    Logger::Info("HandleSaveSettingRequest");

    auto container = json.at("arguments").at("container").template get<std::string>();
    auto setting = json.at("arguments").at("setting").template get<std::string>();
    auto value = json.at("arguments").at("value");

    if (value.is_boolean()) {
        settingsService.SaveSetting(container, setting, value.get<bool>());
    } else if (value.is_number_integer()) {
        settingsService.SaveSetting(container, setting, value.get<int64_t>());
    } else if (value.is_number_float()) {
        settingsService.SaveSetting(container, setting, value.get<double>());
    } else if (value.is_string()) {
        settingsService.SaveSetting(container, setting, value.get<std::string>());
    } else if (value.is_array()) {
        auto tomlArray = JsonArrayToToml(value);
        settingsService.SaveSetting(container, setting, tomlArray);
    } else if (value.is_object()) {
        auto tomlTable = JsonObjectToToml(value);
        settingsService.SaveSetting(container, setting, tomlTable);
    }
}

void HandleGetAllRequest(auto *ws, const nlohmann::json& json, uWS::OpCode opCode, DevicesInfoFetcher& devicesInfoFetcher) {
    Logger::Info("HandleGetAll");

    auto rootObject = nlohmann::json::object();
    auto getDevicesResponseJson = MakeGetDeviceResponse(devicesInfoFetcher);
    auto getDefaultBluetoothAdapterJson = MakeGetDefaultBluetoothAdapterResponse(devicesInfoFetcher);
    auto getDeckyInfoResponseJson = MakeGetActiveDeviceInfoResponse(devicesInfoFetcher);
    rootObject["headphones"] = getDevicesResponseJson["headphones"];
    rootObject["defaultbluetooth"] = getDefaultBluetoothAdapterJson["defaultbluetooth"];
    rootObject["info"] = getDeckyInfoResponseJson["info"];

    auto response = rootObject.dump();
    ws->send(response, opCode, response.length() < 16 * 1024);
}


void HandleIncorrectResponse(auto *ws, uWS::OpCode opCode) {
    std::string emptyResponse{};
    ws->send(emptyResponse, opCode, emptyResponse.length() < 16 * 1024);
}

void HandleRequest(auto *ws, std::string_view message, uWS::OpCode opCode, uWS::App& app, DevicesInfoFetcher& devicesInfoFetcher, SettingsService& settingsService) {
    try {
        auto json = nlohmann::json::parse(message);

        std::string methodName = json.at("method").template get<std::string>();
            if (methodName == "GetDevices")
                HandleGetDevicesRequest(ws, json, opCode, devicesInfoFetcher);
            else if (methodName == "ConnectDevice")
                HandleConnectDeviceRequest(ws, json, opCode, app, devicesInfoFetcher);
            else if (methodName == "DisconnectDevice")
                HandleDisconnectDeviceRequest(ws, json, opCode, app, devicesInfoFetcher);
            else if (methodName == "SetCapabilities")
                HandleSetCapabilitiesRequest(ws, json, opCode, devicesInfoFetcher);
            else if (methodName == "GetDefaultBluetoothAdapter")
                HandleGetDefaultBluetoothAdapterRequest(ws, json, opCode, devicesInfoFetcher);
            else if (methodName == "EnableDefaultBluetoothAdapter")
                HandleEnableDefaultBluetoothAdapter(ws, json, opCode, app, devicesInfoFetcher);
            else if (methodName == "DisableDefaultBluetoothAdapter")
                HandleDisableDefaultBluetoothAdapter(ws, json, opCode, app, devicesInfoFetcher);
            else if (methodName == "GetActiveDeviceInfo")
                HandleGetActiveDeviceInfoRequest(ws, json, opCode, devicesInfoFetcher);
            else if (methodName == "GetAll")
                HandleGetAllRequest(ws, json, opCode, devicesInfoFetcher);
            else if (methodName == "GetSettingsAll")
                HandleGetSettingsAllRequest(ws, json, opCode, settingsService);
            else if (methodName == "GetSettings")
                HandleGetSettingsRequest(ws, json, opCode, settingsService);
            else if (methodName == "GetSetting")
                HandleGetSettingRequest(ws, json, opCode, settingsService);
            else if (methodName == "SetSetting")
                HandleSaveSettingRequest(ws, json, opCode, settingsService);
            else
                HandleIncorrectResponse(ws, opCode);
    }
    catch(const std::exception& exception) {
        HandleIncorrectResponse(ws, opCode);
    }
}

void SubscribeAndHandleBroadcastEvents(uWS::App& app, DevicesInfoFetcher& devicesInfoFetcher, SettingsService& settingsService) {
    auto onCapabilityChangedListener = [&app, &devicesInfoFetcher](std::shared_ptr<Device> device, const Capability& newValues) {
        if (!device)
            return; // unpaired while the event was on its way
        if (auto activeDevice = devicesInfoFetcher.GetActiveDevice(); activeDevice && activeDevice->GetAddress() == device->GetAddress()) {
            Logger::Info("onCapabilityChanged Broadcast was triggered");
            app.getLoop()->defer([&app, &devicesInfoFetcher](){
                auto response = MakeGetActiveDeviceInfoResponse(devicesInfoFetcher).dump();
                app.publish("onCapabilityChanged", response, uWS::OpCode::TEXT, response.length() < 16 * 1024);
            });
        }
    };

    auto onConnectedChangedListener = [&app, &devicesInfoFetcher](std::shared_ptr<Device> device, bool newValue) {
        Logger::Info("OnConnectedChanged Broadcast was triggered");
        app.getLoop()->defer([&app, &devicesInfoFetcher]() {
            auto response = MakeGetDeviceResponse(devicesInfoFetcher).dump();
            app.publish("OnConnectedChanged", response, uWS::OpCode::TEXT, response.length() < 16 * 1024);
        });
    };
    auto onActiveDeviceChanged = [&app, &devicesInfoFetcher](std::shared_ptr<Device> device) {
        Logger::Info("OnActiveDeviceChanged Broadcast was triggered");
        app.getLoop()->defer([&app, &devicesInfoFetcher]() {
            auto response = MakeGetActiveDeviceInfoResponse(devicesInfoFetcher).dump();
            app.publish("OnActiveDeviceChanged", response, uWS::OpCode::TEXT, response.length() < 16 * 1024);
        });
    };
    auto onDefaultAdapterChangeEnabled = [&app, &devicesInfoFetcher](bool newValue) {
        Logger::Info("OnDefaultAdapterChangeEnabled Broadcast was triggered");
        app.getLoop()->defer([&app, &devicesInfoFetcher]() {
            auto response = MakeGetDefaultBluetoothAdapterResponse(devicesInfoFetcher).dump();
            app.publish("OnDefaultAdapterChangeEnabled", response, uWS::OpCode::TEXT, response.length() < 16 * 1024);
        });
    };

    auto onAnimationTriggered = [&app, &settingsService](nlohmann::json newValue) {
        // the BLE scan may also run for automatic switching, so the popup setting is checked here
        if (!settingsService.GetValue<bool>("magicpods", "animation").value_or(true))
            return;
        Logger::Info("onAnimationTriggered Broadcast was triggered");
        app.getLoop()->defer([&app, newValue]() {
            auto response = newValue.dump();
            app.publish("onAnimationTriggered", response, uWS::OpCode::TEXT, response.length() < 16 * 1024);
        });
    };

    for (auto& device : devicesInfoFetcher.GetDevices()) {
        device->GetCapabilityChangedEvent().Subscribe([onCapabilityChangedListener, weakDevice = std::weak_ptr(device)](size_t listenerId, auto& newValues) {
            onCapabilityChangedListener(weakDevice.lock(), newValues);
        });
        device->GetConnectedPropertyChangedEvent().Subscribe([onConnectedChangedListener, weakDevice = std::weak_ptr(device)](size_t listenerId, auto newValue) {
            onConnectedChangedListener(weakDevice.lock(), newValue);
        });

        if (auto aap = std::dynamic_pointer_cast<AapDevice>(device)){
            aap->GetAnimationTriggered().Subscribe([onAnimationTriggered](size_t listenerId, const nlohmann::json &newValue){
                onAnimationTriggered(newValue);
            });
        }
    }
    devicesInfoFetcher.GetOnDeviceAddEvent().Subscribe([onCapabilityChangedListener, onConnectedChangedListener, onAnimationTriggered](size_t listenerId, auto device) {
        device->GetCapabilityChangedEvent().Subscribe([onCapabilityChangedListener, weakDevice = std::weak_ptr(device)](size_t listenerId, auto& newValues) {
            onCapabilityChangedListener(weakDevice.lock(), newValues);
        });
        device->GetConnectedPropertyChangedEvent().Subscribe([onConnectedChangedListener, weakDevice = std::weak_ptr(device)](size_t listenerId, auto newValue) {
            onConnectedChangedListener(weakDevice.lock(), newValue);
        });
        if (auto aap = std::dynamic_pointer_cast<AapDevice>(device)){
            aap->GetAnimationTriggered().Subscribe([onAnimationTriggered](size_t listenerId, const nlohmann::json &newValue){
                onAnimationTriggered(newValue);
            });
        }
        // Pushes the device list, so a newly paired device shows up without a restart.
        onConnectedChangedListener(device, device->GetConnected());
    });
    devicesInfoFetcher.GetOnDeviceRemoveEvent().Subscribe([onConnectedChangedListener](size_t listenerId, auto device) {
        onConnectedChangedListener(device, false);
    });
    devicesInfoFetcher.GetOnActiveDeviceChangedEvent().Subscribe([onActiveDeviceChanged](size_t listenerId, auto newDevice) {
        onActiveDeviceChanged(newDevice);
    });
    devicesInfoFetcher.GetOnDefaultAdapterChangeEnabledEvent().Subscribe([onDefaultAdapterChangeEnabled](size_t listenerId, bool newValue) {
        onDefaultAdapterChangeEnabled(newValue);
    });

    auto onSettingUpdate = [&app, &settingsService](const UpdatedSettingNotification& notification) {
        Logger::Info("OnSettingUpdate Broadcast was triggered");

        std::string container{notification.GetContainerName()};
        std::string setting{notification.GetSettingName()};
        if (IsSecretSetting(setting))
            return;

        app.getLoop()->defer([&app, &settingsService, container, setting]() {
            auto response = MakeGetSettingResponse(settingsService, container, setting).dump();
            app.publish("OnSettingUpdate", response, uWS::OpCode::TEXT, response.length() < 16 * 1024);
        });
    };

    settingsService.GetOnSettingUpdateEvent().Subscribe([onSettingUpdate](size_t listenerId, const UpdatedSettingNotification& notification) {
        onSettingUpdate(notification);
    });
}

bool TryToParseArguments(int argc, char** argv) {
    for (int i = 0; i < argc; ++i) {
        std::string argument{argv[i]};
        if (argument == "--version" || argument == "-version" || argument == "-v") {
            Logger::Info(CMAKE_PROJECT_NAME " " CMAKE_PROJECT_VERSION);
            return true;
        }
        else if (argument == "--help" || argument == "-help" || argument == "-h") {
            Logger::Info(
                "Help is under development. For more information and to contact us, please visit magicpods.app.\n"
                "Developed by Aleksandr Maslov<MagicPods@outlook.com> and Andrei Litvintsev<a.a.litvintsev@gmail.com>");
            return true;
        }
    }
    return false;
}

void StartListeningLogSettings(SettingsService &settingsService) {
    #ifdef DEBUG
    Logger::SetLoggingLevelForGlobalLogger(LogLevel::Debug);
    return;
    #endif

    if (auto currentSettingsLogLevel = settingsService.GetValue<int64_t>("magicpods", "logLevel")) {
        Logger::SetLoggingLevelForGlobalLogger(*currentSettingsLogLevel);
    }

    settingsService.GetOnSettingUpdateEvent().Subscribe([](size_t listenerId, const UpdatedSettingNotification& notification) {
        if (notification.GetContainerName() == "magicpods" && notification.GetSettingName() == "logLevel") {
            if (notification.GetValue().is_integer()) {
                Logger::SetLoggingLevelForGlobalLogger(notification.GetValue().as_integer()->get());
            }
        }
    });
}

int main(int argc, char** argv) {
    if (TryToParseArguments(argc, argv))
        return 0;

    // Byte-level self-checks without hardware: magicpodscore --selftest
    if (argc > 1 && std::string{argv[1]} == "--selftest") {
        int failures = TestsSgb{}.failures + TestsAapBle{}.failures + TestsAapAudio{}.failures + TestsZik{}.failures + TestsCore{}.failures;
        Logger::Info("Selftest: %d failure(s)", failures);
        return failures == 0 ? 0 : 1;
    }
#ifndef _WIN32
    // Emulated AirPods Max through the real audio path, needs PipeWire: magicpodscore --emulate-airpods
    if (argc > 1 && std::string{argv[1]} == "--emulate-airpods")
        return EmulateAirPods();

    // SIGTERM (systemd, the UI) and SIGINT take the effect chain down too; its pipewire process would
    // otherwise outlive the daemon. Blocked before any thread starts, so only the thread below gets them.
    sigset_t stopSignals;
    sigemptyset(&stopSignals);
    sigaddset(&stopSignals, SIGTERM);
    sigaddset(&stopSignals, SIGINT);
    pthread_sigmask(SIG_BLOCK, &stopSignals, nullptr);
    std::thread([stopSignals]() {
        int signal = 0;
        sigwait(&stopSignals, &signal);
        Logger::Info("Signal %d, stopping", signal);
        AudioEffects::Instance().Stop();
        // settings are written on every change and BlueZ ends the discovery of a client that goes away,
        // so nothing else needs an orderly shutdown
        std::_Exit(0);
    }).detach();
#else
    // Bluetooth, media sessions and audio devices are WinRT/COM; every thread of the daemon joins this apartment
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
#endif

    std::shared_ptr<SettingsService> settingsService = std::make_shared<SettingsService>(SettingsService::GetConfigPath("config.toml"));
    StartListeningLogSettings(*settingsService);

    // fix stdout buffering issue, when python does not receive output
    setvbuf(stdout, NULL, _IONBF, 0);

    Logger::Info(CMAKE_PROJECT_NAME " " CMAKE_PROJECT_VERSION);

    std::atomic<bool> initialized{false};

    std::unique_ptr<DevicesInfoFetcher> devicesInfoFetcher;

    /* ws->getUserData returns one of these */
    struct PerSocketData {
        /* Fill with user data */
    };

    /* Keep in mind that uWS::SSLApp({options}) is the same as uWS::App() when compiled without SSL support.
     * You may swap to using uWS:App() if you don't need SSL */
    uWS::App app{};

    app.ws<PerSocketData>("/*", {
        /* Settings */
        .compression = uWS::CompressOptions(uWS::DEDICATED_COMPRESSOR_4KB | uWS::DEDICATED_DECOMPRESSOR),
        .maxPayloadLength = 100 * 1024 * 1024,
        .idleTimeout = 16,
        .maxBackpressure = 100 * 1024 * 1024,
        .closeOnBackpressureLimit = false,
        .resetIdleTimeoutOnSend = false,
        .sendPingsAutomatically = true,
        /* Handlers */
        // Browsers don't apply the same-origin policy to WebSockets, so any web page could otherwise read the
        // AirPods keys and change settings through ws://localhost. They always send Origin, our clients
        // (the Qt UI, websocat, scripts) don't.
        .upgrade = [](auto *res, auto *req, auto *context) {
            if (!req->getHeader("origin").empty()) {
                Logger::Warn("Rejected WebSocket connection from origin %s", std::string{req->getHeader("origin")}.c_str());
                res->writeStatus("403 Forbidden")->end();
                return;
            }
            res->template upgrade<PerSocketData>({}, req->getHeader("sec-websocket-key"), req->getHeader("sec-websocket-protocol"),
                                                 req->getHeader("sec-websocket-extensions"), context);
        },
        .open = [&initialized](auto* ws) {
            if (!initialized.load()) {
                Logger::Info("Connection attempted before initialization complete, closing");
                ws->close();
                return;
            }
            Logger::Info("On open websocket connected");
            openSockets.insert(ws);
            ws->subscribe("onCapabilityChanged");
            ws->subscribe("OnConnectedChanged");
            ws->subscribe("OnActiveDeviceChanged");
            ws->subscribe("OnDefaultAdapterChangeEnabled");
            ws->subscribe("onAnimationTriggered");
            ws->subscribe("OnSettingUpdate");
            InitHandshake(ws);
        },
        .message = [&app, &devicesInfoFetcher, &settingsService](auto *ws, std::string_view message, uWS::OpCode opCode) {
            Logger::Debug("Received: %s", std::string{message}.c_str());
            HandleRequest(ws, message, opCode, app, *devicesInfoFetcher, *settingsService);
        },
        .dropped = [](auto */*ws*/, std::string_view /*message*/, uWS::OpCode /*opCode*/) {
            /* A message was dropped due to set maxBackpressure and closeOnBackpressureLimit limit */
        },
        .drain = [](auto */*ws*/) {
            /* Check ws->getBufferedAmount() here */
        },
        .ping = [](auto */*ws*/, std::string_view) {
            /* Not implemented yet */
        },
        .pong = [](auto */*ws*/, std::string_view) {
            /* Not implemented yet */
        },
        .close = [](auto *ws, int /*code*/, std::string_view /*message*/) {
            openSockets.erase(ws);
            Logger::Info("On open websocket closed");
        }
    }).listen("127.0.0.1", WEBSOCKET_PORT, LIBUS_LISTEN_EXCLUSIVE_PORT, [&](auto *listen_socket) { // loopback only, the API has no authentication
        if (listen_socket) {
            Logger::Info("Listening on port %d", WEBSOCKET_PORT);

            Logger::Info("Starting initialization...");

            #ifdef DEBUG
            TestsSgb sgb;
            TestsAapBle aapBle;
            TestsAapAudio aapAudio;
            #endif

            try {
                devicesInfoFetcher = std::make_unique<DevicesInfoFetcher>(settingsService);
            } catch (const std::exception& e) {
                // systemd (Restart=on-failure) or the UI start the daemon again, so a USB adapter plugged in
                // later or a bluetoothd that is still starting is picked up
                Logger::Error("Bluetooth unavailable (%s), exiting", e.what());
                std::_Exit(1);
            }

            SubscribeAndHandleBroadcastEvents(app, *devicesInfoFetcher, *settingsService);

            Logger::Info("Initialization complete, ready to accept connections");
            initialized.store(true);
        } else {
            Logger::Error("Failed to listen on port %d", WEBSOCKET_PORT);
            exit(1);
        }
    }).run();
}
