// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import mypods as MP

// Rounded field on a quiet fill like MyCAD's, the focus ring while typing.
QQC2.TextField {
    id: control
    implicitHeight: MP.Theme.sizeControlDesktop
    leftPadding: 10
    rightPadding: 10
    topPadding: 0
    bottomPadding: 0
    verticalAlignment: TextInput.AlignVCenter
    font: MP.Theme.fontBody
    color: MP.Theme.labelPrimary
    placeholderTextColor: MP.Theme.labelTertiary
    selectionColor: MP.Theme.accent
    selectedTextColor: MP.Theme.labelOnAccent
    opacity: enabled ? 1 : MP.Theme.opacityDisabled

    background: Rectangle {
        radius: MP.Theme.radiusControl
        color: MP.Theme.fillTertiary
        border.width: control.activeFocus ? 2 : 0
        border.color: MP.Theme.focusRing
    }
}
