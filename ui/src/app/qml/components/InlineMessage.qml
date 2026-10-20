// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Controls.impl as Impl
import QtQuick.Layouts 1.15
import mypods as MP

MP.Card {
    id: root

    readonly property int typeInformation: 0
    readonly property int typePositive:    1
    readonly property int typeWarning:     2
    readonly property int typeError:       3

    property int    type:    typeInformation
    property string text:    ""
    property list<QtObject> actions
    readonly property color tint: [MP.Theme.statusInfo, MP.Theme.statusPositive, MP.Theme.statusWarning, MP.Theme.statusCritical][type]

    verticalPadding: MP.Theme.spaceM
    border.width: 1
    border.color: Qt.rgba(tint.r, tint.g, tint.b, 0.5)

    RowLayout {
        spacing: MP.Theme.spaceS

        // C-5: a symbol of its own per type, never the colour alone
        Impl.IconImage {
            sourceSize: Qt.size(MP.Theme.sizeIconL, MP.Theme.sizeIconL)
            source: MP.Units.asset("icons/" + ["icon-info.svg", "icon-check.svg", "icon-warning.svg", "icon-close.svg"][root.type])
            color: root.tint
        }

        MP.Label {
            text:             root.text
            wrapMode:         Text.WordWrap
            Layout.fillWidth: true
        }

        Repeater {
            model: root.actions
            delegate: MP.Button {
                text:      modelData.text
                icon.name: modelData.icon ? (modelData.icon.name ?? "") : ""
                onClicked: modelData.trigger()
            }
        }
    }
}
