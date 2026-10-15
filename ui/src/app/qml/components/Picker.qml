// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Controls.impl as Impl
import QtQuick.Controls.Material
import QtQuick.Layouts
import magicpods as MP

// HIG pop-up button for list rows: borderless, secondary value text, subtle fill on hover.
// Its menu is a glass panel with a checkmark on the current item.
QQC2.ComboBox {
    id: control
    flat: true
    // Sized to the longest option, so no language gets cut off
    implicitContentWidthPolicy: QQC2.ComboBox.WidestText
    implicitHeight: 30
    font.pixelSize: MP.Theme.bodySize
    Material.foreground: MP.Theme.secondaryText
    opacity: enabled ? 1 : 0.4

    // A new model (language switch, refreshed options) resets ComboBox to index 0 without
    // re-running the caller's currentIndex binding; overriding it for a moment re-applies it.
    Binding on currentIndex { id: reapplyIndex; when: false; value: -1 }
    onModelChanged: { reapplyIndex.when = true; reapplyIndex.when = false; }

    background: Rectangle {
        radius: MP.Theme.smallRadius
        color: control.hovered || control.pressed ? MP.Theme.tertiaryFill : "transparent"
    }

    // macOS pop-up button: the up/down chevron instead of Material's triangle
    indicator: Impl.IconImage {
        x: control.width - width - 8
        y: (control.height - height) / 2
        sourceSize: Qt.size(16, 16)
        source: MP.Theme.asset("icons/icon-chevrons.svg")
        color: MP.Theme.secondaryText
    }
    rightPadding: 30

    delegate: QQC2.ItemDelegate {
        required property var modelData
        required property int index
        width: ListView.view.width
        height: 30
        padding: 0
        leftPadding: 8
        rightPadding: 12
        highlighted: control.highlightedIndex === index

        contentItem: RowLayout {
            spacing: 6

            Impl.IconImage {
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                sourceSize: Qt.size(16, 16)
                source: MP.Theme.asset("icons/icon-check.svg")
                color: MP.Theme.accent
                opacity: control.currentIndex === index ? 1 : 0
            }
            Text {
                Layout.fillWidth: true
                text: modelData
                color: MP.Theme.text
                font.family: control.font.family
                font.pixelSize: MP.Theme.bodySize
                elide: Text.ElideRight
            }
        }
        background: Rectangle {
            radius: MP.Theme.smallRadius // concentric in the menu's tile radius at its 4 px padding
            color: parent.highlighted || parent.hovered ? MP.Theme.tertiaryFill : "transparent"
        }
    }

    popup.padding: 4
    popup.background: MP.Glass { radius: MP.Theme.tileRadius; blurSource: QQC2.ApplicationWindow.window?.contentItem ?? null; tint: MP.Theme.dark ? 0.72 : 0.78 }
    popup.enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.94; to: 1; duration: 200; easing.type: Easing.OutBack }
    }
    popup.exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: 100 }
    }
}
