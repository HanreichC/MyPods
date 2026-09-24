// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import magicpods as MP
import "../components" as Components

Components.ScrollPage {
    id: rootPage

    property var infoData: ({})
    readonly property bool hasInfo: infoData && Object.keys(infoData).length > 0
    readonly property bool backendConnected: !!cppBackend && cppBackend.connected
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
    readonly property var deviceInfoData: capabilities?.deviceInfo ?? null
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

    // [profile id, PipeWire description] -> "AAC", "mSBC (Calls)", ... The codec comes from the
    // description ("… (A2DP Sink, codec AAC)"): the unsuffixed a2dp-sink is whatever codec the
    // headphones do best. The id suffix (a2dp-sink-sbc_xq) is the fallback, the description the last resort.
    function codecLabel(option) {
        var id = String(option[0]), description = String(option[1] ?? "");
        if (id === "off")
            return qsTrId("battery.bluetooth_codec.off");
        var inner = /\(([^()]*)\)\s*$/.exec(description)?.[1] ?? "";
        var last = inner.split(",").pop().trim();
        var codec = inner.indexOf(",") >= 0 && last.indexOf(" ") > 0 ? last.slice(last.indexOf(" ") + 1) : "";
        if (!codec) {
            var suffix = /^(?:a2dp-sink|headset-head-unit)-(.+)$/.exec(id);
            codec = suffix ? suffix[1].replace(/_/g, "-").toUpperCase() : description || id;
        }
        codec = codec.replace(/^MSBC$/i, "mSBC");
        return id.startsWith("headset") ? qsTrId("battery.bluetooth_codec.calls").arg(codec) : codec;
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
            }
        }
        function onConnectedChanged() {
            requestInfo();
        }
    }
    Component.onCompleted: {
        requestInfo();
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

            Components.DeviceImage {
                anchors.centerIn: parent
                width: 168; height: 168
                info: rootPage.infoData
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

    // Noise control: HIG segmented control with a draggable Liquid Glass thumb
    Components.NoiseControl {
        id: noiseControl
        visible: hasInfo && available
        Layout.fillWidth: true
        Layout.topMargin: MP.Units.mediumSpacing
        ancData: rootPage.ancData
        address: rootPage.currentAddress() ?? ""
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
                    lines.push(rootPage.codecLabel(option) + " — " + String(option[1]));
                });
                return lines.join("\n\n");
            }

            Components.Picker {
                model: (rootPage.bluetoothCodec?.options ?? []).map(rootPage.codecLabel)
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
                            if (rootPage.bluetoothCodec.selected !== nextValue)
                                cppBackend.setCapability("bluetoothCodec", rootPage.currentAddress(), nextValue);
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
                model: [qsTrId("battery.noise_level.normal"), qsTrId("battery.noise_level.max")]
                currentIndex: (rootPage.ancData?.level ?? 1) - 1
                enabled: !(rootPage.ancData?.readonly ?? true) && noiseControl.selectedAnc !== noiseControl.ancModes.OFF
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
                    text: qsTrId("format.percent").arg(Math.round(toneVolumeSlider.value))
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
                model: [qsTrId("battery.end_call.twice"), qsTrId("battery.end_call.once")]
                currentIndex: (rootPage.endCallData?.selected === 2) ? 1 : 0
                enabled: false
            }
        }
    }

    // Name, model number, serial and firmware from the headphones; the name is stored on them
    MP.Heading {
        visible: hasInfo && rootPage.deviceInfoData !== null
        level: 5
        Layout.topMargin: MP.Units.largeSpacing
        Layout.leftMargin: MP.Units.largeSpacing
        text: qsTrId("battery.device_info.header")
    }

    Components.Card {
        visible: hasInfo && rootPage.deviceInfoData !== null

        MP.FormRow {
            Layout.fillWidth: true
            label: qsTrId("battery.device_info.name")

            QQC2.TextField {
                implicitWidth: rootPage.mWidth
                text: rootPage.deviceInfoData?.name ?? ""
                enabled: !(rootPage.deviceInfoData?.readonly ?? true)
                maximumLength: rootPage.deviceInfoData?.maxNameBytes ?? 32
                Accessible.name: qsTrId("battery.device_info.name")
                onEditingFinished: {
                    const name = text.trim();
                    // the core checks the byte length (UTF-8) again and ignores what doesn't fit
                    if (rootPage.deviceInfoData && name !== "" && name !== rootPage.deviceInfoData.name)
                        cppBackend.setCapability("deviceInfo", rootPage.currentAddress(), name, "name");
                }
            }
        }

        Repeater {
            model: [
                { label: qsTrId("battery.device_info.model"), value: rootPage.deviceInfoData?.model ?? "" },
                { label: qsTrId("battery.device_info.serial"), value: rootPage.deviceInfoData?.serial ?? "" },
                { label: qsTrId("battery.device_info.firmware"), value: rootPage.deviceInfoData?.firmware ?? "" }
            ]
            delegate: MP.FormRow {
                required property var modelData
                Layout.fillWidth: true
                visible: modelData.value !== ""
                label: modelData.label

                MP.Label {
                    color: MP.Theme.secondaryText
                    text: modelData.value
                    textFormat: Text.PlainText
                }
            }
        }
    }
}
