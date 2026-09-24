// MyPods
// License: GPL-3.0

#pragma once

#include "Event.h"
#ifndef _WIN32
#include <sdbus-c++/sdbus-c++.h>
#endif
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace MagicPodsCore
{
    // Desktop media players over MPRIS (session bus). Used for ear-detection pause/resume and
    // as the "this computer started playing" trigger for taking the AirPods over, like a Mac does.
    // Windows: the system media sessions (GlobalSystemMediaTransportControls, MprisClient_win.cpp),
    // player names are then app user model ids.
    class MprisClient
    {
    public:
        static MprisClient &Instance();

        // Pauses every playing player and returns their bus names, so exactly those can be resumed.
        std::vector<std::string> PausePlaying();
        void Play(const std::vector<std::string> &players);

        // Fired with the player's bus name when it switches to "Playing". Runs on the D-Bus thread.
        // ponytail: never fires on Windows, its only listener is automatic switching, which needs AAP.
        Event<std::string> &GetOnPlaybackStartedEvent() { return _onPlaybackStarted; }

    private:
        MprisClient();
#ifndef _WIN32
        std::vector<std::string> Players();
        std::string Status(const std::string &player);
        void Call(const std::string &player, const std::string &method);

        // Calls and signals on separate connections: sd-bus connections are not thread-safe,
        // and the signal connection is owned by its event-loop thread.
        std::unique_ptr<sdbus::IConnection> _calls;
        std::unique_ptr<sdbus::IConnection> _signals;
        sdbus::Slot _match;
        std::map<std::string, std::string> _lastStatus;
#endif
        std::mutex _callsLock;
        Event<std::string> _onPlaybackStarted{};
    };
}
