// MyPods
// License: GPL-3.0

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Controls.impl as Impl
import QtQuick.Layouts
import magicpods as MP

// Now playing and the volume, like the Mac's Control Center: cover and text across the width, the controls and
// the volume under them. In the tray popup and beside the headphones in the window.
ColumnLayout {
    id: root
    readonly property var player: cppMedia.player
    readonly property bool hasPlayer: Object.keys(player).length > 0
    // whether there is anything to show at all
    readonly property bool shown: hasPlayer || cppMedia.volume >= 0
    // previous, play and next beside the title instead of under it
    property bool controlsBeside: false
    spacing: 10

    // ponytail: polls MPRIS and the volume once a second while the window showing it is open, whether this is
    // shown or not: it hides for want of a player, and only polling finds one again
    Timer {
        interval: 1000
        repeat: true
        running: (root.Window.window?.visible ?? false) && !volumeSlider.pressed
        triggeredOnStart: true
        onTriggered: cppMedia.refresh()
    }

    component VolumeStepButton: QQC2.AbstractButton {
        implicitWidth: 26
        implicitHeight: 26
        opacity: enabled ? 1 : 0.35
        autoRepeat: true
        background: Rectangle {
            radius: width / 2
            color: parent.pressed ? MP.Theme.fill : parent.hovered ? MP.Theme.tertiaryFill : "transparent"
        }
        contentItem: Impl.IconImage {
            sourceSize: Qt.size(14, 14)
            source: parent.icon.source
            color: MP.Theme.text
        }
    }

    // Previous, play/pause (the larger one) and next
    component Transport: RowLayout {
        spacing: 20

        Repeater {
            model: [
                { icon: "icon-backward.svg", name: qsTrId("tray.popup.previous"), enabled: root.player.canGoPrevious ?? false, action: () => cppMedia.previous() },
                { icon: root.player.playing ? "icon-pause.svg" : "icon-play.svg", name: root.player.playing ? qsTrId("tray.popup.pause") : qsTrId("tray.popup.play"), enabled: true, action: () => cppMedia.playPause() },
                { icon: "icon-forward.svg", name: qsTrId("tray.popup.next"), enabled: root.player.canGoNext ?? false, action: () => cppMedia.next() }
            ]
            delegate: QQC2.AbstractButton {
                required property var modelData
                // HIG: play/pause is the primary control, larger than its neighbours
                readonly property bool primary: index === 1
                required property int index
                implicitWidth: primary ? 40 : 34
                implicitHeight: primary ? 40 : 34
                enabled: modelData.enabled
                opacity: enabled ? 1 : 0.35
                Accessible.name: modelData.name
                onClicked: modelData.action()
                background: Rectangle {
                    radius: width / 2
                    color: parent.pressed ? MP.Theme.fill : parent.hovered ? MP.Theme.tertiaryFill : "transparent"
                }
                contentItem: Impl.IconImage {
                    sourceSize: primary ? Qt.size(26, 26) : Qt.size(20, 20)
                    source: MP.Theme.asset("icons/" + modelData.icon)
                    color: MP.Theme.text
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.hasPlayer
        spacing: 12

        Rectangle {
            Layout.preferredWidth: 52
            Layout.preferredHeight: 52
            Layout.alignment: Qt.AlignTop
            radius: 8
            color: MP.Theme.tertiaryFill
            clip: true

            Impl.IconImage {
                anchors.centerIn: parent
                visible: art.status !== Image.Ready
                sourceSize: Qt.size(24, 24)
                source: MP.Theme.asset("icons/icon-headphones.svg")
                color: MP.Theme.secondaryText
            }
            Image {
                id: art
                anchors.fill: parent
                source: root.player.artUrl ?? ""
                sourceSize: Qt.size(104, 104)
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 1

            Text {
                Layout.fillWidth: true
                text: root.player.title ?? ""
                color: MP.Theme.text
                font.pixelSize: 13
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                visible: text !== ""
                text: root.player.artist ?? ""
                color: MP.Theme.secondaryText
                font.pixelSize: 12
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                visible: !!root.player.source
                text: qsTrId("tray.popup.playing_on").arg(root.player.source ?? "")
                color: MP.Theme.secondaryText
                font.pixelSize: 11
                elide: Text.ElideRight
            }
        }

        // the transport beside the title where there is room (the window), under it in the tray popup
        Transport {
            visible: root.controlsBeside
            spacing: 4
        }

        // Apple Music's star (favorite), for the iPhone's player when it takes it
        QQC2.AbstractButton {
            id: star
            readonly property bool liked: root.player.liked ?? false
            visible: root.player.canLike ?? false
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: 34
            implicitHeight: 34
            Accessible.name: liked ? qsTrId("tray.popup.unfavorite") : qsTrId("tray.popup.favorite")
            onClicked: cppMedia.toggleLike()
            background: Rectangle {
                radius: width / 2
                color: star.pressed ? MP.Theme.fill : star.hovered ? MP.Theme.tertiaryFill : "transparent"
            }
            contentItem: Impl.IconImage {
                sourceSize: Qt.size(20, 20)
                source: MP.Theme.asset(star.liked ? "icons/icon-star-fill.svg" : "icons/icon-star.svg")
                color: star.liked ? MP.Theme.red : MP.Theme.text
            }
        }
    }

    Transport {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: -4
        visible: root.hasPlayer && !root.controlsBeside
    }

    // Volume: Control Center style capsule, the fill is the value; − and + step to the
    // neighbouring multiple of 5 (31 → 35 / 30), so a step never lands on an odd value
    RowLayout {
        Layout.fillWidth: true
        visible: cppMedia.volume >= 0
        spacing: 4

        VolumeStepButton {
            icon.source: MP.Theme.asset("icons/icon-minus.svg")
            enabled: volumeSlider.value > 0
            Accessible.name: qsTrId("tray.popup.volume_down")
            onClicked: cppMedia.setVolume(volumeSlider.neighbour(-1))
        }

        QQC2.Slider {
            id: volumeSlider
            Layout.fillWidth: true
            implicitHeight: 26
            padding: 0
            from: 0
            to: 100
            // the iPhone's volume only moves in its own steps
            stepSize: root.player.volumeStep ?? 1
            snapMode: QQC2.Slider.SnapAlways
            // not while dragged: the slider's own value would come back rounded and pull it away
            Binding on value {
                when: !volumeSlider.pressed
                value: Math.max(0, cppMedia.volume)
                restoreMode: Binding.RestoreNone
            }
            // The next value for − and +: the neighbouring multiple of 5, the iPhone's neighbouring step
            function neighbour(direction) {
                const step = root.player.volumeStep;
                if (step)
                    return Math.round((Math.round(value / step) + direction) * step);
                return direction > 0 ? Math.floor(value / 5) * 5 + 5 : Math.ceil(value / 5) * 5 - 5;
            }
            Accessible.name: qsTrId("tray.popup.volume")
            onMoved: cppMedia.setVolume(value)

            background: Rectangle {
                radius: height / 2
                color: MP.Theme.fill

                Text {
                    id: volumeLabel
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    // the iPhone in its own steps, like its volume buttons
                    text: root.player.volumeStep ? Math.round(volumeSlider.value / root.player.volumeStep) + " / " + Math.round(100 / root.player.volumeStep)
                                                  : qsTrId("format.percent").arg(Math.round(volumeSlider.value))
                    color: MP.Theme.secondaryText
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.features: { "tnum": 1 }
                }
                Rectangle {
                    width: Math.max(parent.height, volumeSlider.visualPosition * parent.width)
                    height: parent.height
                    radius: height / 2
                    color: cppMedia.muted ? MP.Theme.gray : MP.Theme.accent
                    clip: true

                    // the same label in white where the fill covers it
                    Text {
                        x: volumeLabel.x
                        y: volumeLabel.y
                        text: volumeLabel.text
                        color: "white"
                        font: volumeLabel.font
                    }
                }
            }
            handle: null

            // HIG: like Control Center, the speaker is the mute button; muting keeps the level
            QQC2.AbstractButton {
                id: muteButton
                enabled: !root.player.source // the iPhone can't be muted from here, the speaker still shows
                width: parent.height
                height: parent.height
                Accessible.name: cppMedia.muted ? qsTrId("tray.popup.unmute") : qsTrId("tray.popup.mute")
                onClicked: cppMedia.setMuted(!cppMedia.muted)
                background: Rectangle {
                    radius: height / 2
                    color: muteButton.pressed ? Qt.rgba(0, 0, 0, 0.2) : muteButton.hovered ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
                }
                contentItem: Impl.IconImage {
                    sourceSize: Qt.size(14, 14)
                    source: MP.Theme.asset(cppMedia.muted ? "icons/icon-speaker-slash.svg"
                                           : volumeSlider.value > 0 ? "icons/icon-speaker-wave.svg" : "icons/icon-speaker.svg")
                    color: "white"
                }
            }
        }

        VolumeStepButton {
            icon.source: MP.Theme.asset("icons/icon-plus.svg")
            enabled: volumeSlider.value < 100
            Accessible.name: qsTrId("tray.popup.volume_up")
            onClicked: cppMedia.setVolume(volumeSlider.neighbour(1))
        }
    }
}
