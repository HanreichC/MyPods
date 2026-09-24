// MyPods
// License: GPL-3.0

// Windows side of MprisClient: the media sessions every app registers with the system media
// controls (browsers, Spotify, the Media Player app, ...), identified by their app user model id.

#include "MprisClient.h"
#include "Logger.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>

#include <algorithm>

namespace MagicPodsCore
{
    using namespace winrt::Windows::Media::Control;

    static GlobalSystemMediaTransportControlsSessionManager Manager()
    {
        // Fetched per call: the manager is cheap to get and has no state worth keeping
        return GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
    }

    MprisClient &MprisClient::Instance()
    {
        static MprisClient instance;
        return instance;
    }

    MprisClient::MprisClient() = default;

    std::vector<std::string> MprisClient::PausePlaying()
    {
        std::vector<std::string> paused;
        std::lock_guard lock{_callsLock};
        try
        {
            for (const auto &session : Manager().GetSessions())
            {
                if (session.GetPlaybackInfo().PlaybackStatus() != GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing)
                    continue;
                auto player = winrt::to_string(session.SourceAppUserModelId());
                if (session.TryPauseAsync().get())
                {
                    paused.push_back(player);
                    Logger::Info("Media: paused %s", player.c_str());
                }
            }
        }
        catch (const winrt::hresult_error &e)
        {
            Logger::Error("Media: %s", winrt::to_string(e.message()).c_str());
        }
        return paused;
    }

    void MprisClient::Play(const std::vector<std::string> &players)
    {
        if (players.empty())
            return;
        std::lock_guard lock{_callsLock};
        try
        {
            for (const auto &session : Manager().GetSessions())
            {
                auto player = winrt::to_string(session.SourceAppUserModelId());
                if (std::find(players.begin(), players.end(), player) == players.end())
                    continue;
                session.TryPlayAsync().get();
                Logger::Info("Media: resumed %s", player.c_str());
            }
        }
        catch (const winrt::hresult_error &e)
        {
            Logger::Error("Media: %s", winrt::to_string(e.message()).c_str());
        }
    }
}
