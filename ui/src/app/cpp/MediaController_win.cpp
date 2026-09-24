// MyPods
// License: GPL-3.0

// Windows side of MediaController: the media sessions apps register with the system media controls,
// and the default output's endpoint volume.

// C++/WinRT before Qt: Qt's keyword macros must not see the projection headers
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

#include "MediaController.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QUrl>

#include <future>

using namespace winrt::Windows::Media::Control;

namespace {

struct Snapshot {
    QString service;
    QString title;
    QString artist;
    QByteArray art;
    bool playing = false;
    bool canGoNext = false;
    bool canGoPrevious = false;
};

// WinRT's blocking get() is not allowed on the GUI (STA) thread, so the query runs on a worker
template <typename F>
auto offGuiThread(F &&work)
{
    return std::async(std::launch::async, [work = std::forward<F>(work)] {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        return work();
    }).get();
}

// ponytail: blocking like the MPRIS side, polled once a second while the popup is open
Snapshot query(const QString &knownArtKey)
{
    Snapshot best;
    try {
        auto manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        // A playing session wins, then a paused one, then whatever else is there
        int bestRank = -1;
        GlobalSystemMediaTransportControlsSession bestSession{nullptr};
        for (const auto &session : manager.GetSessions()) {
            const auto status = session.GetPlaybackInfo().PlaybackStatus();
            const int rank = status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing ? 2
                           : status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused ? 1 : 0;
            if (rank > bestRank) {
                bestRank = rank;
                bestSession = session;
            }
        }
        if (!bestSession)
            return best;

        const auto props = bestSession.TryGetMediaPropertiesAsync().get();
        const auto controls = bestSession.GetPlaybackInfo().Controls();
        best.service = QString::fromStdWString(std::wstring(bestSession.SourceAppUserModelId()));
        best.title = QString::fromStdWString(std::wstring(props.Title()));
        best.artist = QString::fromStdWString(std::wstring(props.Artist()));
        best.playing = bestRank == 2;
        best.canGoNext = controls.IsNextEnabled();
        best.canGoPrevious = controls.IsPreviousEnabled();

        // the cover only when the track changed, reading it costs a stream round trip
        if (props.Thumbnail() && best.title + QLatin1Char('\n') + best.artist != knownArtKey) {
            auto stream = props.Thumbnail().OpenReadAsync().get();
            winrt::Windows::Storage::Streams::Buffer buffer(static_cast<uint32_t>(stream.Size()));
            auto read = stream.ReadAsync(buffer, buffer.Capacity(), winrt::Windows::Storage::Streams::InputStreamOptions::None).get();
            best.art = QByteArray(reinterpret_cast<const char *>(read.data()), static_cast<qsizetype>(read.Length()));
        }
    } catch (const winrt::hresult_error &) {
        // no media service (e.g. Windows N without the Media Feature Pack): nothing playing
    }
    return best;
}

winrt::com_ptr<IAudioEndpointVolume> defaultEndpointVolume()
{
    winrt::com_ptr<IMMDeviceEnumerator> enumerator;
    winrt::com_ptr<IMMDevice> device;
    winrt::com_ptr<IAudioEndpointVolume> volume;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(enumerator.put()))) ||
        FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, device.put())) ||
        FAILED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, volume.put_void())))
        return nullptr;
    return volume;
}

}

void MediaController::refresh()
{
    const QString previousKey = m_artKey;
    const Snapshot snapshot = offGuiThread([previousKey] { return query(previousKey); });

    QVariantMap player;
    // A player with nothing loaded has nothing to show
    if (!snapshot.service.isEmpty() && !snapshot.title.isEmpty()) {
        const QString key = snapshot.title + QLatin1Char('\n') + snapshot.artist;
        QString artUrl = m_player.value(QStringLiteral("artUrl")).toString();
        if (key != m_artKey) {
            artUrl.clear();
            if (!snapshot.art.isEmpty()) {
                // a new file name per track, the Image would keep showing a cached one otherwise
                const QString path = QDir::temp().filePath(QStringLiteral("mypods-cover-%1")
                    .arg(QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5).toHex())));
                QFile::remove(QUrl(m_player.value(QStringLiteral("artUrl")).toString()).toLocalFile());
                QFile file(path);
                if (file.open(QIODevice::WriteOnly) && file.write(snapshot.art) == snapshot.art.size())
                    artUrl = QUrl::fromLocalFile(path).toString();
            }
            m_artKey = key;
        }
        player = {
            {QStringLiteral("title"), snapshot.title},
            {QStringLiteral("artist"), snapshot.artist},
            {QStringLiteral("artUrl"), artUrl},
            {QStringLiteral("playing"), snapshot.playing},
            {QStringLiteral("canGoNext"), snapshot.canGoNext},
            {QStringLiteral("canGoPrevious"), snapshot.canGoPrevious},
        };
    } else {
        m_artKey.clear();
    }
    m_service = player.isEmpty() ? QString() : snapshot.service;
    if (player != m_player) {
        m_player = player;
        emit playerChanged();
    }

    int volume = -1;
    bool muted = false;
    if (auto endpoint = defaultEndpointVolume()) {
        float level = 0;
        BOOL mute = FALSE;
        if (SUCCEEDED(endpoint->GetMasterVolumeLevelScalar(&level)))
            volume = qRound(level * 100);
        if (SUCCEEDED(endpoint->GetMute(&mute)))
            muted = mute;
    }
    if (volume != m_volume || muted != m_muted) {
        m_volume = volume;
        m_muted = muted;
        emit volumeChanged();
    }
}

void MediaController::callPlayer(const QString &method)
{
    if (m_service.isEmpty())
        return;
    const std::wstring service = m_service.toStdWString();
    const std::wstring action = method.toStdWString();
    // Fire and forget, like the async D-Bus call: started here, the session applies it on its own
    std::thread([service, action] {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        try {
            auto manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
            for (const auto &session : manager.GetSessions()) {
                if (session.SourceAppUserModelId() != service)
                    continue;
                if (action == L"PlayPause")
                    session.TryTogglePlayPauseAsync().get();
                else if (action == L"Next")
                    session.TrySkipNextAsync().get();
                else if (action == L"Previous")
                    session.TrySkipPreviousAsync().get();
                break;
            }
        } catch (const winrt::hresult_error &) {
        }
    }).detach();
}

void MediaController::playPause()
{
    if (m_service.isEmpty())
        return;
    callPlayer(QStringLiteral("PlayPause"));
    // Flip at once; the next poll corrects it if the player refused
    m_player[QStringLiteral("playing")] = !m_player.value(QStringLiteral("playing")).toBool();
    emit playerChanged();
}

void MediaController::next()
{
    callPlayer(QStringLiteral("Next"));
}

void MediaController::previous()
{
    callPlayer(QStringLiteral("Previous"));
}

void MediaController::setVolume(int percent)
{
    percent = qBound(0, percent, 100);
    if (percent == m_volume && !m_muted)
        return;
    m_volume = percent;
    // Like the Mac's slider: moving it also unmutes
    setMuted(false);
    if (auto endpoint = defaultEndpointVolume())
        endpoint->SetMasterVolumeLevelScalar(percent / 100.0f, nullptr);
}

// The endpoint keeps its volume while muted, so unmuting brings back the old level
void MediaController::setMuted(bool muted)
{
    m_muted = muted;
    emit volumeChanged();
    if (auto endpoint = defaultEndpointVolume())
        endpoint->SetMute(muted, nullptr);
}
