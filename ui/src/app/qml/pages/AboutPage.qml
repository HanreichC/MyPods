// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick 2.15
import QtQuick.Effects
import QtQuick.Layouts 1.15
import magicpods as MP
import "../components" as Components

Components.ScrollPage {
    id: rootPage
    title: qsTrId("menu.about")
    showTitle: false

    Item {
        Layout.fillWidth: true
        Layout.topMargin: MP.Units.hugeSpacing
        implicitHeight: 112

        RectangularShadow {
            anchors.fill: logo
            anchors.margins: 8
            radius: 26
            blur: 32
            offset.y: 10
            color: Qt.rgba(0, 0, 0, MP.Theme.dark ? 0.5 : 0.18)
        }

        Image {
            id: logo
            anchors.horizontalCenter: parent.horizontalCenter
            width: 112; height: 112
            sourceSize: Qt.size(224, 224)
            source: MP.Theme.asset("icons/mp-logo-color.svg")
        }
    }

    MP.Heading {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: MP.Units.mediumSpacing
        level: 1
        text: "MyPods"
    }

    MP.Label {
        Layout.alignment: Qt.AlignHCenter
        Layout.bottomMargin: MP.Units.hugeSpacing
        color: MP.Theme.secondaryText
        text: Qt.application.version
    }

    MP.Heading {
        level: 5
        Layout.leftMargin: MP.Units.largeSpacing
        text: qsTrId("about.packages")
    }

    Components.Card {

        MP.FormRow {
            label: "MyPods"
            MP.Label { color: MP.Theme.secondaryText; text: Qt.application.version }
        }

        MP.FormRow {
            label: "MyPods Core"
            MP.Label {
                color: MP.Theme.secondaryText
                text: backendManager ? backendManager.version() || qsTrId("about.not_installed") : ""
            }
        }
    }

    // section footer
    MP.Label {
        Layout.fillWidth: true
        Layout.leftMargin: MP.Units.largeSpacing
        Layout.rightMargin: MP.Units.largeSpacing
        wrapMode: Text.WordWrap
        color: MP.Theme.secondaryText
        font.pixelSize: 13
        text: (cppBackend && cppBackend.backendInfoText !== "")
              ? cppBackend.backendInfoText
              : qsTrId("about.not_connected")
    }
}
