// MyPods
// License: GPL-3.0

#include "AapAudioSwitchCapability.h"
#include "media/MprisClient.h"
#include "sdk/aap/Aes.h"
#include <cstdio>
#include <thread>

namespace MagicPodsCore
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
        irk = device.LoadSettingString("irk").value_or("");
        try
        {
            localMac = sdbus::createProxy("org.bluez", "/org/bluez/hci0")->getProperty("Address").onInterface("org.bluez.Adapter1").get<std::string>();
        }
        catch (const sdbus::Error &e)
        {
            Logger::Error("AutoSwitch: no adapter address, cannot talk to other devices: %s", e.getMessage().c_str());
        }

        // ponytail: worker threads capture `this`; devices live until they are unpaired, a shared_ptr handle would close that gap
        playbackEventId = MprisClient::Instance().GetOnPlaybackStartedEvent().Subscribe([this](size_t, const std::string &)
        {
            TakeOver(false);
        });

        // In-ear state while not connected comes from the (RPA-verified) proximity advertisement, byte 5 bits 1 and 3
        leEventId = this->device.GetLeDataReceived().Subscribe([this](size_t, const BleAdertisingData &ad)
        {
            if (irk.empty())
                irk = this->device.LoadSettingString("irk").value_or("");
            for (const auto &[company, bytes] : ad.GetManufacturerData())
            {
                if (company != this->device.GetVendorId() || bytes.size() < 27 || bytes[0] != 0x07 ||
                    ((bytes[4] << 8) | bytes[3]) != this->device.GetProductId() || irk.empty() || !Aes::VerifyRPA(ad.GetAddress(), irk))
                    continue;
                if (bytes[5] & 0x0A)
                    bleInEarAt = NowMs();
            }
        });

        cardEventId = device.GetAudioClient()->GatAudioCardPropertyChangedEvent().Subscribe([this](size_t, const CardInfo &info)
        {
            if (info.name != this->device.GetAudioClient()->GetNameFromMac(this->device.GetAddress()) || !info.activeProfile.starts_with("a2dp"))
                return;
            {
                std::lock_guard lock{stateLock};
                lastA2dp = info.activeProfile;
            }
            // headphones connected on their own or profile changed: keep effects and default sink in step.
            // Off the PulseAudio thread, RouteAudio waits for PulseAudio replies.
            if (this->device.ownsAudio && !busy)
                std::thread([this]() { this->device.RouteAudio(); }).detach();
        });
    }

    AapAudioSwitchCapability::~AapAudioSwitchCapability()
    {
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
        AudioEffects::Instance().Stop();
        AapCapability::Reset();
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

        // right after taking over, stale "someone else streams" reports are still in flight
        bool grace = NowMs() - takeoverAt < 3000;
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

        std::thread([this]()
        {
            // Like a Mac: whatever played here pauses, and the computer falls back to its own speakers
            // (A2DP off keeps the Bluetooth link and AAP up, so taking the audio back is instant).
            auto pac = device.GetAudioClient();
            auto card = pac->GetNameFromMac(device.GetAddress());
            auto info = pac->GetCardInfoByName(card);
            if (!info || info->activeProfile == "off")
                return;
            MprisClient::Instance().PausePlaying();
            AudioEffects::Instance().Stop();
            pac->SetCardProfile(card, "off");
        }).detach();
    }

    void AapAudioSwitchCapability::TakeOver(bool manual)
    {
        if (busy.exchange(true))
            return;

        std::thread([this, manual]()
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

            if (!device.GetConnected())
            {
                try
                {
                    device.Connect();
                }
                catch (const sdbus::Error &e)
                {
                    Logger::Error("AutoSwitch: connect failed: %s", e.getMessage().c_str());
                    MprisClient::Instance().Play(paused);
                    busy = false;
                    return;
                }
                // AAP handshake, notification setup and key request go out with 300 ms spacing after connecting
                std::this_thread::sleep_for(milliseconds(2500));
            }

            // the hijack goes to the devices from the 0x2E list, which arrives shortly after the handshake
            for (int i = 0; i < 20; i++)
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
            for (auto &other : others)
            {
                device.SendData(MediaInformation(other, localMac, "Linux"));
                device.SendData(ShowNearbyUI(other));
                device.SendData(HijackRequest(other));
            }
            device.ownsAudio = true;
            released = false;
            sourceType = 0;
            _onChanged.FireEvent(*this);

            info = pac->GetCardInfoByName(card);
            if (info && !info->activeProfile.starts_with("a2dp"))
            {
                std::string profile;
                {
                    std::lock_guard lock{stateLock};
                    profile = lastA2dp;
                }
                auto has = [&](const std::string &name)
                {
                    for (auto &p : info->profiles)
                        if (p.first == name)
                            return true;
                    return false;
                };
                if (profile.empty() || !has(profile))
                    profile = has("a2dp-sink-aac") ? "a2dp-sink-aac" : "a2dp-sink";
                pac->SetCardProfile(card, profile);
            }
            device.RouteAudio();

            // the AirPods pause the stream once right after switching (seen by LibrePods), so resume twice
            std::this_thread::sleep_for(milliseconds(500));
            MprisClient::Instance().Play(paused);
            std::this_thread::sleep_for(milliseconds(1000));
            MprisClient::Instance().Play(paused);
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
            _onChanged.FireEvent(*this);
        }
    }
}
