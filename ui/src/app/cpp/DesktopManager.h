// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

#pragma once

#include <QObject>
#include <QString>

// A path as one argument of a .desktop file's Exec key (Desktop Entry spec): quoted, with ", `, $ and \ escaped,
// then the string escape (every \ doubled) and %% for a literal %. A plain path breaks at its first space.
inline QString desktopExecArgument(const QString &path)
{
    QString quoted;
    for (const QChar c : path) {
        if (c == QLatin1Char('"') || c == QLatin1Char('`') || c == QLatin1Char('$') || c == QLatin1Char('\\'))
            quoted += QLatin1Char('\\');
        quoted += c;
    }
    quoted.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    quoted.replace(QLatin1Char('%'), QStringLiteral("%%"));
    return QLatin1Char('"') + quoted + QLatin1Char('"');
}

class QNetworkAccessManager;

// BlueZ presents itself as an Apple device when [General] in main.conf has an uncommented DeviceID with Apple's vendor
// (bluetooth:004C:...). The iPhone and the AirPods leave a computer without it out of their handoff.
inline bool hasAppleDeviceId(const QString &mainConf)
{
    bool general = false;
    for (QString line : mainConf.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (line.startsWith(QLatin1Char('[')))
            general = line == QStringLiteral("[General]");
        else if (general && line.startsWith(QStringLiteral("DeviceID"))) {
            const QString value = line.section(QLatin1Char('='), 1).trimmed();
            if (line.section(QLatin1Char('='), 0, 0).trimmed() == QStringLiteral("DeviceID") &&
                value.startsWith(QStringLiteral("bluetooth:004C"), Qt::CaseInsensitive))
                return true;
        }
    }
    return false;
}

class DesktopManager : public QObject
{
    Q_OBJECT
    // Windows: a newer release on GitHub (the MSI is installed by hand); empty while there is none
    Q_PROPERTY(QString updateVersion READ updateVersion NOTIFY updateChanged)
    Q_PROPERTY(QString updateUrl READ updateUrl NOTIFY updateChanged)
    // the person may not want MyPods to ask GitHub; stored with the language (QSettings)
    Q_PROPERTY(bool checkUpdates READ checkUpdates WRITE setCheckUpdates NOTIFY updateChanged)
    // Linux: BlueZ presents itself as an Apple device (DeviceID in /etc/bluetooth/main.conf); always true on Windows
    Q_PROPERTY(bool appleDeviceId READ appleDeviceId NOTIFY appleDeviceIdChanged)
    // Linux: MyPods starts hidden in the tray at login (~/.config/autostart/app.mypods.desktop)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY autostartChanged)
    // Windows: Ctrl+Alt+Shift+N/O/H anywhere run the actions of `mypods --action` (noise-next, output-next, move-here)
    Q_PROPERTY(bool hotkeys READ hotkeys WRITE setHotkeys NOTIFY hotkeysChanged)

public:
    explicit DesktopManager(QObject *parent = nullptr);
    Q_INVOKABLE bool install();
    Q_INVOKABLE bool uninstall();
    Q_INVOKABLE bool isInstalled() const;
    // Where the logs go (the daemon's core.log, the UI's ui.log): %LOCALAPPDATA%/MyPods/logs, ~/.local/state/mypods
    static QString logDir();
    Q_INVOKABLE void openLogDir() const;

    QString updateVersion() const { return m_updateVersion; }
    QString updateUrl() const { return m_updateUrl; }
    bool checkUpdates() const;
    void setCheckUpdates(bool check);
    // Asks GitHub for the latest release, 30 s after the start and then daily (Windows)
    Q_INVOKABLE void checkForUpdate();

    bool hotkeys() const;
    void setHotkeys(bool on);

    bool appleDeviceId() const;
    // Writes the DeviceID and restarts bluetoothd, as root through pkexec: the desktop asks for the password
    Q_INVOKABLE void setUpAppleDeviceId();
    // The same, blocking, for `mypods --set-up-bluez` (install.sh); pkexec asks in the terminal there. Exit code
    static int setUpAppleDeviceIdNow();

    bool autostart() const;
    void setAutostart(bool on);

signals:
    void updateChanged();
    void hotkeysChanged();
    void appleDeviceIdChanged();
    void autostartChanged();

private:
    QNetworkAccessManager *m_network = nullptr; // made on the first check; QtNetwork stays out of this header
    QString m_updateVersion;
    QString m_updateUrl;
    QString desktopFilePath() const;
    void applyHotkeys();
    bool createDesktopFile();
    // A desktop entry at `path` that starts this MyPods (the AppImage file for an AppImage) with `arguments`
    bool writeDesktopFile(const QString &path, const QString &arguments);
};
