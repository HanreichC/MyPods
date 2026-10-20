// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

pragma Singleton
import QtQuick
import mypods as MP

// What MyPods adds to MyStyle's Theme.qml (generated from the tokens, copied unchanged from MyStyle's platforms/qml)
QtObject {
    readonly property int gridUnit: 18

    function asset(path) {
        return Qt.resolvedUrl("assets/" + path);
    }

    function batteryColor(level, charging) {
        if (charging)
            return MP.Theme.green;
        return level <= 20 ? MP.Theme.red : MP.Theme.accent;
    }

    // A text style with a change, e.g. font(MP.Theme.fontCallout, { weight: Font.DemiBold }). A whole font, because
    // setting font.weight beside font: is not allowed, and setting it on a component drops the component's font: binding.
    function font(style, changes) {
        return Qt.font(Object.assign({ family: style.family, pixelSize: style.pixelSize, weight: style.weight,
                                       letterSpacing: style.letterSpacing, capitalization: style.capitalization }, changes));
    }

    // MyStyle: kein Token für 11 Regular (Achsenbeschriftung § 9.2, Detailzeilen); Section-Größe ohne Versalien
    readonly property font fontDetail: font(MP.Theme.fontSection, { weight: Font.Normal, letterSpacing: MP.Theme.fontTouchCaption2.letterSpacing,
                                                                capitalization: Font.MixedCase })
}
