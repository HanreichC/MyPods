// MyPods
// License: GPL-3.0

#pragma once
#include "AapCapability.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>

namespace MagicPodsCore
{
    // Apple's "Connect to This Mac: Automatically": the AirPods follow whichever device starts playing.
    // The AirPods relay "smart routing" messages between their sources (AAP opcode 0x10 out, 0x11 in),
    // report who streams (0x0E) and who is connected (0x2E); control command 0x06 = "owns connection".
    // Packet layouts from LibrePods (docs/opcodes.md, AACPManager.kt). The iPhone only talks to this
    // computer if BlueZ announces an Apple DeviceID (see README).
    class AapAudioSwitchCapability : public AapCapability
    {
    private:
        std::atomic<int> mode{0}; // 0 = automatically, 1 = when last connected to this computer
        std::string localMac;
        std::string irk;
        std::string otherDevice; // "iPhone", "iPad", "Mac" … whoever took the audio
        std::string lastA2dp;
        std::vector<std::string> connectedDevices;
        std::mutex stateLock;
        std::atomic<int> sourceType{0}; // 0 none, 1 call, 2 media (from 0x0E)
        std::atomic<bool> busy{false};
        std::atomic<bool> released{false};
        std::atomic<int64_t> bleInEarAt{0};
        std::atomic<int64_t> takeoverAt{0};
        size_t playbackEventId{};
        size_t leEventId{};
        size_t cardEventId{};

        bool InEar();
        void TakeOver(bool manual);
        void Release(bool answerRequest);

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnReceivedData(const std::vector<unsigned char> &data) override;
        void Reset() override;

    public:
        explicit AapAudioSwitchCapability(AapDevice &device);
        ~AapAudioSwitchCapability() override;
        void SetFromJson(const nlohmann::json &json) override;

        static std::vector<unsigned char> OwnsConnection(bool owns);
        static std::vector<unsigned char> SmartRouting(const std::string &targetMac, const std::vector<unsigned char> &body);
        static std::vector<unsigned char> MediaInformation(const std::string &targetMac, const std::string &selfMac, const std::string &selfName);
        static std::vector<unsigned char> ShowNearbyUI(const std::string &targetMac);
        static std::vector<unsigned char> HijackRequest(const std::string &targetMac);
        static std::optional<std::pair<std::string, int>> ParseAudioSource(const std::vector<unsigned char> &data);
        static std::vector<std::string> ParseConnectedDevices(const std::vector<unsigned char> &data);
    };
}
