// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick 2.15
import QtQuick.Layouts 1.15
import magicpods as MP

ColumnLayout {
    required property url iconSource
    required property string titleText
    required property string bodyText
    Layout.fillWidth: true
    Layout.topMargin: MP.Units.hugeSpacing * 2
    spacing: MP.Units.mediumSpacing

    Image {
        Layout.preferredWidth: 112
        Layout.preferredHeight: 112
        Layout.alignment: Qt.AlignHCenter
        Layout.bottomMargin: MP.Units.mediumSpacing
        sourceSize: Qt.size(224, 224)
        source: iconSource
    }

    MP.Heading {
        text: titleText
        level: 2
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
    }

    MP.Label {
        text: bodyText
        color: MP.Theme.secondaryText
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
    }
}
