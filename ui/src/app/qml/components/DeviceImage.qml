// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick
import magicpods as MP

// Render of the device from its info: exact color, then the model, then a generic pair.
Image {
    id: root
    property var info: ({})
    // info is replaced on every update; only a different device or color needs a new image
    readonly property string key: (info?.vendor ?? 0) + "_" + (info?.model ?? 0) + "_" + (info?.color ?? 0)
    property var candidates: []
    property int candidateIndex: 0
    sourceSize: Qt.size(512, 512)
    fillMode: Image.PreserveAspectFit
    smooth: true
    mipmap: true

    function buildCandidates() {
        var v = info?.vendor ?? 0;
        var m = info?.model ?? 0;
        var c = info?.color ?? 0;
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
    onKeyChanged: buildCandidates()
    Component.onCompleted: buildCandidates()
}
