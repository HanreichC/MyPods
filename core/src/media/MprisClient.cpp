// MyPods
// License: GPL-3.0

#include "MprisClient.h"
#include "Logger.h"

namespace MagicPodsCore
{
    static constexpr const char *MPRIS_PATH = "/org/mpris/MediaPlayer2";
    static constexpr const char *MPRIS_PLAYER = "org.mpris.MediaPlayer2.Player";

    MprisClient &MprisClient::Instance()
    {
        static MprisClient instance;
        return instance;
    }

    MprisClient::MprisClient()
    {
        try
        {
            _calls = sdbus::createSessionBusConnection();
            _signals = sdbus::createSessionBusConnection();
            _match = _signals->addMatch(
                "type='signal',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged',path='/org/mpris/MediaPlayer2'",
                [this](sdbus::Message &msg)
                {
                    std::string iface;
                    std::map<std::string, sdbus::Variant> changed;
                    msg >> iface >> changed;
                    if (iface != MPRIS_PLAYER || !changed.contains("PlaybackStatus"))
                        return;

                    auto status = changed.at("PlaybackStatus").get<std::string>();
                    auto sender = msg.getSender();
                    bool started = status == "Playing" && _lastStatus[sender] != "Playing";
                    _lastStatus[sender] = status;
                    if (started)
                    {
                        Logger::Info("MPRIS: %s started playing", sender.c_str());
                        _onPlaybackStarted.FireEvent(sender);
                    }
                });
            _signals->enterEventLoopAsync();
        }
        catch (const sdbus::Error &e)
        {
            // No session bus (e.g. started outside a desktop session): features depending on it stay inactive.
            Logger::Error("MPRIS unavailable: %s", e.getMessage().c_str());
            _calls.reset();
        }
    }

    std::vector<std::string> MprisClient::Players()
    {
        std::vector<std::string> names, players;
        auto dbus = sdbus::createProxy(*_calls, "org.freedesktop.DBus", "/org/freedesktop/DBus");
        dbus->callMethod("ListNames").onInterface("org.freedesktop.DBus").storeResultsTo(names);
        for (auto &name : names)
            if (name.starts_with("org.mpris.MediaPlayer2."))
                players.push_back(name);
        return players;
    }

    std::string MprisClient::Status(const std::string &player)
    {
        auto proxy = sdbus::createProxy(*_calls, player, MPRIS_PATH);
        return proxy->getProperty("PlaybackStatus").onInterface(MPRIS_PLAYER).get<std::string>();
    }

    void MprisClient::Call(const std::string &player, const std::string &method)
    {
        auto proxy = sdbus::createProxy(*_calls, player, MPRIS_PATH);
        proxy->callMethod(method).onInterface(MPRIS_PLAYER);
    }

    std::vector<std::string> MprisClient::PausePlaying()
    {
        std::vector<std::string> paused;
        std::lock_guard lock{_callsLock};
        if (!_calls)
            return paused;
        try
        {
            for (auto &player : Players())
            {
                try
                {
                    if (Status(player) == "Playing")
                    {
                        Call(player, "Pause");
                        paused.push_back(player);
                        Logger::Info("MPRIS: paused %s", player.c_str());
                    }
                }
                catch (const sdbus::Error &e)
                {
                    Logger::Debug("MPRIS: %s: %s", player.c_str(), e.getMessage().c_str());
                }
            }
        }
        catch (const sdbus::Error &e)
        {
            Logger::Error("MPRIS: %s", e.getMessage().c_str());
        }
        return paused;
    }

    void MprisClient::Play(const std::vector<std::string> &players)
    {
        std::lock_guard lock{_callsLock};
        if (!_calls)
            return;
        for (auto &player : players)
        {
            try
            {
                Call(player, "Play");
                Logger::Info("MPRIS: resumed %s", player.c_str());
            }
            catch (const sdbus::Error &e)
            {
                Logger::Debug("MPRIS: %s: %s", player.c_str(), e.getMessage().c_str());
            }
        }
    }
}
