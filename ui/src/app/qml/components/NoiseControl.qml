// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Controls.impl as Impl
import QtQuick.Layouts
import QtQuick.Effects
import mypods as MP

// Noise control segmented control plus the name of the current mode, shared by the device page
// and the tray popup.
ColumnLayout {
    id: root

    property var ancData: null
    property string address: ""
    // On the window's backdrop: the track is glass like the cards (in the tray popup a quiet fill)
    property bool floating: false
    property int selectedAnc: 0
    readonly property var ancModes: ({
            OFF: 1,
            TRANSPARENCY: 2,
            ADAPTIVE: 4,
            WIND: 8,
            ANC: 16
        })
    readonly property var buttons: [
        { mode: ancModes.OFF, icon: "icon-off.svg", text: qsTrId("battery.anc_off") },
        { mode: ancModes.TRANSPARENCY, icon: "icon-tra.svg", text: qsTrId("battery.anc_tra") },
        { mode: ancModes.ADAPTIVE, icon: "icon-adaptive.svg", text: qsTrId("battery.anc_adaptive") },
        { mode: ancModes.WIND, icon: "icon-wind.svg", text: qsTrId("battery.anc_wind") },
        { mode: ancModes.ANC, icon: "icon-noise.svg", text: qsTrId("battery.anc_anc") }
    ]

    readonly property bool available: ancData !== null && ancTrack.modes.length > 0

    visible: available
    spacing: MP.Theme.spaceS

    // while a switch is pending, only its confirmation may move the thumb
    onAncDataChanged: {
        var anc = ancData?.selected;
        if (anc !== undefined && (!ancRevert.running || anc === selectedAnc)) {
            selectedAnc = anc;
            ancRevert.stop();
        }
    }

    // Optimistic: the thumb moves at once, ancData keeps the device's last confirmed mode.
    // The core only broadcasts when the mode actually changes, so if the headphones ignore
    // the command nothing arrives and ancRevert snaps the thumb back to the real mode.
    function setAnc(mode) {
        if (!ancData)
            return;
        selectedAnc = mode;
        cppBackend.setAnc(address, mode);
        ancRevert.restart();
    }

    Timer {
        id: ancRevert
        interval: 2000
        onTriggered: root.selectedAnc = root.ancData?.selected ?? root.selectedAnc
    }

    Rectangle {
        id: ancTrack
        readonly property var modes: root.buttons.filter(b => (root.ancData?.options ?? 0) & b.mode)
        readonly property int selectedIndex: modes.findIndex(b => b.mode === root.selectedAnc)
        readonly property real segmentWidth: ancLane.width / Math.max(1, modes.length)
        readonly property bool glass: ancMouse.pressed
        // segment under the thumb's centre, i.e. what a release would select
        readonly property int thumbIndex: Math.max(0, Math.min(modes.length - 1, Math.floor((ancThumb.x + segmentWidth / 2) / segmentWidth)))

        function step(delta) {
            var i = Math.max(0, Math.min(modes.length - 1, selectedIndex + delta));
            if (i !== selectedIndex)
                root.setAnc(modes[i].mode);
        }

        enabled: !(root.ancData?.readonly ?? true)
        opacity: enabled ? 1 : MP.Theme.opacityDisabled
        Layout.fillWidth: true
        implicitHeight: MP.Theme.sizeSegmented
        radius: height / 2
        color: root.floating ? MP.Theme.materialFill : MP.Theme.fillTertiary
        border.width: activeFocus ? 2 : root.floating ? 1 : 0
        border.color: activeFocus ? MP.Theme.focusRing : MP.Theme.materialBorder

        RectangularShadow {
            z: -1
            visible: root.floating
            anchors.fill: parent
            radius: parent.radius
            // MyStyle elevation.card, like the cards around it
            blur: 24
            offset.y: 4
            color: Qt.rgba(0, 0, 0, MP.Theme.dark ? 0 : 0.05)
        }

        activeFocusOnTab: true
        Keys.onLeftPressed: step(-1)
        Keys.onRightPressed: step(1)
        Accessible.role: Accessible.PageTabList
        Accessible.name: root.buttons.find(b => b.mode === root.selectedAnc)?.text ?? ""

        Item {
            id: ancLane
            anchors.fill: parent
            anchors.margins: MP.Theme.spaceXs

            Rectangle {
                id: ancThumb
                visible: ancTrack.selectedIndex >= 0 || ancMouse.dragging
                x: ancMouse.dragging ? Math.max(0, Math.min(ancLane.width - width, ancMouse.mouseX - width / 2))
                                     : Math.max(0, ancTrack.selectedIndex) * width
                width: ancTrack.segmentWidth
                height: parent.height
                radius: height / 2
                // GM-5: a glass lens while dragged (Reduce Motion: no scaling)
                scale: ancTrack.glass && !MP.Theme.reduceMotion ? 1.14 : 1
                // MyStyle: kein Token für die Daumen-Linse (Weiß 30 % / 12 %)
                color: ancTrack.glass ? Qt.rgba(1, 1, 1, MP.Theme.dark ? 0.12 : 0.3) : MP.Theme.materialThumb
                border.width: 1
                border.color: ancTrack.glass ? MP.Theme.glassBorder : "transparent"

                Behavior on x { enabled: !ancMouse.dragging; NumberAnimation { duration: MP.Theme.durationGlide; easing.type: MP.Theme.easeSpring; easing.overshoot: MP.Theme.glideOvershoot } }
                Behavior on scale { NumberAnimation { duration: MP.Theme.durationSpring; easing.type: MP.Theme.easeSpring; easing.overshoot: MP.Theme.springOvershoot } }
                Behavior on color { ColorAnimation { duration: MP.Theme.durationToggle } }
                Behavior on border.color { ColorAnimation { duration: MP.Theme.durationToggle } }

                // specular sheen on the upper half of the lens
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    opacity: ancTrack.glass ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: MP.Theme.durationToggle } }
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: MP.Theme.glassSheen }
                        GradientStop { position: 0.55; color: "transparent" }
                    }
                }

                // effects need a shader-capable scene graph; the software renderer drops the item
                layer.enabled: GraphicsInfo.api !== GraphicsInfo.Software
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowBlur: ancTrack.glass ? 0.8 : 0.4
                    shadowOpacity: ancTrack.glass ? 0.25 : 0.15
                    shadowVerticalOffset: ancTrack.glass ? 4 : 2
                }
            }

            Row {
                anchors.fill: parent

                Repeater {
                    model: ancTrack.modes
                    delegate: Item {
                        required property var modelData
                        required property int index
                        readonly property bool hovered: ancMouse.containsMouse && !ancMouse.pressed
                                                        && Math.floor(ancMouse.mouseX / ancTrack.segmentWidth) === index
                        width: ancTrack.segmentWidth
                        height: parent.height

                        Rectangle {
                            anchors.fill: parent
                            radius: height / 2
                            color: MP.Theme.fillTertiary
                            visible: parent.hovered && index !== ancTrack.selectedIndex
                        }

                        Impl.IconImage {
                            anchors.centerIn: parent
                            sourceSize: Qt.size(MP.Theme.sizeIconL, MP.Theme.sizeIconL)
                            source: MP.Units.asset("icons/" + modelData.icon)
                            color: MP.Theme.labelPrimary
                            // lens magnification under the glass thumb
                            scale: ancTrack.glass && index === ancTrack.thumbIndex && !MP.Theme.reduceMotion ? 1.2 : 1
                            Behavior on scale { NumberAnimation { duration: MP.Theme.durationToggle; easing.type: MP.Theme.easeSpring } }
                        }

                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: MP.Theme.durationTooltipDelay
                        QQC2.ToolTip.text: modelData.text
                    }
                }
            }

            MouseArea {
                id: ancMouse
                property real pressX: 0
                property bool dragging: false
                anchors.fill: parent
                hoverEnabled: true
                preventStealing: true
                cursorShape: ancTrack.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

                onPressed: mouse => { pressX = mouse.x; ancTrack.forceActiveFocus(); }
                onPositionChanged: mouse => {
                    if (pressed && !dragging && Math.abs(mouse.x - pressX) > 4)
                        dragging = true;
                }
                onReleased: {
                    // tap selects the segment under the pointer, drag the one under the thumb
                    var i = dragging ? ancTrack.thumbIndex
                                     : Math.max(0, Math.min(ancTrack.modes.length - 1, Math.floor(mouseX / ancTrack.segmentWidth)));
                    dragging = false;
                    if (i !== ancTrack.selectedIndex)
                        root.setAnc(ancTrack.modes[i].mode);
                }
                onCanceled: dragging = false
            }
        }
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: 2

        MP.Label {
            color: MP.Theme.labelSecondary
            font: MP.Theme.fontCaption
            text: root.buttons.find(b => b.mode === root.selectedAnc)?.text ?? ""
        }
        InfoButton {
            title: qsTrId("battery.noise_control")
            info: qsTrId("battery.noise_control.info")
        }
    }
}
