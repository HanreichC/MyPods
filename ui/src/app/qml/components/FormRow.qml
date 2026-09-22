// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as Impl
import QtQuick.Layouts
import magicpods as MP

// Inset-grouped list row: [icon tile] label [info] ........ control
// Draws a separator above itself when a visible row precedes it in the same section.
Item {
    id: root

    property string label: ""
    property string tooltip: ""
    property url iconSource: ""
    property color iconColor: MP.Theme.accent
    default property alias content: contentHolder.data
    readonly property bool isFormRow: true

    readonly property bool hasRowAbove: {
        const siblings = parent ? parent.children : [];
        for (let i = 0; i < siblings.length && siblings[i] !== root; ++i)
            if (siblings[i].visible && siblings[i].isFormRow)
                return true;
        return false;
    }

    Layout.fillWidth: true
    implicitHeight: Math.max(44, row.implicitHeight + MP.Units.smallSpacing * 2)

    Rectangle {
        visible: root.hasRowAbove
        x: labelText.x
        width: parent.width - x
        height: 1
        color: MP.Theme.separator
    }

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: MP.Units.mediumSpacing + 4

        Rectangle {
            visible: root.iconSource.toString() !== ""
            implicitWidth: 30
            implicitHeight: 30
            radius: 8
            color: root.iconColor

            Impl.IconImage {
                anchors.centerIn: parent
                sourceSize: Qt.size(18, 18)
                source: root.iconSource
                color: "white"
            }
        }

        Label {
            id: labelText
            Layout.fillWidth: true
            text: root.label.replace(/:\s*$/, "")
            color: MP.Theme.text
            font.pixelSize: 17
            elide: Text.ElideRight
            opacity: root.enabled ? 1 : 0.4
        }

        ToolButton {
            visible: root.tooltip !== ""
            implicitWidth: 28
            implicitHeight: 28
            padding: 4
            icon.source: MP.Theme.asset("icons/icon-info.svg")
            icon.color: MP.Theme.accent
            icon.width: 20
            icon.height: 20

            ToolTip.visible: hovered
            ToolTip.delay: 250
            ToolTip.timeout: 10000
            ToolTip.text: root.tooltip
        }

        Item {
            id: contentHolder
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
