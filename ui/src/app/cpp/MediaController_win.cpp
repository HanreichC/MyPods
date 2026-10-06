// MyPods
// License: GPL-3.0

// Windows side of MediaController: the media sessions apps register with the system media controls,
// the default output's endpoint volume, and a paired iPhone's now playing (Apple Media Service).

// C++/WinRT before Qt: Qt's keyword macros must not see the projection headers
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

#include "Ams.h"
#include "MediaController.h"

#include <QCoreApplication>
#include <QMetaObject>


#include <algorithm>
#include <chrono>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <vector>

using namespace winrt::Windows::Media::Control;
namespace bt = winrt::Windows::Devices::Bluetooth;
namespace gatt = winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
using winrt::Windows::Storage::Streams::IBuffer;

namespace {

struct Snapshot {
    QString service;
    QString source; // the iPhone's name when the player is on it
    QString title;
    QString artist;
    QByteArray art;
    bool playing = false;
    bool canLike = false;
    bool canGoNext = false;
    bool canGoPrevious = false;
    int volume = -1; // the iPhone's, 0-100
};

using namespace Ams;

// The Apple Media Service's UUIDs (Ams.h)
const winrt::guid AmsService{0x89D3502B, 0x0F36, 0x433A, {0x8E, 0xF4, 0xC5, 0x02, 0xAD, 0x55, 0xF8, 0xDC}};
const winrt::guid AmsRemoteCommand{0x9B3C81D8, 0x57B1, 0x4A8A, {0xB8, 0xDF, 0x0E, 0x56, 0xF7, 0xCA, 0x51, 0xC2}};
const winrt::guid AmsEntityUpdate{0x2F7CABCE, 0x808D, 0x411F, {0x9A, 0x0C, 0xBB, 0x92, 0xBA, 0x96, 0xC1, 0x02}};
const winrt::guid AmsEntityAttribute{0xC6B2F38C, 0x23AB, 0x46D8, {0xA6, 0xAB, 0xA3, 0xA8, 0x70, 0xBB, 0xD5, 0xD7}};

IBuffer bytes(std::initializer_list<uint8_t> values)
{
    winrt::Windows::Storage::Streams::DataWriter writer;
    for (uint8_t value : values)
        writer.WriteByte(value);
    return writer.DetachBuffer();
}

QString utf8(const IBuffer &buffer, uint32_t offset = 0)
{
    return buffer.Length() <= offset ? QString()
        : QString::fromUtf8(reinterpret_cast<const char *>(buffer.data() + offset), buffer.Length() - offset);
}

// The paired iPhone. The iPhone pushes every change, so reading the state costs nothing; all
// Bluetooth work runs on worker threads, WinRT's blocking get() is not allowed on the GUI thread.
class AppleMedia
{
public:
    static AppleMedia &instance()
    {
        // never destroyed: Bluetooth callbacks may still arrive while the process exits
        static auto *media = new AppleMedia;
        return *media;
    }

    // Called (on any thread) when the list of iPhones or their connection changes; set before starting
    void onIphonesChanged(std::function<void()> changed) { m_changed = std::move(changed); }
    // Called (on any thread, maybe under m_lock) when the track or whether it plays changes; set before starting
    void onNowPlayingChanged(std::function<void()> changed) { m_nowPlayingChanged = std::move(changed); }

    // Looks for iPhones among the paired devices and follows the first. Again at most every 30 s while
    // there is none; always, for a list that is new
    void ensureStarted(bool always = false)
    {
        const auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard lock{m_lock};
            if (m_searching || (!always && (m_device || (m_searched && now - m_lastSearch < std::chrono::seconds(30)))))
                return;
            m_searching = m_searched = true;
            m_lastSearch = now;
        }
        onWorker([this] {
            find();
            std::lock_guard lock{m_lock};
            m_searching = false;
        });
    }

