// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Effects
import magicpods as MP

// Liquid Glass like MyCAD's, for the bars, the sidebar and every popup, with a soft shadow a little below
// (a window needs room around it for that).
// - blurSource: what lies behind the glass in the same window. MyCAD's clear glass over it (glass.frag):
//   blurred, bent like a lens towards the edge, a light rim.
// - translucent: a window of its own whose desktop the compositor already blurs (BlurBehind.h): a tint
//   and a rim over it; the lens can't reach the desktop.
// - neither (or the software renderer): MyCAD's glass alert, nearly opaque and light at the top.
Item {
    id: root
    property real radius: 22
    property Item blurSource: null
    property bool translucent: false
    readonly property bool lens: blurSource !== null && GraphicsInfo.api !== GraphicsInfo.Software
    // the backdrop is captured this much larger, so the blur has something to draw from at the glass's edge
    readonly property int margin: 24

    // Holding on to the window's content only while shown, and letting go before the app quits: a reference
    // still held while the window is torn down crashes Qt (QQuickDeliveryAgent, 6.11)
    property bool quitting: false
    Connections {
        target: Qt.application
        function onAboutToQuit() { root.quitting = true; }
    }

    RectangularShadow {
        anchors.fill: parent
        radius: root.radius
        blur: 28
        offset.y: 6
        color: MP.Theme.shadow
    }

    ShaderEffectSource {
        id: backdrop
        x: -root.margin
        y: -root.margin
        width: root.width + root.margin * 2
        height: root.height + root.margin * 2
        visible: false
        sourceItem: root.lens && root.visible && !root.quitting ? root.blurSource : null
        // where the glass lies over the content; it moves with the window and grows while a popup opens
        sourceRect: {
            root.x; root.y; root.width; root.height;
            root.parent?.x; root.parent?.y; root.parent?.scale;
            return sourceItem ? root.mapToItem(sourceItem, -root.margin, -root.margin, width, height) : Qt.rect(0, 0, 0, 0);
        }
    }

    MultiEffect {
        id: blurredBackdrop
        anchors.fill: backdrop
        source: backdrop
        autoPaddingEnabled: false
        blurEnabled: true
        blur: 1.0
        blurMax: root.margin
    }

    ShaderEffectSource {
        id: blurredTexture
        anchors.fill: backdrop
        visible: false
        hideSource: true
        sourceItem: root.lens ? blurredBackdrop : null
    }

    ShaderEffect {
        anchors.fill: parent
        visible: root.lens
        property variant source: blurredTexture
        property size panel: Qt.size(width, height)
        property real radius: root.radius
        property real margin: root.margin
        property real dark: MP.Theme.dark ? 1 : 0
        fragmentShader: Qt.resolvedUrl("../../shaders/glass.frag.qsb")
    }

    Rectangle {
        anchors.fill: parent
        visible: !root.lens
        radius: root.radius
        border.width: root.translucent ? 1.5 : 1
        border.color: root.translucent ? MP.Theme.glassBorder : MP.Theme.popupBorder
        gradient: Gradient {
            GradientStop { position: 0; color: root.translucent ? MP.Theme.popupGlass : MP.Theme.popup }
            GradientStop { position: 1; color: root.translucent ? MP.Theme.popupGlassBottom : MP.Theme.popupBottom }
        }
    }
}
