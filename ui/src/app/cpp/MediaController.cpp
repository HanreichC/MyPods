// MyPods
// License: GPL-3.0

#include "Ams.h"
#include "MediaController.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QElapsedTimer>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>
#include <deque>
#include <functional>
#include <optional>

using namespace Ams;

namespace {
const QString kPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString kPlayer = QStringLiteral("org.mpris.MediaPlayer2.Player");
constexpr int kTimeoutMs = 250;

const QString kBluez = QStringLiteral("org.bluez");
const QString kDevice = QStringLiteral("org.bluez.Device1");
const QString kCharacteristic = QStringLiteral("org.bluez.GattCharacteristic1");
const QString kAdvertisementPath = QStringLiteral("/app/mypods/ams");
// The Apple Media Service's UUIDs (Ams.h)
const QString kAmsService = QStringLiteral("89d3502b-0f36-433a-8ef4-c502ad55f8dc");
const QString kAmsRemoteCommand = QStringLiteral("9b3c81d8-57b1-4a8a-b8df-0e56f7ca51c2");
const QString kAmsEntityUpdate = QStringLiteral("2f7cabce-808d-411f-9a0c-bb92ba96c102");
const QString kAmsEntityAttribute = QStringLiteral("c6b2f38c-23ab-46d8-a6ab-a3a870bbd5d7");
}

// Asks for the Apple Media Service in an LE advertisement, like a watch: a paired iPhone then
// connects over LE by itself, again after every disconnect, and BlueZ resolves its services.
class AmsAdvertisement : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.LEAdvertisement1")
    Q_PROPERTY(QString Type READ type)
    Q_PROPERTY(QStringList SolicitUUIDs READ solicitUuids)

public:
    using QObject::QObject;
    QString type() const { return QStringLiteral("peripheral"); }
    QStringList solicitUuids() const { return {kAmsService}; }

public slots:
    // BlueZ dropped it (adapter gone, bluetoothd restarting)
    void Release() { emit released(); }

signals:
    void released();
};

// The paired iPhone through BlueZ. Everything runs on the GUI thread: the calls are asynchronous and
// the iPhone pushes every change, so reading the state costs nothing.
class AppleMedia : public QObject
{
    Q_OBJECT

public:
    static AppleMedia &instance()
    {
        // never destroyed: the system bus may still deliver while the application exits
        static auto *media = new AppleMedia;
        return *media;
    }

    // Called when the list of iPhones or their connection changes
    void onIphonesChanged(std::function<void()> changed) { m_changed = std::move(changed); }
    QVariantList iphones() const { return m_iphones; }

    // Advertises and follows BlueZ from the first call on
    void ensureStarted()
    {
        if (m_started)
            return;
        m_started = true;
        QDBusConnection bus = QDBusConnection::systemBus();
        if (!bus.registerObject(kAdvertisementPath, &m_advertisement,
                                QDBusConnection::ExportAllProperties | QDBusConnection::ExportAllSlots))
            qWarning("iPhone: advertisement not exported: %s", qPrintable(bus.lastError().message()));
        connect(&m_advertisement, &AmsAdvertisement::released, this, [this] {
            m_advertisedOn.clear();
            rescanSoon();
        });
        // services come and go with the iPhone's LE connection, the adapter with bluetoothd
        const QString objectManager = QStringLiteral("org.freedesktop.DBus.ObjectManager");
        bus.connect(kBluez, QString(), objectManager, QStringLiteral("InterfacesAdded"), this, SLOT(rescanSoon()));
        bus.connect(kBluez, QString(), objectManager, QStringLiteral("InterfacesRemoved"), this, SLOT(rescanSoon()));
        bus.connect(kBluez, QString(), QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"),
                    this, SLOT(onPropertiesChanged(QDBusMessage)));
        // one GetManagedObjects for the burst of objects a connecting iPhone brings
        m_rescan.setSingleShot(true);
        m_rescan.setInterval(300);
        connect(&m_rescan, &QTimer::timeout, this, &AppleMedia::rescan);
        rescan();
    }

