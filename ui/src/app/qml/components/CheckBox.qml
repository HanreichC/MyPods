// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Controls.impl as Impl
import magicpods as MP

// Rounded square, filled in the accent with a white check when on.
QQC2.CheckBox {
    id: control
    spacing: 6
    padding: 4
    font.pixelSize: 15
    opacity: enabled ? 1 : 0.4

    indicator: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 20
        implicitHeight: 20
        radius: 6
        color: control.checked ? MP.Theme.accent : MP.Theme.fill
        border.width: control.visualFocus ? 2 : 0
        border.color: MP.Theme.accent
        Behavior on color { ColorAnimation { duration: 120 } }

        Impl.IconImage {
            anchors.centerIn: parent
            visible: control.checked
            sourceSize: Qt.size(16, 16)
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
