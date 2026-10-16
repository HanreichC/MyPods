// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Controls.impl as Impl
import QtQuick.Controls.Material
import QtQuick.Effects
import QtQuick.Layouts
import "pages"
import "components" as Components
import magicpods as MP

QQC2.ApplicationWindow {
    id: root
    // Frameless like MyCAD: the window buttons sit in a glass capsule; GameScope runs it fullscreen anyway
    flags: gameScopeMode ? Qt.Window : Qt.Window | Qt.FramelessWindowHint
    minimumWidth: gameScopeMode ? 0 : 720
    minimumHeight: gameScopeMode ? 0 : 480
    width: gameScopeMode ? Screen.width : 800
    height: gameScopeMode ? Screen.height : 720
    // Nur visibility setzen: zusammen mit visible gewinnt visibility und --hidden waere wirkungslos
    visibility: startHidden ? Window.Hidden : gameScopeMode ? Window.FullScreen : Window.Windowed
    // Rounded like the popups: the corners outside the backdrop stay see-through
    color: gameScopeMode ? MP.Theme.base : "transparent"
    // Material hard-codes Roboto/Noto for controls; the window font overrides it for every control
    font.family: "Inter"

    Material.theme: MP.Theme.dark ? Material.Dark : Material.Light
    Material.accent: MP.Theme.accent
    Material.primary: MP.Theme.accent

    property alias currentIndex: swipeView.currentIndex
    readonly property int cornerRadius: gameScopeMode ? 0 : MP.Theme.windowRadius
    // Pages span the whole window and slide under the glass sidebar; their content starts right of it.
    // contentInset and titleInset (clear of the window buttons) are read by ScrollPage
    readonly property int contentInset: sidebar.x + sidebar.width
    readonly property int titleInset: windowBar.visible ? Math.max(0, contentInset + (width - contentInset + Math.min(width - contentInset - MP.Units.hugeSpacing * 2, MP.Theme.maxContentWidth)) / 2
                                                                     - windowBar.x + MP.Units.mediumSpacing) : 0
    readonly property var sections: [
        { page: 1, text: qsTrId("menu.headphones"), icon: "icon-bluetooth.svg" },
        { page: 2, text: qsTrId("menu.settings"), icon: "icon-settings-fill.svg" },
        { page: 3, text: qsTrId("menu.about"), icon: "icon-info-fill.svg" }
    ]

    // "Links 80 % · Rechts 70 % · Case 40 %" for the sidebar, wired headphones say how they are connected
    function batteryText(info) {
        const battery = info?.capabilities?.battery;
        const parts = [
            { part: battery?.single, name: qsTrId("battery.battery_single") },
            { part: battery?.left, name: qsTrId("battery.battery_left") },
            { part: battery?.right, name: qsTrId("battery.battery_right") },
            { part: battery?.case, name: qsTrId("battery.battery_case") }
        ].filter(p => p.part && p.part.status !== 0 && p.part.status !== 1)
         .map(p => p.name + " " + qsTrId("format.percent").arg(p.part.battery));
        if (parts.length > 0)
            return parts.join(" · ");
        if (info?.wired)
            return info.wired === "usb" ? qsTrId("battery.wired.usb") : qsTrId("battery.wired.jack");
        return qsTrId("tray.popup.connected");
    }

    // With a tray the window just hides; without one there would be no way to quit or reopen it.
    onClosing: (event) => {
        if (!trayAvailable) {
            Qt.quit();
            return;
        }
        event.accepted = false;
        root.visible = false;
    }

    // Floating Liquid Glass (control layer): MyCAD's clear glass over what scrolls or slides under it
    component GlassBar: MP.Glass {
        blurSource: scene
        radius: height / 2
    }

    component WindowButton: QQC2.AbstractButton {
        id: windowButton
        property bool danger: false
        implicitWidth: 36
        implicitHeight: 36
        focusPolicy: Qt.NoFocus
        Accessible.name: text
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: 600
        QQC2.ToolTip.text: text
        background: Rectangle {
            radius: height / 2
            color: windowButton.danger && windowButton.hovered ? (windowButton.pressed ? Qt.darker(MP.Theme.closeHover, 1.15) : MP.Theme.closeHover)
                   : windowButton.pressed ? MP.Theme.fill : windowButton.hovered ? MP.Theme.tertiaryFill : "transparent"
            Behavior on color { ColorAnimation { duration: 100 } }
        }
        contentItem: Impl.IconImage {
            sourceSize: Qt.size(18, 18)
            source: windowButton.icon.source
            color: windowButton.danger && windowButton.hovered ? "white" : MP.Theme.text
        }
    }

    // Everything the window buttons' glass blurs lives in here.
    Item {
        id: scene
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            radius: root.cornerRadius
            gradient: Gradient {
                GradientStop { position: 0; color: MP.Theme.base }
                GradientStop { position: 1; color: MP.Theme.baseBottom }
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            QQC2.ProgressBar {
                visible: !(cppBackend?.connected ?? false) && !(cppBackend?.unsupportedApi ?? false)
                indeterminate: true
                Layout.fillWidth: true
            }

            Components.InlineMessage {
                Layout.margins: MP.Units.largeSpacing
                Layout.leftMargin: root.contentInset + MP.Units.largeSpacing
                Layout.topMargin: 12 + MP.Theme.barHeight + MP.Units.largeSpacing // under the window buttons
                visible: cppBackend?.unsupportedApi ?? false
                type: 2
                text: qsTrId("Error.service_api_wrong")
            }

            QQC2.SwipeView {
                id: swipeView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                // HIG soft scroll edge effect: content fades out where it leaves at the top,
                // and stays inside the window's rounded corners when a page slides past them.
                // effects need a shader-capable scene graph; the software renderer drops the item
                layer.enabled: GraphicsInfo.api !== GraphicsInfo.Software
                layer.effect: MultiEffect {
                    maskEnabled: true
                    maskSource: scrollEdgeMask
                    maskSpreadAtMin: 1.0
                    maskThresholdMin: 0.0
                }

                BatteryPage { id: batteryPage }
                HeadphonesPage {}
                SettingsPage {}
                AboutPage {}
            }
        }
    }

    Rectangle {
        id: scrollEdgeMask
        width: swipeView.width
        height: swipeView.height
        visible: false
        layer.enabled: true
        radius: root.cornerRadius
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.15) }
            GradientStop { position: Math.min(0.5, 14 / scrollEdgeMask.height); color: "black" }
            GradientStop { position: 1; color: "black" }
        }
    }

    // A row in the sidebar: the headphones with their picture, the sections with their symbol; a second line
    // below the name, highlighted while its page is open
    component SidebarItem: QQC2.AbstractButton {
        id: sidebarItem
        property bool current: false
        property string detail: ""
        property var device: null
        property bool output: false // the system plays to these headphones
        Layout.fillWidth: true
        implicitHeight: Math.max(device ? 48 : 32, contentItem.implicitHeight + 12)
        Accessible.role: Accessible.PageTab
        Accessible.name: text
        Accessible.description: output ? qsTrId("sidebar.output") : ""

        background: Rectangle {
            radius: MP.Theme.tileRadius
            color: sidebarItem.current ? MP.Theme.fill : sidebarItem.hovered ? MP.Theme.tertiaryFill : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }
        }
        contentItem: RowLayout {
            spacing: 10

            Item {
                Layout.leftMargin: sidebarItem.device ? 6 : 10
                implicitWidth: sidebarItem.device ? 34 : 16
                implicitHeight: implicitWidth

                Components.DeviceImage {
                    anchors.fill: parent
                    visible: !!sidebarItem.device
                    sourceSize: Qt.size(68, 68)
                    info: sidebarItem.device ?? ({})
                }
                Impl.IconImage {
                    anchors.centerIn: parent
                    visible: !sidebarItem.device
                    sourceSize: Qt.size(16, 16)
                    source: sidebarItem.icon.source
                    color: sidebarItem.current ? MP.Theme.accent : MP.Theme.secondaryText
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.rightMargin: 8
                spacing: 1

                Text {
                    Layout.fillWidth: true
                    text: sidebarItem.text
                    color: MP.Theme.text
                    font.pixelSize: 13
                    font.weight: sidebarItem.current || sidebarItem.device ? Font.DemiBold : Font.Normal
                    wrapMode: Text.Wrap // long names on two lines rather than cut off
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: sidebarItem.detail
                    color: MP.Theme.secondaryText
                    font.pixelSize: MP.Theme.sectionSize
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
            }

            Impl.IconImage {
                visible: sidebarItem.output
                Layout.rightMargin: 8
                sourceSize: Qt.size(14, 14)
                source: MP.Theme.asset("icons/icon-speaker-wave.svg")
                color: MP.Theme.accent
            }
        }
    }

    // Navigation on the left like MyCAD's dashboard and the Mac's System Settings: the headphones that are
    // connected (a click makes them the active ones, plays the sound there and opens their controls), the sections, the version
    GlassBar {
        id: sidebar
        // the headphones to list: the connected ones, or the active ones before the list has come
        readonly property var devices: batteryPage.connectedHeadphones.length > 0 ? batteryPage.connectedHeadphones
                                       : batteryPage.hasInfo ? [batteryPage.infoData] : []
        x: 12
        y: 12
        width: 232
        height: parent.height - 24
        radius: MP.Theme.radius

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            anchors.topMargin: 16
            anchors.bottomMargin: 14
            spacing: 2

            MP.Heading {
                Layout.leftMargin: 10
                Layout.bottomMargin: 14
                level: 2
                text: "MyPods"
            }

            MP.Heading {
                Layout.leftMargin: 10
                Layout.bottomMargin: 4
                level: 5
                text: qsTrId("headphones.headphones")
            }

            Repeater {
                model: sidebar.devices
                delegate: SidebarItem {
                    required property var modelData
                    readonly property bool active: modelData.address === batteryPage.currentAddress()
                    device: modelData
                    output: modelData.output ?? false
                    text: modelData.name ?? ""
                    detail: active ? root.batteryText(batteryPage.infoData) : qsTrId("tray.popup.connected")
                    current: active && swipeView.currentIndex === 0
                    onClicked: {
                        // also when they are the active ones already: the sound may play elsewhere
                        cppBackend.setActiveDevice(modelData.address);
                        swipeView.currentIndex = 0;
                    }
                }
            }

            // nothing connected: the way to the help on connecting
            SidebarItem {
                visible: sidebar.devices.length === 0
                text: qsTrId("battery.connect_headphones.header")
                icon.source: MP.Theme.asset("icons/icon-headphones.svg")
                current: swipeView.currentIndex === 0
                onClicked: swipeView.currentIndex = 0
            }

            Item { implicitHeight: 14 }

            Repeater {
                model: root.sections
                delegate: SidebarItem {
                    required property var modelData
                    text: modelData.text
                    icon.source: MP.Theme.asset("icons/" + modelData.icon)
                    current: swipeView.currentIndex === modelData.page
                    onClicked: swipeView.currentIndex = modelData.page
                }
            }

            Item { Layout.fillHeight: true }

            Text {
                Layout.leftMargin: 10
                text: "MyPods " + Qt.application.version
                color: MP.Theme.secondaryText
                font.pixelSize: MP.Theme.sectionSize
            }
        }
    }

    // A fine edge around the rounded window, like the popups'
    Rectangle {
        anchors.fill: parent
        visible: !gameScopeMode
        radius: root.cornerRadius
        color: "transparent"
        border.width: 1
        border.color: MP.Theme.popupBorder
    }

    // Window buttons in a glass capsule at the top right, as in MyCAD: grip to move, minimize, close
    GlassBar {
        id: windowBar
        visible: !gameScopeMode
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 12
        width: windowRow.implicitWidth + 12
        height: MP.Theme.barHeight

        DragHandler {
            target: null
            onActiveChanged: if (active) root.startSystemMove()
        }

        Row {
            id: windowRow
            anchors.centerIn: parent

            Item {
                width: 30
                height: 36
                Accessible.role: Accessible.Grip
                Accessible.name: qsTrId("window.move")

                Impl.IconImage {
                    anchors.centerIn: parent
                    sourceSize: Qt.size(18, 18)
                    source: MP.Theme.asset("icons/icon-grip.svg")
                    color: MP.Theme.secondaryText
                }
                HoverHandler { cursorShape: Qt.OpenHandCursor }
            }
            WindowButton {
                text: qsTrId("window.minimize")
                icon.source: MP.Theme.asset("icons/icon-minus.svg")
                onClicked: root.showMinimized()
            }
            WindowButton {
                danger: true
                text: qsTrId("window.close")
                icon.source: MP.Theme.asset("icons/icon-close.svg")
                onClicked: root.close()
            }
        }
    }

    // Frameless: the edges still resize the window
    Repeater {
        model: gameScopeMode ? [] : [Qt.TopEdge, Qt.BottomEdge, Qt.LeftEdge, Qt.RightEdge]
        delegate: MouseArea {
            required property int modelData
            readonly property bool vertical: modelData === Qt.TopEdge || modelData === Qt.BottomEdge
            x: modelData === Qt.RightEdge ? root.width - width : 0
            y: modelData === Qt.BottomEdge ? root.height - height : 0
            width: vertical ? root.width : 5
            height: vertical ? 5 : root.height
            cursorShape: vertical ? Qt.SizeVerCursor : Qt.SizeHorCursor
            onPressed: root.startSystemResize(modelData)
        }
    }
}
