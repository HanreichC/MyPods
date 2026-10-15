// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import magicpods as MP

// Page with a large title and a centered, scrollable column.
QQC2.Page {
    id: page
    default property alias content: column.data
    property bool showTitle: true
    padding: 0
    background: null

    QQC2.ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth

        // between the bars at the top and the window's rounded corner at the bottom
        QQC2.ScrollBar.vertical: MP.ScrollBar {
            parent: scroll
            x: scroll.width - width - MP.Units.smallSpacing
            y: 12 + MP.Theme.barHeight + MP.Units.mediumSpacing
            height: scroll.height - y - MP.Theme.radius
        }

        Item {
            width: scroll.availableWidth
            implicitHeight: column.y + column.implicitHeight + MP.Units.hugeSpacing

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
                y: 20 // the large title level with the window buttons
                // right of the window's sidebar, which the page slides under
                readonly property int inset: QQC2.ApplicationWindow.window?.contentInset ?? 0
                width: Math.min(parent.width - inset - MP.Units.hugeSpacing * 2, MP.Theme.maxContentWidth)
                x: inset + Math.round((parent.width - inset - width) / 2)
                spacing: MP.Units.mediumSpacing

                MP.Heading {
                    visible: page.showTitle
                    level: 1
                    text: page.title
                    Layout.fillWidth: true
                    Layout.leftMargin: MP.Units.smallSpacing
                    // clear of the window buttons
                    Layout.rightMargin: QQC2.ApplicationWindow.window?.titleInset ?? 0
                    Layout.bottomMargin: MP.Units.mediumSpacing
                    wrapMode: Text.Wrap // a long device name on two lines rather than cut off
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
            }
        }
    }
}
