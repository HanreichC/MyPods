// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include <pulse/pulseaudio.h>
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <atomic>
#include <thread>
#include "Event.h"

namespace MagicPodsCore{

    struct CardInfo {
        std::string name;
        std::string activeProfile;
        std::vector<std::pair<std::string,std::string>> profiles;

    bool operator==(const CardInfo& other) const {
        return name == other.name &&
               activeProfile == other.activeProfile &&
               profiles == other.profiles;
    }
    };
    
    class PulseAudioClient{
        public:
            PulseAudioClient();
            ~PulseAudioClient();
            // Blocks until the card reports the profile (up to ~12 s), false if it never does
            bool SetCardProfile(const std::string& name, const std::string& profile);
            std::optional<CardInfo> GetCardInfoByName(const std::string& name);
            std::string GetNameFromMac(const std::string& mac);
            // First sink whose name contains `part`, e.g. the MAC with underscores for a bluez sink
            std::optional<std::string> FindSink(const std::string& part);
            bool SetDefaultSink(const std::string& name);
            Event<CardInfo>& GatAudioCardPropertyChangedEvent() {
                return _onAudioCardPropertyChangedEvent;
            }
        private:
            Event<CardInfo> _onAudioCardPropertyChangedEvent{};
            std::atomic<bool> ready{false};
            pa_threaded_mainloop* ml {nullptr};
            pa_context* ctx {nullptr};
            bool Usable();
            // Waits with the loop lock held until the operation finishes, false if it could not start
            bool Wait(pa_operation* op);
            bool RequestCardProfile(const std::string& name, const std::string& profile);
            void Free();

    };
}
