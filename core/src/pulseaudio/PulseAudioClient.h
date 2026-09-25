// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#ifndef _WIN32
#include <pulse/pulseaudio.h>
#endif
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
    
    // What a sink plays: sample rate, format ("s24le"), channels and the Bluetooth codec PipeWire negotiated ("ldac", empty if none)
    struct SinkDetails {
        uint32_t rate = 0;
        std::string format;
        uint8_t channels = 0;
        std::string codec;

        bool operator==(const SinkDetails&) const = default;
    };

    // Sound server access for codec display, output switching and effects. Windows has no A2DP/HFP
    // profiles or codecs to pick and routes to connected headphones itself, so there it's an empty
    // stub (PulseAudioClient_win.cpp) and the capabilities that need it stay hidden.
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
            // Sink volume averaged over its channels, 1.0 = 100 %
            std::optional<double> GetSinkVolume(const std::string& name);
            bool SetSinkVolume(const std::string& name, double volume);
            std::optional<SinkDetails> GetSinkDetails(const std::string& name);
            Event<CardInfo>& GatAudioCardPropertyChangedEvent() {
                return _onAudioCardPropertyChangedEvent;
            }
            // A sink changed (volume, mute, port), with its index; fired on the PulseAudio thread like the card event
            Event<uint32_t>& GetSinkChangedEvent() {
                return _onSinkChangedEvent;
            }
        private:
            Event<CardInfo> _onAudioCardPropertyChangedEvent{};
            Event<uint32_t> _onSinkChangedEvent{};
#ifndef _WIN32
            std::atomic<bool> ready{false};
            pa_threaded_mainloop* ml {nullptr};
            pa_context* ctx {nullptr};
            bool Usable();
            // Waits with the loop lock held until the operation finishes, false if it could not start
            bool Wait(pa_operation* op);
            bool RequestCardProfile(const std::string& name, const std::string& profile);
            void Free();
#endif

    };
}
