// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick 2.15
import QtQuick.Effects
import QtQuick.Layouts 1.15
import mypods as MP
import "../components" as Components

Components.ScrollPage {
    id: rootPage
    title: qsTrId("menu.about")
    showTitle: false

    // The file next to the app, asked once: asked whenever the connection drops, it started the daemon while
    // Windows shut down (which ends the daemon first), and that start fails with a 0xc0000142 popup
    readonly property string fileVersion: backendManager ? backendManager.version() : ""

    Item {
        Layout.fillWidth: true
        Layout.topMargin: MP.Theme.spaceXl
        implicitHeight: 88

        RectangularShadow {
            anchors.fill: logo
            anchors.margins: 8
            radius: MP.Theme.radiusPanel
            blur: 32
            offset.y: 10
            color: MP.Theme.shadow
        }

        Image {
            id: logo
            anchors.horizontalCenter: parent.horizontalCenter
            width: 88; height: 88
            sourceSize: Qt.size(224, 224)
            source: MP.Units.asset("icons/mp-logo-color.svg")
        }
    }

    MP.Heading {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: MP.Theme.spaceS
        level: 1
        text: "MyPods"
    }

    MP.Label {
        Layout.alignment: Qt.AlignHCenter
        Layout.bottomMargin: MP.Theme.spaceXl
        color: MP.Theme.labelSecondary
        text: Qt.application.version
    }

    MP.Heading {
        level: 5
        Layout.leftMargin: MP.Theme.spaceL
        text: qsTrId("about.packages")
    }

    Components.Card {

        MP.FormRow {
            visible: desktopManager.updateVersion !== ""
            label: qsTrId("about.update").arg(desktopManager.updateVersion)
            iconSource: MP.Units.asset("icons/icon-sparkles.svg")
            iconColor: MP.Theme.accent
            MP.Button {
                text: qsTrId("about.update.download")
                highlighted: true
                onClicked: Qt.openUrlExternally(desktopManager.updateUrl)
            }
        }

        MP.FormRow {
            label: "MyPods"
            iconSource: MP.Units.asset("icons/icon-apps.svg")
            iconColor: MP.Theme.accent
            MP.Label { color: MP.Theme.labelSecondary; text: Qt.application.version }
        }

        // The running daemon's version; only without a connection the file next to the app is asked
        MP.FormRow {
            label: "MyPods Core"
            iconSource: MP.Units.asset("icons/icon-chip.svg")
            iconColor: MP.Theme.gray
            MP.Label {
                color: MP.Theme.labelSecondary
                text: (cppBackend && cppBackend.backendVersion) || rootPage.fileVersion || qsTrId("about.not_installed")
            }
        }
    }

    // A daemon from another installation (an old systemd service, a second copy) still running
    MP.Label {
        visible: !!cppBackend && cppBackend.backendVersion !== "" && cppBackend.backendVersion !== Qt.application.version
        Layout.fillWidth: true
        Layout.leftMargin: MP.Theme.spaceL
        Layout.rightMargin: MP.Theme.spaceL
        wrapMode: Text.WordWrap
        color: MP.Theme.labelSecondary
        font: MP.Theme.fontCallout
        text: qsTrId("about.version_mismatch")
    }

    // section footer
    MP.Label {
        Layout.fillWidth: true
        Layout.leftMargin: MP.Theme.spaceL
        Layout.rightMargin: MP.Theme.spaceL
        wrapMode: Text.WordWrap
        color: MP.Theme.labelSecondary
        font: MP.Theme.fontCallout
        text: (cppBackend && cppBackend.backendInfoText !== "")
              ? cppBackend.backendInfoText
              : qsTrId("about.not_connected")
    }

    // What helps when something doesn't work: the logs, and whether the parts MyPods relies on are there
    MP.Heading {
        level: 5
        Layout.topMargin: MP.Theme.spaceL
        Layout.leftMargin: MP.Theme.spaceL
        text: qsTrId("about.diagnostics")
    }

    Components.Card {

        MP.FormRow {
            label: qsTrId("about.logs")
            info: qsTrId("about.logs.info")
            iconSource: MP.Units.asset("icons/icon-info-fill.svg")
            iconColor: MP.Theme.gray
            MP.Button {
                text: qsTrId("about.logs.open")
                onClicked: desktopManager.openLogDir()
            }
        }

        // the effects' engine on Windows, installed with MyPods
        MP.FormRow {
            visible: Qt.platform.os === "windows" && (cppBackend?.connected ?? false)
            label: "Equalizer APO"
            info: qsTrId("about.effects.info")
            iconSource: MP.Units.asset("icons/icon-eq.svg")
            iconColor: MP.Theme.gray
            MP.Label {
                color: cppBackend?.effectsAvailable ? MP.Theme.labelSecondary : MP.Theme.red
                text: cppBackend?.effectsAvailable ? qsTrId("about.effects.ready") : qsTrId("about.effects.missing")
            }
        }

        Repeater {
            model: cppMedia.iphones
            MP.FormRow {
                required property var modelData
                label: modelData.name
                info: qsTrId("about.iphone.info")
                iconSource: MP.Units.asset("icons/icon-iphone.svg")
                iconColor: MP.Theme.gray
                MP.Label {
                    color: MP.Theme.labelSecondary
                    text: modelData.connected ? qsTrId("headphones.iphone.connected") : qsTrId("headphones.iphone.not_connected")
                }
            }
        }
    }
}
