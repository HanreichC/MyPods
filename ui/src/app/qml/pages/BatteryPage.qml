// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Controls.impl as Impl
import QtQuick.Layouts 1.15
import QtQuick.Effects
import magicpods as MP
import "../components" as Components

Components.ScrollPage {
    id: rootPage

    property var infoData: ({})
    readonly property bool hasInfo: infoData && Object.keys(infoData).length > 0
    readonly property bool backendConnected: !!cppBackend && cppBackend.connected
    property int selectedAnc: ancData?.selected ?? 0
    readonly property var ancModes: ({
            OFF: 1,
            TRANSPARENCY: 2,
            ADAPTIVE: 4,
            WIND: 8,
            ANC: 16
        })
    readonly property var capabilities: infoData?.capabilities ?? null
    readonly property var ancData: capabilities?.anc ?? null
    readonly property var conversationAwarenessData: capabilities?.conversationAwareness ?? null
    readonly property var personalizedVolumeData: capabilities?.personalizedVolume ?? null
    readonly property var ancOneAirPodData: capabilities?.ancOneAirPod ?? null
    readonly property var volumeSwipeData: capabilities?.volumeSwipe ?? null
    readonly property var adaptiveAudioNoiseData: capabilities?.adaptiveAudioNoise ?? null
    readonly property var pressAndHoldDurationData: capabilities?.pressAndHoldDuration ?? null
    readonly property var pressSpeedData: capabilities?.pressSpeed ?? null
    readonly property var toneVolumeData: capabilities?.toneVolume ?? null
    readonly property var volumeSwipeLengthData: capabilities?.volumeSwipeLength ?? null
    readonly property var endCallData: capabilities?.endCall ?? null
    readonly property var bluetoothCodec: capabilities?.bluetoothCodec ?? null
    readonly property var spatialAudioData: capabilities?.spatialAudio ?? null
    readonly property var equalizerData: capabilities?.equalizer ?? null
    readonly property var autoSwitchData: capabilities?.autoSwitch ?? null
    readonly property var earDetectionData: capabilities?.earDetection ?? null
    readonly property int mWidth: MP.Units.gridUnit * 10

    // Parrot Zik: plain switches and lists. The core sends selected as bool (switch) or index (list);
    // a row with dependsOn is only editable while that switch is on.
    readonly property var zikSwitches: [
        { key: "concertHall", label: qsTrId("battery.concert_hall") },
        { key: "smartAudioTune", label: qsTrId("battery.smart_audio_tune") },
        { key: "ancPhoneMode", label: qsTrId("battery.anc_phone_mode") },
        { key: "voicePrompts", label: qsTrId("battery.voice_prompts") },
        { key: "autoConnection", label: qsTrId("battery.auto_connection_zik") }
    ]
    readonly property var zikLists: [
        { key: "concertHallRoom", dependsOn: "concertHall", label: qsTrId("battery.concert_hall_room"),
          options: [qsTrId("battery.concert_hall_room.silent"), qsTrId("battery.concert_hall_room.living"), qsTrId("battery.concert_hall_room.jazz"), qsTrId("battery.concert_hall_room.concert")] },
        { key: "concertHallAngle", dependsOn: "concertHall", label: qsTrId("battery.concert_hall_angle"),
          options: ["30°", "60°", "90°", "120°", "150°", "180°"] },
        { key: "autoPowerOff", label: qsTrId("battery.auto_power_off"),
          options: [qsTrId("battery.auto_power_off.never"), "5 min", "10 min", "15 min", "30 min", "60 min"] }
    ]

    readonly property bool hasCapabilities: !!capabilities && [ancData, conversationAwarenessData, personalizedVolumeData, ancOneAirPodData, volumeSwipeData, adaptiveAudioNoiseData, pressAndHoldDurationData, pressSpeedData, toneVolumeData, volumeSwipeLengthData, endCallData, bluetoothCodec, spatialAudioData, equalizerData, autoSwitchData, earDetectionData].some(function (v) {
        return v !== null;
    }) || zikSwitches.concat(zikLists).some(z => capabilities?.[z.key] !== undefined)

    title: hasInfo ? (infoData?.name ?? "") : qsTrId("menu.battery")

    function currentAddress() {
        return infoData?.address;
    }

    function requestInfo() {
        if (cppBackend && cppBackend.connected) {
            cppBackend.getInfo();
        }
    }

    Connections {
        target: cppBackend
        enabled: !!cppBackend
        function onDataReceived(json) {
            if (!json || Object.keys(json).length === 0) {
                rootPage.infoData = ({});
            } else if (json.info) {
                rootPage.infoData = json.info;
                // while a switch is pending, only its confirmation may move the thumb
                var anc = json.info?.capabilities?.anc?.selected;
                if (anc !== undefined && (!ancRevert.running || anc === rootPage.selectedAnc)) {
                    rootPage.selectedAnc = anc;
                    ancRevert.stop();
                }
            }
        }
        function onConnectedChanged() {
            requestInfo();
        }
    }
    Component.onCompleted: {
        requestInfo();
    }

    readonly property var ancButtons: [
        { mode: ancModes.OFF, icon: "icon-off.svg", text: qsTrId("battery.anc_off") },
        { mode: ancModes.TRANSPARENCY, icon: "icon-tra.svg", text: qsTrId("battery.anc_tra") },
        { mode: ancModes.ADAPTIVE, icon: "icon-adaptive.svg", text: qsTrId("battery.anc_adaptive") },
        { mode: ancModes.WIND, icon: "icon-wind.svg", text: qsTrId("battery.anc_wind") },
        { mode: ancModes.ANC, icon: "icon-noise.svg", text: qsTrId("battery.anc_anc") }
    ]

    // Optimistic: the thumb moves at once, ancData keeps the device's last confirmed mode.
    // The core only broadcasts when the mode actually changes, so if the headphones ignore
    // the command nothing arrives and ancRevert snaps the thumb back to the real mode.
    function setAnc(mode) {
        if (!rootPage.ancData)
            return;
        rootPage.selectedAnc = mode;
        cppBackend.setAnc(rootPage.currentAddress(), mode);
        ancRevert.restart();
    }

    Timer {
        id: ancRevert
        interval: 2000
        onTriggered: rootPage.selectedAnc = rootPage.ancData?.selected ?? rootPage.selectedAnc
    }

    Components.HelpMessage {
        visible: !hasInfo
        iconSource: MP.Theme.asset("icons/illustration-connect.svg")
        titleText: qsTrId("battery.connect_headphones.header")
        bodyText: qsTrId("battery.connect_headphones.description")
    }

    // Hero: device render + battery rings
    Components.Card {
        visible: hasInfo
        padding: MP.Units.hugeSpacing
        verticalPadding: MP.Units.hugeSpacing
        spacing: MP.Units.largeSpacing

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 168

            Image {
                id: headphonesImage
                property var candidates: []
                property int candidateIndex: 0
                anchors.centerIn: parent
                width: 168; height: 168
                sourceSize: Qt.size(512, 512)
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                source: ""

                function buildCandidates() {
                    var v = infoData?.vendor ?? 0;
                    var m = infoData?.model ?? 0;
                    var c = infoData?.color ?? 0;
                    var basePath = "headphones/" + v + "_";
                    candidates = [
                        MP.Theme.asset(basePath + m + "_" + c + ".png"),
                        MP.Theme.asset(basePath + m + ".png"),
                        MP.Theme.asset("headphones/0_0_0.png")
                    ];
                    candidateIndex = 0;
                    tryNext();
                }

                function tryNext() {
                    if (candidateIndex < candidates.length)
                        source = candidates[candidateIndex];
                }

                onStatusChanged: {
                    if (status === Image.Error) {
                        candidateIndex += 1;
                        tryNext();
                    }
                }

                Component.onCompleted: headphonesImage.buildCandidates()
                Connections {
                    target: rootPage
                    function onInfoDataChanged() {
                        headphonesImage.buildCandidates();
                    }
                }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: MP.Units.hugeSpacing

            Components.BatteryRing {
                name: qsTrId("battery.battery_single")
                battery: rootPage.capabilities?.battery?.single?.battery ?? 0
                isCharging: rootPage.capabilities?.battery?.single?.charging ?? false
                status: rootPage.capabilities?.battery?.single?.status ?? 0
            }
            Components.BatteryRing {
                name: qsTrId("battery.battery_left")
                battery: rootPage.capabilities?.battery?.left?.battery ?? 0
                isCharging: rootPage.capabilities?.battery?.left?.charging ?? false
                status: rootPage.capabilities?.battery?.left?.status ?? 0
            }
            Components.BatteryRing {
                name: qsTrId("battery.battery_right")
                battery: rootPage.capabilities?.battery?.right?.battery ?? 0
                isCharging: rootPage.capabilities?.battery?.right?.charging ?? false
                status: rootPage.capabilities?.battery?.right?.status ?? 0
            }
            Components.BatteryRing {
                name: qsTrId("battery.battery_case")
                battery: rootPage.capabilities?.battery?.case?.battery ?? 0
                isCharging: rootPage.capabilities?.battery?.case?.charging ?? false
                status: rootPage.capabilities?.battery?.case?.status ?? 0
            }
        }
    }

    // Like the Mac's "Moved to iPhone" banner, with the way back
    Components.Card {
        visible: hasInfo && rootPage.autoSwitchData !== null && !(rootPage.autoSwitchData?.owns ?? true)

        MP.FormRow {
            Layout.fillWidth: true
            label: rootPage.autoSwitchData?.source ? qsTrId("battery.moved").arg(rootPage.autoSwitchData.source) : qsTrId("battery.moved_other")
            iconSource: MP.Theme.asset("icons/icon-headphones.svg")

            QQC2.Button {
                text: qsTrId("battery.move_here")
                highlighted: true
                onClicked: cppBackend.setCapability("autoSwitch", rootPage.currentAddress(), true, "takeover")
            }
        }
    }

    // Noise control: HIG segmented control with a draggable thumb. Per HIG, content-layer
    // sliders only turn into Liquid Glass while touched: at rest the thumb is solid, while
    // pressed/dragged it lifts into a translucent lens (grows, rim + sheen, magnifies the icon
    // below) and springs into the nearest segment on release.
    Rectangle {
        id: ancTrack
        readonly property var modes: rootPage.ancButtons.filter(b => (rootPage.ancData?.options ?? 0) & b.mode)
        readonly property int selectedIndex: modes.findIndex(b => b.mode === rootPage.selectedAnc)
        readonly property real segmentWidth: ancLane.width / Math.max(1, modes.length)
        readonly property bool glass: ancMouse.pressed
        // segment under the thumb's centre, i.e. what a release would select
        readonly property int thumbIndex: Math.max(0, Math.min(modes.length - 1, Math.floor((ancThumb.x + segmentWidth / 2) / segmentWidth)))

        function step(delta) {
            var i = Math.max(0, Math.min(modes.length - 1, selectedIndex + delta));
            if (i !== selectedIndex)
                rootPage.setAnc(modes[i].mode);
        }

        visible: hasInfo && rootPage.ancData !== null && modes.length > 0
        enabled: !(rootPage.ancData?.readonly ?? true)
        opacity: enabled ? 1 : 0.4
        Layout.fillWidth: true
        Layout.topMargin: MP.Units.mediumSpacing
        implicitHeight: 56
        radius: height / 2
        color: MP.Theme.tertiaryFill
        border.width: activeFocus ? 2 : 0
        border.color: MP.Theme.accent

        activeFocusOnTab: true
        Keys.onLeftPressed: step(-1)
        Keys.onRightPressed: step(1)
        Accessible.role: Accessible.PageTabList
        Accessible.name: rootPage.ancButtons.find(b => b.mode === rootPage.selectedAnc)?.text ?? ""

        Item {
            id: ancLane
            anchors.fill: parent
            anchors.margins: 4

            Rectangle {
                id: ancThumb
                visible: ancTrack.selectedIndex >= 0 || ancMouse.dragging
                x: ancMouse.dragging ? Math.max(0, Math.min(ancLane.width - width, ancMouse.mouseX - width / 2))
                                     : Math.max(0, ancTrack.selectedIndex) * width
                width: ancTrack.segmentWidth
                height: parent.height
                radius: height / 2
                scale: ancTrack.glass ? 1.14 : 1
                color: ancTrack.glass ? Qt.rgba(1, 1, 1, MP.Theme.dark ? 0.12 : 0.3) : MP.Theme.thumb
                border.width: 1
                border.color: ancTrack.glass ? MP.Theme.glassBorder : "transparent"

                Behavior on x { enabled: !ancMouse.dragging; NumberAnimation { duration: 380; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }
                Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutBack; easing.overshoot: 2 } }
                Behavior on color { ColorAnimation { duration: 180 } }
                Behavior on border.color { ColorAnimation { duration: 180 } }

                // specular sheen on the upper half of the lens
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    opacity: ancTrack.glass ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: 180 } }
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
                            color: MP.Theme.tertiaryFill
                            visible: parent.hovered && index !== ancTrack.selectedIndex
                        }

                        Impl.IconImage {
                            anchors.centerIn: parent
                            sourceSize: Qt.size(24, 24)
                            source: MP.Theme.asset("icons/" + modelData.icon)
                            color: MP.Theme.text
                            // lens magnification under the glass thumb
                            scale: ancTrack.glass && index === ancTrack.thumbIndex ? 1.2 : 1
                            Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                        }

                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: 500
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
                        rootPage.setAnc(ancTrack.modes[i].mode);
                }
                onCanceled: dragging = false
            }
        }
    }

    MP.Label {
        visible: hasInfo && rootPage.ancData !== null
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        color: MP.Theme.secondaryText
        font.pixelSize: 13
        font.weight: Font.Medium
        text: rootPage.ancButtons.find(b => b.mode === rootPage.selectedAnc)?.text ?? ""
    }

    MP.Heading {
        visible: hasInfo && hasCapabilities
        level: 5
        Layout.topMargin: MP.Units.largeSpacing
        Layout.leftMargin: MP.Units.largeSpacing
        text: qsTrId("battery.capabilities.header")
    }

    Components.Card {
        visible: hasInfo && hasCapabilities

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.bluetoothCodec !== null
            label: qsTrId("battery.bluetooth_codec")
            tooltip: {
                var options = rootPage.bluetoothCodec?.options || [];
                var lines = [];
                options.forEach(function (option) {
                    if (!option || option.length < 2) return;
                    lines.push(String(option[0]) + " — " + String(option[1]));
                });
                return lines.join("\n\n");
            }

            Components.Picker {
                implicitWidth: rootPage.mWidth
                model: (rootPage.bluetoothCodec?.options ?? []).map(function (option) {
                    return option[0];
                })
                currentIndex: {
                    var options = rootPage.bluetoothCodec?.options || [];
                    for (var i = 0; i < options.length; i++) {
                        if (options[i][0] === rootPage.bluetoothCodec.selected) return i;
                    }
                    return -1;
                }
                enabled: !(rootPage.bluetoothCodec?.readonly ?? true)
                onActivated: {
                    if (rootPage.bluetoothCodec && currentIndex >= 0) {
                        var options = rootPage.bluetoothCodec.options || [];
                        if (options[currentIndex]) {
                            var nextValue = options[currentIndex][0];
                            if (rootPage.bluetoothCodec.selected !== nextValue) {
                                rootPage.bluetoothCodec.selected = nextValue;
                                cppBackend.setCapability("bluetoothCodec", rootPage.currentAddress(), nextValue);
                            }
                        }
                    }
                }
            }
        }

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.spatialAudioData !== null
            label: qsTrId("battery.spatial_audio")

            Components.Picker {
                implicitWidth: rootPage.mWidth
                model: [qsTrId("battery.spatial_audio.off"), qsTrId("battery.spatial_audio.fixed")].concat((rootPage.spatialAudioData?.headTracking ?? true) ? [qsTrId("battery.spatial_audio.head_tracked")] : [])
                currentIndex: rootPage.spatialAudioData?.selected ?? 0
                enabled: !(rootPage.spatialAudioData?.readonly ?? true)
                onActivated: {
                    if (rootPage.spatialAudioData) {
                        rootPage.spatialAudioData.selected = currentIndex;
                        cppBackend.setCapability("spatialAudio", rootPage.currentAddress(), currentIndex);
                    }
                }
            }
        }

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.equalizerData !== null
            label: qsTrId("battery.equalizer")

            Components.Picker {
                implicitWidth: rootPage.mWidth
                model: rootPage.equalizerData?.options ?? []
                currentIndex: (rootPage.equalizerData?.options ?? []).indexOf(rootPage.equalizerData?.selected)
                enabled: !(rootPage.equalizerData?.readonly ?? true)
                onActivated: {
                    if (rootPage.equalizerData && currentIndex >= 0) {
                        rootPage.equalizerData.selected = rootPage.equalizerData.options[currentIndex];
                        cppBackend.setCapability("equalizer", rootPage.currentAddress(), rootPage.equalizerData.selected);
                    }
                }
            }
        }

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.autoSwitchData !== null
            label: qsTrId("battery.auto_switch")

            Components.Picker {
                implicitWidth: rootPage.mWidth
                model: [qsTrId("battery.auto_switch.automatically"), qsTrId("battery.auto_switch.last_connected")]
                currentIndex: rootPage.autoSwitchData?.selected ?? 0
                enabled: !(rootPage.autoSwitchData?.readonly ?? true)
                onActivated: {
                    if (rootPage.autoSwitchData) {
                        rootPage.autoSwitchData.selected = currentIndex;
                        cppBackend.setCapability("autoSwitch", rootPage.currentAddress(), currentIndex);
                    }
                }
            }
        }

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.earDetectionData !== null
            label: qsTrId("battery.ear_detection")

            Components.Toggle {
                checked: rootPage.earDetectionData?.selected ?? true
                enabled: !(rootPage.earDetectionData?.readonly ?? true)
                onToggled: {
                    if (rootPage.earDetectionData) {
                        rootPage.earDetectionData.selected = checked;
                        cppBackend.setCapability("earDetection", rootPage.currentAddress(), checked);
                    }
                }
            }
        }

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.ancData?.level !== undefined
            label: qsTrId("battery.noise_level")

            Components.Picker {
                implicitWidth: rootPage.mWidth
                model: [qsTrId("battery.noise_level.normal"), qsTrId("battery.noise_level.max")]
                currentIndex: (rootPage.ancData?.level ?? 1) - 1
                enabled: !(rootPage.ancData?.readonly ?? true) && rootPage.selectedAnc !== rootPage.ancModes.OFF
                onActivated: cppBackend.setCapability("anc", rootPage.currentAddress(), currentIndex + 1, "level")
            }
        }

        Repeater {
            model: rootPage.zikSwitches
            delegate: MP.FormRow {
                id: row
                required property var modelData
                readonly property var cap: rootPage.capabilities?.[modelData.key] ?? null
                Layout.fillWidth: true
                visible: cap !== null
                label: modelData.label

                Components.Toggle {
                    checked: row.cap?.selected ?? false
                    enabled: !(row.cap?.readonly ?? true)
                    onToggled: {
                        row.cap.selected = checked;
                        cppBackend.setCapability(row.modelData.key, rootPage.currentAddress(), checked);
                    }
                }
            }
        }

        Repeater {
            model: rootPage.zikLists
            delegate: MP.FormRow {
                id: row
                required property var modelData
                readonly property var cap: rootPage.capabilities?.[modelData.key] ?? null
                Layout.fillWidth: true
                visible: cap !== null
                label: modelData.label

                Components.Picker {
                    implicitWidth: rootPage.mWidth
                    model: row.modelData.options
                    currentIndex: row.cap?.selected ?? -1
                    enabled: !(row.cap?.readonly ?? true)
                             && (!row.modelData.dependsOn || (rootPage.capabilities?.[row.modelData.dependsOn]?.selected ?? false))
                    onActivated: {
                        row.cap.selected = currentIndex;
                        cppBackend.setCapability(row.modelData.key, rootPage.currentAddress(), currentIndex);
                    }
                }
            }
        }

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.conversationAwarenessData !== null
            label: qsTrId("battery.conversation_awareness")

            Components.Toggle {
                checked: rootPage.conversationAwarenessData?.selected ?? false
                enabled: !(rootPage.conversationAwarenessData?.readonly ?? true)
                onToggled: {
                    if (rootPage.conversationAwarenessData) {
                        rootPage.conversationAwarenessData.selected = checked;
                        cppBackend.setCapability("conversationAwareness", rootPage.currentAddress(), checked);
                    }
                }
            }
        }

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.personalizedVolumeData !== null
            label: qsTrId("battery.personalized_volume")

            Components.Toggle {
                checked: rootPage.personalizedVolumeData?.selected ?? false
                enabled: !(rootPage.personalizedVolumeData?.readonly ?? true)
                onToggled: {
                    if (rootPage.personalizedVolumeData) {
                        rootPage.personalizedVolumeData.selected = checked;
                        cppBackend.setCapability("personalizedVolume", rootPage.currentAddress(), checked);
                    }
                }
            }
        }

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.adaptiveAudioNoiseData !== null
            label: qsTrId("battery.adaptive_audio_noise")

            Components.Picker {
                implicitWidth: rootPage.mWidth
                model: [qsTrId("battery.adaptive_audio_noise.more"), qsTrId("battery.adaptive_audio_noise.default"), qsTrId("battery.adaptive_audio_noise.less")]
                currentIndex: {
                    const val = rootPage.adaptiveAudioNoiseData?.selected;
                    if (val === 0) return 0;
                    if (val === 50) return 1;
                    if (val === 100) return 2;
                    return 1;
                }
                enabled: !(rootPage.adaptiveAudioNoiseData?.readonly ?? true)
                onActivated: {
                    if (rootPage.adaptiveAudioNoiseData) {
                        if (currentIndex === 0) {
                            rootPage.adaptiveAudioNoiseData.selected = 0;
                        } else if (currentIndex === 1) {
                            rootPage.adaptiveAudioNoiseData.selected = 50;
                        } else {
                            rootPage.adaptiveAudioNoiseData.selected = 100;
                        }
                        cppBackend.setCapability("adaptiveAudioNoise", rootPage.currentAddress(), rootPage.adaptiveAudioNoiseData.selected);
                    }
                }
            }
        }

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.ancOneAirPodData !== null
            label: qsTrId("battery.anc_one_airpod")

            Components.Toggle {                        
                checked: rootPage.ancOneAirPodData?.selected ?? false
                enabled: !(rootPage.ancOneAirPodData?.readonly ?? true)
                onToggled: {
                    if (rootPage.ancOneAirPodData) {
                        rootPage.ancOneAirPodData.selected = checked;
                        cppBackend.setCapability("ancOneAirPod", rootPage.currentAddress(), checked);
                    }
                }
            }
        }

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.pressAndHoldDurationData !== null
            label: qsTrId("battery.press_and_hold_duration")

            Components.Picker {
                implicitWidth: rootPage.mWidth
                model: [qsTrId("battery.press_and_hold_duration.default"), qsTrId("battery.press_and_hold_duration.shorter"), qsTrId("battery.press_and_hold_duration.shortest")]
                currentIndex: rootPage.pressAndHoldDurationData?.selected ?? 0
                enabled: !(rootPage.pressAndHoldDurationData?.readonly ?? true)
                onActivated: {
                    if (rootPage.pressAndHoldDurationData) {
                        rootPage.pressAndHoldDurationData.selected = currentIndex;
                        cppBackend.setCapability("pressAndHoldDuration", rootPage.currentAddress(), currentIndex);
                    }
                }
            }
        }

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.pressSpeedData !== null
            label: qsTrId("battery.press_speed")

            Components.Picker {
                implicitWidth: rootPage.mWidth
                model: [qsTrId("battery.press_speed.default"), qsTrId("battery.press_speed.slower"), qsTrId("battery.press_speed.slowest")]
                currentIndex: rootPage.pressSpeedData?.selected ?? 0
                enabled: !(rootPage.pressSpeedData?.readonly ?? true)
                onActivated: {
                    if (rootPage.pressSpeedData) {
                        rootPage.pressSpeedData.selected = currentIndex;
                        cppBackend.setCapability("pressSpeed", rootPage.currentAddress(), currentIndex);
                    }
                }
            }
        }

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.toneVolumeData !== null
            label: qsTrId("battery.tone_volume")

            RowLayout {
                spacing: MP.Units.smallSpacing

                QQC2.Slider {
                    id: toneVolumeSlider
                    implicitWidth: rootPage.mWidth - 48
                    from: 15
                    to: 125
                    value: rootPage.toneVolumeData?.selected ?? 50
                    enabled: !(rootPage.toneVolumeData?.readonly ?? true)
                    onMoved: {
                        if (rootPage.toneVolumeData) {
                            rootPage.toneVolumeData.selected = Math.round(value);
                        }
                    }
                    onPressedChanged: {
                        if (!pressed && rootPage.toneVolumeData) {
                            cppBackend.setCapability("toneVolume", rootPage.currentAddress(), rootPage.toneVolumeData.selected);
                        }
                    }
                }

                MP.Label {
                    Layout.preferredWidth: 44
                    horizontalAlignment: Text.AlignRight
                    color: MP.Theme.secondaryText
                    text: Math.round(toneVolumeSlider.value) + "%"
                }
            }
        }

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.volumeSwipeData !== null
            label: qsTrId("battery.volume_swipe")

            Components.Toggle {
                id: volumeSwipe
                checked: rootPage.volumeSwipeData?.selected ?? false
                enabled: !(rootPage.volumeSwipeData?.readonly ?? true)
                onToggled: {
                    if (rootPage.volumeSwipeData) {
                        rootPage.volumeSwipeData.selected = checked;
                        cppBackend.setCapability("volumeSwipe", rootPage.currentAddress(), checked);
                    }
                }
            }
        }

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.volumeSwipeLengthData !== null
            label: qsTrId("battery.volume_swipe_length")

            Components.Picker {
                implicitWidth: rootPage.mWidth
                model: [qsTrId("battery.volume_swipe_length.default"), qsTrId("battery.volume_swipe_length.longer"), qsTrId("battery.volume_swipe_length.longest")]
                currentIndex: rootPage.volumeSwipeLengthData?.selected ?? 0
                enabled: (!(rootPage.volumeSwipeLengthData?.readonly ?? true) && volumeSwipe.checked)
                onActivated: {
                    if (rootPage.volumeSwipeLengthData) {
                        rootPage.volumeSwipeLengthData.selected = currentIndex;
                        cppBackend.setCapability("volumeSwipeLength", rootPage.currentAddress(), currentIndex);
                    }
                }
            }
        }

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.endCallData !== null
            label: qsTrId("battery.end_call")

            Components.Picker {
                implicitWidth: rootPage.mWidth
                model: [qsTrId("battery.end_call.twice"), qsTrId("battery.end_call.once")]
                currentIndex: (rootPage.endCallData?.selected === 3) ? 1 : 0
                enabled: !(rootPage.endCallData?.readonly ?? true)
                onActivated: {
                    if (rootPage.endCallData) {
                        rootPage.endCallData.selected = currentIndex === 0 ? 2 : 3;
                        cppBackend.setCapability("endCall", rootPage.currentAddress(), rootPage.endCallData.selected);
                    }
                }
            }
        }

        MP.FormRow {
            Layout.fillWidth: true
            visible: rootPage.endCallData !== null
            label: qsTrId("battery.mute_unmute")

            Components.Picker {
                implicitWidth: rootPage.mWidth
                model: [qsTrId("battery.end_call.twice"), qsTrId("battery.end_call.once")]
                currentIndex: (rootPage.endCallData?.selected === 2) ? 1 : 0
                enabled: false
            }
        }
    }
}
