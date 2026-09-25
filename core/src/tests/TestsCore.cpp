// MyPods
// License: GPL-3.0

#include "TestsCore.h"
#include "BlockingQueue.h"
#include "client/Client.h"
#include "settings/SettingsService.h"
#include "device/capabilities/aap/AapDeviceInfoCapability.h"
#include "StringUtils.h"
#include "Logger.h"
#include "DevicesInfoFetcher.h"
#include "dbus/BatteryProvider.h"
#include "device/capabilities/aap/AapControlCapability.h"
#include "device/capabilities/aap/AapAttCapabilities.h"
#include "device/capabilities/aap/AapConversationAwarenessStateCapability.h"
#include "sdk/aap/Att.h"
#include "media/EarDetectionPause.h"
#include "sdk/aap/watchers/AapAncWatcher.h"

#include <atomic>
#include <thread>
#include <cmath>

#include <filesystem>
#include <fstream>

using namespace MagicPodsCore;

#ifndef _WIN32
namespace
{
    // records where the control channel gets stopped
    struct StopProbe : Device
    {
        std::atomic<bool> stopped{false};
        std::thread::id stoppedOn{};
        StopProbe(std::shared_ptr<DBusDeviceInfo> info, std::shared_ptr<SettingsService> settings) : Device(info, nullptr, settings)
        {
            _client = Client::CreateRFCOMM("00:00:00:00:00:00", ""); // empty UUID: fails at once, no Bluetooth
            Init();
        }
        ~StopProbe() override { Shutdown(); }
        void OnResponseDataReceived(const std::vector<unsigned char> &) override {}
        void OnClientStopped() override
        {
            if (!stopped)
                stoppedOn = std::this_thread::get_id();
            stopped = true;
        }
    };
}
#endif

