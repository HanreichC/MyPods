// MyPods
// License: GPL-3.0

#pragma once

#include <QObject>
#include <QSet>
#include <QVariantMap>

class QNetworkAccessManager;

// Now playing and the default output's volume, for the tray popup, or a paired iPhone's (Ams.h).
// Linux: MPRIS, pactl and BlueZ (MediaController.cpp). Windows: system media sessions, Core Audio and
// WinRT Bluetooth (MediaController_win.cpp).
class MediaController final : public QObject
{
    Q_OBJECT
    // Empty without a player; otherwise title, artist, artUrl, playing, canGoNext, canGoPrevious.
    // The iPhone's also: source (its name), volumeStep, canLike, liked
    Q_PROPERTY(QVariantMap player READ player NOTIFY playerChanged)
    // 0-100, -1 when the volume can't be read
    Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY volumeChanged)
    // The paired iPhones, by name: name, connected (their Apple Media Service answers, so the popup can
    // show and control them)
    Q_PROPERTY(QVariantList iphones READ iphones NOTIFY iphonesChanged)

public:
    using QObject::QObject;

    QVariantMap player() const { return m_player; }
    int volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    QVariantList iphones() const;

    Q_INVOKABLE void refresh();
    // Starts following a paired iPhone, so it is there by the first click
    void watchIphone();
    // Looks for newly paired iPhones (Windows lists them once; BlueZ reports them anyway)
    Q_INVOKABLE void refreshIphones();
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    // Apple Music's star for the iPhone's track. AMS doesn't say whether a track is a favorite, so
    // "liked" is what was clicked here
    Q_INVOKABLE void toggleLike();
    Q_INVOKABLE void setVolume(int percent);
    Q_INVOKABLE void setMuted(bool muted);

signals:
    void playerChanged();
    void volumeChanged();
    void iphonesChanged();

private:
    void callPlayer(const QString &method);
#ifndef Q_OS_WIN
    static QString pactl(const QStringList &args);
#endif
    // Covers (MediaController_cover.cpp)
    void fetchIphoneArt(const QString &key, const QString &title, const QString &artist);
    void showArt(const QString &key, const QByteArray &art);
    static QString saveArt(const QString &key, const QByteArray &art);
    static void dropArt(const QString &url);

    QString m_artKey; // title + artist the cover in m_player belongs to
    QSet<QString> m_liked; // title + artist of the iPhone's tracks starred here
    QNetworkAccessManager *m_network = nullptr; // for the iPhone's covers, created on first use

    QString m_service; // MPRIS bus name / app user model id of the shown player
    QVariantMap m_player;
    int m_volume = -1;
    bool m_muted = false;
};
