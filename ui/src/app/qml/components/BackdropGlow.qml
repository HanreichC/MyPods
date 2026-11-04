// MyPods
// License: GPL-3.0

// MyStyle's light behind the glass, GL-6 (platforms/qml/BackdropGlow.qml).
// Put it right above the backdrop gradient, inside the Item the glass takes as blurSource, so the glass has
// something to bend at rest. Off with Increase Contrast, Reduce Transparency and the software renderer.

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import mypods as MP

Item {
    id: root
    property real radius: MP.Theme.radiusWindow // stays inside the window's rounded corners
    readonly property real reach: Math.max(width, height)

    visible: GraphicsInfo.api !== GraphicsInfo.Software && !MP.Theme.reduceTransparency && !MP.Theme.increaseContrast
    layer.enabled: visible
    layer.effect: MultiEffect {
        maskEnabled: true
        maskSource: windowMask
        maskSpreadAtMin: 1.0
        maskThresholdMin: 0.5 // with 0.0 the mask does nothing
    }

    Rectangle {
        id: windowMask
        anchors.fill: parent
        radius: root.radius
        visible: false
        layer.enabled: true
    }

    component Light: ShapePath {
        id: light
        property color color
        property real cx
        property real cy
        property real r
        strokeWidth: 0
        strokeColor: "transparent"
        fillGradient: RadialGradient {
            centerX: light.cx; centerY: light.cy; centerRadius: light.r
            focalX: light.cx; focalY: light.cy
            GradientStop { position: 0; color: light.color }
            GradientStop { position: 1; color: Qt.alpha(light.color, 0) }
        }
        startX: 0; startY: 0
        PathLine { x: root.width; y: 0 }
        PathLine { x: root.width; y: root.height }
        PathLine { x: 0; y: root.height }
        PathLine { x: 0; y: 0 }
    }

    Shape {
        anchors.fill: parent
        Light { color: MP.Theme.backdropGlowPrimary; cx: root.width * 0.11; cy: root.height * 0.35; r: root.reach * 0.65 }
        Light { color: MP.Theme.backdropGlowSecondary; cx: root.width * 0.925; cy: root.height * 0.89; r: root.reach * 0.7 }
    }
}