    bool hasTrack() const { return !m_title.isEmpty(); }
    QString name() const { return m_name; }
    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    bool playing() const { return m_playing; }
    int volume() const { return m_volume; }
    bool supports(Command command) const
    {
        // before the first list arrives, everything is offered
        return m_commands.isEmpty() || m_commands.contains(char(command));
    }
    // Only when the player lists it: the star is no use to one that ignores it
    bool offers(Command command) const { return m_commands.contains(char(command)); }

    void send(Command command)
    {
        if (!m_remote.isEmpty())
            write(m_remote, {command});
    }

    // Louder (> 0) or quieter (< 0) by that many iOS steps, one after another, so dragging the slider
    // back and forth can't overtake itself; opposite steps not sent yet cancel out
    void stepVolume(int steps)
    {
        m_pendingSteps += steps;
        m_steppedAt.start();
        if (!m_stepping)
            sendStep();
    }

    // While steps are on their way, and a moment after, the iPhone still reports the old volume
    bool volumeSettling() const
    {
        return m_stepping || (m_steppedAt.isValid() && m_steppedAt.elapsed() < 700);
    }

public slots:
    // one GetManagedObjects for a burst of changes
    void rescanSoon() { m_rescan.start(); }

private slots:

    void onPropertiesChanged(const QDBusMessage &message)
    {
        const QList<QVariant> args = message.arguments();
        if (args.size() < 2)
            return;
        const QVariantMap changed = qdbus_cast<QVariantMap>(args.at(1));
        const QString path = message.path();
        if (path == m_update && changed.contains(QStringLiteral("Value")))
            onEntityUpdate(changed.value(QStringLiteral("Value")).toByteArray());
        else if (path == m_remote && changed.contains(QStringLiteral("Value")))
            m_commands = changed.value(QStringLiteral("Value")).toByteArray(); // the commands the player supports now
        else if (changed.contains(QStringLiteral("ServicesResolved")) || changed.contains(QStringLiteral("Paired"))
                 || changed.contains(QStringLiteral("Connected")) || changed.contains(QStringLiteral("Alias")))
            rescanSoon(); // a device came, went or was renamed, maybe an iPhone
    }

private:
    using Interfaces = QMap<QString, QVariantMap>;

    struct Read {
        uint8_t entity;
        uint8_t attribute;
        std::function<void(const QString &)> done;
    };

    QDBusPendingCall call(const QString &path, const QString &method, const QVariantList &args = {})
    {
        QDBusMessage message = QDBusMessage::createMethodCall(kBluez, path, kCharacteristic, method);
        message.setArguments(args);
        return QDBusConnection::systemBus().asyncCall(message);
    }

    QDBusPendingCall write(const QString &path, std::initializer_list<uint8_t> bytes)
    {
        QByteArray value;
        for (uint8_t byte : bytes)
            value.append(char(byte));
        return call(path, QStringLiteral("WriteValue"), {value, QVariantMap{{QStringLiteral("type"), QStringLiteral("request")}}});
    }

