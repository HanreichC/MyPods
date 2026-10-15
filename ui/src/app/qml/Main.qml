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
    minimumWidth: gameScopeMode ? 0 : 560
    maximumWidth: gameScopeMode ? Screen.width : 560
    minimumHeight: gameScopeMode ? 0 : 420
    width: gameScopeMode ? Screen.width : 560
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

        RectangularShadow {
            anchors.fill: parent
            radius: height / 2
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
            radius: height / 2
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
            radius: height / 2
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

    // Everything the glass bars blur lives in here.
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
                Layout.topMargin: 12 + MP.Theme.barHeight + MP.Units.largeSpacing // under the bars
                visible: cppBackend?.unsupportedApi ?? false
                type: 2
                text: qsTrId("Error.service_api_wrong")
            }

            QQC2.SwipeView {
                id: swipeView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                // HIG soft scroll edge effect: content fades out where it passes under the floating glass,
                // and stays inside the window's rounded corners when a page slides past them.
                // effects need a shader-capable scene graph; the software renderer drops the item
                layer.enabled: GraphicsInfo.api !== GraphicsInfo.Software
                layer.effect: MultiEffect {
                    maskEnabled: true
                    maskSource: scrollEdgeMask
                    maskSpreadAtMin: 1.0
                    maskThresholdMin: 0.0
                }

                BatteryPage {}
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
            GradientStop { position: Math.min(0.5, (12 + MP.Theme.barHeight) / scrollEdgeMask.height); color: "black" }
            GradientStop { position: 1; color: "black" }
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

    // The bars at the top like MyCAD's toolbar and window buttons, equally tall: the tabs on the left,
    // the window buttons (grip to move, minimize, close) on the right
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

    // Floating Liquid Glass tab bar (control layer)
    GlassBar {
        id: tabBar
        readonly property var tabs: [
            { text: qsTrId("menu.battery"), icon: "icon-headphones-fill.svg" },
            { text: qsTrId("menu.headphones"), icon: "icon-bluetooth.svg" },
            { text: qsTrId("menu.settings"), icon: "icon-settings-fill.svg" },
            { text: qsTrId("menu.about"), icon: "icon-info-fill.svg" }
        ]
        height: MP.Theme.barHeight
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: windowBar.visible ? windowBar.left : parent.right
        anchors.margins: 12

        // Sliding selection pill
        Rectangle {
            readonly property Item target: tabRepeater.count, tabRepeater.itemAt(swipeView.currentIndex)
            x: tabRow.x + (target?.x ?? 0)
            y: tabRow.y
            width: target?.width ?? 0
            height: tabRow.height
            radius: height / 2
            color: MP.Theme.fill
            Behavior on x { NumberAnimation { duration: 380; easing.type: Easing.OutBack; easing.overshoot: 1.2 } }
        }

        Row {
            id: tabRow
            anchors.fill: parent
            anchors.margins: 6

            Repeater {
                id: tabRepeater
                model: tabBar.tabs
                delegate: QQC2.TabButton {
                    required property var modelData
                    required property int index
                    readonly property bool current: swipeView.currentIndex === index
                    width: tabRow.width / tabBar.tabs.length
                    height: tabRow.height
                    padding: 0
                    text: modelData.text
                    display: QQC2.AbstractButton.TextUnderIcon
                    spacing: 1
                    font.pixelSize: 10
                    font.weight: current ? Font.DemiBold : Font.Medium
                    icon.source: MP.Theme.asset("icons/" + modelData.icon)
                    icon.width: 20
                    icon.height: 20
                    icon.color: current ? MP.Theme.accent : MP.Theme.secondaryText
                    Material.foreground: current ? MP.Theme.accent : MP.Theme.secondaryText
                    background: null
                    onClicked: swipeView.currentIndex = index
                }
            }
        }
    }

    // Frameless: the top and bottom edges still resize the window (its width is fixed)
    Repeater {
        model: gameScopeMode ? [] : [Qt.TopEdge, Qt.BottomEdge]
        delegate: MouseArea {
            required property int modelData
            width: root.width
            height: 5
            y: modelData === Qt.TopEdge ? 0 : root.height - height
            cursorShape: Qt.SizeVerCursor
            onPressed: root.startSystemResize(modelData)
        }
    }
}
