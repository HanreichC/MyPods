// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick
import QtQuick.Shapes
import magicpods as MP

// Apple-widget style battery ring.
Column {
    id: root
    property int battery: 0
    property bool isCharging: false
    property int status: 2
    property string name: ""
    property int size: 64

    readonly property real level: Math.max(0, Math.min(100, battery)) / 100
    readonly property color tint: MP.Theme.batteryColor(battery, isCharging)
    readonly property real stroke: Math.max(4, size / 11)

    visible: status !== 0 && status !== 1
    opacity: status === 3 ? 0.45 : 1
    spacing: MP.Units.smallSpacing

    Item {
        width: root.size
        height: root.size
        anchors.horizontalCenter: parent.horizontalCenter

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: "transparent"
                strokeColor: Qt.rgba(root.tint.r, root.tint.g, root.tint.b, 0.2)
                strokeWidth: root.stroke
                PathAngleArc {
                    centerX: root.size / 2; centerY: root.size / 2
                    radiusX: (root.size - root.stroke) / 2; radiusY: radiusX
                    startAngle: 0; sweepAngle: 360
                }
            }
            ShapePath {
                fillColor: "transparent"
                strokeColor: root.tint
                strokeWidth: root.stroke
                capStyle: ShapePath.RoundCap
                PathAngleArc {
                    centerX: root.size / 2; centerY: root.size / 2
                    radiusX: (root.size - root.stroke) / 2; radiusY: radiusX
                    startAngle: -90
                    sweepAngle: 360 * root.level
                    Behavior on sweepAngle { NumberAnimation { duration: 600; easing.type: Easing.OutCubic } }
                }
            }
        }

        Text {
            anchors.centerIn: parent
            text: root.battery + "%"
            color: MP.Theme.text
            font.pixelSize: Math.round(root.size * 0.24)
            font.weight: Font.DemiBold
        }
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 2
        visible: root.name !== "" || root.isCharging

        Image {
            visible: root.isCharging
            anchors.verticalCenter: parent.verticalCenter
            width: 12; height: 12
            sourceSize: Qt.size(24, 24)
            source: MP.Theme.asset("icons/icon-bolt.svg")
        }
        Text {
            text: root.name
            color: MP.Theme.secondaryText
            font.pixelSize: 12
            font.weight: Font.Medium
        }
    }
}
