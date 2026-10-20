// MyPods
// License: GPL-3.0

#include "AapAudioSwitchCapability.h"
#include "media/MprisClient.h"
#include "dbus/DBusService.h"
#include "sdk/aap/Aes.h"
#include <algorithm>
#include <cstdio>
#include <thread>

namespace MyPods
{
    using namespace std::chrono;

    static int64_t NowMs()
    {
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    // Smart routing bodies are OPACK: 0x40+n = string of n bytes, 0x30 = int8, 0x31 = int16 LE,
    // 0xE0+n = dictionary of n pairs, 0x01 = true, 0xA0+i = back-reference to the i-th value.
    static void Str(std::vector<unsigned char> &out, const std::string &s)
    {
        out.push_back(static_cast<unsigned char>(0x40 + s.size())); // callers keep strings <= 32 bytes
        out.insert(out.end(), s.begin(), s.end());
    }

    static std::vector<unsigned char> MacBytesReversed(const std::string &mac)
    {
        unsigned int b[6]{};
        std::sscanf(mac.c_str(), "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
        return {static_cast<unsigned char>(b[5]), static_cast<unsigned char>(b[4]), static_cast<unsigned char>(b[3]),
                static_cast<unsigned char>(b[2]), static_cast<unsigned char>(b[1]), static_cast<unsigned char>(b[0])};
    }

    static std::string FormatMac(const unsigned char *b, bool reversed)
    {
        char buf[18];
        if (reversed)
            std::snprintf(buf, sizeof buf, "%02X:%02X:%02X:%02X:%02X:%02X", b[5], b[4], b[3], b[2], b[1], b[0]);
        else
            std::snprintf(buf, sizeof buf, "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
        return buf;
    }

    std::vector<unsigned char> AapAudioSwitchCapability::OwnsConnection(bool owns)
    {
        return {0x04, 0x00, 0x04, 0x00, 0x09, 0x00, 0x06, static_cast<unsigned char>(owns ? 0x01 : 0x00), 0x00, 0x00, 0x00};
    }

    std::vector<unsigned char> AapAudioSwitchCapability::SmartRouting(const std::string &targetMac, const std::vector<unsigned char> &body)
    {
        std::vector<unsigned char> packet{0x04, 0x00, 0x04, 0x00, 0x10, 0x00};
        auto mac = MacBytesReversed(targetMac);
        packet.insert(packet.end(), mac.begin(), mac.end());
        packet.push_back(static_cast<unsigned char>(body.size() & 0xff));
        packet.push_back(static_cast<unsigned char>(body.size() >> 8));
        packet.insert(packet.end(), body.begin(), body.end());
        return packet;
    }

    std::string AapAudioSwitchCapability::BannerName(const std::string &name)
    {
        static constexpr size_t MAX_BYTES = 32; // 0x40 + length, OPACK's short string
        if (name.empty())
            return "Linux";
        if (name.size() <= MAX_BYTES)
            return name;
        size_t cut = MAX_BYTES;
        while (cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xC0) == 0x80) // continuation byte: inside a character
            cut--;
        return name.substr(0, cut);
    }

    std::vector<unsigned char> AapAudioSwitchCapability::MediaInformation(const std::string &targetMac, const std::string &selfMac, const std::string &selfName)
    {
        std::vector<unsigned char> b{0x01, 0xE5};
        Str(b, "PlayingApp");
        Str(b, "com.google.ios.youtube"); // any iOS bundle id; the iPhone uses it for the "moved" banner
        Str(b, "HostStreamingState");
        Str(b, "NO");
        Str(b, "btAddress");
        Str(b, selfMac);
        Str(b, "btName");
        Str(b, selfName);
        Str(b, "otherDeviceAudioCategory");
        b.insert(b.end(), {0x31, 0x2D, 0x01});
        return SmartRouting(targetMac, b);
    }

    std::vector<unsigned char> AapAudioSwitchCapability::ShowNearbyUI(const std::string &targetMac)
    {
        std::vector<unsigned char> b{0x01, 0xE6};
        Str(b, "SmartRoutingKeyShowNearbyUI");
        b.push_back(0x01);
        Str(b, "localscore");
        b.insert(b.end(), {0x31, 0x2D, 0x01});
        Str(b, "reason");
        Str(b, "hijackv2");
        Str(b, "audioRoutingScore");
        b.push_back(0xA2);
        Str(b, "audioRoutingSetOwnershipToFalse");
        b.push_back(0x01);
        Str(b, "remotescore");
        b.push_back(0xA2);
        return SmartRouting(targetMac, b);
    }

    std::vector<unsigned char> AapAudioSwitchCapability::HijackRequest(const std::string &targetMac)
    {
        std::vector<unsigned char> b{0x01, 0xE5};
        Str(b, "localscore");
        b.insert(b.end(), {0x30, 0x64});
        Str(b, "reason");
        Str(b, "Hijackv2");
        Str(b, "audioRoutingScore");
        b.insert(b.end(), {0x31, 0x2D, 0x01});
        Str(b, "audioRoutingSetOwnershipToFalse");
        b.push_back(0x01);
        Str(b, "remotescore");
        b.push_back(0xA5);
        return SmartRouting(targetMac, b);
    }

    std::optional<std::pair<std::string, int>> AapAudioSwitchCapability::ParseAudioSource(const std::vector<unsigned char> &data)
    {
        if (data.size() < 13 || data[4] != 0x0E)
            return std::nullopt;
        return std::make_pair(FormatMac(&data[6], true), static_cast<int>(data[12]));
    }

    std::vector<std::string> AapAudioSwitchCapability::ParseConnectedDevices(const std::vector<unsigned char> &data)
    {
        std::vector<std::string> devices;
        if (data.size() < 9 || data[4] != 0x2E)
            return devices;
        for (size_t i = 0, offset = 9; i < data[8] && offset + 8 <= data.size(); i++, offset += 8)
            devices.push_back(FormatMac(&data[offset], false));
        return devices;
    }

    AapAudioSwitchCapability::AapAudioSwitchCapability(AapDevice &device) : AapCapability("autoSwitch", false, device)
    {
        mode = static_cast<int>(device.LoadSettingInt("autoSwitch").value_or(0));
        try
        {
            localMac = DBusService::GetAdapterAddress();
        }
        catch (const std::exception &e)
        {
            Logger::Error("AutoSwitch: no adapter address, cannot talk to other devices: %s", e.what());
        }

        // Worker threads hold device.KeepAlive(): unpairing mid-takeover can't free the device under them
        playbackEventId = MprisClient::Instance().GetOnPlaybackStartedEvent().Subscribe([this](size_t, const std::string &)
        {
            TakeOver(false);
        });

        // In-ear state while not connected comes from the (RPA-verified) proximity advertisement, byte 5 bits 1 and 3
        leEventId = this->device.GetLeDataReceived().Subscribe([this](size_t, const BleAdertisingData &ad)
        {
            if (auto message = this->device.OwnProximityMessage(ad))
                bleInEarAt = ((*message)[5] & 0x0A) ? NowMs() : 0; // taken out counts at once, not 15 s later
        });

        cardEventId = device.GetAudioClient()->GatAudioCardPropertyChangedEvent().Subscribe([this](size_t, const CardInfo &info)
        {
            if (info.name != this->device.GetAudioClient()->GetNameFromMac(this->device.GetAddress()))
                return;
            // A new card comes with the profile WirePlumber stored: after another device took the AirPods that is
            // the "off" Release set, and RouteAudio switches it on. Later an "off" is the user's and stays.
            bool appeared = !cardSeen.exchange(true);
            bool a2dp = info.activeProfile.starts_with("a2dp");
            if (!a2dp && !appeared)
                return;
            if (a2dp)
            {
                std::lock_guard lock{stateLock};
                lastA2dp = info.activeProfile;
            }
            // headphones connected on their own or profile changed: keep effects and default sink in step.
            // Off the PulseAudio thread, RouteAudio waits for PulseAudio replies.
            if (this->device.ownsAudio && !busy)
                std::thread([this, keep = this->device.KeepAlive()]() { this->device.RouteAudio(); }).detach();
        });

        holdWorker = std::thread([this]() { HoldWorker(); });
    }

    AapAudioSwitchCapability::~AapAudioSwitchCapability()
    {
        {
            std::lock_guard lock{holdLock};
            holdExit = true;
        }
        holdWake.notify_one();
        if (holdWorker.joinable())
            holdWorker.join();
        MprisClient::Instance().GetOnPlaybackStartedEvent().Unsubscribe(playbackEventId);
        device.GetLeDataReceived().Unsubscribe(leEventId);
        device.GetAudioClient()->GatAudioCardPropertyChangedEvent().Unsubscribe(cardEventId);
    }

    nlohmann::json AapAudioSwitchCapability::CreateJsonBody()
    {
        std::lock_guard lock{stateLock};
        return {{"selected", mode.load()}, {"owns", device.ownsAudio.load()}, {"source", otherDevice}};
    }

    void AapAudioSwitchCapability::Reset()
    {
        {
            std::lock_guard lock{stateLock};
            connectedDevices.clear();
            otherDevice.clear();
        }
        device.ownsAudio = true;
        released = false;
        sourceType = 0;
        cardSeen = false;
        device.StopEffects();
        WakeHold();
        AapCapability::Reset();
    }

    void AapAudioSwitchCapability::WakeHold()
    {
        {
            std::lock_guard lock{holdLock};
            holdKick = true;
        }
        holdWake.notify_one();
    }

    void AapAudioSwitchCapability::HoldWorker()
    {
        std::unique_lock lock{holdLock};
        while (!holdExit)
        {
            holdKick = false;
            lock.unlock();
            UpdateHold();
            lock.lock();
            // the BLE in-ear bits have no event of their own; a second is quick enough to be in place before anything plays
            holdWake.wait_for(lock, seconds(1), [this] { return holdExit || holdKick; });
        }
        lock.unlock();
        holdBlocked = true; // gone with the device: give the output back
        UpdateHold();
    }

    void AapAudioSwitchCapability::UpdateHold()
    {
        auto pac = device.GetAudioClient();
        bool worn = InEar();
        if (!worn)
            holdBlocked = false;
        auto current = pac->GetDefaultSink().value_or("");
        bool playsHere = device.ownsAudio && (PulseAudioClient::SinkOf(current, device.SinkPart()) ||
                         (current == AudioEffects::SINK_NAME && AudioEffects::Instance().PlaysInto(device.SinkPart())));
        // while this computer has them (connected, not taken by another device), the output is the user's to pick
        bool otherHasThem = !device.ownsAudio || !device.GetConnected();

        if (holding)
        {
            // mid-takeover the AirPods are ours before they play: giving the speakers back now would let them blare,
            // or even switch the output back after RouteAudio; the takeover wakes this once it's done
            if (busy)
                return;
            bool userPicked = current != AudioEffects::HOLD_SINK && !playsHere;
            if (userPicked)
                holdBlocked = true;
            if (mode == 0 && worn && otherHasThem && !holdBlocked && !playsHere)
                return;
            if (current == AudioEffects::HOLD_SINK && pac->FindSink(heldOutput))
                pac->SetDefaultSink(heldOutput);
            AudioEffects::Instance().HoldSink(false);
            holding = false;
            Logger::Info("AutoSwitch: %s", playsHere ? "the AirPods play, silent output gone" : "the speakers play again");
            return;
        }

        // speakers: a sound card output that isn't headphones on a jack or USB
        auto isSpeakers = [&]
        {
            auto outputs = pac->GetOutputs();
            auto wired = pac->GetWiredHeadphones();
            return std::ranges::any_of(outputs, [&](const auto &o) { return o.sink == current; }) &&
                   std::ranges::none_of(wired, [&](const auto &o) { return o.sink == current; });
        };
        if (mode != 0 || !worn || !otherHasThem || holdBlocked || playsHere || current.empty() || !isSpeakers())
            return;
        AudioEffects::Instance().HoldSink(true);
        for (int i = 0; i < 20 && !pac->FindSink(AudioEffects::HOLD_SINK); i++)
            std::this_thread::sleep_for(milliseconds(100));
        if (!pac->FindSink(AudioEffects::HOLD_SINK) || !pac->SetDefaultSink(AudioEffects::HOLD_SINK))
        {
            AudioEffects::Instance().HoldSink(false);
            holdBlocked = true;
            return;
        }
        heldOutput = current;
        holding = true;
        Logger::Info("AutoSwitch: AirPods worn, %s stays silent until they play here", current.c_str());
    }

    bool AapAudioSwitchCapability::InEar()
    {
        int pods = device.podsInEar;
        if (device.GetConnected() && pods >= 0)
            return pods > 0;
        return NowMs() - bleInEarAt < 15000;
    }

    void AapAudioSwitchCapability::OnReceivedData(const std::vector<unsigned char> &data)
    {
        if (!isAvailable)
        {
            isAvailable = true;
            _onChanged.FireEvent(*this);
        }
        if (data.size() < 6 || data[0] != 0x04 || data[1] != 0x00 || data[2] != 0x04 || data[3] != 0x00)
            return;

        if (data[4] == 0x06)
            WakeHold(); // put in or taken out

        // while taking over (the A2DP switch alone takes seconds) and right after, stale "someone else streams"
        // reports are still in flight; giving the audio back on them would bounce it between the devices
        bool grace = busy || NowMs() - takeoverAt < 3000;
        switch (data[4])
        {
        case 0x09:
            if (data.size() >= 8 && data[6] == 0x06)
            {
                bool owns = data[7] == 0x01;
                Logger::Info("AutoSwitch: owns connection %d", owns);
                if (!owns && !grace)
                    Release(false);
            }
            break;
        case 0x0E:
            if (auto source = ParseAudioSource(data))
            {
                sourceType = source->second;
                Logger::Info("AutoSwitch: audio source %s type %d", source->first.c_str(), source->second);
                if (source->second != 0 && !localMac.empty() && source->first != localMac && !grace)
                    Release(true);
            }
            break;
        case 0x2E:
        {
            auto devices = ParseConnectedDevices(data);
            std::lock_guard lock{stateLock};
            connectedDevices = devices;
            break;
        }
        case 0x11:
        {
            if (data.size() < 12)
                break;
            std::string text(data.begin(), data.end());
            {
                std::lock_guard lock{stateLock};
                for (const char *kind : {"iPhone", "iPad", "Mac"})
                    if (text.find(kind) != std::string::npos)
                    {
                        otherDevice = kind;
                        break;
                    }
            }
            Logger::Info("AutoSwitch: smart routing message from %s", FormatMac(&data[6], true).c_str());
            if (text.find("SetOwnershipToFalse") != std::string::npos && !grace)
                Release(true);
            break;
        }
        }
    }

    void AapAudioSwitchCapability::Release(bool answerRequest)
    {
        if (answerRequest)
            device.SendData(OwnsConnection(false));
        device.ownsAudio = false;
        if (released.exchange(true))
            return;
        Logger::Info("AutoSwitch: another device took the audio");
        _onChanged.FireEvent(*this);

        std::thread([this, keep = device.KeepAlive()]()
        {
            // Like a Mac: whatever played here pauses, and the computer falls back to its own speakers
            // (A2DP off keeps the Bluetooth link and AAP up, so taking the audio back is instant).
            auto pac = device.GetAudioClient();
            auto card = pac->GetNameFromMac(device.GetAddress());
            auto info = pac->GetCardInfoByName(card);
            if (!info || info->activeProfile == "off")
                return;
            MprisClient::Instance().PausePlaying();
            device.StopEffects();
            pac->SetCardProfile(card, "off");
            WakeHold(); // the system falls back to the speakers: silent while they're worn
        }).detach();
    }

    void AapAudioSwitchCapability::TakeOver(bool manual)
    {
        if (busy.exchange(true))
            return;

        std::thread([this, manual, keep = device.KeepAlive()]()
        {
            auto pac = device.GetAudioClient();
            auto card = pac->GetNameFromMac(device.GetAddress());
            auto info = device.GetConnected() ? pac->GetCardInfoByName(card) : std::nullopt;
            bool streaming = info && info->activeProfile.starts_with("a2dp");

            if (!manual && (device.ownsAudio && streaming))
            {
                busy = false; // already ours
                return;
            }
            if (!manual && (mode != 0 || !InEar() || (sourceType == 1 && !device.ownsAudio)))
            {
                busy = false; // setting says no, nobody wears them, or the other device is in a call (Apple never steals calls)
                return;
            }
            Logger::Info("AutoSwitch: taking the audio over (%s)", manual ? "manual" : "playback started");

            // keep the first seconds off the laptop speakers
            auto paused = streaming ? std::vector<std::string>{} : MprisClient::Instance().PausePlaying();

            bool connecting = !device.GetConnected();
            if (connecting)
            {
                try
                {
                    device.Connect();
                }
                catch (const std::exception &e)
                {
                    Logger::Error("AutoSwitch: connect failed: %s", e.what());
                    holdBlocked = true; // they won't play here: the speakers do
                    WakeHold();
                    MprisClient::Instance().Play(paused);
                    busy = false;
                    return;
                }
            }

            // The hijack goes to the devices from the 0x2E list. The AirPods send it once the AAP handshake is through
            // (its packets go out 300 ms apart after connecting), so it also says the channel is ready: measured
            // ~2 s after the Bluetooth connection, 5 s at most here.
            for (int i = 0; i < 50; i++)
            {
                {
                    std::lock_guard lock{stateLock};
                    if (!connectedDevices.empty())
                        break;
                }
                std::this_thread::sleep_for(milliseconds(100));
            }

            takeoverAt = NowMs();
            device.SendData(OwnsConnection(true));
            std::vector<std::string> others;
            {
                std::lock_guard lock{stateLock};
                for (auto &mac : connectedDevices)
                    if (mac != localMac)
                        others.push_back(mac);
                otherDevice.clear();
            }
            Logger::Info("AutoSwitch: asking %zu other device(s) to let go", others.size());
            std::string name;
            try
            {
                name = DBusService::GetAdapterAlias(); // what the iPhone already calls this computer
            }
            catch (const std::exception &e)
            {
                Logger::Debug("AutoSwitch: no adapter name: %s", e.what());
            }
            name = BannerName(name);
            for (auto &other : others)
            {
                device.SendData(MediaInformation(other, localMac, name));
                device.SendData(ShowNearbyUI(other));
                device.SendData(HijackRequest(other));
            }
            device.ownsAudio = true;
            released = false;
            sourceType = 0;
            _onChanged.FireEvent(*this);

            info = pac->GetCardInfoByName(card);
            // just connected: PipeWire makes the card a moment later
            for (int i = 0; i < 50 && connecting && !info && device.GetConnected(); i++)
            {
                std::this_thread::sleep_for(milliseconds(100));
                info = pac->GetCardInfoByName(card);
            }
            if (info && !info->activeProfile.starts_with("a2dp"))
            {
                std::string preferred;
                {
                    std::lock_guard lock{stateLock};
                    preferred = lastA2dp; // the codec in use before another device took them
                }
                if (auto profile = Device::A2dpProfile(*info, preferred); !profile.empty())
                    pac->SetCardProfile(card, profile);
            }
            device.RouteAudio();
            WakeHold(); // they play here: the silent output goes

            // the AirPods pause the stream once right after switching (seen by LibrePods), so resume twice
            std::this_thread::sleep_for(milliseconds(500));
            MprisClient::Instance().Play(paused);
            std::this_thread::sleep_for(milliseconds(1000));
            MprisClient::Instance().Play(paused);
            takeoverAt = NowMs();
            busy = false;
        }).detach();
    }

    void AapAudioSwitchCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name))
            return;
        const auto &capability = json.at(name);
        if (capability.contains("takeover") && capability["takeover"].is_boolean() && capability["takeover"].get<bool>())
        {
            TakeOver(true);
            return;
        }
        if (capability.contains("selected") && capability["selected"].is_number_integer())
        {
            int selected = capability["selected"].get<int>();
            if (selected != 0 && selected != 1)
            {
                Logger::Error("AapAudioSwitchCapability::SetFromJson: unexpected mode %d", selected);
                return;
            }
            mode = selected;
            device.SaveSettingInt("autoSwitch", selected);
            WakeHold(); // "When last connected" plays the speakers
            _onChanged.FireEvent(*this);
        }
    }
}
