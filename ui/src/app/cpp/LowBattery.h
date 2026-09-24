// MyPods
// License: GPL-3.0

#pragma once

#include <QSet>
#include <QString>
#include <QVariantMap>

// Like macOS: one warning when an earbud (or single-unit headphones) drops to 10 %. It comes back
// only after charging or once the level is above 20 %, so a reading hovering at 10/11 % stays quiet.
namespace LowBattery {
constexpr int kWarnAt = 10;
constexpr int kRearmAbove = 20;

// Lowest level that just became low, -1 if nothing new to warn about. `notified` keeps the parts
// already warned about ("<address>/<part>") across calls.
inline int newlyLow(const QVariantMap &battery, const QString &address, QSet<QString> &notified)
{
    int lowest = -1;
    for (const char *part : {"single", "left", "right"}) { // the case isn't worn, macOS doesn't warn for it either
        const QVariantMap b = battery.value(QLatin1String(part)).toMap();
        if (b.value(QStringLiteral("status")).toInt() != 2) // live readings only (DeviceBatteryStatus::Connected), not cached ones
            continue;
        const QString key = address + QLatin1Char('/') + QLatin1String(part);
        const int level = b.value(QStringLiteral("battery")).toInt();
        if (b.value(QStringLiteral("charging")).toBool() || level > kRearmAbove) {
            notified.remove(key);
            continue;
        }
        if (level <= kWarnAt && !notified.contains(key)) {
            notified.insert(key);
            lowest = lowest < 0 ? level : qMin(lowest, level);
        }
    }
    return lowest;
}
}
