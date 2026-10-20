// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import mypods as MP

// Capsule button like MyCAD's: highlighted = the prominent action (accent, white text), otherwise a quiet fill.
QQC2.Button {
    id: control
    implicitHeight: MP.Theme.sizeControlDesktop
    topInset: 0 // Material insets the background 6 px top and bottom
    bottomInset: 0
    leftPadding: MP.Theme.spaceL
    rightPadding: MP.Theme.spaceL
    font: MP.Units.font(MP.Theme.fontCallout, { weight: Font.DemiBold })
    opacity: enabled ? 1 : MP.Theme.opacityDisabled

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.highlighted ? MP.Theme.labelOnAccent : MP.Theme.labelPrimary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: height / 2
        color: control.highlighted ? (control.pressed ? Qt.darker(MP.Theme.accent, 1.15) : MP.Theme.accent)
                                   : (control.pressed ? MP.Theme.fillSecondary : MP.Theme.fillTertiary)
        border.width: control.visualFocus ? 2 : (control.hovered ? 1 : 0)
        // MyStyle: kein Token für den Hover-Rand (§ 9.3: 1 px Weiß 35 %)
        border.color: control.visualFocus ? MP.Theme.focusRing : Qt.rgba(1, 1, 1, 0.35)
        Behavior on color { ColorAnimation { duration: MP.Theme.durationHover } }
    }
}
