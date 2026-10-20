// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Effects
import mypods as MP

// Switch at desktop size like MyCAD's: 38x22 capsule, white knob, system green when on.
QQC2.Switch {
    id: control
    implicitWidth: MP.Theme.sizeSwitchWidth
    implicitHeight: MP.Theme.sizeSwitchHeight
    padding: 0
    spacing: 0
    contentItem: null
    opacity: enabled ? 1 : MP.Theme.opacityDisabled

    indicator: Rectangle {
        x: 0
        y: 0
        implicitWidth: MP.Theme.sizeSwitchWidth
        implicitHeight: MP.Theme.sizeSwitchHeight
        radius: height / 2
        color: control.checked ? MP.Theme.green : MP.Theme.fillSecondary
        border.width: control.visualFocus ? 2 : 0
        border.color: MP.Theme.focusRing
        Behavior on color { ColorAnimation { duration: MP.Theme.durationToggle } }

        Rectangle {
            readonly property int inset: (MP.Theme.sizeSwitchHeight - MP.Theme.sizeSwitchKnob) / 2
            x: control.checked ? parent.width - width - inset : inset
            y: inset
            // a little wider while pressed (22)
            width: control.pressed ? MP.Theme.sizeSwitchKnob + 2 * inset : MP.Theme.sizeSwitchKnob
            height: MP.Theme.sizeSwitchKnob
            radius: height / 2
            color: "#FFFFFF" // C-1: the knob is white in every mode
            Behavior on x { NumberAnimation { duration: MP.Theme.durationSpring; easing.type: MP.Theme.easeSpring; easing.overshoot: MP.Theme.springOvershoot } }
            Behavior on width { NumberAnimation { duration: MP.Theme.durationFade } }
            // effects need a shader-capable scene graph; the software renderer drops the item
            layer.enabled: GraphicsInfo.api !== GraphicsInfo.Software
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: 0.3
                shadowOpacity: 0.25
                shadowVerticalOffset: 2
            }
        }
    }
}
