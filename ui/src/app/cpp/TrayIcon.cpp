// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

#include "TrayIcon.h"

#include "LowBattery.h"

#include <QApplication>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QSettings>
#include <QStyleHints>
#include <QDebug>

namespace {
QString iconPath(const QString &name) {
    return QStringLiteral(":/qt/qml/magicpods/src/app/qml/assets/icons/") + name;
}

// Drawn for each size a panel may ask for rather than scaled: 16 px on Windows at 100 % up to 64 on a
// large KDE panel. The shapes sit on a 16-unit grid.
QImage render(const TrayIcon::State &state, int size, bool darkPanel)
{
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 16.0, size / 16.0);
    const QColor fg = darkPanel ? QColor(Qt::white) : QColor(0x1a, 0x1a, 0x1a);
    const QColor red = darkPanel ? QColor(0xff, 0x45, 0x3a) : QColor(0xe0, 0x28, 0x1e); // system red
    p.setPen(Qt::NoPen);
    p.setBrush(fg);

    if (state.kind == TrayIcon::Kind::Headphones) {
        // the battery ring, clockwise from 12 o'clock over a faint track; red at 20 % like on iOS
        const QRectF ring(1.1, 1.1, 13.8, 13.8);
        QPen pen(fg, 1.6);
        pen.setCapStyle(Qt::RoundCap);
        p.setBrush(Qt::NoBrush);
        p.setPen(pen);
        p.setOpacity(0.28);
        p.drawEllipse(ring);
        p.setOpacity(1);
        if (state.battery > 0) {
            pen.setColor(state.battery <= LowBattery::kRearmAbove ? red : fg);
            p.setPen(pen);
            p.drawArc(ring, 90 * 16, -qRound(state.battery * 3.6 * 16));
        }
        // the headphones of icon-headphones-fill.svg (24 units), 8.4 units wide
        p.save();
        p.translate(3.8, 3.6);
        p.scale(0.35, 0.35);
        QPainterPath band;
        band.moveTo(4, 15.5);
        band.lineTo(4, 12);
        band.arcTo(4, 4, 16, 16, 180, -180);
        band.lineTo(20, 15.5);
        pen = QPen(fg, 2.6);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        p.drawPath(band);
        p.setPen(Qt::NoPen);
        p.setBrush(fg);
        p.drawRoundedRect(QRectF(2.5, 13, 6, 8.5), 2.6, 2.6);
        p.drawRoundedRect(QRectF(15.5, 13, 6, 8.5), 2.6, 2.6);
        p.restore();
    } else {
        // the app logo (logo-dark.svg), dimmed like an inactive menu bar extra
        p.setOpacity(0.45);
        p.drawEllipse(QPointF(3.69, 5.69), 3.69, 3.69);
        p.drawEllipse(QPointF(12.31, 5.69), 3.69, 3.69);
        p.setOpacity(0.45 * 0.66);
        p.drawRoundedRect(QRectF(4.92, 5.08, 2.46, 9.85), 1.23, 1.23);
        p.drawRoundedRect(QRectF(8.62, 5.08, 2.46, 9.85), 1.23, 1.23);
        p.setOpacity(1);
    }

    if (state.badge != TrayIcon::Badge::None) {
        // cut a gap around the badge, then draw it: a play triangle for here, a phone for the iPhone
        p.setPen(Qt::NoPen);
        p.setBrush(fg);
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.drawEllipse(QPointF(12.6, 12.6), 4.4, 4.4);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        if (state.badge == TrayIcon::Badge::Play) {
            QPen pen(fg, 0.8);
            pen.setJoinStyle(Qt::RoundJoin);
            p.setPen(pen);
            p.drawPolygon(QPolygonF({QPointF(10.9, 10.3), QPointF(15.2, 12.6), QPointF(10.9, 14.9)}));
        } else {
            p.drawRoundedRect(QRectF(10.7, 9.4, 3.9, 6.4), 1.1, 1.1);
            p.setCompositionMode(QPainter::CompositionMode_Clear);
            p.drawRoundedRect(QRectF(11.6, 10.4, 2.1, 4.2), 0.4, 0.4);
        }
    }
    return image;
}
}

TrayIcon::TrayIcon(QObject *parent)
    : QSystemTrayIcon(parent) {
    connect(this, &QSystemTrayIcon::activated, this, [this](ActivationReason reason) {
        qDebug() << "TrayIcon activated, reason =" << reason;
        if (reason == QSystemTrayIcon::DoubleClick) {
            emit doubleClicked();
        } else if (reason == QSystemTrayIcon::Trigger) {
            // StatusNotifierItem (KDE, Wayland) has no double click, only two activations in a row
            if (m_lastTrigger.isValid() && m_lastTrigger.elapsed() < QApplication::doubleClickInterval()) {
                m_lastTrigger.invalidate();
                emit doubleClicked();
            } else {
                m_lastTrigger.start();
                emit leftClicked();
            }
        }
    });

    if (qApp) {
        qApp->installEventFilter(this);
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &TrayIcon::updateIcon);
    }

    updateIcon();
}

void TrayIcon::setState(const State &state) {
    if (m_state == state)
        return;
    m_state = state;
    updateIcon();
}

bool TrayIcon::eventFilter(QObject *watched, QEvent *event) {
    if (watched == qApp && event && event->type() == QEvent::ApplicationPaletteChange) {
        updateIcon();
    }
    return QSystemTrayIcon::eventFilter(watched, event);
}

int TrayIcon::themeMode() const {
    return static_cast<int>(m_themeMode);
}

void TrayIcon::setThemeMode(int mode) {
    const auto newMode = static_cast<ThemeMode>(mode);
    if (m_themeMode == newMode)
        return;
    m_themeMode = newMode;
    emit themeModeChanged();
    updateIcon();
}

bool TrayIcon::isDarkTheme() const {
    // Light/Dark name the icon's own color; a light (white) icon is the variant for dark panels.
    if (m_themeMode == ThemeMode::Light)
        return true;
    if (m_themeMode == ThemeMode::Dark)
        return false;
#ifdef Q_OS_WIN
    // The taskbar has a theme of its own (Settings > Personalization > Colors), apart from the apps'.
    // ponytail: read on each icon update; a switch of the taskbar alone shows with the next state change.
    const QSettings personalize(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
                                QSettings::NativeFormat);
    if (const QVariant light = personalize.value(QStringLiteral("SystemUsesLightTheme")); light.isValid())
        return light.toInt() == 0;
#endif
    const QColor bg = QApplication::palette().color(QPalette::Window);
    const qreal luminance = (0.2126 * bg.redF()) + (0.7152 * bg.greenF()) + (0.0722 * bg.blueF());
    return luminance < 0.5;
}

void TrayIcon::updateIcon() {
    const bool darkPanel = isDarkTheme();
    if (m_state.kind == Kind::Offline) {
        setIcon(QIcon(iconPath(darkPanel ? QStringLiteral("logo-warning_dark.svg") : QStringLiteral("logo-warning_light.svg"))));
        return;
    }
    QIcon icon;
    for (int size : {16, 20, 22, 24, 32, 40, 44, 48, 64})
        icon.addPixmap(QPixmap::fromImage(render(m_state, size, darkPanel)));
    setIcon(icon);
}