    void rescan()
    {
        const QDBusMessage get = QDBusMessage::createMethodCall(kBluez, QStringLiteral("/"),
            QStringLiteral("org.freedesktop.DBus.ObjectManager"), QStringLiteral("GetManagedObjects"));
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(get), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
            watcher->deleteLater();
            const QDBusMessage reply = watcher->reply();
            if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
                clear(); // no bluetoothd
                listIphones({});
                return;
            }
            // path -> interface -> properties
            QMap<QString, Interfaces> objects;
            const QDBusArgument argument = reply.arguments().constFirst().value<QDBusArgument>();
            argument.beginMap();
            while (!argument.atEnd()) {
                QDBusObjectPath path;
                Interfaces interfaces;
                argument.beginMapEntry();
                argument >> path >> interfaces;
                argument.endMapEntry();
                objects.insert(path.path(), interfaces);
            }
            argument.endMap();
            onObjects(objects);
        });
    }

    void onObjects(const QMap<QString, Interfaces> &objects)
    {
        listIphones(objects);
        QString adapter, service, device;
        for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
            if (adapter.isEmpty() && it->contains(QStringLiteral("org.bluez.LEAdvertisingManager1")))
                adapter = it.key();
            const QVariantMap gatt = it->value(QStringLiteral("org.bluez.GattService1"));
            if (gatt.value(QStringLiteral("UUID")).toString().compare(kAmsService, Qt::CaseInsensitive) != 0)
                continue;
            const QString owner = gatt.value(QStringLiteral("Device")).value<QDBusObjectPath>().path();
            // BlueZ keeps the services of a bonded iPhone that went away; only a connected one answers
            if (objects.value(owner).value(kDevice).value(QStringLiteral("ServicesResolved")).toBool()) {
                service = it.key();
                device = owner;
            }
        }
        advertise(adapter);
        if (service.isEmpty()) {
            clear();
            return;
        }

        QString remote, update, attribute;
        bool remoteNotifying = false, updateNotifying = false;
        for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
            const QVariantMap characteristic = it->value(kCharacteristic);
            if (characteristic.value(QStringLiteral("Service")).value<QDBusObjectPath>().path() != service)
                continue;
            const QString uuid = characteristic.value(QStringLiteral("UUID")).toString().toLower();
            const bool notifying = characteristic.value(QStringLiteral("Notifying")).toBool();
            if (uuid == kAmsRemoteCommand) {
                remote = it.key();
                remoteNotifying = notifying;
            } else if (uuid == kAmsEntityUpdate) {
                update = it.key();
                updateNotifying = notifying;
            } else if (uuid == kAmsEntityAttribute) {
                attribute = it.key();
            }
        }
        if (remote.isEmpty() || update.isEmpty() || attribute.isEmpty())
            return; // still being resolved, the next InterfacesAdded comes
        m_name = objects.value(device).value(kDevice).value(QStringLiteral("Alias")).toString();
        if (m_subscribed && update == m_update)
            return;
        m_remote = remote;
        m_update = update;
        m_attribute = attribute;
        m_subscribed = true;

        // StartNotify only when off: BlueZ keeps it over reconnects, and stopping and starting it has
        // crashed bluetoothd (5.87)
        if (!remoteNotifying)
            call(m_remote, QStringLiteral("StartNotify"));
        if (!updateNotifying)
            call(m_update, QStringLiteral("StartNotify"));
        // the iPhone forgets these with every disconnect; it then sends each on every change, but only
        // then: what is there already is read
        write(m_update, {Track, TrackArtist, TrackTitle});
        write(m_update, {Player, PlayerPlaybackInfo, PlayerVolume});
        const std::pair<uint8_t, uint8_t> current[] = {
            {Track, TrackArtist}, {Track, TrackTitle}, {Player, PlayerPlaybackInfo}, {Player, PlayerVolume}};
        for (const auto &entry : current) {
            const uint8_t entity = entry.first, attribute = entry.second;
            read(entity, attribute, [this, entity, attribute](const QString &text) { apply(entity, attribute, text); });
        }
    }

    // Paired devices that offer AMS, or that BlueZ calls a phone by Apple (vendor 0x004C) while their
    // services aren't known; connected while AMS answers
    void listIphones(const QMap<QString, Interfaces> &objects)
    {
        QMap<QString, bool> ams; // device -> its AMS answers
        for (const Interfaces &interfaces : objects) {
            const QVariantMap gatt = interfaces.value(QStringLiteral("org.bluez.GattService1"));
            if (gatt.value(QStringLiteral("UUID")).toString().compare(kAmsService, Qt::CaseInsensitive) == 0) {
                const QString owner = gatt.value(QStringLiteral("Device")).value<QDBusObjectPath>().path();
                ams.insert(owner, objects.value(owner).value(kDevice).value(QStringLiteral("ServicesResolved")).toBool());
            }
        }
        std::vector<std::pair<QString, bool>> phones;
        for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
            const QVariantMap device = it->value(kDevice);
            if (!device.value(QStringLiteral("Paired")).toBool())
                continue;
            const bool apple = device.value(QStringLiteral("Modalias")).toString().startsWith(QLatin1String("bluetooth:v004C"), Qt::CaseInsensitive);
            if (ams.contains(it.key()) || (apple && device.value(QStringLiteral("Icon")).toString() == QLatin1String("phone")))
                phones.emplace_back(device.value(QStringLiteral("Alias")).toString(), ams.value(it.key()));
        }
        std::sort(phones.begin(), phones.end(), [](const auto &a, const auto &b) { return a.first.localeAwareCompare(b.first) < 0; });
        QVariantList list;
        for (const auto &phone : phones)
            list.append(QVariantMap{{QStringLiteral("name"), phone.first}, {QStringLiteral("connected"), phone.second}});
        if (list != m_iphones) {
            m_iphones = list;
            if (m_changed)
                m_changed();
        }
    }

    void advertise(const QString &adapter)
    {
        if (adapter.isEmpty() || adapter == m_advertisedOn)
            return;
        m_advertisedOn = adapter;
        QDBusMessage registration = QDBusMessage::createMethodCall(kBluez, adapter,
            QStringLiteral("org.bluez.LEAdvertisingManager1"), QStringLiteral("RegisterAdvertisement"));
        registration << QVariant::fromValue(QDBusObjectPath(kAdvertisementPath)) << QVariantMap();
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(registration), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher] {
            watcher->deleteLater();
            // not tried again: an adapter without LE advertising stays without it
            if (watcher->isError())
                qWarning("iPhone: no LE advertisement, the iPhone won't connect by itself: %s",
                         qPrintable(watcher->error().message()));
        });
    }

    // Entity id, attribute id, flags, UTF-8 value
    void onEntityUpdate(const QByteArray &value)
    {
        if (value.size() < 3)
            return;
        const uint8_t entity = uint8_t(value.at(0));
        const uint8_t attribute = uint8_t(value.at(1));
        const QString text = QString::fromUtf8(value.mid(3));
        apply(entity, attribute, text);
        // a notification carries what fits into one packet; the whole title is read separately
        if ((uint8_t(value.at(2)) & Truncated) && entity == Track) {
            read(Track, attribute, [this, attribute, text](const QString &whole) {
                QString &field = attribute == TrackTitle ? m_title : m_artist;
                if (field == text) // not if the next track came in meanwhile
                    field = whole;
            });
        }
    }

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
    }

    // Entity Attribute holds one value at a time: which one is written, then it is read. So the reads
    // queue up.
    void read(uint8_t entity, uint8_t attribute, std::function<void(const QString &)> done)
    {
        m_reads.push_back({entity, attribute, std::move(done)});
        if (m_reads.size() == 1)
            readNext();
    }

    void readNext()
    {
        if (m_reads.empty())
            return;
        const QString path = m_attribute;
        auto *writing = new QDBusPendingCallWatcher(write(path, {m_reads.front().entity, m_reads.front().attribute}), this);
        connect(writing, &QDBusPendingCallWatcher::finished, this, [this, writing, path] {
            writing->deleteLater();
            if (writing->isError()) {
                finishRead(std::nullopt);
                return;
            }
            auto *reading = new QDBusPendingCallWatcher(call(path, QStringLiteral("ReadValue"), {QVariantMap()}), this);
            connect(reading, &QDBusPendingCallWatcher::finished, this, [this, reading] {
                reading->deleteLater();
                const QDBusMessage reply = reading->reply();
                if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty())
                    finishRead(QString::fromUtf8(reply.arguments().constFirst().toByteArray()));
                else
                    finishRead(std::nullopt);
            });
        });
    }

    void finishRead(const std::optional<QString> &value)
    {
        auto done = std::move(m_reads.front().done);
        m_reads.pop_front();
        if (value)
            done(*value);
        readNext();
    }

    void sendStep()
    {
        if (m_pendingSteps == 0 || m_remote.isEmpty()) {
            m_pendingSteps = 0;
            m_stepping = false;
            m_steppedAt.start();
            return;
        }
        m_stepping = true;
        const Command command = m_pendingSteps > 0 ? VolumeUp : VolumeDown;
        m_pendingSteps += m_pendingSteps > 0 ? -1 : 1;
        auto *watcher = new QDBusPendingCallWatcher(write(m_remote, {command}), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
            watcher->deleteLater();
            sendStep();
        });
    }

    // The iPhone went away
    void clear()
    {
        m_subscribed = false;
        m_remote.clear();
        m_update.clear();
        m_attribute.clear();
        m_title.clear();
        m_artist.clear();
        m_playing = false;
        m_volume = -1;
        m_commands.clear();
    }

    bool m_started = false;
    std::function<void()> m_changed;
    QVariantList m_iphones;
    AmsAdvertisement m_advertisement;
    QString m_advertisedOn; // adapter path
    QTimer m_rescan;
    bool m_subscribed = false;
    QString m_remote; // characteristic paths
    QString m_update;
    QString m_attribute;
    std::deque<Read> m_reads;
    QString m_name;
    QString m_title;
    QString m_artist;
    bool m_playing = false;
    int m_volume = -1;
    QByteArray m_commands;
    int m_pendingSteps = 0;
    bool m_stepping = false;
    QElapsedTimer m_steppedAt;
};

