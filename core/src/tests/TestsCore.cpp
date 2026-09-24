// MyPods
// License: GPL-3.0

#include "TestsCore.h"
#include "BlockingQueue.h"
#include "client/Client.h"
#include "settings/SettingsService.h"
#include "device/capabilities/aap/AapDeviceInfoCapability.h"
#include "StringUtils.h"
#include "Logger.h"

#include <filesystem>
#include <fstream>

using namespace MagicPodsCore;

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
        auto irk = reloaded.GetSetting("AA_BB", "irk").value<std::string>();
        Test("Settings persist, no temp file left", irk == "00112233" && !std::filesystem::exists(path + ".tmp"));
#ifndef _WIN32
        auto perms = std::filesystem::status(path).permissions();
        Test("Settings file private (0600)", perms == (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
#endif
        std::filesystem::remove_all(dir);
    }

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
}

void TestsCore::Test(const char *name, bool ok)
{
    if (!ok)
        failures++;
    Logger::Info("%-50s %s", name, ok ? "OK" : "FAILED");
}
