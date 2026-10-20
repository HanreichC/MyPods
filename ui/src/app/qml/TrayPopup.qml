// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Controls.impl as Impl
import QtQuick.Controls.Material
import QtQuick.Effects
import QtQuick.Layouts
import mypods as MP
import "components" as Components

// Menu bar extra: opens under the tray icon on a single click, like the AirPods module in the
// Mac's Control Center. Placement is done by main.cpp (layer-shell on Wayland, cursor on X11).
QQC2.ApplicationWindow {
    id: popup

    signal openAppRequested()

    property var infoData: ({})
    readonly property bool hasInfo: infoData && Object.keys(infoData).length > 0
    readonly property var capabilities: infoData?.capabilities ?? null
    readonly property var battery: capabilities?.battery ?? null
    readonly property var autoSwitchData: capabilities?.autoSwitch ?? null
    readonly property var conversationAwarenessData: capabilities?.conversationAwareness ?? null
    readonly property var player: cppMedia.player
    readonly property bool hasPlayer: Object.keys(player).length > 0
    // the music on the iPhone opens the popup even without headphones here
    readonly property bool canShow: hasInfo || !!player.source
    property double hiddenAt: 0

    // room for the shadow around the panel; the panel itself sits just under the menu bar
    readonly property int shadowMargin: 28
    readonly property int topInset: 6
    // KDE blurs the desktop behind the panel (main.cpp, BlurBehind.h): where, and whether it does
    readonly property rect glassRect: Qt.rect(panel.x, panel.y, panel.width, panel.height)
    readonly property real glassRadius: MP.Theme.radius
    property bool desktopBlur: false

    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    font.family: "Inter"
    Material.theme: MP.Theme.dark ? Material.Dark : Material.Light
    Material.accent: MP.Theme.accent
    width: 320 + shadowMargin * 2
    height: panel.height + topInset + shadowMargin
    visible: false

    function toggle() {
        if (visible) {
            visible = false;
            return;
        }
        // the click on the tray icon has just taken the focus away and closed us
        if (Date.now() - hiddenAt < 300)
            return;
        cppMedia.refresh();
        panel.opacity = 0;
        show();
        raise();
        requestActivate();
        fadeIn.restart();
    }

    onVisibleChanged: if (!visible) hiddenAt = Date.now()
    // HIG: a menu bar extra closes as soon as the person clicks elsewhere
    onActiveChanged: if (!active) visible = false
    onCanShowChanged: if (!canShow) visible = false

    Connections {
        target: cppBackend
        function onDataReceived(json) {
            if (!json || Object.keys(json).length === 0)
                popup.infoData = ({});
            else if (json.info)
                popup.infoData = json.info;
        }
    }
    Component.onCompleted: if (cppBackend.connected) cppBackend.getInfo()

    Shortcut {
        sequence: "Esc"
        onActivated: popup.visible = false
    }

    NumberAnimation {
        id: fadeIn
        target: panel
        property: "opacity"
        from: 0
        to: 1
        duration: 140
        easing.type: Easing.OutCubic
    }

    // A module of the popup like in the Mac's Control Center: a quiet tile with rounded corners
    component Tile: Rectangle {
        default property alias content: tileColumn.data
        Layout.fillWidth: true
        implicitHeight: tileColumn.implicitHeight + 24
        radius: MP.Theme.tileRadius
        color: MP.Theme.tertiaryFill

        ColumnLayout {
            id: tileColumn
            x: 12
            y: 12
            width: parent.width - 24
            spacing: 10
        }
    }

    Item {
        id: panel
        x: popup.shadowMargin
        y: popup.topInset
        width: popup.width - popup.shadowMargin * 2
        height: content.implicitHeight + 28

        MP.Glass {
            anchors.fill: parent
            radius: popup.glassRadius
            translucent: popup.desktopBlur
        }

        ColumnLayout {
            id: content
            x: 14
            y: 14
            width: parent.width - 28
            spacing: 12

            // Device: render, name and the battery rings
            RowLayout {
                Layout.fillWidth: true
                visible: popup.hasInfo
                spacing: 12

                Components.DeviceImage {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    sourceSize: Qt.size(96, 96)
                    info: popup.infoData
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        Layout.fillWidth: true
                        text: popup.infoData?.name ?? ""
                        color: MP.Theme.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap // long names on two lines rather than cut off
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: !popup.infoData?.wired ? qsTrId("tray.popup.connected")
                              : popup.infoData.wired === "usb" ? qsTrId("battery.wired.usb") : qsTrId("battery.wired.jack")
                        color: MP.Theme.secondaryText
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                visible: popup.hasInfo
                spacing: MP.Units.largeSpacing

                Repeater {
                    model: [
                        { key: "single", name: qsTrId("battery.battery_single") },
                        { key: "left", name: qsTrId("battery.battery_left") },
                        { key: "right", name: qsTrId("battery.battery_right") },
                        { key: "case", name: qsTrId("battery.battery_case") }
                    ]
                    delegate: Components.BatteryRing {
                        required property var modelData
                        size: 44
                        name: modelData.name
                        battery: popup.battery?.[modelData.key]?.battery ?? 0
                        isCharging: popup.battery?.[modelData.key]?.charging ?? false
                        status: popup.battery?.[modelData.key]?.status ?? 0
                    }
                }
            }

            // Like the Mac's "Moved to iPhone" banner, with the way back
            RowLayout {
                Layout.fillWidth: true
                visible: popup.autoSwitchData !== null && !(popup.autoSwitchData?.owns ?? true)
                spacing: MP.Units.mediumSpacing

                Text {
                    Layout.fillWidth: true
                    text: popup.autoSwitchData?.source ? qsTrId("battery.moved").arg(popup.autoSwitchData.source) : qsTrId("battery.moved_other")
                    color: MP.Theme.text
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                }
                MP.Button {
                    text: qsTrId("battery.move_here")
                    highlighted: true
                    font.pixelSize: 13
                    onClicked: cppBackend.setCapability("autoSwitch", popup.infoData.address, true, "takeover")
                }
            }

            Components.NoiseControl {
                Layout.fillWidth: true
                ancData: popup.capabilities?.anc ?? null
                address: popup.infoData?.address ?? ""
            }

            Tile {
                visible: popup.conversationAwarenessData !== null

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        Layout.fillWidth: true
                        text: qsTrId("battery.conversation_awareness").replace(/:\s*$/, "") // a label, not a form field
                        color: MP.Theme.text
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                    }
                    Components.Toggle {
                        checked: popup.conversationAwarenessData?.selected ?? false
                        enabled: !(popup.conversationAwarenessData?.readonly ?? true)
                        Accessible.name: qsTrId("battery.conversation_awareness")
                        onToggled: cppBackend.setCapability("conversationAwareness", popup.infoData.address, checked)
                    }
                }
            }

            // Now playing, like the Mac's Control Center: cover and text across the width, the controls and the volume under them
            Tile {
                visible: nowPlaying.shown

                Components.NowPlaying {
                    id: nowPlaying
                    Layout.fillWidth: true
                }
            }

            Components.Separator { Layout.fillWidth: true }

            // Menu item, HIG: the ellipsis says it opens a window
            QQC2.AbstractButton {
                Layout.fillWidth: true
                Layout.topMargin: -4
                Layout.bottomMargin: -6
                implicitHeight: 30
                Accessible.name: contentItem.text
                onClicked: {
                    popup.visible = false;
                    popup.openAppRequested();
                }
                background: Rectangle {
                    radius: 8
                    color: parent.hovered ? MP.Theme.tertiaryFill : "transparent"
                }
                contentItem: Text {
                    leftPadding: 8
                    verticalAlignment: Text.AlignVCenter
                    text: qsTrId("tray.popup.open_app")
                    color: MP.Theme.text
                    font.pixelSize: 13
                }
            }
        }
    }
}
