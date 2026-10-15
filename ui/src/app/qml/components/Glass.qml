// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Effects
import magicpods as MP

// MyCAD's glass alert as a surface for popups and floating windows: light at the top, a fine edge and a
// soft shadow a little below. A window needs room around it for the shadow.
// Liquid Glass where something lies behind it: a popup inside a window blurs the window's content
// (blurSource), a window of its own lets the compositor blur the desktop (translucent, see BlurBehind.h).
Item {
    id: root
    property real radius: 22
    property Item blurSource: null
    property bool translucent: false
    readonly property bool blurs: translucent || (blurSource !== null && GraphicsInfo.api !== GraphicsInfo.Software)

    RectangularShadow {
        anchors.fill: parent
        radius: root.radius
        blur: 28
        offset.y: 6
        color: MP.Theme.shadow
    }

    // Holding on to the window's content only while shown, and letting go before the app quits: a reference
    // still held while the window is torn down crashes Qt (QQuickDeliveryAgent, 6.11)
    property bool quitting: false
    Connections {
        target: Qt.application
        function onAboutToQuit() { root.quitting = true; }
    }

    ShaderEffectSource {
        id: backdrop
        anchors.fill: parent
        visible: false
        sourceItem: root.visible && !root.quitting ? root.blurSource : null
        // where the glass lies over the content; the popup moves and grows while it opens
        sourceRect: {
            root.width; root.height;
            root.parent?.x; root.parent?.y; root.parent?.scale;
            return root.blurSource ? root.mapToItem(root.blurSource, 0, 0, root.width, root.height) : Qt.rect(0, 0, 0, 0);
        }
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        radius: root.radius
        visible: false
        layer.enabled: true
    }

    MultiEffect {
        anchors.fill: parent
        visible: root.blurSource !== null
        source: backdrop
        autoPaddingEnabled: false
        blurEnabled: true
        blur: 1.0
        blurMax: 48
        saturation: 0.4
        maskEnabled: true
        maskSource: mask
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        border.width: 1
        border.color: MP.Theme.popupBorder
        gradient: Gradient {
            GradientStop { position: 0; color: root.blurs ? MP.Theme.popupGlass : MP.Theme.popup }
            GradientStop { position: 1; color: root.blurs ? MP.Theme.popupGlassBottom : MP.Theme.popupBottom }
        }
    }
}
