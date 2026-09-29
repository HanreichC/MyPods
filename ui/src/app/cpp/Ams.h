// MyPods
// License: GPL-3.0

#pragma once

#include <QString>
#include <QtMath>

#include <cstdint>

// The Apple Media Service: the BLE service a watch uses for the iPhone's now playing and remote control.
// It keeps working while the iPhone plays to its own headphones. MediaController_win.cpp (WinRT) and
// MediaController.cpp (BlueZ) talk to it. Spec:
// https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleMediaService_Reference/Specification/Specification.html
namespace Ams {

enum Command : uint8_t { TogglePlayPause = 2, NextTrack = 3, PreviousTrack = 4, VolumeUp = 5, VolumeDown = 6 };
enum Entity : uint8_t { Player = 0, Track = 2 };
enum Attribute : uint8_t { PlayerPlaybackInfo = 1, PlayerVolume = 2, TrackArtist = 0, TrackTitle = 2 };
// Entity Update flag: the value goes on, the whole one is read through Entity Attribute
constexpr uint8_t Truncated = 1;

// MediaController's m_service while the iPhone's player is shown; MPRIS names and app ids never look like this
inline const QString IphoneService = QStringLiteral("ams:iphone");

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