TestsCore::TestsCore()
{
    // send queue: Close wakes the writer, Reopen drops what a previous session left behind
    {
        BlockingQueue<int> queue;
        queue.Put(1);
        queue.Close();
        bool closed = !queue.Take().has_value();
        queue.Reopen();
        queue.Put(2);
        Test("Queue close wakes Take, reopen drops stale items", closed && queue.Take() == 2);
    }

    // a channel that can't be opened must report it, not end the daemon (it used to call exit)
    {
        auto client = Client::CreateRFCOMM("00:00:00:00:00:00", ""); // empty UUID: fails without touching Bluetooth
        bool started = client->Start();
        client->Stop();
        Test("Client start failure returns false", !started && !client->IsStarted());
    }

    // settings: a damaged file is kept aside instead of stopping the daemon, writes survive a restart
    {
        auto dir = std::filesystem::temp_directory_path() / "mypods-selftest";
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        auto path = (dir / "config.toml").string();
        std::ofstream(path) << "this is = = not toml [";

        bool survived = true;
        try
        {
            SettingsService settings{path};
            settings.SaveSetting("AA_BB", "irk", std::string{"00112233"});
        }
        catch (const std::exception &)
        {
            survived = false;
        }
        Test("Settings survive a damaged file", survived && std::filesystem::exists(path + ".broken"));

        SettingsService reloaded{path};
        auto irk = reloaded.GetValue<std::string>("AA_BB", "irk");
        Test("Settings persist, no temp file left", irk == "00112233" && !std::filesystem::exists(path + ".tmp"));
#ifndef _WIN32
        auto perms = std::filesystem::status(path).permissions();
        Test("Settings file private (0600)", perms == (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
#endif

        // a file that can't be written (here: its directory is a file) is logged, not thrown: writes
        // come from the AAP reader and D-Bus threads, where an exception ends the daemon
        bool unwritableSurvived = true;
        try
        {
            SettingsService unwritable{path + "/config.toml"};
            unwritable.SaveSetting("AA_BB", "enc", std::string{"44"});
            unwritableSurvived = unwritable.GetValue<std::string>("AA_BB", "enc") == "44";
        }
        catch (const std::exception &)
        {
            unwritableSurvived = false;
        }
        Test("Settings write failure keeps the daemon running", unwritableSurvived);
        Test("Settings typed read of a missing or other-typed value", !reloaded.GetValue<int64_t>("AA_BB", "irk") &&
                                                                          !reloaded.GetValue<bool>("nope", "irk"));
        std::filesystem::remove_all(dir);
    }

    // BLE scan: only for AirPods; on Linux paused while they are connected (AAP has everything, the inquiry can
    // make A2DP stutter), kept for automatic switching even with the popup off; on Windows the ads are all there is
    Test("BLE scan off without AirPods", !DevicesInfoFetcher::ShouldScan(true, true, false, false, true));
    Test("BLE scan for the popup", DevicesInfoFetcher::ShouldScan(true, true, true, false, false));
    Test("BLE scan paused while connected (Linux)", !DevicesInfoFetcher::ShouldScan(true, true, true, true, true));
    Test("BLE scan for switching with the popup off", DevicesInfoFetcher::ShouldScan(false, true, true, false, true));
    Test("BLE scan off when neither needs it", !DevicesInfoFetcher::ShouldScan(false, true, true, false, false));
    Test("BLE scan while connected (Windows)", DevicesInfoFetcher::ShouldScan(false, false, true, true, false));

    // Control commands (LibrePods docs/control_commands.md)
    Test("Control packet: allow Off", AapControlCapability::Packet(0x34, 0x01) == StringUtils::HexStringToBytes("0400040009003401000000"));
    Test("Control packet: hearing aid on", AapControlCapability::Packet(0x2C, 0x01, 0x01) == StringUtils::HexStringToBytes("0400040009002C01010000"));
    Test("Listening modes need two", !AapControlCapability::IsValidListeningModes(AapControlCapability::MODE_ANC) &&
                                         AapControlCapability::IsValidListeningModes(AapControlCapability::MODE_ANC | AapControlCapability::MODE_TRANSPARENCY) &&
                                         !AapControlCapability::IsValidListeningModes(0) && !AapControlCapability::IsValidListeningModes(0x10 | 0x03));

    // Conversation Awareness levels: 1/2 lower the volume, 6/8/9 bring it back
    Test("CA level to speaking", AapConversationAwarenessStateCapability::SpeakingFromLevel(1) == true &&
                                     AapConversationAwarenessStateCapability::SpeakingFromLevel(2) == true &&
                                     AapConversationAwarenessStateCapability::SpeakingFromLevel(9) == false &&
                                     !AapConversationAwarenessStateCapability::SpeakingFromLevel(4).has_value());

    // Battery for BlueZ: the emptier bud, the case and stale readings don't count
    Test("Battery level for the system", BatteryProvider::Level({{DeviceBatteryType::Left, DeviceBatteryStatus::Connected, 80, false},
                                                                  {DeviceBatteryType::Right, DeviceBatteryStatus::Connected, 60, false},
                                                                  {DeviceBatteryType::Case, DeviceBatteryStatus::Connected, 5, false}}) == 60 &&
                                             BatteryProvider::Level({{DeviceBatteryType::Left, DeviceBatteryStatus::Cached, 40, false}}) == std::nullopt);

    // ATT: request PDUs and the transparency characteristic (LibrePods Transparency.kt layout)
    {
        Test("ATT read/write PDUs", Att::Read(Att::LOUD_SOUND_REDUCTION) == std::vector<uint8_t>{0x0A, 0x1B, 0x00} &&
                                        Att::Write(Att::LOUD_SOUND_REDUCTION, {0x01}) == std::vector<uint8_t>{0x12, 0x1B, 0x00, 0x01});
        Att::TransparencySettings settings;
        settings.enabled = true;
        settings.left.eq[3] = 42.5f;
        settings.left.amplification = 0.25f;
        settings.right.amplification = 0.75f;
        settings.ownVoice = 0.5f;
        auto bytes = settings.Encode();
        auto parsed = Att::TransparencySettings::Parse(bytes);
        Test("Transparency encode/parse", bytes.size() == 104 && parsed && parsed->enabled && parsed->left.eq[3] == 42.5f &&
                                              parsed->right.amplification == 0.75f && parsed->ownVoice == 0.5f &&
                                              bytes[0] == 0x00 && bytes[3] == 0x3F); // 1.0f little endian: 00 00 80 3F
        auto json = AapTransparencyCapability::ToJson(*parsed);
        Test("Transparency amplification and balance", std::abs(json["amplification"].get<float>() - 0.5f) < 1e-6 &&
                                                           std::abs(json["balance"].get<float>() - 0.5f) < 1e-6);
        AapTransparencyCapability::Apply(*parsed, {{"balance", 0.0}, {"tone", 0.3}});
        Test("Transparency apply keeps the rest", parsed->left.amplification == 0.5f && parsed->right.amplification == 0.5f &&
                                                      std::abs(parsed->left.tone - 0.3f) < 1e-6 && parsed->left.eq[3] == 42.5f);
        Test("Transparency too short", !Att::TransparencySettings::Parse(std::vector<uint8_t>(60)).has_value());

        // one ATT request at a time; answers name the handle read; an unanswered request is dropped after 2 s
        Att::RequestQueue queue;
        auto t0 = Att::RequestQueue::Clock::now();
        auto first = queue.Push(Att::Read(Att::LOUD_SOUND_REDUCTION), Att::LOUD_SOUND_REDUCTION, t0);
        auto second = queue.Push(Att::Read(Att::TRANSPARENCY), Att::TRANSPARENCY, t0);
        auto [answered, next] = queue.Answered(t0);
        Test("ATT queue: one request in flight", first == Att::Read(Att::LOUD_SOUND_REDUCTION) && !second &&
                                                     answered == Att::LOUD_SOUND_REDUCTION && next == Att::Read(Att::TRANSPARENCY));
        auto stuck = queue.Push(Att::Write(Att::LOUD_SOUND_REDUCTION, {0x01}), 0, t0 + std::chrono::seconds(3));
        Test("ATT queue: unanswered request given up", stuck == Att::Write(Att::LOUD_SOUND_REDUCTION, {0x01}) &&
                                                           queue.Answered(t0).first == 0);
    }

    // Ear detection: pause when a bud comes out, resume once as many are back as before
    Test("Ear pause: first reading does nothing", EarDetectionPause::Decide(-1, 1, false, 0) == 0);
    Test("Ear pause: bud out pauses", EarDetectionPause::Decide(2, 1, false, 0) == -1);
    Test("Ear pause: second bud out keeps it paused", EarDetectionPause::Decide(1, 0, true, 2) == 0);
    Test("Ear pause: one back of two waits", EarDetectionPause::Decide(0, 1, true, 2) == 0);
    Test("Ear pause: both back resumes", EarDetectionPause::Decide(1, 2, true, 2) == 1);
    Test("Ear pause: out without playback, nothing to resume", EarDetectionPause::Decide(1, 2, false, 2) == 0);

    // AirPods Pro information packet as captured by LibrePods (docs/AAP Definitions.md)
    {
        auto packet = StringUtils::HexStringToBytes(
            "040004001d0002d5000400416972506f64732050726f004133303438004170706c6520496e632e0051584e524848595850360036312e"
            "313836383034303030323030303030302e323731330036312e313836383034303030323030303030302e3237313300312e302e3000");
        auto info = AapDeviceInfoCapability::Parse(packet);
        Test("AAP device info parsed", info && info->name == "AirPods Pro" && info->model == "A3048" &&
                                           info->manufacturer == "Apple Inc." && info->serial == "QXNRHHYXP6" &&
                                           info->firmware == "61.1868040002000000.2713");
        packet.resize(40); // cut off inside the strings
        Test("AAP device info cut off is ignored", !AapDeviceInfoCapability::Parse(packet));

        Test("AAP rename packet", AapDeviceInfoCapability::RenamePacket("Pods") ==
                                      std::vector<unsigned char>{0x04, 0x00, 0x04, 0x00, 0x1A, 0x00, 0x01, 0x04, 0x00, 'P', 'o', 'd', 's'});
        Test("AAP rename rejects bad names", AapDeviceInfoCapability::RenamePacket("").empty() &&
                                                 AapDeviceInfoCapability::RenamePacket(std::string(33, 'x')).empty() &&
                                                 AapDeviceInfoCapability::RenamePacket("a\nb").empty());
    }

    // setters range-check before narrowing to a byte: 257 must not become 1 (ANC Off)
    Test("Selected byte in range", Capability::SelectedByte({{"selected", 0}}) == 0 && Capability::SelectedByte({{"selected", 255}}) == 255);
    Test("Selected byte rejects out of range and non-integers",
         !Capability::SelectedByte({{"selected", 257}}) && !Capability::SelectedByte({{"selected", -1}}) &&
             !Capability::SelectedByte({{"selected", 4294967297LL}}) && !Capability::SelectedByte({{"selected", "1"}}) &&
             !Capability::SelectedByte(nlohmann::json::object()));

    // an ANC byte outside 1..4 is ignored, not shown as Off
    {
        AapAncWatcher watcher;
        bool fired = false;
        watcher.GetEvent().Subscribe([&](size_t, AapAncMode) { fired = true; });
        watcher.ProcessResponse({0x04, 0x00, 0x04, 0x00, 0x09, 0x00, 0x0d, 0x07, 0x00, 0x00, 0x00});
        Test("ANC unknown mode ignored", !fired);
    }

#ifndef _WIN32
    // BlueZ reports the disconnect on the D-Bus thread; stopping there waited for a Start() still connecting
    try
    {
        std::map<std::string, std::map<std::string, sdbus::Variant>> interfaces{{"org.bluez.Device1", {
            {"Address", sdbus::Variant{std::string("00:00:00:00:00:01")}},
            {"Connected", sdbus::Variant{true}},
        }}};
        auto info = std::make_shared<DBusDeviceInfo>(sdbus::ObjectPath{"/org/bluez/hci0/dev_00_00_00_00_00_01"}, interfaces);
        auto path = std::filesystem::temp_directory_path() / "mypods-selftest-stop.toml";
        StopProbe probe{info, std::make_shared<SettingsService>(path.string())};
        info->GetConnectionStatus().SetValue(false); // what PropertiesChanged does
        for (int i = 0; i < 200 && !probe.stopped; i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        Test("Disconnect stops the channel off the D-Bus thread", probe.stopped && probe.stoppedOn != std::this_thread::get_id());
        std::filesystem::remove(path);
    }
    catch (const sdbus::Error &e)
    {
        Logger::Info("Disconnect stop check skipped, no system bus: %s", e.getMessage().c_str());
    }
#endif
}

void TestsCore::Test(const char *name, bool ok)
{
    if (!ok)
        failures++;
    Logger::Info("%-50s %s", name, ok ? "OK" : "FAILED");
}