void MediaController::watchIphone()
{
    AppleMedia::instance().onIphonesChanged([this] { emit iphonesChanged(); });
    AppleMedia::instance().ensureStarted();
}

QVariantList MediaController::iphones() const
{
    return AppleMedia::instance().iphones();
}

void MediaController::refreshIphones()
{
    AppleMedia::instance().rescanSoon();
}

// ponytail: blocking calls, polled by the popup while it is open. A hung player costs kTimeoutMs per
// poll; subscribe to PropertiesChanged and go async if the popup ever stays open for long.
void MediaController::refresh()
{
    AppleMedia &iphone = AppleMedia::instance();
    iphone.ensureStarted();
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QStringList names = bus.interface()->registeredServiceNames().value();

    // A playing player wins, then a paused one, then whatever else is there
    int bestRank = -1;
    QString bestService;
    QVariantMap bestProps;
    for (const QString &name : names) {
        // playerctld only mirrors the other players
        if (!name.startsWith(QStringLiteral("org.mpris.MediaPlayer2.")) || name.endsWith(QStringLiteral(".playerctld")))
            continue;
        QDBusMessage call = QDBusMessage::createMethodCall(name, kPath, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                           QStringLiteral("GetAll"));
        call << kPlayer;
        const QDBusMessage reply = bus.call(call, QDBus::Block, kTimeoutMs);
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
            continue;
        const QVariantMap props = qdbus_cast<QVariantMap>(reply.arguments().constFirst());
        const QString status = props.value(QStringLiteral("PlaybackStatus")).toString();
        const int rank = status == QLatin1String("Playing") ? 2 : status == QLatin1String("Paused") ? 1 : 0;
        if (rank > bestRank) {
            bestRank = rank;
            bestService = name;
            bestProps = props;
        }
    }

    QVariantMap player;
    if (!bestService.isEmpty()) {
        const QVariantMap meta = qdbus_cast<QVariantMap>(bestProps.value(QStringLiteral("Metadata")));
        const QString title = meta.value(QStringLiteral("xesam:title")).toString();
        // A player with nothing loaded has nothing to show
        if (!title.isEmpty()) {
            player = {
                {QStringLiteral("title"), title},
                {QStringLiteral("artist"), meta.value(QStringLiteral("xesam:artist")).toStringList().join(QStringLiteral(", "))},
                {QStringLiteral("artUrl"), meta.value(QStringLiteral("mpris:artUrl")).toString()},
                {QStringLiteral("playing"), bestRank == 2},
                {QStringLiteral("canGoNext"), bestProps.value(QStringLiteral("CanGoNext")).toBool()},
                {QStringLiteral("canGoPrevious"), bestProps.value(QStringLiteral("CanGoPrevious")).toBool()},
            };
        }
    }
    QString service = bestService;
    // The iPhone when nothing plays here but there, or when there is nothing here at all
    if (iphone.hasTrack() && (player.isEmpty() || (iphone.playing() && bestRank != 2))) {
        const QString key = iphone.title() + QLatin1Char('\n') + iphone.artist();
        QString artUrl = m_player.value(QStringLiteral("artUrl")).toString();
        if (key != m_artKey) {
            dropArt(artUrl);
            artUrl.clear();
            m_artKey = key;
            fetchIphoneArt(key, iphone.title(), iphone.artist());
        }
        player = {
            {QStringLiteral("title"), iphone.title()},
            {QStringLiteral("artist"), iphone.artist()},
            {QStringLiteral("artUrl"), artUrl},
            {QStringLiteral("playing"), iphone.playing()},
            {QStringLiteral("canGoNext"), iphone.supports(NextTrack)},
            {QStringLiteral("canGoPrevious"), iphone.supports(PreviousTrack)},
            {QStringLiteral("source"), iphone.name()},
            {QStringLiteral("volumeStep"), Ams::VolumeStep},
            {QStringLiteral("canLike"), iphone.offers(LikeTrack)},
            {QStringLiteral("liked"), m_liked.contains(key)},
        };
        service = IphoneService;
    } else {
        m_artKey.clear();
    }
    m_service = player.isEmpty() ? QString() : service;
    if (player != m_player) {
        m_player = player;
        emit playerChanged();
    }

    // The volume of where the music plays: the iPhone's while it is shown
    int volume = -1;
    bool muted = false;
    if (m_service == IphoneService) {
        volume = iphone.volumeSettling() ? m_volume : iphone.volume();
    } else {
        // pactl talks to PulseAudio and PipeWire alike; "Volume: front-left: 32768 /  50% / ..." -> 50
        const QString volumeOut = pactl({QStringLiteral("get-sink-volume"), QStringLiteral("@DEFAULT_SINK@")});
        const auto match = QRegularExpression(QStringLiteral("(\\d+)%")).match(volumeOut);
        if (match.hasMatch())
            volume = match.captured(1).toInt();
        muted = pactl({QStringLiteral("get-sink-mute"), QStringLiteral("@DEFAULT_SINK@")}).contains(QLatin1String("yes"));
    }
    if (volume != m_volume || muted != m_muted) {
        m_volume = volume;
        m_muted = muted;
        emit volumeChanged();
    }
}

