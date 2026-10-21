// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Effects
import mypods as MP

// MyStyle's Liquid Glass (platforms/qml/Glass.qml) for the bars, the sidebar and every popup, with a soft shadow a
// little below (a window needs room around it for that).
// - blurSource: what lies behind the glass in the same window -> the full lens (glass.frag).
// - translucent: a window of its own whose desktop the compositor blurs (BlurBehind.h) -> tint and rim only.
// - neither, the software renderer, or Reduce Transparency -> frosted, nearly opaque, light at the top.
// MyPods adds GM-4: while the window is in the background the glass steps back (more tint, half the rim).
Item {
    id: root
    property real radius: MP.Theme.radiusPanel
    property Item blurSource: null
    property bool translucent: false
    // menus, popovers, alerts: the denser glass for reading (not sidebars: they stay regular, the content shows through)
    property bool textSurface: false
    // "thin" (small capsules, transient control glass), "regular" (bars, sidebar), "thick" (menus, popovers, alerts)
    property string thickness: "regular"

    readonly property var spec: thickness === "thin" ? MP.Theme.glassThin : thickness === "thick" ? MP.Theme.glassThick : MP.Theme.glassRegular
    readonly property bool windowActive: Window.active
    readonly property real tint: Math.min(1, MP.Theme.glassTintAmount(textSurface) + (windowActive ? 0 : 0.1))
    readonly property bool lens: blurSource !== null && GraphicsInfo.api !== GraphicsInfo.Software && !MP.Theme.reduceTransparency
    // the backdrop is captured this much larger, so the blur has something to draw from at the glass's edge
    readonly property int margin: Math.round(MP.Theme.glassBlur + spec.extraBlur)

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
        blur: root.spec.shadowBlur
        offset.y: root.spec.shadowY
        color: Qt.rgba(MP.Theme.shadow.r, MP.Theme.shadow.g, MP.Theme.shadow.b, Math.min(1, MP.Theme.shadow.a * root.spec.shadowScale))
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
        property real tint: root.tint
        property real lensZone: root.spec.lensZone
        property real edgeBlur: root.spec.edgeBlur
        property real lensStrength: MP.Theme.reduceMotion ? root.spec.lensStrength * 0.5 : root.spec.lensStrength
        property real chroma: root.spec.chroma
        property real saturation: MP.Theme.glassSaturation
        property real rim: MP.Theme.glassRim * (root.windowActive ? 1 : 0.5)
        property real edge: MP.Theme.glassEdge.a
        property color tintColor: MP.Theme.glassTint
        fragmentShader: Qt.resolvedUrl("../../shaders/glass.frag.qsb")
    }

    // Increase Contrast: a clear border around the lens
    Rectangle {
        anchors.fill: parent
        visible: root.lens && MP.Theme.increaseContrast
        radius: root.radius
        color: "transparent"
        border.width: 1
        border.color: MP.Theme.glassBorder
    }

    Rectangle {
        anchors.fill: parent
        visible: !root.lens
        radius: root.radius
        border.width: root.translucent && !MP.Theme.reduceTransparency ? 1.5 : 1
        border.color: root.translucent && !MP.Theme.reduceTransparency ? MP.Theme.glassBorder : MP.Theme.glassPopupBorder
        gradient: Gradient {
            GradientStop { position: 0; color: root.translucent && !MP.Theme.reduceTransparency ? MP.Theme.glassTranslucentTop : MP.Theme.glassOpaqueTop }
            GradientStop { position: 1; color: root.translucent && !MP.Theme.reduceTransparency ? MP.Theme.glassTranslucentBottom : MP.Theme.glassOpaqueBottom }
        }
    }
}
