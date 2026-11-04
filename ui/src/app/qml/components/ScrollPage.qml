// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import mypods as MP

// Page with a centered, scrollable column. The window's title row shows its title.
QQC2.Page {
    id: page
    default property alias content: column.data
    readonly property alias contentX: column.x // the title row lines the title up with it
    padding: 0
    background: null

    QQC2.ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth

        // between the title row at the top and the window's rounded corner at the bottom
        QQC2.ScrollBar.vertical: MP.ScrollBar {
            parent: scroll
            x: scroll.width - width - MP.Theme.spaceXs
            y: MP.Theme.sizeWindowInset + MP.Theme.sizeBarHeight + MP.Theme.spaceS
            height: scroll.height - y - MP.Theme.radiusPanel
        }

        Item {
            width: scroll.availableWidth
            implicitHeight: column.y + column.implicitHeight + MP.Theme.spaceXl

            // Flickable's default wheel/touchpad step is tiny; scroll a fixed distance per notch
            // and amplify touchpad pixel deltas (Wayland reports them roughly 1:1 with finger travel)
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: event => {
                    const f = scroll.contentItem;
                    const dy = event.pixelDelta.y !== 0 ? event.pixelDelta.y * 3
                                                        : event.angleDelta.y / 120 * Qt.styleHints.wheelScrollLines * 40;
                    f.contentY = Math.max(0, Math.min(f.contentHeight - f.height, f.contentY - dy));
                }
            }

            ColumnLayout {
                id: column
                // directly below the title row
                y: MP.Theme.sizeWindowInset + MP.Theme.sizeBarHeight + MP.Theme.sizeWindowInset
                // right of the window's sidebar, which the page slides under
                readonly property int inset: QQC2.ApplicationWindow.window?.contentInset ?? 0
                width: Math.min(parent.width - inset - MP.Theme.spaceXl * 2, MP.Theme.sizeContentMaxWidth)
                x: inset + Math.round((parent.width - inset - width) / 2)
                spacing: MP.Theme.spaceS
            }
        }
    }
}
