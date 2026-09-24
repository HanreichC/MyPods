// MyPods
// License: GPL-3.0

#include "TestsCore.h"
#include "BlockingQueue.h"
#include "client/Client.h"
#include "settings/SettingsService.h"
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
}

void TestsCore::Test(const char *name, bool ok)
{
    if (!ok)
        failures++;
    Logger::Info("%-50s %s", name, ok ? "OK" : "FAILED");
}
