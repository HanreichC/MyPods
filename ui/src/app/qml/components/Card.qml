// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick
import QtQuick.Layouts
import magicpods as MP

// Inset-grouped section on a standard material (HIG: no Liquid Glass in the content layer).
Rectangle {
    id: root
    default property alias content: column.data
    property alias spacing: column.spacing
    property int padding: MP.Units.largeSpacing
    property int verticalPadding: MP.Units.smallSpacing

    Layout.fillWidth: true
    implicitHeight: column.implicitHeight + verticalPadding * 2
    radius: MP.Theme.radius
    color: MP.Theme.material

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
