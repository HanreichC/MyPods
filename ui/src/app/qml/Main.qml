// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Controls.Material
import QtQuick.Effects
import QtQuick.Layouts
import "pages"
import "components" as Components
import magicpods as MP

QQC2.ApplicationWindow {
    id: root
    minimumWidth: gameScopeMode ? 0 : 420
    width: gameScopeMode ? Screen.width : 560
    height: gameScopeMode ? Screen.height : 720
    visibility: gameScopeMode ? Window.FullScreen : Window.Windowed
    visible: !startHidden
    color: MP.Theme.base
    // Material hard-codes Roboto/Noto for controls; the window font overrides it for every control
    font.family: "Inter"

    Material.theme: MP.Theme.dark ? Material.Dark : Material.Light
    Material.accent: MP.Theme.accent
    Material.primary: MP.Theme.accent

    property alias currentIndex: swipeView.currentIndex
    // With a tray the window just hides; without one there would be no way to quit or reopen it.
    onClosing: (event) => {
        if (!trayAvailable) {
            Qt.quit();
            return;
        }
        event.accepted = false;
        root.visible = false;
    }

    // Everything the glass tab bar blurs lives in here.
    Item {
        id: scene
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            color: MP.Theme.base
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
                visible: cppBackend?.unsupportedApi ?? false
                type: 2
                text: qsTrId("Error.service_api_wrong")
            }

            QQC2.SwipeView {
                id: swipeView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                // HIG soft scroll edge effect: content fades out where it passes under the floating tab bar
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
        gradient: Gradient {
            GradientStop { position: 0; color: "black" }
            GradientStop { position: Math.max(0, 1 - 120 / scrollEdgeMask.height); color: "black" }
            GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0.15) }
        }
    }

    // Floating Liquid Glass tab bar (control layer)
    Item {
        id: tabBar
        readonly property var tabs: [
            { text: qsTrId("menu.battery"), icon: "icon-battery-fill.svg" },
            { text: qsTrId("menu.headphones"), icon: "icon-headphones-fill.svg" },
            { text: qsTrId("menu.settings"), icon: "icon-settings-fill.svg" },
            { text: qsTrId("menu.about"), icon: "icon-info-fill.svg" }
        ]
        width: Math.min(parent.width - MP.Units.hugeSpacing * 2, 400)
        height: 64
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: MP.Units.largeSpacing

        RectangularShadow {
            anchors.fill: parent
            radius: height / 2
            blur: 30
            offset.y: 8
            color: Qt.rgba(0, 0, 0, MP.Theme.dark ? 0.5 : 0.12)
        }

        ShaderEffectSource {
            id: barBackdrop
            anchors.fill: parent
            visible: false
            sourceItem: scene
            sourceRect: Qt.rect(tabBar.x, tabBar.y, tabBar.width, tabBar.height)
        }

        Rectangle {
            id: barMask
            anchors.fill: parent
            radius: height / 2
            visible: false
            layer.enabled: true
        }

        MultiEffect {
            anchors.fill: parent
            source: barBackdrop
            autoPaddingEnabled: false
            blurEnabled: true
            blur: 1.0
            blurMax: 48
            saturation: 0.4
            maskEnabled: true
            maskSource: barMask
        }

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: MP.Theme.glassStrong
            border.width: 1
            border.color: MP.Theme.glassBorder
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.tint(MP.Theme.glassStrong, MP.Theme.glassSheen) }
                GradientStop { position: 0.5; color: MP.Theme.glassStrong }
            }
        }

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
                    spacing: 2
                    font.pixelSize: 11
                    font.weight: current ? Font.DemiBold : Font.Medium
                    icon.source: MP.Theme.asset("icons/" + modelData.icon)
                    icon.width: 22
                    icon.height: 22
                    icon.color: current ? MP.Theme.accent : MP.Theme.secondaryText
                    Material.foreground: current ? MP.Theme.accent : MP.Theme.secondaryText
                    background: null
                    onClicked: swipeView.currentIndex = index
                }
            }
        }
    }
}
