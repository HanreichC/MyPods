// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

pragma Singleton
import QtQuick

QtObject {
    // "auto" follows the system, "light"/"dark" force a mode (Settings > Appearance)
    property string appearance: "auto"
    readonly property bool dark: appearance === "auto" ? Qt.styleHints.colorScheme === Qt.ColorScheme.Dark
                                                       : appearance === "dark"

    readonly property color base: dark ? "#000000" : "#F2F2F7"                                  // systemGroupedBackground

    // Apple HIG system colors (light / dark)
    readonly property color text: dark ? "#FFFFFF" : "#000000"                                  // label
    readonly property color secondaryText: dark ? Qt.rgba(235/255, 235/255, 245/255, 0.6) : Qt.rgba(60/255, 60/255, 67/255, 0.6)
    readonly property color accent: dark ? "#0091FF" : "#0088FF"                                // systemBlue
    readonly property color green: dark ? "#30D158" : "#34C759"
    readonly property color orange: dark ? "#FF9230" : "#FF8D28"
    readonly property color red: dark ? "#FF4245" : "#FF383C"
    readonly property color purple: dark ? "#DB34F2" : "#CB30E0"
    readonly property color indigo: dark ? "#6D7CFF" : "#6155F5"
    readonly property color gray: "#8E8E93"
    readonly property color fill: dark ? Qt.rgba(120/255, 120/255, 128/255, 0.32) : Qt.rgba(120/255, 120/255, 128/255, 0.16)
    readonly property color tertiaryFill: dark ? Qt.rgba(118/255, 118/255, 128/255, 0.24) : Qt.rgba(118/255, 118/255, 128/255, 0.12)
    readonly property color separator: dark ? Qt.rgba(84/255, 84/255, 88/255, 0.65) : Qt.rgba(60/255, 60/255, 67/255, 0.29)

    // Content layer: secondarySystemGroupedBackground, no glass
    readonly property color material: dark ? "#1C1C1E" : "#FFFFFF"
    // Menus / pop-up lists (elevated, opaque)
    readonly property color menu: dark ? "#2C2C2E" : "#FFFFFF"
    // Segmented-control thumb
    readonly property color thumb: dark ? "#636366" : "#FFFFFF"

    // Control layer: Liquid Glass (regular)
    readonly property color glassStrong: dark ? Qt.rgba(0.17, 0.17, 0.2, 0.72) : Qt.rgba(1, 1, 1, 0.72)
    readonly property color glassBorder: dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.85)
    readonly property color glassSheen: dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(1, 1, 1, 0.45)
    readonly property color popup: dark ? Qt.rgba(0.11, 0.11, 0.13, 0.97) : Qt.rgba(0.98, 0.98, 1, 0.97)

    readonly property int radius: 26
    readonly property int maxContentWidth: 480

    function asset(path) {
        return Qt.resolvedUrl("assets/" + path);
    }

    function batteryColor(level, charging) {
        if (charging)
            return green;
        return level <= 20 ? red : accent;
    }
}
