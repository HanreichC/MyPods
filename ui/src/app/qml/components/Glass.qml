// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Effects
import magicpods as MP

// MyCAD's glass alert as a surface for popups and floating windows: light at the top, a fine edge and a
// soft shadow a little below. A window needs room around it for the shadow.
Item {
    id: root
    property real radius: 22

    RectangularShadow {
        anchors.fill: parent
        radius: root.radius
        blur: 28
        offset.y: 6
        color: MP.Theme.shadow
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        border.width: 1
        border.color: MP.Theme.popupBorder
        gradient: Gradient {
            GradientStop { position: 0; color: MP.Theme.popup }
            GradientStop { position: 1; color: MP.Theme.popupBottom }
        }
    }
}