    QVariantList iphones()
    {
        std::vector<std::pair<QString, bool>> phones;
        {
            std::lock_guard lock{m_lock};
            for (const auto &phone : m_phones) {
                try {
                    phones.emplace_back(QString::fromStdWString(std::wstring(phone.Name())),
                                        phone.ConnectionStatus() == bt::BluetoothConnectionStatus::Connected);
                } catch (const winrt::hresult_error &) {
                }
            }
        }
        std::sort(phones.begin(), phones.end(), [](const auto &a, const auto &b) { return a.first.localeAwareCompare(b.first) < 0; });
        QVariantList list;
        for (const auto &[name, connected] : phones)
            list.append(QVariantMap{{QStringLiteral("name"), name}, {QStringLiteral("connected"), connected}});
        return list;
    }

    Snapshot snapshot()
    {
        std::lock_guard lock{m_lock};
        Snapshot snapshot;
        if (m_title.isEmpty())
            return snapshot;
        snapshot.service = IphoneService;
        snapshot.source = m_name;
        snapshot.title = m_title;
        snapshot.artist = m_artist;
        snapshot.playing = m_playing;
        snapshot.volume = m_volume;
        snapshot.canGoNext = supports(NextTrack);
        snapshot.canGoPrevious = supports(PreviousTrack);
        // only when the player lists it: the star is no use to one that ignores it
        snapshot.canLike = std::find(m_commands.begin(), m_commands.end(), LikeTrack) != m_commands.end();
        return snapshot;
    }

    // `done` (on the GUI thread) learns whether the iPhone took it. A command it can't do right now comes back as
    // an ATT error (AMS: 0xA0 invalid state, 0xA1 invalid command, 0xA2 absent attribute), not as an exception.
    void send(Command command, std::function<void(bool)> done = {})
    {
        onWorker([this, command, done] {
            gatt::GattCharacteristic remote{nullptr};
            {
                std::lock_guard lock{m_lock};
                remote = m_remote;
            }
            bool ok = false;
            try {
                if (remote) {
                    const auto result = remote.WriteValueWithResultAsync(bytes({command}), gatt::GattWriteOption::WriteWithResponse).get();
                    ok = result.Status() == gatt::GattCommunicationStatus::Success;
                    if (!ok) {
                        const auto error = result.ProtocolError();
                        qWarning("iPhone: command %d refused (status %d, ATT error 0x%02x)", int(command), int(result.Status()),
                                 error ? int(error.Value()) : 0);
                    }
                }
            } catch (const winrt::hresult_error &) {
                // out of range: the next poll shows the player gone
            }
            if (done)
                QMetaObject::invokeMethod(QCoreApplication::instance(), [done, ok] { done(ok); }, Qt::QueuedConnection);
        });
    }

    // Louder (> 0) or quieter (< 0) by that many iOS steps. One worker sends them one after another,
    // so dragging the slider back and forth can't overtake itself; opposite steps not sent yet cancel out.
    void stepVolume(int steps)
    {
        std::lock_guard lock{m_lock};
        m_pendingSteps += steps;
        m_steppedAt = std::chrono::steady_clock::now();
        if (m_stepping)
            return;
        m_stepping = true;
        onWorker([this] {
            for (;;) {
                gatt::GattCharacteristic remote{nullptr};
                Command command;
                {
                    std::lock_guard lock{m_lock};
                    if (m_pendingSteps == 0 || !m_remote) {
                        m_pendingSteps = 0;
                        m_stepping = false;
                        m_steppedAt = std::chrono::steady_clock::now();
                        return;
                    }
                    command = m_pendingSteps > 0 ? VolumeUp : VolumeDown;
                    m_pendingSteps += m_pendingSteps > 0 ? -1 : 1;
                    remote = m_remote;
                }
                try {
                    remote.WriteValueAsync(bytes({command}), gatt::GattWriteOption::WriteWithResponse).get();
                } catch (const winrt::hresult_error &) {
                }
            }
        });
    }

    // While steps are on their way, and a moment after, the iPhone still reports the old volume
    bool volumeSettling()
    {
        std::lock_guard lock{m_lock};
        return m_stepping || std::chrono::steady_clock::now() - m_steppedAt < std::chrono::milliseconds(700);
    }

private:
    template <typename F>
    static void onWorker(F &&work)
    {
        std::thread([work = std::forward<F>(work)] {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            work();
        }).detach();
    }

