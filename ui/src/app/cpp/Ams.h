// MyPods
// License: GPL-3.0

#pragma once

#include <QString>
#include <QtMath>

// The values of the Apple Media Service (the iPhone's now playing over BLE, MediaController_win.cpp)
namespace Ams {

// iOS moves the volume in 16 steps
constexpr double VolumeStep = 100.0 / 16;

// Playback info is "state,rate,elapsed": 0 paused, 1 playing, 2 rewinding, 3 fast forwarding
inline bool isPlaying(const QString &playbackInfo)
{
    return playbackInfo.section(QLatin1Char(','), 0, 0).toInt() != 0;
}

// Volume is "0.0" to "1.0", as percent on the 16 steps (it can sit a bit off them); -1 when the iPhone sends none
inline int volumePercent(const QString &volume)
{
    bool ok = false;
    const double value = volume.toDouble(&ok);
    return ok ? qBound(0, qRound(qRound(value * 100 / VolumeStep) * VolumeStep), 100) : -1;
}

// Louder (> 0) or quieter (< 0) steps from one percentage to another, AMS has nothing else.
// Both round to the nearest step, so a slider value that comes back rounded moves nothing.
inline int volumeSteps(int from, int to)
{
    return qRound(to / VolumeStep) - qRound(from / VolumeStep);
}

// Where those steps land
inline int afterSteps(int from, int steps)
{
    return qBound(0, qRound((qRound(from / VolumeStep) + steps) * VolumeStep), 100);
}

}
