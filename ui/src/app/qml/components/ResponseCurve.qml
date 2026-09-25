// MyPods
// License: GPL-3.0

import QtQuick
import magicpods as MP

// What the effects do to the sound: dB over frequency (log axis) per ear, as the core computes it
// (equalizer capability "response"). The right ear is drawn only where it differs (hearing profile).
Canvas {
    id: root

    property var response: null // { frequencies: [...], left: [...], right: [...] }
    readonly property var freqs: response?.frequencies ?? []
    readonly property bool splitEars: (response?.left ?? []).some((v, i) => Math.abs(v - (response?.right?.[i] ?? v)) > 0.05)
    // +-12 dB, more when a curve goes beyond
    readonly property real range: Math.max(12, Math.ceil(Math.max(0, ...(response?.left ?? []).concat(response?.right ?? []).map(Math.abs)) / 6) * 6)

    implicitHeight: 140
    Accessible.role: Accessible.Graphic

    function xOf(f) { return width * Math.log(f / 20) / Math.log(1000); }
    function yOf(db) { return height / 2 - db / range * (height / 2 - 8); }

    onResponseChanged: requestPaint()
    onWidthChanged: requestPaint()
    Connections {
        target: MP.Theme
        function onDarkChanged() { root.requestPaint(); }
    }

    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        ctx.font = "11px sans-serif";
        ctx.lineWidth = 1;

        // grid: 0 dB and every 6 dB, 100 Hz / 1 kHz / 10 kHz
        ctx.strokeStyle = MP.Theme.separator;
        ctx.fillStyle = MP.Theme.secondaryText;
        for (let db = -range; db <= range; db += 6) {
            ctx.globalAlpha = db === 0 ? 1 : 0.5;
            ctx.beginPath();
            ctx.moveTo(0, yOf(db));
            ctx.lineTo(width, yOf(db));
            ctx.stroke();
        }
        ctx.globalAlpha = 1;
        ctx.fillText("+" + range + " dB", 2, 11);
        ctx.fillText("−" + range + " dB", 2, height - 2);
        [[100, "100"], [1000, "1k"], [10000, "10k"]].forEach(([f, text]) => {
            ctx.beginPath();
            ctx.moveTo(xOf(f), 0);
            ctx.lineTo(xOf(f), height);
            ctx.stroke();
            ctx.fillText(text, xOf(f) + 3, height - 2);
        });

        const curve = (values, color) => {
            if (!values || values.length !== freqs.length || !freqs.length)
                return;
            ctx.strokeStyle = color;
            ctx.lineWidth = 2.5;
            ctx.beginPath();
            values.forEach((db, i) => i ? ctx.lineTo(xOf(freqs[i]), yOf(db)) : ctx.moveTo(xOf(freqs[i]), yOf(db)));
            ctx.stroke();
        };
        if (splitEars)
            curve(response.right, MP.Theme.orange);
        curve(response?.left, MP.Theme.accent);
    }
}
