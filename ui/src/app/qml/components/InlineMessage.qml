// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Controls.impl as Impl
import QtQuick.Layouts 1.15
import magicpods as MP

MP.Card {
    id: root

    readonly property int typeInformation: 0
    readonly property int typePositive:    1
    readonly property int typeWarning:     2
    readonly property int typeError:       3

    property int    type:    typeInformation
    property string text:    ""
    property list<QtObject> actions
    readonly property color tint: [MP.Theme.accent, MP.Theme.green, MP.Theme.orange, MP.Theme.red][type]

    verticalPadding: MP.Units.mediumSpacing + 4
    border.width: 1
    border.color: Qt.rgba(tint.r, tint.g, tint.b, 0.5)

    RowLayout {
        spacing: MP.Units.mediumSpacing

        Impl.IconImage {
            sourceSize: Qt.size(22, 22)
            source: MP.Theme.asset("icons/icon-info.svg")
            color: root.tint
        }

        MP.Label {
            text:             root.text
            wrapMode:         Text.WordWrap
            Layout.fillWidth: true
        }

        Repeater {
            model: root.actions
            delegate: QQC2.Button {
                text:      modelData.text
                icon.name: modelData.icon ? (modelData.icon.name ?? "") : ""
                onClicked: modelData.trigger()
            }
        }
    }
}
