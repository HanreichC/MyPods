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

class DesktopManager : public QObject
{
    Q_OBJECT

public:
    explicit DesktopManager(QObject *parent = nullptr);
    Q_INVOKABLE bool install();
    Q_INVOKABLE bool uninstall();
    Q_INVOKABLE bool isInstalled() const;

private:
    QString desktopFilePath() const;
    bool createDesktopFile();
};
