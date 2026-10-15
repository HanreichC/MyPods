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
    readonly property int cornerRadius: gameScopeMode ? 0 : MP.Theme.radius
    // Pages span the whole window and slide under the glass sidebar; their content starts right of it.
    // contentInset and titleInset (clear of the window buttons) are read by ScrollPage
    readonly property int contentInset: sidebar.x + sidebar.width
    readonly property int titleInset: windowBar.visible ? Math.max(0, contentInset + (width - contentInset + Math.min(width - contentInset - MP.Units.hugeSpacing * 2, MP.Theme.maxContentWidth)) / 2
                                                                     - windowBar.x + MP.Units.mediumSpacing) : 0
    readonly property var sections: [
        { text: qsTrId("menu.battery"), icon: "icon-headphones-fill.svg" },
        { text: qsTrId("menu.headphones"), icon: "icon-bluetooth.svg" },
        { text: qsTrId("menu.settings"), icon: "icon-settings-fill.svg" },
        { text: qsTrId("menu.about"), icon: "icon-info-fill.svg" }
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

    // Floating Liquid Glass capsule (control layer): blurs what scrolls under it
    component GlassBar: Item {
        id: bar
        property real radius: height / 2

        RectangularShadow {
            anchors.fill: parent
            radius: bar.radius
            blur: 30
            offset.y: 8
            color: MP.Theme.shadow
        }

        ShaderEffectSource {
            id: backdrop
            anchors.fill: parent
            visible: false
            sourceItem: scene
            sourceRect: Qt.rect(bar.x, bar.y, bar.width, bar.height)
        }

        Rectangle {
            id: mask
            anchors.fill: parent
            radius: bar.radius
            visible: false
            layer.enabled: true
        }

        MultiEffect {
            anchors.fill: parent
            source: backdrop
            autoPaddingEnabled: false
            blurEnabled: true
            blur: 1.0
            blurMax: 48
            saturation: 0.4
            maskEnabled: true
            maskSource: mask
        }

        Rectangle {
            anchors.fill: parent
            radius: bar.radius
            border.width: 1
            border.color: MP.Theme.glassBorder
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.tint(MP.Theme.glassStrong, MP.Theme.glassSheen) }
                GradientStop { position: 0.5; color: MP.Theme.glassStrong }
            }
        }
    }

    component WindowButton: QQC2.AbstractButton {
        id: windowButton
        property bool danger: false
        implicitWidth: 44
        implicitHeight: 44
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

    // Navigation on the left like MyCAD's dashboard: the app, the headphones in use, the sections, the version
    GlassBar {
        id: sidebar
        x: 12
        y: 12
        width: 240
        height: parent.height - 24
        radius: 20

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            anchors.topMargin: 16
            anchors.bottomMargin: 16
            spacing: 2

            MP.Heading {
                Layout.leftMargin: 6
                Layout.bottomMargin: 12
                level: 2
                text: "MyPods"
            }

            // The headphones in use; a click opens their controls
            QQC2.AbstractButton {
                id: deviceButton
                readonly property var info: batteryPage.infoData
                readonly property bool hasInfo: batteryPage.hasInfo
                Layout.fillWidth: true
                Layout.bottomMargin: 14
                implicitHeight: Math.max(64, contentItem.implicitHeight + 20)
                Accessible.name: hasInfo ? (info?.name ?? "") : qsTrId("battery.connect_headphones.header")
                onClicked: swipeView.currentIndex = 0

                background: Rectangle {
                    radius: 14
                    color: deviceButton.pressed || deviceButton.hovered ? MP.Theme.fill : MP.Theme.tertiaryFill
                }
                contentItem: RowLayout {
                    spacing: 10

                    Item {
                        Layout.leftMargin: 8
                        implicitWidth: 44
                        implicitHeight: 44

                        Components.DeviceImage {
                            anchors.fill: parent
                            visible: deviceButton.hasInfo
                            sourceSize: Qt.size(88, 88)
                            info: deviceButton.info
                        }
                        Impl.IconImage {
                            anchors.centerIn: parent
                            visible: !deviceButton.hasInfo
                            sourceSize: Qt.size(26, 26)
                            source: MP.Theme.asset("icons/icon-headphones.svg")
                            color: MP.Theme.secondaryText
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.rightMargin: 8
                        spacing: 2

                        Text {
                            Layout.fillWidth: true
                            text: deviceButton.Accessible.name
                            color: MP.Theme.text
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap // long names on two lines rather than cut off
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: deviceButton.hasInfo
                            text: root.batteryText(deviceButton.info)
                            color: MP.Theme.secondaryText
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            Repeater {
                model: root.sections
                delegate: QQC2.AbstractButton {
                    id: navItem
                    required property var modelData
                    required property int index
                    readonly property bool current: swipeView.currentIndex === index
                    Layout.fillWidth: true
                    implicitHeight: 38
                    text: modelData.text
                    Accessible.role: Accessible.PageTab
                    Accessible.name: text
                    onClicked: swipeView.currentIndex = index

                    background: Rectangle {
                        radius: 10
                        color: navItem.current ? MP.Theme.fill : navItem.hovered ? MP.Theme.tertiaryFill : "transparent"
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }
                    contentItem: RowLayout {
                        spacing: 10

                        Impl.IconImage {
                            Layout.leftMargin: 10
                            sourceSize: Qt.size(18, 18)
                            source: MP.Theme.asset("icons/" + navItem.modelData.icon)
                            color: navItem.current ? MP.Theme.accent : MP.Theme.secondaryText
                        }
                        Text {
                            Layout.fillWidth: true
                            text: navItem.text
                            color: MP.Theme.text
                            font.pixelSize: 14
                            font.weight: navItem.current ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }

            Text {
                Layout.leftMargin: 6
                text: "MyPods " + Qt.application.version
                color: MP.Theme.secondaryText
                font.pixelSize: 11
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
                height: 44
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
