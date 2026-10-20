// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import mypods as MP

// Capsule button like MyCAD's: highlighted = the prominent action (accent, white text), otherwise a quiet fill.
QQC2.Button {
    id: control
    implicitHeight: 30
    topInset: 0 // Material insets the background 6 px top and bottom
    bottomInset: 0
    leftPadding: 16
    rightPadding: 16
    font.pixelSize: 13
    font.weight: Font.DemiBold
    opacity: enabled ? 1 : 0.4

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.highlighted ? "white" : MP.Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: height / 2
        color: control.highlighted ? (control.pressed ? Qt.darker(MP.Theme.accent, 1.15) : MP.Theme.accent)
                                   : (control.pressed ? MP.Theme.fill : MP.Theme.tertiaryFill)
        border.width: control.visualFocus ? 2 : (control.hovered ? 1 : 0)
        border.color: control.visualFocus ? MP.Theme.accent : Qt.rgba(1, 1, 1, 0.35)
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
