// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "PulseAudioClient.h"
#include "Logger.h"

#include <algorithm>
#include <cctype>

namespace MagicPodsCore
{
    namespace
    {
        // libpulse is not thread-safe: every call from outside the loop thread holds the loop lock
        struct Lock
        {
            pa_threaded_mainloop *ml;
            explicit Lock(pa_threaded_mainloop *ml) : ml(ml) { pa_threaded_mainloop_lock(ml); }
            ~Lock() { pa_threaded_mainloop_unlock(ml); }
        };

        CardInfo ToCardInfo(const pa_card_info *info)
        {
            CardInfo out;
            if (info->name) out.name = info->name;
            if (info->active_profile && info->active_profile->name)
                out.activeProfile = info->active_profile->name;
            // Unavailable profiles (e.g. A2DP before its link is up) can't be activated, so they aren't offered
            for (auto p = info->profiles2; p && *p; ++p)
                if ((*p)->name && ((*p)->available || out.activeProfile == (*p)->name))
                    out.profiles.emplace_back((*p)->name, (*p)->description ? (*p)->description : "");
            return out;
        }
    }

    PulseAudioClient::PulseAudioClient()
    {
        ml = pa_threaded_mainloop_new();
        auto *api = pa_threaded_mainloop_get_api(ml);
        // armed whenever the connection fails or drops: the sound server may start after the daemon or restart
        retry = api->time_new(api, nullptr, [](pa_mainloop_api *, pa_time_event *, const struct timeval *, void *userdata) {
            static_cast<PulseAudioClient*>(userdata)->Connect();
        }, this);
        Connect();
        pa_threaded_mainloop_start(ml);

        // the first answers are wanted right away (wired headphones at startup), so wait for the first attempt
        for (int i = 0; i < 300 && !ready && !attempted; i++) // 3 s
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (!ready)
            Logger::Error("PulseAudioClient: no connection to the sound server yet, retrying in the background");
    }

    void PulseAudioClient::Connect()
    {
        // on the loop thread (timer) or before it runs: the old context's callbacks are not on the stack here
        if (ctx)
        {
            pa_context_set_state_callback(ctx, nullptr, nullptr);
            pa_context_disconnect(ctx);
            pa_context_unref(ctx);
        }
        ctx = pa_context_new(pa_threaded_mainloop_get_api(ml), "MagicPodsCore");
        pa_context_set_state_callback(ctx, [](pa_context *c, void *userdata) {
            auto *self = static_cast<PulseAudioClient*>(userdata);
            auto state = pa_context_get_state(c);
            if (state == PA_CONTEXT_READY)
                return self->OnReady();
            if (PA_CONTEXT_IS_GOOD(state))
                return;
            if (self->ready.exchange(false))
                Logger::Error("PulseAudioClient: lost the sound server, reconnecting");
            self->attempted = true;
            self->RetrySoon();
        }, this);
        if (pa_context_connect(ctx, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr) < 0)
        {
            attempted = true;
            RetrySoon();
        }
    }

    void PulseAudioClient::RetrySoon()
    {
        struct timeval tv;
        pa_gettimeofday(&tv);
        pa_timeval_add(&tv, 2 * PA_USEC_PER_SEC);
        auto *api = pa_threaded_mainloop_get_api(ml);
        api->time_restart(retry, &tv);
    }

    void PulseAudioClient::OnCardInfo(pa_context *, const pa_card_info *info, int eol, void *userdata)
    {
        if (!eol && info)
            static_cast<PulseAudioClient*>(userdata)->_onAudioCardPropertyChangedEvent.FireEvent(ToCardInfo(info));
    }

