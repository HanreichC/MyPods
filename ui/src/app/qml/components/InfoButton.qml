// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Controls
import magicpods as MP

// (i) beside a setting: a click (or Space, Enter) opens what it does in a few sentences. A popup rather than a
// hover tooltip: the texts are long, and a tooltip can't be reached by keyboard or touch.
ToolButton {
    id: root

    property string title: ""
    property string info: ""

    implicitWidth: 28
    implicitHeight: 28
    padding: 4
    display: AbstractButton.IconOnly
    icon.source: MP.Theme.asset("icons/icon-info.svg")
    icon.color: MP.Theme.accent
    icon.width: 20
    icon.height: 20
    focusPolicy: Qt.StrongFocus
    Accessible.name: qsTrId("info.about").arg(title)
    Accessible.description: info

    onClicked: dialog.open()

    Popup {
        id: dialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(400, (parent?.width ?? 432) - 32)
        height: Math.min(implicitHeight, (parent?.height ?? 600) - 32)
        modal: true
        focus: true
        padding: 20
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 14
            color: MP.Theme.menu
            border.width: 1
            border.color: MP.Theme.separator
        }

        contentItem: Flickable {
            implicitHeight: column.implicitHeight
            contentHeight: column.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: column
                width: parent.width
                spacing: 8

                Label {
                    width: parent.width
                    text: root.title.replace(/:\s*$/, "")
                    color: MP.Theme.text
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    Accessible.role: Accessible.Heading
                }
                Label {
                    width: parent.width
                    text: root.info
                    textFormat: Text.PlainText
                    color: MP.Theme.text
                    font.pixelSize: 14
                    lineHeight: 1.15
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
