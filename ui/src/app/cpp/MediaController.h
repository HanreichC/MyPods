// MyPods
// License: GPL-3.0

#pragma once

#include <QObject>
#include <QVariantMap>

// Now playing and the default output's volume, for the tray popup.
// Linux: MPRIS and pactl (MediaController.cpp). Windows: system media sessions and Core Audio (MediaController_win.cpp).
class MediaController final : public QObject
{
    Q_OBJECT
    // Empty without a player; otherwise title, artist, artUrl, playing, canGoNext, canGoPrevious
    Q_PROPERTY(QVariantMap player READ player NOTIFY playerChanged)
    // 0-100, -1 when the volume can't be read
    Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY volumeChanged)

public:
    using QObject::QObject;

    QVariantMap player() const { return m_player; }
    int volume() const { return m_volume; }
    bool muted() const { return m_muted; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void setVolume(int percent);
    Q_INVOKABLE void setMuted(bool muted);

signals:
    void playerChanged();
    void volumeChanged();

private:
    void callPlayer(const QString &method);
#ifndef Q_OS_WIN
    static QString pactl(const QStringList &args);
#else
    QString m_artKey; // title + artist the cover in m_player belongs to
#endif

    QString m_service; // MPRIS bus name / app user model id of the shown player
    QVariantMap m_player;
    int m_volume = -1;
    bool m_muted = false;
};
