// MyPods
// License: GPL-3.0

#pragma once

#include <QWindow>
#ifdef HAVE_KWINDOWSYSTEM
#include <KWindowEffects>
#include <QPainterPath>
#include <QRegion>
#include <QTimer>
#endif

// Liquid Glass for the popups that are windows of their own (tray popup, lid popup): KDE's KWin blurs the
// desktop behind their rounded panel. The window names the panel in its properties glassRect and glassRadius
// and learns from desktopBlur that its glass may be translucent. Without KWin's blur (GNOME, Windows, a build
// without KWindowSystem) nothing changes and the glass stays nearly opaque.
inline void enableBlurBehind(QWindow *window)
{
#ifdef HAVE_KWINDOWSYSTEM
    if (!window || !KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind))
        return;
    // once per change, after QML has settled the panel (it moves every frame while sliding in)
    auto *update = new QTimer(window);
    update->setSingleShot(true);
    update->setInterval(0);
    QObject::connect(update, &QTimer::timeout, window, [window]() {
        // only on a shown window: asking for its native window earlier would get in the way of layer-shell
        if (!window->isVisible())
            return;
        const QRectF rect = window->property("glassRect").toRectF();
        const qreal radius = window->property("glassRadius").toReal();
        QPainterPath panel;
        panel.addRoundedRect(rect, radius, radius);
        KWindowEffects::enableBlurBehind(window, true, QRegion(panel.toFillPolygon().toPolygon()));
    });
    QObject::connect(window, SIGNAL(glassRectChanged()), update, SLOT(start()));
    // the region goes with the surface, which a hidden popup gives up
    QObject::connect(window, &QWindow::visibleChanged, update, qOverload<>(&QTimer::start));
    window->setProperty("desktopBlur", true);
#else
    Q_UNUSED(window);
#endif
}
