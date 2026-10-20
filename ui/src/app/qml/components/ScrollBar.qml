// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import mypods as MP

// macOS overlay scroller: a thin capsule without a track, only while scrolling or under the pointer,
// a little wider while hovered or dragged.
QQC2.ScrollBar {
    id: control
    padding: 2
    minimumSize: 0.08

    contentItem: Rectangle {
        implicitWidth: control.hovered || control.pressed ? 9 : 6
        implicitHeight: implicitWidth
        radius: width / 2
        color: MP.Theme.dark ? Qt.rgba(1, 1, 1, control.pressed ? 0.5 : 0.35) : Qt.rgba(0, 0, 0, control.pressed ? 0.4 : 0.28)
        opacity: control.policy === QQC2.ScrollBar.AlwaysOn || (control.active && control.size < 1) ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 250 } }
        Behavior on implicitWidth { NumberAnimation { duration: 120 } }
    }
    background: null
}
