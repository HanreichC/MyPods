// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Controls.impl as Impl
import mypods as MP

// Rounded square, filled in the accent with a white check when on.
QQC2.CheckBox {
    id: control
    spacing: 6
    padding: MP.Theme.spaceXs
    font: MP.Theme.fontBody
    opacity: enabled ? 1 : MP.Theme.opacityDisabled

    indicator: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: MP.Theme.sizeCheckbox
        implicitHeight: MP.Theme.sizeCheckbox
        radius: MP.Theme.radiusCheckbox
        color: control.checked ? MP.Theme.accent : MP.Theme.fillSecondary
        border.width: control.visualFocus ? 2 : 0
        border.color: MP.Theme.focusRing
        Behavior on color { ColorAnimation { duration: MP.Theme.durationHover } }

        Impl.IconImage {
            anchors.centerIn: parent
            visible: control.checked
            sourceSize: Qt.size(13, 13) // MyStyle: no token for the check inside the 16 px box
            source: MP.Units.asset("icons/icon-check.svg")
            color: MP.Theme.labelOnAccent
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        font: control.font
        color: MP.Theme.labelPrimary
        verticalAlignment: Text.AlignVCenter
    }
}
