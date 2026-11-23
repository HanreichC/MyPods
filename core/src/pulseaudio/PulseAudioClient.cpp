// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "PulseAudioClient.h"
#include "Logger.h"

#include <algorithm>
#include <cctype>

namespace MyPods
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
        ctx = pa_context_new(pa_threaded_mainloop_get_api(ml), "MyPods");
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
            if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK ||
                (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SERVER) // the default sink changed
                static_cast<PulseAudioClient*>(userdata)->_onSinkChangedEvent.FireEvent(idx);
            if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK_INPUT)
                static_cast<PulseAudioClient*>(userdata)->_onStreamChangedEvent.FireEvent(idx);
            if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) != PA_SUBSCRIPTION_EVENT_CARD)
                return;
            if (auto op = pa_context_get_card_info_by_index(c, idx, OnCardInfo, userdata))
                pa_operation_unref(op);
        }, this);
        auto mask = PA_SUBSCRIPTION_MASK_CARD | PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SINK_INPUT | PA_SUBSCRIPTION_MASK_SERVER;
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

    std::optional<std::string> PulseAudioClient::GetDefaultSink()
    {
        if (!Usable()) return std::nullopt;

        std::optional<std::string> sink;
        Lock lock{ml};
        Wait(pa_context_get_server_info(ctx,
            [](pa_context*, const pa_server_info* info, void* userdata) {
                if (info && info->default_sink_name)
                    *static_cast<std::optional<std::string>*>(userdata) = info->default_sink_name;
            }, &sink));
        return sink;
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

    std::vector<WiredOutput> PulseAudioClient::GetOutputs(bool playback)
    {
        if (!Usable()) return {};

        // hardware only: the effect chain and MyPods' silent sink are no place to pick
        std::pair<std::vector<WiredOutput>, bool> query{{}, playback};
        Lock lock{ml};
        Wait(pa_context_get_sink_info_list(ctx,
            [](pa_context*, const pa_sink_info* info, int eol, void* userdata) {
                auto *q = static_cast<std::pair<std::vector<WiredOutput>, bool>*>(userdata);
                const char *api = info && !eol ? pa_proplist_gets(info->proplist, "device.api") : nullptr;
                if (!api || !info->name || (std::string(api) != "alsa" && !(q->second && std::string(api) == "bluez5")))
                    return;
                const char *bus = pa_proplist_gets(info->proplist, "device.bus");
                q->first.push_back({info->name, info->description ? info->description : info->name, bus && std::string(bus) == "usb"});
            }, &query));
        if (playback)
            Wait(pa_context_get_card_info_list(ctx,
                [](pa_context*, const pa_card_info* info, int eol, void* userdata) {
                    if (eol || !info)
                        return;
                    auto &outputs = static_cast<std::pair<std::vector<WiredOutput>, bool>*>(userdata)->first;
                    for (auto &output : OtherProfileOutputs(*info))
                        outputs.push_back(std::move(output));
                }, &query));
        return query.first;
    }

    std::vector<WiredOutput> PulseAudioClient::OtherProfileOutputs(const pa_card_info &card)
    {
        auto prop = [](pa_proplist *list, const char *key) { const char *v = list ? pa_proplist_gets(list, key) : nullptr; return std::string(v ? v : ""); };
        std::vector<WiredOutput> outputs;
        // sound cards only: a Bluetooth card's profiles are codecs, switched on the device page
        if (!card.name || !card.ports || prop(card.proplist, "device.api") != "alsa")
            return outputs;
        std::string active = card.active_profile2 && card.active_profile2->name ? card.active_profile2->name : "";
        // ranked like the sound server ranks them, so a profile is named after the port it would play on
        std::vector<const pa_card_port_info*> ports(card.ports, card.ports + card.n_ports);
        std::ranges::stable_sort(ports, std::greater{}, [](const pa_card_port_info *p) { return p->priority; });
        std::vector<std::string> offered;
        for (const auto *port : ports)
        {
            // an empty jack or an HDMI port without a monitor plays nowhere
            if (!port || port->direction != PA_DIRECTION_OUTPUT || port->available == PA_PORT_AVAILABLE_NO || !port->profiles2)
                continue;
            const pa_card_profile_info2 *best = nullptr;
            bool playsNow = false;
            for (uint32_t i = 0; i < port->n_profiles; i++)
            {
                const auto *profile = port->profiles2[i];
                if (!profile || !profile->name)
                    continue;
                std::string name = profile->name;
                playsNow = playsNow || name == active;
                // the one the sound server would pick, which also keeps the microphone ("output:hdmi-stereo+input:analog-stereo")
                if (profile->available && profile->n_sinks > 0 && !name.starts_with("pro-audio") && (!best || profile->priority > best->priority))
                    best = profile;
            }
            if (playsNow || !best || std::ranges::find(offered, best->name) != offered.end())
                continue;
            offered.push_back(best->name);
            // HDMI: the monitor's name from its EDID-like data (ELD); else the port ("HDMI / DisplayPort", "Speakers") and the card
            std::string name = prop(port->proplist, "device.product.name");
            if (name.empty())
            {
                name = port->description ? port->description : port->name ? port->name : best->name;
                if (auto cardName = prop(card.proplist, "device.description"); !cardName.empty())
                    name += " (" + cardName + ")";
            }
            outputs.push_back({std::string(card.name) + "#" + best->name, name, prop(card.proplist, "device.bus") == "usb"});
        }
        return outputs;
    }

    bool PulseAudioClient::PlayOn(const std::string &output)
    {
        auto hash = output.find('#');
        if (hash == std::string::npos)
            return SetDefaultSink(output);
        auto card = output.substr(0, hash), profile = output.substr(hash + 1);
        if (!SetCardProfile(card, profile))
        {
            Logger::Error("PulseAudioClient: could not switch %s to %s", card.c_str(), profile.c_str());
            return false;
        }
        // the profile's sink comes a moment after the card reports it
        for (int i = 0; i < 30; i++)
        {
            if (auto sink = CardSink(card, profile))
                return SetDefaultSink(*sink);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        Logger::Error("PulseAudioClient: %s brought no sink for %s", card.c_str(), profile.c_str());
        return false;
    }

    std::optional<std::string> PulseAudioClient::CardSink(const std::string &card, const std::string &profile)
    {
        if (!Usable()) return std::nullopt;

        struct Query { uint32_t card = PA_INVALID_INDEX; std::string profile; std::optional<std::string> sink; } query{PA_INVALID_INDEX, profile, std::nullopt};
        Lock lock{ml};
        Wait(pa_context_get_card_info_by_name(ctx, card.c_str(),
            [](pa_context*, const pa_card_info* info, int eol, void* userdata) {
                if (!eol && info)
                    static_cast<Query*>(userdata)->card = info->index;
            }, &query));
        if (query.card == PA_INVALID_INDEX)
            return std::nullopt;
        Wait(pa_context_get_sink_info_list(ctx,
            [](pa_context*, const pa_sink_info* info, int eol, void* userdata) {
                auto *q = static_cast<Query*>(userdata);
                if (eol || !info || !info->name || info->card != q->card || q->sink)
                    return;
                // the sink of the new profile's output ("hdmi-stereo" in "output:hdmi-stereo+input:analog-stereo"),
                // not one of the old profile still on its way out
                if (const char *mapping = pa_proplist_gets(info->proplist, "device.profile.name"))
                {
                    auto part = "output:" + std::string(mapping);
                    auto at = q->profile.find(part);
                    if (at == std::string::npos || (at + part.size() < q->profile.size() && q->profile[at + part.size()] != '+'))
                        return;
                }
                q->sink = info->name;
            }, &query));
        return query.sink;
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
