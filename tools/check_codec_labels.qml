// Self-check: BatteryPage's codecLabel turns PipeWire card profiles into readable codec names.
// It evaluates the function straight from BatteryPage.qml, so it tests the shipped code.
// Run: QT_FORCE_STDERR_LOGGING=1 QML_XHR_ALLOW_FILE_READ=1 qml6 tools/check_codec_labels.qml
import QtQml

QtObject {
    Component.onCompleted: {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", Qt.resolvedUrl("../ui/src/app/qml/pages/BatteryPage.qml"), false);
        xhr.send();
        var source = /function codecLabel[\s\S]*?\n    }\n/.exec(xhr.responseText)[0];

        var strings = {"battery.bluetooth_codec.off": "Off", "battery.bluetooth_codec.calls": "%1 (calls)"};
        var qsTrId = function (id) { return strings[id]; };
        var codecLabel = eval("(" + source + ")");

        var cases = [
            // German PipeWire, the Zik (the unsuffixed a2dp-sink is AAC there)
            [["a2dp-sink", "High Fidelity-Wiedergabe (A2DP-Senke, Codec AAC)"], "AAC"],
            [["a2dp-sink-sbc_xq", "High Fidelity-Wiedergabe (A2DP-Senke, Codec SBC-XQ)"], "SBC-XQ"],
            [["headset-head-unit", "Sprechkopfhörer-Einheit (HSP/HFP, Codec MSBC)"], "mSBC (calls)"],
            // English PipeWire, AirPods-style list and codecs with a space in the name
            [["a2dp-sink-aac", "High Fidelity Playback (A2DP Sink, codec AAC)"], "AAC"],
            [["a2dp-sink-sbc", "High Fidelity Playback (A2DP Sink, codec SBC)"], "SBC"],
            [["headset-head-unit-cvsd", "Headset Head Unit (HSP/HFP, codec CVSD)"], "CVSD (calls)"],
            [["a2dp-sink", "High Fidelity Playback (A2DP Sink, codec aptX HD)"], "aptX HD"],
            // no codec in the description: id suffix, then the description itself
            [["a2dp-sink-ldac", "High Fidelity Playback (A2DP Sink)"], "LDAC"],
            [["a2dp-sink", "High Fidelity Playback (A2DP Sink)"], "High Fidelity Playback (A2DP Sink)"],
            [["off", "Aus"], "Off"],
        ];
        var failed = 0;
        cases.forEach(function (c) {
            var got = codecLabel(c[0]);
            if (got !== c[1]) {
                console.log("FAIL", JSON.stringify(c[0]), "->", JSON.stringify(got), "expected", JSON.stringify(c[1]));
                failed++;
            }
        });
        console.log(failed ? failed + " FAILED" : "PASS: " + cases.length + " profiles");
        Qt.exit(failed ? 1 : 0);
    }
}
