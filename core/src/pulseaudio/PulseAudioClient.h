// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#ifndef _WIN32
#include <pulse/pulseaudio.h>
#endif
#include <iostream>
#include <memory>
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

    // Headphones on a jack or USB that this computer plays into right now
    struct WiredOutput {
        std::string sink;   // the sink's name
        std::string name;   // what the system calls them: the jack's port ("Headphones"), the USB product
        bool usb = false;

        bool operator==(const WiredOutput&) const = default;
    };

#ifdef _WIN32
    // A playback endpoint as Core Audio describes it; the ID ("{0.0.0.00000000}.{guid}") is its sink name
    struct AudioEndpoint {
        std::string id;
        unsigned formFactor = 0; // EndpointFormFactor: 3 headphones, 5 headset
        std::string enumerator;  // the bus: "USB", "HDAUDIO", "SOUNDWIRE", "BTHENUM", "INTELAUDIO" (Bluetooth offload)
        std::string instance;    // the adapter's device instance; Bluetooth carries the MAC in it ("BTHENUM\...&A0143D1F0BE1_C...")
        std::string bluetooth;   // the Bluetooth device instance behind an offloaded endpoint, empty otherwise
        std::string name;        // the endpoint ("Kopfhörer", "Headset Earphone")
        std::string adapter;     // the hardware ("Jabra EVOLVE LINK MS")
    };
#endif

    // Sound server access for codec display, output switching and effects. Windows has no A2DP/HFP
    // profiles or codecs to pick and routes to connected headphones itself; there it answers from
    // Core Audio (PulseAudioClient_win.cpp) with endpoints for sinks, and the rest stays empty.
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
            // Wired headphones that are plugged in and active; blocking
            std::vector<WiredOutput> GetWiredHeadphones();
            // Every output that isn't Bluetooth (speakers, monitors, jacks, USB), for the user to say which are headphones; blocking
            std::vector<WiredOutput> GetOutputs();
#ifndef _WIN32
            // The sink plays into wired headphones: a headphone jack whose port is active and not empty, or a USB headset
            static std::optional<WiredOutput> WiredHeadphones(const pa_sink_info& info);
#else
            // The endpoint is wired headphones: a headphone or headset form factor that isn't Bluetooth
            static std::optional<WiredOutput> WiredHeadphones(const AudioEndpoint& endpoint);
#endif
            Event<CardInfo>& GatAudioCardPropertyChangedEvent() {
                return _onAudioCardPropertyChangedEvent;
            }
            // A sink appeared, changed (volume, mute, port) or went away, with its index; fired on the PulseAudio thread like the card event.
            // Windows: an endpoint came, went, was plugged or unplugged (index 0), on Core Audio's notification thread
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
#else
            struct Native;
            std::unique_ptr<Native> _native;
#endif

    };
}
