// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import mypods as MP

// (i) beside a setting: a click (or Space, Enter) opens what it does in a few sentences. A popup rather than a
// hover tooltip: the texts are long, and a tooltip can't be reached by keyboard or touch.
ToolButton {
    id: root

    property string title: ""
    property string info: ""

    implicitWidth: MP.Theme.sizeHitMinDesktop
    implicitHeight: MP.Theme.sizeHitMinDesktop
    padding: MP.Theme.spaceXs
    display: AbstractButton.IconOnly
    icon.source: MP.Units.asset("icons/icon-info.svg")
    icon.color: MP.Theme.accentText
    icon.width: MP.Theme.sizeIconM
    icon.height: MP.Theme.sizeIconM
    focusPolicy: Qt.StrongFocus
    Accessible.name: qsTrId("info.about").arg(title)
    Accessible.description: info

    onClicked: dialog.open()

    // MyCAD's glass alert: no frame, the title bold above the text, a capsule button; it grows in from the middle
    Popup {
        id: dialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(360, (parent?.width ?? 412) - 32)
        height: Math.min(implicitHeight, (parent?.height ?? 600) - 32)
        modal: true
        focus: true
        padding: 22
        topPadding: 20
        bottomPadding: 18
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Overlay.modal: Rectangle { color: MP.Theme.scrim }
        // Reduce Motion: popupFromScale is 1, a plain fade
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: MP.Theme.durationFade; easing.type: MP.Theme.easeStandard }
            NumberAnimation { property: "scale"; from: MP.Theme.popupFromScale; to: 1; duration: MP.Theme.durationSpring; easing.type: MP.Theme.easeSpring; easing.overshoot: MP.Theme.springOvershoot }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; to: 0; duration: MP.Theme.durationHover; easing.type: MP.Theme.easeExit }
            NumberAnimation { property: "scale"; to: MP.Theme.reduceMotion ? 1 : 0.96; duration: MP.Theme.durationHover; easing.type: MP.Theme.easeExit }
        }

        // blurs the window's content behind it (the overlay it lies in is not part of that content)
        background: MP.Glass { blurSource: ApplicationWindow.window?.contentItem ?? null; textSurface: true; thickness: "thick" }

        contentItem: ColumnLayout {
            spacing: MP.Theme.spaceL

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
                        text: root.title
                        color: MP.Theme.labelPrimary
                        font: MP.Theme.fontHeadline
                        wrapMode: Text.Wrap
                        Accessible.role: Accessible.Heading
                    }
                    Label {
                        width: parent.width
                        text: root.info
                        textFormat: Text.PlainText
                        color: MP.Theme.labelSecondary
                        font: MP.Theme.fontCallout
                        lineHeight: MP.Theme.lineCallout
                        lineHeightMode: Text.FixedHeight
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
