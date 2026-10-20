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

class DesktopManager : public QObject
{
    Q_OBJECT
    // Windows: a newer release on GitHub (the MSI is installed by hand); empty while there is none
    Q_PROPERTY(QString updateVersion READ updateVersion NOTIFY updateChanged)
    Q_PROPERTY(QString updateUrl READ updateUrl NOTIFY updateChanged)
    // the person may not want MyPods to ask GitHub; stored with the language (QSettings)
    Q_PROPERTY(bool checkUpdates READ checkUpdates WRITE setCheckUpdates NOTIFY updateChanged)
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

signals:
    void updateChanged();
    void hotkeysChanged();

private:
    QNetworkAccessManager *m_network = nullptr; // made on the first check; QtNetwork stays out of this header
    QString m_updateVersion;
    QString m_updateUrl;
    QString desktopFilePath() const;
    void applyHotkeys();
    bool createDesktopFile();
};
