// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import magicpods as MP

// Rounded field on a quiet fill like MyCAD's, the accent ring while typing.
QQC2.TextField {
    id: control
    implicitHeight: 34
    leftPadding: 12
    rightPadding: 12
    topPadding: 0
    bottomPadding: 0
    verticalAlignment: TextInput.AlignVCenter
    font.pixelSize: 15
    color: MP.Theme.text
    placeholderTextColor: MP.Theme.secondaryText
    selectionColor: MP.Theme.accent
    selectedTextColor: "white"
    opacity: enabled ? 1 : 0.4

    background: Rectangle {
        radius: 10
        color: MP.Theme.tertiaryFill
        border.width: control.activeFocus ? 2 : 0
        border.color: MP.Theme.accent
    }
}
