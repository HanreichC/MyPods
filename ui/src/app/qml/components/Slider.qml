// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Effects
import magicpods as MP

// Apple slider: thin capsule track filled in the accent up to a white knob with a soft shadow.
// The knob's center sits padding + 10.5 px from the ends (the equalizer's scale labels line up with it).
QQC2.Slider {
    id: control
    padding: 2
    implicitWidth: vertical ? 25 : 160
    implicitHeight: vertical ? 160 : 25
    opacity: enabled ? 1 : 0.4

    background: Rectangle {
        x: control.leftPadding + (control.vertical ? (control.availableWidth - width) / 2 : 0)
        y: control.topPadding + (control.vertical ? 0 : (control.availableHeight - height) / 2)
        width: control.vertical ? 4 : control.availableWidth
        height: control.vertical ? control.availableHeight : 4
        radius: 2
        color: MP.Theme.fill

        Rectangle {
            // from the bottom when vertical, from the left otherwise
            y: control.vertical ? control.visualPosition * parent.height : 0
            width: control.vertical ? parent.width : control.visualPosition * parent.width
            height: control.vertical ? parent.height - y : parent.height
            radius: 2
            color: MP.Theme.accent
        }
    }

    handle: Rectangle {
        x: control.leftPadding + (control.vertical ? (control.availableWidth - width) / 2 : control.visualPosition * (control.availableWidth - width))
        y: control.topPadding + (control.vertical ? control.visualPosition * (control.availableHeight - height) : (control.availableHeight - height) / 2)
        implicitWidth: 21
        implicitHeight: 21
        radius: width / 2
        color: "white"
        border.width: control.visualFocus ? 2 : 0
        border.color: MP.Theme.accent
        scale: control.pressed ? 1.12 : 1
        Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutBack } }
        // effects need a shader-capable scene graph; the software renderer drops the item
        layer.enabled: GraphicsInfo.api !== GraphicsInfo.Software
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 0.4
            shadowOpacity: 0.3
            shadowVerticalOffset: 1
        }
    }
}
