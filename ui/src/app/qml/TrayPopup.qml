// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Controls.impl as Impl
import QtQuick.Controls.Material
import QtQuick.Effects
import QtQuick.Layouts
import magicpods as MP
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
    property double hiddenAt: 0

    // room for the shadow around the panel; the panel itself sits just under the menu bar
    readonly property int shadowMargin: 20
    readonly property int topInset: 6

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
    onHasInfoChanged: if (!hasInfo) visible = false

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

    // ponytail: polls MPRIS and the volume once a second, only while the popup is open
    Timer {
        interval: 1000
        repeat: true
        running: popup.visible && !volumeSlider.pressed
        onTriggered: cppMedia.refresh()
    }

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

    RectangularShadow {
        anchors.fill: panel
        radius: panel.radius
        blur: 28
        offset.y: 8
        opacity: panel.opacity
        color: Qt.rgba(0, 0, 0, MP.Theme.dark ? 0.55 : 0.2)
    }

    Rectangle {
        id: panel
        x: popup.shadowMargin
        y: popup.topInset
        width: popup.width - popup.shadowMargin * 2
        height: content.implicitHeight + 28
        radius: 22
        color: MP.Theme.popup
        border.width: 1
        border.color: MP.Theme.dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.1)

        ColumnLayout {
            id: content
            x: 14
            y: 14
            width: parent.width - 28
            spacing: 12

            // Device: render, name and the battery rings
            RowLayout {
                Layout.fillWidth: true
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
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTrId("tray.popup.connected")
                        color: MP.Theme.secondaryText
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
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
                QQC2.Button {
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

            RowLayout {
                Layout.fillWidth: true
                visible: popup.conversationAwarenessData !== null

                Text {
                    Layout.fillWidth: true
                    text: qsTrId("battery.conversation_awareness")
                    color: MP.Theme.text
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
                Components.Toggle {
                    checked: popup.conversationAwarenessData?.selected ?? false
                    enabled: !(popup.conversationAwarenessData?.readonly ?? true)
                    Accessible.name: qsTrId("battery.conversation_awareness")
                    onToggled: cppBackend.setCapability("conversationAwareness", popup.infoData.address, checked)
                }
            }

            Components.Separator { Layout.fillWidth: true; visible: popup.hasPlayer }

            // Now playing
            RowLayout {
                Layout.fillWidth: true
                visible: popup.hasPlayer
                spacing: 10

                Rectangle {
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    radius: 8
                    color: MP.Theme.tertiaryFill
                    clip: true

                    Impl.IconImage {
                        anchors.centerIn: parent
                        visible: art.status !== Image.Ready
                        sourceSize: Qt.size(22, 22)
                        source: MP.Theme.asset("icons/icon-headphones.svg")
                        color: MP.Theme.secondaryText
                    }
                    Image {
                        id: art
                        anchors.fill: parent
                        source: popup.player.artUrl ?? ""
                        sourceSize: Qt.size(88, 88)
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        Layout.fillWidth: true
                        text: popup.player.title ?? ""
                        color: MP.Theme.text
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: text !== ""
                        text: popup.player.artist ?? ""
                        color: MP.Theme.secondaryText
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                Repeater {
                    model: [
                        { icon: "icon-backward.svg", name: qsTrId("tray.popup.previous"), enabled: popup.player.canGoPrevious ?? false, action: () => cppMedia.previous() },
                        { icon: popup.player.playing ? "icon-pause.svg" : "icon-play.svg", name: popup.player.playing ? qsTrId("tray.popup.pause") : qsTrId("tray.popup.play"), enabled: true, action: () => cppMedia.playPause() },
                        { icon: "icon-forward.svg", name: qsTrId("tray.popup.next"), enabled: popup.player.canGoNext ?? false, action: () => cppMedia.next() }
                    ]
                    delegate: QQC2.AbstractButton {
                        required property var modelData
                        implicitWidth: 30
                        implicitHeight: 30
                        enabled: modelData.enabled
                        opacity: enabled ? 1 : 0.35
                        Accessible.name: modelData.name
                        onClicked: modelData.action()
                        background: Rectangle {
                            radius: width / 2
                            color: parent.pressed ? MP.Theme.fill : parent.hovered ? MP.Theme.tertiaryFill : "transparent"
                        }
                        contentItem: Impl.IconImage {
                            sourceSize: Qt.size(18, 18)
                            source: MP.Theme.asset("icons/" + modelData.icon)
                            color: MP.Theme.text
                        }
                    }
                }
            }

            // Volume: Control Center style capsule, the fill is the value
            QQC2.Slider {
                id: volumeSlider
                Layout.fillWidth: true
                visible: cppMedia.volume >= 0
                implicitHeight: 26
                padding: 0
                from: 0
                to: 100
                stepSize: 1
                value: Math.max(0, cppMedia.volume)
                Accessible.name: qsTrId("tray.popup.volume")
                onMoved: cppMedia.setVolume(value)

                background: Rectangle {
                    radius: height / 2
                    color: MP.Theme.fill

                    Rectangle {
                        width: Math.max(parent.height, volumeSlider.visualPosition * parent.width)
                        height: parent.height
                        radius: height / 2
                        color: MP.Theme.accent
                    }
                    Impl.IconImage {
                        x: 6
                        anchors.verticalCenter: parent.verticalCenter
                        sourceSize: Qt.size(14, 14)
                        source: MP.Theme.asset(volumeSlider.value > 0 ? "icons/icon-speaker-wave.svg" : "icons/icon-speaker.svg")
                        color: "white"
                    }
                }
                handle: null
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
