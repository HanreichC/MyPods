// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Controls.impl as Impl
import QtQuick.Controls.Material
import QtQuick.Layouts
import mypods as MP

// HIG pop-up button for list rows: borderless, secondary value text, subtle fill on hover.
// Its menu is a glass panel with a checkmark on the current item.
QQC2.ComboBox {
    id: control
    flat: true
    // Sized to the longest option, so no language gets cut off
    implicitContentWidthPolicy: QQC2.ComboBox.WidestText
    implicitHeight: MP.Theme.sizeControlDesktop
    font: MP.Theme.fontBody
    opacity: enabled ? 1 : MP.Theme.opacityDisabled

    // The value in secondary text. Set on the text itself: since Qt 6.12 Material's ComboBox no longer takes
    // Material.foreground for it and drew white, invisible in light mode.
    Binding {
        target: control.contentItem
        property: "color"
        value: MP.Theme.labelSecondary
    }
    // next to the chevron, so the values of a group's pickers line up whatever their widths
    Binding {
        target: control.contentItem
        property: "horizontalAlignment"
        value: Text.AlignRight
    }

    // A new model (language switch, refreshed options) resets ComboBox to index 0 without
    // re-running the caller's currentIndex binding; overriding it for a moment re-applies it.
    Binding on currentIndex { id: reapplyIndex; when: false; value: -1 }
    onModelChanged: { reapplyIndex.when = true; reapplyIndex.when = false; }

    background: Rectangle {
        radius: MP.Theme.radiusControl
        color: control.hovered || control.pressed ? MP.Theme.fillTertiary : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: MP.Theme.focusRing
    }

    // macOS pop-up button: the up/down chevron instead of Material's triangle
    indicator: Impl.IconImage {
        x: control.width - width - MP.Theme.spaceS
        y: (control.height - height) / 2
        sourceSize: Qt.size(MP.Theme.sizeIconM, MP.Theme.sizeIconM)
        source: MP.Units.asset("icons/icon-chevrons.svg")
        color: MP.Theme.labelSecondary
    }
    rightPadding: 30

    delegate: QQC2.ItemDelegate {
        required property var modelData
        required property int index
        width: ListView.view.width
        height: MP.Theme.sizeControlDesktop
        padding: 0
        leftPadding: MP.Theme.spaceS
        rightPadding: MP.Theme.spaceM
        highlighted: control.highlightedIndex === index

        contentItem: RowLayout {
            spacing: 6

            Impl.IconImage {
                Layout.preferredWidth: MP.Theme.sizeIconM
                Layout.preferredHeight: MP.Theme.sizeIconM
                sourceSize: Qt.size(MP.Theme.sizeIconM, MP.Theme.sizeIconM)
                source: MP.Units.asset("icons/icon-check.svg")
                color: MP.Theme.accentText
                opacity: control.currentIndex === index ? 1 : 0
            }
            Text {
                Layout.fillWidth: true
                text: modelData
                color: MP.Theme.labelPrimary
                font: MP.Theme.fontBody
                elide: Text.ElideRight
            }
        }
        background: Rectangle {
            radius: MP.Theme.radiusControl // concentric in the menu's tile radius at its 4 px padding
            color: parent.highlighted || parent.hovered ? MP.Theme.fillTertiary : "transparent"
        }
    }

    popup.padding: MP.Theme.spaceXs
    popup.background: MP.Glass { radius: MP.Theme.radiusTile; blurSource: QQC2.ApplicationWindow.window?.contentItem ?? null; textSurface: true; thickness: "thick" }
    // GM-1: the glass materializes, 0.94 -> 1 (a plain fade with Reduce Motion)
    popup.enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: MP.Theme.durationFade; easing.type: MP.Theme.easeStandard }
        NumberAnimation { property: "scale"; from: MP.Theme.reduceMotion ? 1 : 0.94; to: 1; duration: MP.Theme.durationSpring; easing.type: MP.Theme.easeSpring }
    }
    popup.exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: MP.Theme.durationHover; easing.type: MP.Theme.easeExit }
    }
}
