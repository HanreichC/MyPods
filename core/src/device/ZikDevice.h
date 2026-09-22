// Parrot Zik 2.0 over its RFCOMM XML API, see sdk/zik/ZikProtocol.h

#pragma once

#include "Device.h"
#include "sdk/zik/ZikProtocol.h"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <thread>

namespace MagicPodsCore
{
    class ZikDevice : public Device
    {
    private:
        Event<std::string> _onAnswer{}; // XML of every answer, capabilities pick their elements
        Zik::Framer _framer{};
        std::mutex _queueMutex{};
        std::deque<std::string> _pending{};
        std::vector<std::string> _initialQueries{};
        bool _inFlight = false;
        std::chrono::steady_clock::time_point _sentAt{};
        std::condition_variable_any _pollWake{};
        std::jthread _poller{};

        void OnResponseDataReceived(const std::vector<unsigned char> &data) override;
        void SendNextLocked();
        void ResetSession();

    public:
        explicit ZikDevice(std::shared_ptr<DBusDeviceInfo> deviceInfo, std::shared_ptr<PulseAudioClient> audioClient, std::shared_ptr<SettingsService> settingsService);

        Event<std::string> &GetAnswerEvent() { return _onAnswer; }

        // The Zik answers one request at a time, so requests are queued and sent one by one
        void Query(const std::string &query);
        void Get(const std::string &path) { Query(path + "/get"); }
        // Every set is followed by a get, the answer to that updates the capability
        void Set(const std::string &path, const std::string &arg)
        {
            Query(path + "/set?arg=" + arg);
            Get(path);
        }

        static bool IsZikDevice(const std::vector<std::string> &uuids);
        static std::unique_ptr<ZikDevice> Create(std::shared_ptr<DBusDeviceInfo> deviceInfo, std::shared_ptr<PulseAudioClient> audioClient, std::shared_ptr<SettingsService> settingsService);
    };
}
