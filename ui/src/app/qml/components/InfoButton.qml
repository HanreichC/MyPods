// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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

    // MyCAD's glass alert: no frame, the title bold above the text, a capsule button; it grows in from the middle
    Popup {
        id: dialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(380, (parent?.width ?? 412) - 32)
        height: Math.min(implicitHeight, (parent?.height ?? 600) - 32)
        modal: true
        focus: true
        padding: 22
        topPadding: 20
        bottomPadding: 18
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, MP.Theme.dark ? 0.35 : 0.14) }
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 150; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.9; to: 1; duration: 280; easing.type: Easing.OutBack; easing.overshoot: 1.3 }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; to: 0; duration: 120; easing.type: Easing.InCubic }
            NumberAnimation { property: "scale"; to: 0.96; duration: 120; easing.type: Easing.InCubic }
        }

        background: MP.Glass {}

        contentItem: ColumnLayout {
            spacing: 16

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: column.implicitHeight
                Layout.minimumHeight: 40
                contentHeight: column.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: MP.ScrollBar {}

                Column {
                    id: column
                    width: parent.width
                    spacing: 6

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
                        color: MP.Theme.secondaryText
                        font.pixelSize: 14
                        lineHeight: 1.2
                        wrapMode: Text.Wrap
                    }
                }
            }

            MP.Button {
                Layout.fillWidth: true
                highlighted: true
                text: qsTrId("info.done")
                onClicked: dialog.close()
            }
        }
    }
}
