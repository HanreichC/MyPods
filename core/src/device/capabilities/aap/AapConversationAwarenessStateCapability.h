// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2025 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once
#include "AapCapability.h"
#include "sdk/aap/watchers/AapConversationAwarenessStateWatcher.h"
#include "sdk/aap/enums/AapConversationAwarenessState.h"
#include <atomic>
#include <mutex>
#include <optional>

namespace MagicPodsCore
{
    // Whether the wearer is speaking (Conversation Awareness). Like an iPhone, this computer lowers its
    // media volume on the AirPods while they speak and brings it back afterwards.
    class AapConversationAwarenessStateCapability : public AapCapability
    {
    private:
        bool option;
        AapConversationAwarenessStateWatcher watcher{};
        size_t watcherEventId;
        bool AapConversationAwarenessStateToBoolean(AapConversationAwarenessState mode);

        std::mutex duckLock{};
        std::optional<double> volumeBeforeDuck{};
        std::atomic<int> duckGeneration{0};
        void Duck(bool speaking);

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnReceivedData(const std::vector<unsigned char> &data) override;
        void Reset() override;

    public:
        explicit AapConversationAwarenessStateCapability(AapDevice& device);
        ~AapConversationAwarenessStateCapability() override;
        void SetFromJson(const nlohmann::json &json) override {};

        // Share of the volume left while speaking
        // ponytail: fixed, iOS lets the user pick it; a setting would go next to Conversation Awareness
        static constexpr double DUCKED_VOLUME = 0.3;
        // Level byte of 04 00 04 00 4B 00 02 00 01 <level>: true = started speaking (1, 2),
        // false = back to normal (6, 8, 9), nullopt = in between. Mapping from LibrePods (AirPodsService.kt).
        static std::optional<bool> SpeakingFromLevel(int level);
    };
}
