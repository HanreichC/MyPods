// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import mypods as MP

// Inset-grouped section as MyCAD's settings glass: translucent over the backdrop, a light edge, lifted a little.
Rectangle {
    id: root
    default property alias content: column.data
    property alias spacing: column.spacing
    property int padding: MP.Theme.spaceL
    property int verticalPadding: MP.Theme.spaceXs

    Layout.fillWidth: true
    implicitHeight: column.implicitHeight + verticalPadding * 2
    radius: MP.Theme.radiusPanel
    color: MP.Theme.materialFill
    border.width: 1
    border.color: MP.Theme.materialBorder

    RectangularShadow {
        z: -1
        anchors.fill: parent
        radius: root.radius
        // MyStyle elevation.card (the QML Theme carries no shadows): light mode only
        blur: 24
        offset.y: 4
        color: Qt.rgba(0, 0, 0, MP.Theme.dark ? 0 : 0.05)
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.leftMargin: root.padding
        anchors.rightMargin: root.padding
        anchors.topMargin: root.verticalPadding
        anchors.bottomMargin: root.verticalPadding
        spacing: 0
    }
}
