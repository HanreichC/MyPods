// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Effects
import magicpods as MP

// iOS switch: 51x31 capsule, white knob, system green when on.
QQC2.Switch {
    id: control
    implicitWidth: 51
    implicitHeight: 31
    padding: 0
    spacing: 0
    contentItem: null
    opacity: enabled ? 1 : 0.4

    indicator: Rectangle {
        x: 0
        y: 0
        implicitWidth: 51
        implicitHeight: 31
        radius: height / 2
        color: control.checked ? MP.Theme.green : MP.Theme.fill
        Behavior on color { ColorAnimation { duration: 200 } }

        Rectangle {
            x: control.checked ? parent.width - width - 2 : 2
            y: 2
            width: control.pressed ? 31 : 27
            height: 27
            radius: height / 2
            color: "white"
            Behavior on x { NumberAnimation { duration: 280; easing.type: Easing.OutBack; easing.overshoot: 1.3 } }
            Behavior on width { NumberAnimation { duration: 150 } }
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
