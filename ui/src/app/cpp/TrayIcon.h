// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

#pragma once

#include <QElapsedTimer>
#include <QSystemTrayIcon>

class TrayIcon final : public QSystemTrayIcon {
    Q_OBJECT
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)

public:
    // A monochrome glyph like a menu bar extra: the app logo (dimmed while nothing is connected) or the
    // headphones in a ring that shows their battery, red when low. A badge says where music plays.
    enum class Kind { Offline, Idle, Headphones };
    enum class Badge { None, Play, Phone };
    struct State {
        Kind kind = Kind::Idle;
        int battery = -1; // 0-100, -1 unknown
        Badge badge = Badge::None;
        bool operator==(const State &o) const { return kind == o.kind && battery == o.battery && badge == o.badge; }
    };

    enum class ThemeMode {
        Auto  = 0,
        Light = 1,
        Dark  = 2
    };

    explicit TrayIcon(QObject *parent = nullptr);

    void setState(const State &state);

    int themeMode() const;
    void setThemeMode(int mode);

signals:
    void leftClicked();
    void doubleClicked();
    void themeModeChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool isDarkTheme() const;
    void updateIcon();

    State m_state;
    ThemeMode m_themeMode = ThemeMode::Auto;
    QElapsedTimer m_lastTrigger;
};
