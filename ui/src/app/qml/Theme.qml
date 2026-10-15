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

    // Backdrop like MyCAD's viewport: a calm vertical gradient the glass floats on
    readonly property color base: dark ? "#2A2A2E" : "#F4F5F7"
    readonly property color baseBottom: dark ? "#1C1C1F" : "#D5D9E0"

    // Text and system colors as in MyCAD (Apple HIG, light / dark)
    readonly property color text: dark ? "#F5F5F7" : "#1D1D1F"
    readonly property color secondaryText: dark ? "#98989D" : "#6E6E73"
    readonly property color accent: dark ? "#0A84FF" : "#007AFF"                                // systemBlue
    readonly property color green: dark ? "#30D158" : "#34C759"
    readonly property color orange: dark ? "#FF9230" : "#FF8D28"
    readonly property color red: dark ? "#FF4245" : "#FF383C"
    readonly property color purple: dark ? "#DB34F2" : "#CB30E0"
    readonly property color indigo: dark ? "#6D7CFF" : "#6155F5"
    readonly property color gray: "#8E8E93"
    readonly property color fill: dark ? Qt.rgba(120/255, 120/255, 128/255, 0.32) : Qt.rgba(120/255, 120/255, 128/255, 0.16)
    readonly property color tertiaryFill: dark ? Qt.rgba(118/255, 118/255, 128/255, 0.24) : Qt.rgba(118/255, 118/255, 128/255, 0.12)
    readonly property color separator: dark ? Qt.rgba(84/255, 84/255, 88/255, 0.65) : Qt.rgba(60/255, 60/255, 67/255, 0.29)

    // Content layer: MyCAD's settings glass, the backdrop shows through
    readonly property color material: dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(1, 1, 1, 0.62)
    readonly property color materialBorder: dark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(1, 1, 1, 0.95)
    // Segmented-control thumb
    readonly property color thumb: dark ? "#636366" : "#FFFFFF"

    // Control layer: Liquid Glass (its clear glass itself is glass.frag)
    readonly property color glassBorder: dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(1, 1, 1, 0.95)
    readonly property color glassSheen: dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(1, 1, 1, 0.45)
    // Floating windows and alerts without a blur behind: MyCAD's glass alert, light at the top
    readonly property color popup: dark ? Qt.rgba(52/255, 52/255, 58/255, 0.98) : Qt.rgba(1, 1, 1, 0.99)
    readonly property color popupBottom: dark ? Qt.rgba(38/255, 38/255, 43/255, 0.98) : Qt.rgba(244/255, 245/255, 248/255, 0.99)
    // ... over the desktop KWin blurs: clearer, yet readable over any wallpaper
    readonly property color popupGlass: dark ? Qt.rgba(52/255, 52/255, 58/255, 0.55) : Qt.rgba(1, 1, 1, 0.55)
    readonly property color popupGlassBottom: dark ? Qt.rgba(38/255, 38/255, 43/255, 0.55) : Qt.rgba(244/255, 245/255, 248/255, 0.55)
    readonly property color popupBorder: dark ? Qt.rgba(1, 1, 1, 0.13) : Qt.rgba(0, 0, 0, 0.09)
    readonly property color shadow: Qt.rgba(0, 0, 0, dark ? 0.45 : 0.13)
    readonly property color closeHover: "#E5484D"

    readonly property int radius: 26
    // The glass bars at the top of the window (tabs, window buttons); their buttons are 6 px smaller all round
    readonly property int barHeight: 56
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
