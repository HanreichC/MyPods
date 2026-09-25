// MyPods
// License: GPL-3.0

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QStringList>

#include <iterator>

// Keyboard shortcuts: `magicpods --action <name>` changes the active headphones through the daemon and
// exits. Every desktop can bind a command to a key (KDE: System Settings > Shortcuts, GNOME: Settings >
// Keyboard > Custom Shortcuts), on X11 and Wayland alike, so MyPods needs no global hotkey code of its own.
namespace Actions {

inline const QStringList names = {
    QStringLiteral("noise-next"), QStringLiteral("noise-off"), QStringLiteral("noise-anc"),
    QStringLiteral("noise-transparency"), QStringLiteral("noise-adaptive"),
    QStringLiteral("conversation-awareness"), QStringLiteral("move-here"),
    QStringLiteral("eq-next"), QStringLiteral("effects-bypass"),
};

// Noise control modes as the daemon sends them (DeviceAncModes): bits, cycled in this order
inline constexpr int kNoiseModes[] = {1 /*off*/, 2 /*transparency*/, 4 /*adaptive*/, 8 /*wind*/, 16 /*anc*/};

// The mode after `current` among `options`, -1 if there is none to switch to
inline int nextNoiseMode(int current, int options)
{
    const int count = static_cast<int>(std::size(kNoiseModes));
    int start = 0;
    for (int i = 0; i < count; ++i)
        if (kNoiseModes[i] == current)
            start = i + 1;
    for (int step = 0; step < count; ++step) {
        const int mode = kNoiseModes[(start + step) % count];
        if ((options & mode) && mode != current)
            return mode;
    }
    return -1;
}

// The SetCapabilities request for `action` on the active device `info` (GetActiveDeviceInfo's "info"),
// empty when the headphones can't do it
inline QJsonObject request(const QString &action, const QJsonObject &info)
{
    const QJsonObject caps = info.value(QStringLiteral("capabilities")).toObject();
    const QJsonObject anc = caps.value(QStringLiteral("anc")).toObject();
    QJsonObject change;

    if (action == QStringLiteral("noise-next") && !anc.isEmpty()) {
        const int mode = nextNoiseMode(anc.value(QStringLiteral("selected")).toInt(), anc.value(QStringLiteral("options")).toInt());
        if (mode > 0)
            change.insert(QStringLiteral("anc"), QJsonObject{{QStringLiteral("selected"), mode}});
    } else if (action.startsWith(QStringLiteral("noise-")) && !anc.isEmpty()) {
        static const QHash<QString, int> modes = {
            {QStringLiteral("noise-off"), 1}, {QStringLiteral("noise-transparency"), 2},
            {QStringLiteral("noise-adaptive"), 4}, {QStringLiteral("noise-anc"), 16},
        };
        const int mode = modes.value(action);
        if (anc.value(QStringLiteral("options")).toInt() & mode)
            change.insert(QStringLiteral("anc"), QJsonObject{{QStringLiteral("selected"), mode}});
    } else if (action == QStringLiteral("conversation-awareness") && caps.contains(QStringLiteral("conversationAwareness"))) {
        const bool on = caps.value(QStringLiteral("conversationAwareness")).toObject().value(QStringLiteral("selected")).toBool();
        change.insert(QStringLiteral("conversationAwareness"), QJsonObject{{QStringLiteral("selected"), !on}});
    } else if (action == QStringLiteral("move-here") && caps.contains(QStringLiteral("autoSwitch"))) {
        change.insert(QStringLiteral("autoSwitch"), QJsonObject{{QStringLiteral("takeover"), true}});
    } else if (action == QStringLiteral("eq-next") && caps.contains(QStringLiteral("equalizer"))) {
        const QJsonObject eq = caps.value(QStringLiteral("equalizer")).toObject();
        const QJsonArray options = eq.value(QStringLiteral("options")).toArray();
        const qsizetype current = options.toVariantList().indexOf(eq.value(QStringLiteral("selected")).toVariant());
        if (!options.isEmpty())
            change.insert(QStringLiteral("equalizer"), QJsonObject{{QStringLiteral("selected"), options.at((current + 1) % options.size())}});
    } else if (action == QStringLiteral("effects-bypass") && caps.contains(QStringLiteral("equalizer"))) {
        const bool on = caps.value(QStringLiteral("equalizer")).toObject().value(QStringLiteral("bypass")).toBool();
        change.insert(QStringLiteral("equalizer"), QJsonObject{{QStringLiteral("bypass"), !on}});
    }

    if (change.isEmpty())
        return {};
    return {
        {QStringLiteral("method"), QStringLiteral("SetCapabilities")},
        {QStringLiteral("arguments"), QJsonObject{
             {QStringLiteral("address"), info.value(QStringLiteral("address"))},
             {QStringLiteral("capabilities"), change},
         }},
    };
}

}