// Empty on failure. LC_ALL=C because pactl translates "Mute: yes"
QString MediaController::pactl(const QStringList &args)
{
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    process.setProcessEnvironment(env);
    process.start(QStringLiteral("pactl"), args);
    if (!process.waitForFinished(kTimeoutMs) || process.exitCode() != 0)
        return {};
    return QString::fromUtf8(process.readAllStandardOutput());
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
    QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(m_service, kPath, kPlayer, method));
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
    AppleMedia::instance().send(LikeTrack);
    const bool liked = !m_liked.contains(m_artKey);
    if (liked)
        m_liked.insert(m_artKey);
    else
        m_liked.remove(m_artKey);
    m_player[QStringLiteral("liked")] = liked;
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
    QProcess::startDetached(QStringLiteral("pactl"), {QStringLiteral("set-sink-volume"), QStringLiteral("@DEFAULT_SINK@"),
                                                      QString::number(percent) + QLatin1Char('%')});
}

// The sink keeps its volume while muted, so unmuting brings back the old level
void MediaController::setMuted(bool muted)
{
    if (m_service == IphoneService)
        return; // AMS has no mute
    m_muted = muted;
    emit volumeChanged();
    QProcess::startDetached(QStringLiteral("pactl"), {QStringLiteral("set-sink-mute"), QStringLiteral("@DEFAULT_SINK@"),
                                                      muted ? QStringLiteral("1") : QStringLiteral("0")});
}

#include "MediaController.moc"
