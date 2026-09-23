// MyPods
// License: GPL-3.0

#include "MediaController.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QProcess>
#include <QRegularExpression>

namespace {
const QString kPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString kPlayer = QStringLiteral("org.mpris.MediaPlayer2.Player");
constexpr int kTimeoutMs = 250;
}

// ponytail: blocking calls, polled by the popup while it is open. A hung player costs kTimeoutMs per
// poll; subscribe to PropertiesChanged and go async if the popup ever stays open for long.
void MediaController::refresh()
{
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
    m_service = player.isEmpty() ? QString() : bestService;
    if (player != m_player) {
        m_player = player;
        emit playerChanged();
    }

    // pactl talks to PulseAudio and PipeWire alike; "Volume: front-left: 32768 /  50% / ..." -> 50
    QProcess pactl;
    pactl.start(QStringLiteral("pactl"), {QStringLiteral("get-sink-volume"), QStringLiteral("@DEFAULT_SINK@")});
    int volume = -1;
    if (pactl.waitForFinished(kTimeoutMs) && pactl.exitCode() == 0) {
        const auto match = QRegularExpression(QStringLiteral("(\\d+)%")).match(QString::fromUtf8(pactl.readAllStandardOutput()));
        if (match.hasMatch())
            volume = match.captured(1).toInt();
    }
    if (volume != m_volume) {
        m_volume = volume;
        emit volumeChanged();
    }
}

void MediaController::callPlayer(const QString &method)
{
    if (m_service.isEmpty())
        return;
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
    if (percent == m_volume)
        return;
    m_volume = percent;
    emit volumeChanged();
    // Like the Mac's slider: moving it also unmutes
    QProcess::startDetached(QStringLiteral("pactl"), {QStringLiteral("set-sink-mute"), QStringLiteral("@DEFAULT_SINK@"), QStringLiteral("0")});
    QProcess::startDetached(QStringLiteral("pactl"), {QStringLiteral("set-sink-volume"), QStringLiteral("@DEFAULT_SINK@"),
                                                      QString::number(percent) + QLatin1Char('%')});
}
