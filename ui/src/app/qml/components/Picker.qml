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
    implicitHeight: 36
    font.pixelSize: 17
    Material.foreground: MP.Theme.secondaryText
    opacity: enabled ? 1 : 0.4

    // A new model (language switch, refreshed options) resets ComboBox to index 0 without
    // re-running the caller's currentIndex binding; overriding it for a moment re-applies it.
    Binding on currentIndex { id: reapplyIndex; when: false; value: -1 }
    onModelChanged: { reapplyIndex.when = true; reapplyIndex.when = false; }

    background: Rectangle {
        radius: 10
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
        height: 40
        padding: 0
        leftPadding: 8
        rightPadding: 12
        highlighted: control.highlightedIndex === index

        contentItem: RowLayout {
            spacing: 6

            Impl.IconImage {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                sourceSize: Qt.size(18, 18)
                source: MP.Theme.asset("icons/icon-check.svg")
                color: MP.Theme.accent
                opacity: control.currentIndex === index ? 1 : 0
            }
            Text {
                Layout.fillWidth: true
                text: modelData
                color: MP.Theme.text
                font.family: control.font.family
                font.pixelSize: 15
                elide: Text.ElideRight
            }
        }
        background: Rectangle {
            radius: 8
            color: parent.highlighted || parent.hovered ? MP.Theme.tertiaryFill : "transparent"
        }
    }

    popup.padding: 5
    popup.background: MP.Glass { radius: 14; blurSource: QQC2.ApplicationWindow.window?.contentItem ?? null }
    popup.enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.94; to: 1; duration: 200; easing.type: Easing.OutBack }
    }
    popup.exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: 100 }
    }
}
