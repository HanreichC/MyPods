// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as Impl
import QtQuick.Layouts
import mypods as MP

// Inset-grouped list row: [icon tile] label [info] ........ control; the (i) stays with its label, like the Mac's
// Draws a separator above itself when a visible row precedes it in the same section.
Item {
    id: root

    property string label: ""
    // What the setting does, opened from the (i) beside the label
    property string info: ""
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
    implicitHeight: Math.max(40, row.implicitHeight + MP.Units.mediumSpacing)

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
        spacing: MP.Units.mediumSpacing + 2

        Rectangle {
            visible: root.iconSource.toString() !== ""
            implicitWidth: 26
            implicitHeight: 26
            radius: MP.Theme.smallRadius - 1
            color: root.iconColor

            Impl.IconImage {
                anchors.centerIn: parent
                sourceSize: Qt.size(16, 16)
                source: root.iconSource
                color: "white"
            }
        }

        Label {
            id: labelText
            Layout.fillWidth: true
            // as wide as its text, wrapped only when the row runs short; rounded up, as the layout rounds down
            Layout.maximumWidth: Math.ceil(implicitWidth)
            text: root.label.replace(/:\s*$/, "")
            color: MP.Theme.text
            font.pixelSize: MP.Theme.bodySize
            wrapMode: Text.Wrap // long labels and names wrap rather than being cut off
            maximumLineCount: 3
            elide: Text.ElideRight
            opacity: root.enabled ? 1 : 0.4
        }

        InfoButton {
            visible: root.info !== ""
            title: root.label
            info: root.info
        }

        Item { Layout.fillWidth: true }

        Item {
            id: contentHolder
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
