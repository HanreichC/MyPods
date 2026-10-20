// MyPods
// License: GPL-3.0

#pragma once

#include <QString>
#include <QVariantMap>
#include <QtGlobal>

// "About 3 h left" for the tray tooltip: the worn part with the least charge, over the rate it has dropped at since
// it last stopped charging. Only once there is something to go by (20 min, 3 %), and gone again while charging.
// ponytail: a straight line from the start of the discharge; real batteries drop faster near empty, and readings in
// 10 % steps (AirPods without AAP, on Windows) make it jump. A per-model curve would be the upgrade.
namespace BatteryEstimate {
constexpr qint64 kMinMinutes = 20;
constexpr int kMinDrop = 3;

struct Track {
    qint64 since = 0; // ms, when this discharge was first seen
    int from = -1;    // the level then
    int level = -1;   // the last level
};

// The lowest live level of the worn parts (single, left, right; not the case), -1 if none; `charging` if one charges
inline int lowestWorn(const QVariantMap &battery, bool &charging)
{
    int lowest = -1;
    charging = false;
    for (const char *part : {"single", "left", "right"}) {
        const QVariantMap b = battery.value(QLatin1String(part)).toMap();
        if (b.value(QStringLiteral("status")).toInt() != 2) // live readings only (DeviceBatteryStatus::Connected)
            continue;
        charging = charging || b.value(QStringLiteral("charging")).toBool();
        const int level = b.value(QStringLiteral("battery")).toInt();
        lowest = lowest < 0 ? level : qMin(lowest, level);
    }
    return lowest;
}

// Feeds one reading at `nowMs`; the minutes left, -1 while there is too little to tell
inline int update(Track &track, int level, bool charging, qint64 nowMs)
{
    // nothing worn, charging, or charged meanwhile (back from the case): a new discharge starts
    if (level < 0 || charging || track.from < 0 || level > track.level) {
        track = level < 0 || charging ? Track{} : Track{nowMs, level, level};
        return -1;
    }
    track.level = level;
    const qint64 minutes = (nowMs - track.since) / 60000;
    const int drop = track.from - level;
    if (minutes < kMinMinutes || drop < kMinDrop)
        return -1;
    return static_cast<int>(qMin<qint64>(24 * 60, level * minutes / drop));
}

// "45 min", "2 h": rounded as far as such a guess deserves
inline QString format(int minutes)
{
    if (minutes < 90)
        return QStringLiteral("%1 min").arg(qMax(5, (minutes + 2) / 5 * 5));
    return QStringLiteral("%1 h").arg((minutes + 30) / 60);
}
}