    void find()
    {
        try {
            const auto paired = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
                bt::BluetoothLEDevice::GetDeviceSelectorFromPairingState(true)).get();
            std::vector<bt::BluetoothLEDevice> phones;
            for (const auto &info : paired) {
                auto device = bt::BluetoothLEDevice::FromIdAsync(info.Id()).get();
                // from the cache, so paired mice and keyboards aren't woken up for this
                if (device && device.GetGattServicesForUuidAsync(AmsService, bt::BluetoothCacheMode::Cached).get().Services().Size() > 0)
                    phones.push_back(device);
            }
            bool follow = false;
            {
                std::lock_guard lock{m_lock};
                m_phoneStatus.clear();
                for (const auto &phone : phones) // for the list's "connected"
                    m_phoneStatus.push_back(phone.ConnectionStatusChanged(winrt::auto_revoke, [this](const auto &, const auto &) { changed(); }));
                m_phones = phones;
                follow = !m_device;
            }
            changed();
            if (follow && !phones.empty()) {
                const bt::BluetoothLEDevice device = phones.front();
                auto session = gatt::GattSession::FromDeviceIdAsync(device.BluetoothDeviceId()).get();
                session.MaintainConnection(true); // Windows reconnects whenever the iPhone comes back in range
                {
                    std::lock_guard lock{m_lock};
                    m_device = device;
                    m_session = session;
                    m_name = QString::fromStdWString(std::wstring(device.Name()));
                }
                // the iPhone forgets the registrations with every disconnect
                device.ConnectionStatusChanged([this](const bt::BluetoothLEDevice &sender, const auto &) {
                    if (sender.ConnectionStatus() == bt::BluetoothConnectionStatus::Connected)
                        onWorker([this] { subscribe(); });
                    else
                        clear();
                });
                subscribe();
            }
        } catch (const winrt::hresult_error &) {
            // Bluetooth off: the next search tries again
        }
    }

    void subscribe()
    {
        std::lock_guard subscribing{m_subscribeLock};
        if (m_subscribed)
            return; // connecting for the lookup below fires Connected too
        try {
            const auto services = m_device.GetGattServicesForUuidAsync(AmsService, bt::BluetoothCacheMode::Uncached).get();
            if (services.Status() != gatt::GattCommunicationStatus::Success || services.Services().Size() == 0)
                return;
            const auto found = services.Services().GetAt(0).GetCharacteristicsAsync(bt::BluetoothCacheMode::Uncached).get();
            if (found.Status() != gatt::GattCommunicationStatus::Success)
                return;
            gatt::GattCharacteristic remote{nullptr}, update{nullptr}, attribute{nullptr};
            for (const auto &characteristic : found.Characteristics()) {
                if (characteristic.Uuid() == AmsRemoteCommand)
                    remote = characteristic;
                else if (characteristic.Uuid() == AmsEntityUpdate)
                    update = characteristic;
                else if (characteristic.Uuid() == AmsEntityAttribute)
                    attribute = characteristic;
            }
            if (!remote || !update || !attribute)
                return;

            m_remoteChanged = remote.ValueChanged(winrt::auto_revoke, [this](const auto &, const gatt::GattValueChangedEventArgs &args) {
                // the commands the player supports right now, one byte each
                const IBuffer value = args.CharacteristicValue();
                std::lock_guard lock{m_lock};
                m_commands.assign(value.data(), value.data() + value.Length());
            });
            m_updateChanged = update.ValueChanged(winrt::auto_revoke, [this](const auto &, const gatt::GattValueChangedEventArgs &args) {
                onEntityUpdate(args.CharacteristicValue());
            });
            {
                std::lock_guard lock{m_lock};
                m_remote = remote;
                m_update = update; // its ValueChanged ends with the last reference to it
                m_attribute = attribute;
            }
            const auto notify = gatt::GattClientCharacteristicConfigurationDescriptorValue::Notify;
            remote.WriteClientCharacteristicConfigurationDescriptorAsync(notify).get();
            update.WriteClientCharacteristicConfigurationDescriptorAsync(notify).get();
            // the iPhone then sends each of these on every change, but only then: what is there already has to be read
            update.WriteValueAsync(bytes({Track, TrackArtist, TrackTitle}), gatt::GattWriteOption::WriteWithResponse).get();
            update.WriteValueAsync(bytes({Player, PlayerPlaybackInfo, PlayerVolume}), gatt::GattWriteOption::WriteWithResponse).get();
            for (auto [entity, attribute] : {std::pair{Track, TrackArtist}, {Track, TrackTitle}, {Player, PlayerPlaybackInfo}, {Player, PlayerVolume}}) {
                if (auto text = read(entity, attribute)) {
                    std::lock_guard lock{m_lock};
                    apply(entity, attribute, *text);
                }
            }
            m_subscribed = true;
        } catch (const winrt::hresult_error &) {
            // out of range: the next Connected event subscribes again
        }
    }

    // Entity id, attribute id, flags, UTF-8 value
    void onEntityUpdate(const IBuffer &value)
    {
        if (value.Length() < 3)
            return;
        const uint8_t entity = value.data()[0];
        const uint8_t attribute = value.data()[1];
        const QString text = utf8(value, 3);
        {
            std::lock_guard lock{m_lock};
            apply(entity, attribute, text);
        }
        // a notification carries what fits into one packet; the whole title is read separately
        if ((value.data()[2] & Truncated) && entity == Track) {
            onWorker([this, attribute, text] {
                const auto whole = read(Track, attribute);
                std::lock_guard lock{m_lock};
                QString &field = attribute == TrackTitle ? m_title : m_artist;
                if (whole && field == text) // not if the next track came in meanwhile
                    field = *whole;
            });
        }
    }

    // Under m_lock
    void apply(uint8_t entity, uint8_t attribute, const QString &text)
    {
        if (entity == Track && attribute == TrackTitle)
            m_title = text;
        else if (entity == Track && attribute == TrackArtist)
            m_artist = text;
        else if (entity == Player && attribute == PlayerPlaybackInfo)
            m_playing = Ams::isPlaying(text);
        else if (entity == Player && attribute == PlayerVolume)
            m_volume = Ams::volumePercent(text);
        if (entity != Player || attribute != PlayerVolume)
            nowPlayingChanged();
    }

    void nowPlayingChanged()
    {
        if (m_nowPlayingChanged)
            m_nowPlayingChanged();
    }

    // The whole value of one attribute, through the Entity Attribute characteristic
    std::optional<QString> read(uint8_t entity, uint8_t attribute)
    {
        std::lock_guard reading{m_attributeLock}; // write and read are one exchange
        try {
            gatt::GattCharacteristic characteristic{nullptr};
            {
                std::lock_guard lock{m_lock};
                characteristic = m_attribute;
            }
            if (!characteristic || characteristic.WriteValueAsync(bytes({entity, attribute}), gatt::GattWriteOption::WriteWithResponse).get()
                    != gatt::GattCommunicationStatus::Success)
                return std::nullopt;
            const auto result = characteristic.ReadValueAsync(bt::BluetoothCacheMode::Uncached).get();
            if (result.Status() == gatt::GattCommunicationStatus::Success)
                return utf8(result.Value());
        } catch (const winrt::hresult_error &) {
        }
        return std::nullopt;
    }

    void clear()
    {
        {
            std::lock_guard subscribing{m_subscribeLock};
            m_subscribed = false;
        }
        std::lock_guard lock{m_lock};
        m_title.clear();
        m_artist.clear();
        m_playing = false;
        m_volume = -1;
        m_commands.clear();
        nowPlayingChanged();
    }

    void changed()
    {
        if (m_changed)
            m_changed();
    }

    bool supports(Command command) const
    {
        // before the first list arrives, everything is offered
        return m_commands.empty() || std::find(m_commands.begin(), m_commands.end(), command) != m_commands.end();
    }

    std::mutex m_lock; // everything below, apart from the revokers
    std::mutex m_subscribeLock;
    std::mutex m_attributeLock;
    bool m_searching = false;
    bool m_searched = false;
    std::chrono::steady_clock::time_point m_lastSearch;
    std::function<void()> m_changed; // set before any thread starts
    std::function<void()> m_nowPlayingChanged; // likewise
    std::vector<bt::BluetoothLEDevice> m_phones; // every paired iPhone, for the list
    std::vector<bt::BluetoothLEDevice::ConnectionStatusChanged_revoker> m_phoneStatus;
    bt::BluetoothLEDevice m_device{nullptr}; // the one followed
    gatt::GattSession m_session{nullptr};
    bool m_subscribed = false; // under m_subscribeLock
    gatt::GattCharacteristic m_remote{nullptr};
    gatt::GattCharacteristic m_update{nullptr};
    gatt::GattCharacteristic m_attribute{nullptr};
    gatt::GattCharacteristic::ValueChanged_revoker m_remoteChanged;
    gatt::GattCharacteristic::ValueChanged_revoker m_updateChanged;
    QString m_name;
    QString m_title;
    QString m_artist;
    bool m_playing = false;
    int m_volume = -1;
    std::vector<uint8_t> m_commands;
    int m_pendingSteps = 0;
    bool m_stepping = false;
    std::chrono::steady_clock::time_point m_steppedAt;
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
Snapshot query(const QString &knownArtKey, bool art = true)
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
        if (art && props.Thumbnail() && best.title + QLatin1Char('\n') + best.artist != knownArtKey) {
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

// The system's media sessions, followed for the tray icon: every session's playback and track, again
// whenever sessions come and go. Never destroyed, like AppleMedia.
class MediaSessions
{
public:
    static void watch(std::function<void()> changed)
    {
        static auto *sessions = new MediaSessions;
        sessions->m_changed = std::move(changed);
        std::thread([] {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            try {
                auto manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
                {
                    std::lock_guard lock{sessions->m_lock};
                    sessions->m_manager = manager;
                    sessions->m_sessionsChanged = manager.SessionsChanged(winrt::auto_revoke, [](auto &&, auto &&) { sessions->follow(); });
                }
                sessions->follow();
            } catch (const winrt::hresult_error &) {
                // no media service (Windows N without the Media Feature Pack): nothing ever plays here
            }
        }).detach();
    }

private:
    void follow()
    {
        {
            std::lock_guard lock{m_lock};
            m_playback.clear();
            m_media.clear();
            try {
                for (const auto &session : m_manager.GetSessions()) {
                    m_playback.push_back(session.PlaybackInfoChanged(winrt::auto_revoke, [this](auto &&, auto &&) { m_changed(); }));
                    m_media.push_back(session.MediaPropertiesChanged(winrt::auto_revoke, [this](auto &&, auto &&) { m_changed(); }));
                }
            } catch (const winrt::hresult_error &) {
            }
        }
        m_changed();
    }

    std::mutex m_lock;
    std::function<void()> m_changed;
    GlobalSystemMediaTransportControlsSessionManager m_manager{nullptr};
    GlobalSystemMediaTransportControlsSessionManager::SessionsChanged_revoker m_sessionsChanged;
    std::vector<GlobalSystemMediaTransportControlsSession::PlaybackInfoChanged_revoker> m_playback;
    std::vector<GlobalSystemMediaTransportControlsSession::MediaPropertiesChanged_revoker> m_media;
};

void MediaController::watchPlayback()
{
    m_nowPlayingTimer.setSingleShot(true);
    m_nowPlayingTimer.setInterval(300);
    connect(&m_nowPlayingTimer, &QTimer::timeout, this, &MediaController::updateNowPlaying);
    const auto soon = [this] { QMetaObject::invokeMethod(this, &MediaController::playbackChangedSoon, Qt::QueuedConnection); };
    AppleMedia::instance().onNowPlayingChanged(soon);
    MediaSessions::watch(soon);
}

void MediaController::updateNowPlaying()
{
    const Snapshot here = offGuiThread([] { return query(QString(), false); });
    const Snapshot phone = AppleMedia::instance().snapshot();
    QVariantMap now;
    if (here.playing)
        now = {{QStringLiteral("where"), QStringLiteral("here")}, {QStringLiteral("title"), here.title}, {QStringLiteral("artist"), here.artist}};
    else if (phone.playing)
        now = {{QStringLiteral("where"), QStringLiteral("iphone")}, {QStringLiteral("title"), phone.title},
               {QStringLiteral("artist"), phone.artist}, {QStringLiteral("source"), phone.source}};
    if (now != m_nowPlaying) {
        m_nowPlaying = now;
        emit nowPlayingChanged();
    }
}

void MediaController::watchIphone()
{
    AppleMedia::instance().onIphonesChanged([this] {
        QMetaObject::invokeMethod(this, [this] { emit iphonesChanged(); }, Qt::QueuedConnection);
    });
    AppleMedia::instance().ensureStarted();
}

QVariantList MediaController::iphones() const
{
    return AppleMedia::instance().iphones();
}

void MediaController::refreshIphones()
{
    AppleMedia::instance().ensureStarted(true);
}

void MediaController::refresh()
{
    AppleMedia &iphone = AppleMedia::instance();
    iphone.ensureStarted();
    const QString previousKey = m_artKey;
    Snapshot snapshot = offGuiThread([previousKey] { return query(previousKey); });
    // The iPhone when nothing plays here but there, or when there is nothing here at all
    if (Snapshot phone = iphone.snapshot(); !phone.title.isEmpty() && (snapshot.title.isEmpty() || (phone.playing && !snapshot.playing)))
        snapshot = phone;

    QVariantMap player;
    // A player with nothing loaded has nothing to show
    if (!snapshot.service.isEmpty() && !snapshot.title.isEmpty()) {
        const QString key = snapshot.title + QLatin1Char('\n') + snapshot.artist;
        QString artUrl = m_player.value(QStringLiteral("artUrl")).toString();
        if (key != m_artKey) {
            dropArt(artUrl);
            artUrl = saveArt(key, snapshot.art);
            m_artKey = key;
            if (!snapshot.source.isEmpty())
                fetchIphoneArt(key, snapshot.title, snapshot.artist);
        }
        player = {
            {QStringLiteral("title"), snapshot.title},
            {QStringLiteral("artist"), snapshot.artist},
            {QStringLiteral("artUrl"), artUrl},
            {QStringLiteral("playing"), snapshot.playing},
            {QStringLiteral("canGoNext"), snapshot.canGoNext},
            {QStringLiteral("canGoPrevious"), snapshot.canGoPrevious},
        };
        if (!snapshot.source.isEmpty()) {
            player.insert(QStringLiteral("source"), snapshot.source);
            player.insert(QStringLiteral("volumeStep"), Ams::VolumeStep);
            player.insert(QStringLiteral("canLike"), snapshot.canLike);
            player.insert(QStringLiteral("liked"), m_liked.contains(key));
        }
    } else {
        m_artKey.clear();
    }
    m_service = player.isEmpty() ? QString() : snapshot.service;
    if (player != m_player) {
        m_player = player;
        emit playerChanged();
    }

    // The volume of where the music plays: the iPhone's while it is shown
    int volume = -1;
    bool muted = false;
    if (m_service == IphoneService) {
        volume = iphone.volumeSettling() ? m_volume : snapshot.volume;
    } else if (auto endpoint = defaultEndpointVolume()) {
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
    if (m_service == IphoneService) {
        AppleMedia::instance().send(method == QLatin1String("PlayPause") ? TogglePlayPause
                                    : method == QLatin1String("Next") ? NextTrack : PreviousTrack);
        return;
    }
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

void MediaController::toggleLike()
{
    if (m_service != IphoneService)
        return;
    const QString key = m_artKey;
    const bool liked = !m_liked.contains(key);
    // shown at once; refused by the iPhone, the star goes back to what it was
    if (liked)
        m_liked.insert(key);
    else
        m_liked.remove(key);
    m_player[QStringLiteral("liked")] = liked;
    emit playerChanged();
    AppleMedia::instance().send(LikeTrack, [this, key, liked](bool ok) {
        if (ok || key != m_artKey)
            return;
        if (liked)
            m_liked.remove(key);
        else
            m_liked.insert(key);
        m_player[QStringLiteral("liked")] = !liked;
        emit playerChanged();
    });
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
    if (m_service == IphoneService) {
        const int steps = Ams::volumeSteps(m_volume, percent);
        if (steps == 0 || m_volume < 0)
            return;
        AppleMedia::instance().stepVolume(steps);
        m_volume = Ams::afterSteps(m_volume, steps);
        emit volumeChanged();
        return;
    }
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
    if (m_service == IphoneService)
        return; // AMS has no mute
    m_muted = muted;
    emit volumeChanged();
    if (auto endpoint = defaultEndpointVolume())
        endpoint->SetMute(muted, nullptr);
}