    void PulseAudioClient::OnReady()
    {
        // Runs on the loop thread with the lock held: subscribers must not call back into this client
        pa_context_set_subscribe_callback(ctx, [](pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata) {
            if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK)
                static_cast<PulseAudioClient*>(userdata)->_onSinkChangedEvent.FireEvent(idx);
            if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK_INPUT)
                static_cast<PulseAudioClient*>(userdata)->_onStreamChangedEvent.FireEvent(idx);
            if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) != PA_SUBSCRIPTION_EVENT_CARD)
                return;
            if (auto op = pa_context_get_card_info_by_index(c, idx, OnCardInfo, userdata))
                pa_operation_unref(op);
        }, this);
        auto mask = PA_SUBSCRIPTION_MASK_CARD | PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SINK_INPUT;
        if (auto op = pa_context_subscribe(ctx, static_cast<pa_subscription_mask_t>(mask), nullptr, nullptr))
            pa_operation_unref(op);
        ready = true;
        if (!connectedBefore.exchange(true))
            return;
        // back after a restart: what changed meanwhile was never reported. The sink event refreshes wired
        // headphones, the cards route Bluetooth headphones and their effect chain again.
        Logger::Info("PulseAudioClient: sound server back");
        _onReconnectedEvent.FireEvent(true);
        _onSinkChangedEvent.FireEvent(PA_INVALID_INDEX);
        if (auto op = pa_context_get_card_info_list(ctx, OnCardInfo, this))
            pa_operation_unref(op);
    }

    PulseAudioClient::~PulseAudioClient()
    {
        Free();
    }

    bool PulseAudioClient::Usable()
    {
        if (!ready.load())
            return false;
        if (pa_threaded_mainloop_in_thread(ml))
        {
            Logger::Error("PulseAudioClient: called from a PulseAudio callback, it would wait for itself");
            return false;
        }
        return true;
    }

    bool PulseAudioClient::Wait(pa_operation *op)
    {
        if (!op)
            return false;
        pa_operation_set_state_callback(op, [](pa_operation*, void* ml) {
            pa_threaded_mainloop_signal(static_cast<pa_threaded_mainloop*>(ml), 0);
        }, ml);
        while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
            pa_threaded_mainloop_wait(ml);
        pa_operation_unref(op);
        return true;
    }

    bool PulseAudioClient::SetCardProfile(const std::string &name, const std::string &profile)
    {
        // PipeWire acks at once but switches asynchronously (an A2DP codec change renegotiates for ~3 s)
        // and loses a request that arrives mid-switch, so wait for the card and resend if it never arrives.
        // Resending sooner than a switch takes restarts it, hence the 4 s window.
        for (int attempt = 0; attempt < 3; attempt++)
        {
            if (!RequestCardProfile(name, profile))
                return false;
            for (int i = 0; i < 40; i++)
            {
                auto info = GetCardInfoByName(name);
                if (!info)
                    return false;
                if (info->activeProfile == profile)
                    return true;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        return false;
    }

    bool PulseAudioClient::RequestCardProfile(const std::string &name, const std::string &profile)
    {
        if (!Usable()) return false;

        bool ok = false;
        Lock lock{ml};
        return Wait(pa_context_set_card_profile_by_name(ctx, name.c_str(), profile.c_str(),
            [](pa_context*, int success, void* userdata) { *static_cast<bool*>(userdata) = success; }, &ok)) && ok;
    }

    std::optional<CardInfo> PulseAudioClient::GetCardInfoByName(const std::string &name)
    {
        if (!Usable()) return std::nullopt;

        std::optional<CardInfo> card;
        Lock lock{ml};
        Wait(pa_context_get_card_info_by_name(ctx, name.c_str(),
            [](pa_context*, const pa_card_info* info, int eol, void* userdata) {
                if (!eol && info)
                    *static_cast<std::optional<CardInfo>*>(userdata) = ToCardInfo(info);
            }, &card));
        return card;
    }

    std::optional<std::string> PulseAudioClient::FindSink(const std::string &part)
    {
        if (!Usable()) return std::nullopt;

        std::pair<std::string, std::optional<std::string>> query{part, std::nullopt};
        Lock lock{ml};
        Wait(pa_context_get_sink_info_list(ctx,
            [](pa_context*, const pa_sink_info* info, int eol, void* userdata) {
                auto* q = static_cast<std::pair<std::string, std::optional<std::string>>*>(userdata);
                if (eol || !info || !info->name || q->second) return;
                if (SinkOf(info->name, q->first))
                    q->second = info->name;
            }, &query));
        return query.second;
    }

    bool PulseAudioClient::SetDefaultSink(const std::string &name)
    {
        if (!Usable()) return false;

        bool ok = false;
        Lock lock{ml};
        return Wait(pa_context_set_default_sink(ctx, name.c_str(),
            [](pa_context*, int success, void* userdata) { *static_cast<bool*>(userdata) = success; }, &ok)) && ok;
    }

    std::optional<double> PulseAudioClient::GetSinkVolume(const std::string &name)
    {
        if (!Usable()) return std::nullopt;

        std::optional<double> volume;
        Lock lock{ml};
        Wait(pa_context_get_sink_info_by_name(ctx, name.c_str(),
            [](pa_context*, const pa_sink_info* info, int eol, void* userdata) {
                if (!eol && info)
                    *static_cast<std::optional<double>*>(userdata) = static_cast<double>(pa_cvolume_avg(&info->volume)) / PA_VOLUME_NORM;
            }, &volume));
        return volume;
    }

    std::optional<SinkDetails> PulseAudioClient::GetSinkDetails(const std::string &name)
    {
        if (!Usable()) return std::nullopt;

        std::optional<SinkDetails> details;
        Lock lock{ml};
        Wait(pa_context_get_sink_info_by_name(ctx, name.c_str(),
            [](pa_context*, const pa_sink_info* info, int eol, void* userdata) {
                if (eol || !info)
                    return;
                const char *codec = pa_proplist_gets(info->proplist, "api.bluez5.codec");
                *static_cast<std::optional<SinkDetails>*>(userdata) = SinkDetails{info->sample_spec.rate,
                    pa_sample_format_to_string(info->sample_spec.format), info->sample_spec.channels, codec ? codec : "", info->index};
            }, &details));
        return details;
    }

    std::vector<StreamInfo> PulseAudioClient::GetStreams()
    {
        if (!Usable()) return {};

        std::vector<StreamInfo> streams;
        Lock lock{ml};
        Wait(pa_context_get_sink_input_info_list(ctx,
            [](pa_context*, const pa_sink_input_info* info, int eol, void* userdata) {
                if (eol || !info)
                    return;
                static_cast<std::vector<StreamInfo>*>(userdata)->push_back({info->sink, info->sample_spec.rate,
                    pa_sample_format_to_string(info->sample_spec.format), info->sample_spec.channels,
                    static_cast<double>(pa_cvolume_avg(&info->volume)) / PA_VOLUME_NORM, info->corked != 0});
            }, &streams));
        return streams;
    }

    std::optional<WiredOutput> PulseAudioClient::WiredHeadphones(const pa_sink_info &info)
    {
        auto prop = [&](const char *key) { const char *v = pa_proplist_gets(info.proplist, key); return std::string(v ? v : ""); };
        auto lower = [](std::string text) { std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); }); return text; };
        // ALSA only: not Bluetooth, not our effect chain or another virtual sink
        if (!info.name || prop("device.api") != "alsa")
            return std::nullopt;
        const auto *port = info.active_port;
        // an empty jack: the sink plays on the speakers (or nowhere)
        if (port && port->available == PA_PORT_AVAILABLE_NO)
            return std::nullopt;
        bool usb = prop("device.bus") == "usb";
        bool jack = port && (port->type == PA_DEVICE_PORT_TYPE_HEADPHONES || port->type == PA_DEVICE_PORT_TYPE_HEADSET ||
                             lower(port->name ? port->name : "").find("headphone") != std::string::npos);
        auto formFactor = prop("device.form_factor");
        auto text = lower(std::string(info.name) + " " + (info.description ? info.description : "") + " " + prop("device.product.name"));
        // ponytail: a USB DAC that calls itself neither headphones nor headset counts as speakers; a setting to mark outputs would cover it
        if (!jack && formFactor != "headphone" && formFactor != "headset" &&
            !(usb && (text.find("headphone") != std::string::npos || text.find("headset") != std::string::npos)))
            return std::nullopt;
        // the jack by its port ("Headphones", in the system's language), USB by its product
        std::string name = !usb && port && port->description ? port->description : prop("device.product.name");
        if (name.empty())
            name = info.description ? info.description : info.name;
        return WiredOutput{info.name, name, usb};
    }

    std::vector<WiredOutput> PulseAudioClient::GetWiredHeadphones()
    {
        if (!Usable()) return {};

        std::vector<WiredOutput> outputs;
        Lock lock{ml};
        Wait(pa_context_get_sink_info_list(ctx,
            [](pa_context*, const pa_sink_info* info, int eol, void* userdata) {
                if (eol || !info) return;
                if (auto output = WiredHeadphones(*info))
                    static_cast<std::vector<WiredOutput>*>(userdata)->push_back(*output);
            }, &outputs));
        return outputs;
    }

    std::vector<WiredOutput> PulseAudioClient::GetOutputs()
    {
        if (!Usable()) return {};

        std::vector<WiredOutput> outputs;
        Lock lock{ml};
        Wait(pa_context_get_sink_info_list(ctx,
            [](pa_context*, const pa_sink_info* info, int eol, void* userdata) {
                const char *api = info && !eol ? pa_proplist_gets(info->proplist, "device.api") : nullptr;
                if (!api || std::string(api) != "alsa" || !info->name)
                    return;
                const char *bus = pa_proplist_gets(info->proplist, "device.bus");
                static_cast<std::vector<WiredOutput>*>(userdata)->push_back(
                    {info->name, info->description ? info->description : info->name, bus && std::string(bus) == "usb"});
            }, &outputs));
        return outputs;
    }

    bool PulseAudioClient::SetSinkVolume(const std::string &name, double volume)
    {
        if (!Usable()) return false;

        uint8_t channels = 0;
        Lock lock{ml};
        Wait(pa_context_get_sink_info_by_name(ctx, name.c_str(),
            [](pa_context*, const pa_sink_info* info, int eol, void* userdata) {
                if (!eol && info)
                    *static_cast<uint8_t*>(userdata) = info->volume.channels;
            }, &channels));
        if (channels == 0)
            return false;
        pa_cvolume cv;
        pa_cvolume_set(&cv, channels, static_cast<pa_volume_t>(std::clamp(volume, 0.0, 1.5) * PA_VOLUME_NORM));
        bool ok = false;
        return Wait(pa_context_set_sink_volume_by_name(ctx, name.c_str(), &cv,
            [](pa_context*, int success, void* userdata) { *static_cast<bool*>(userdata) = success; }, &ok)) && ok;
    }

    std::string PulseAudioClient::GetNameFromMac(const std::string &mac)
    {
        std::string name = mac;
        std::replace(name.begin(), name.end(), ':', '_');
        std::transform(name.begin(), name.end(), name.begin(),
               [](unsigned char c){ return std::toupper(c); });

        return "bluez_card." + name;
    }

    void PulseAudioClient::Free()
    {
        ready.store(false);
        if (ml)
            pa_threaded_mainloop_stop(ml);
        if (ctx)
        {
            pa_context_set_state_callback(ctx, nullptr, nullptr);
            pa_context_disconnect(ctx);
            pa_context_unref(ctx);
            ctx = nullptr;
        }
        if (ml)
        {
            pa_threaded_mainloop_free(ml);
            ml = nullptr;
        }
    }
}
