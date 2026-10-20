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
    padding: 4
    font.pixelSize: MP.Theme.bodySize
    opacity: enabled ? 1 : 0.4

    indicator: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 16
        implicitHeight: 16
        radius: 5
        color: control.checked ? MP.Theme.accent : MP.Theme.fill
        border.width: control.visualFocus ? 2 : 0
        border.color: MP.Theme.accent
        Behavior on color { ColorAnimation { duration: 120 } }

        Impl.IconImage {
            anchors.centerIn: parent
            visible: control.checked
            sourceSize: Qt.size(13, 13)
            source: MP.Theme.asset("icons/icon-check.svg")
            color: "white"
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        font: control.font
        color: MP.Theme.text
        verticalAlignment: Text.AlignVCenter
    }
}
