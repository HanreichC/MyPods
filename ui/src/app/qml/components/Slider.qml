// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Effects
import mypods as MP

// Apple slider: thin capsule track filled in the accent up to a white knob with a soft shadow.
// The knob's center sits padding + 10.5 px from the ends (the equalizer's scale labels line up with it).
QQC2.Slider {
    id: control
    padding: MP.Theme.spaceXxs
    implicitWidth: vertical ? MP.Theme.sizeSliderKnob + 2 * padding : 160
    implicitHeight: vertical ? 160 : MP.Theme.sizeSliderKnob + 2 * padding
    opacity: enabled ? 1 : MP.Theme.opacityDisabled

    background: Rectangle {
        x: control.leftPadding + (control.vertical ? (control.availableWidth - width) / 2 : 0)
        y: control.topPadding + (control.vertical ? 0 : (control.availableHeight - height) / 2)
        width: control.vertical ? MP.Theme.sizeSliderTrack : control.availableWidth
        height: control.vertical ? control.availableHeight : MP.Theme.sizeSliderTrack
        radius: MP.Theme.sizeSliderTrack / 2
        color: MP.Theme.fillSecondary

        Rectangle {
            // from the bottom when vertical, from the left otherwise
            y: control.vertical ? control.visualPosition * parent.height : 0
            width: control.vertical ? parent.width : control.visualPosition * parent.width
            height: control.vertical ? parent.height - y : parent.height
            radius: parent.radius
            color: MP.Theme.accent
        }
    }

    handle: Rectangle {
        x: control.leftPadding + (control.vertical ? (control.availableWidth - width) / 2 : control.visualPosition * (control.availableWidth - width))
        y: control.topPadding + (control.vertical ? control.visualPosition * (control.availableHeight - height) : (control.availableHeight - height) / 2)
        implicitWidth: MP.Theme.sizeSliderKnob
        implicitHeight: MP.Theme.sizeSliderKnob
        radius: width / 2
        color: "#FFFFFF" // C-1: the knob is white in every mode
        border.width: control.visualFocus ? 2 : 0
        border.color: MP.Theme.focusRing
        // Reduce Motion: no scaling
        scale: control.pressed && !MP.Theme.reduceMotion ? 1.12 : 1
        Behavior on scale { NumberAnimation { duration: MP.Theme.durationSpring; easing.type: MP.Theme.easeSpring; easing.overshoot: MP.Theme.springOvershoot } }
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
