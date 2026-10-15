// MyPods
// License: GPL-3.0
// MyCAD's Liquid Glass (GlassPanel.cpp, "Glass: Clear"): the blurred backdrop bent inwards towards the edge like a
// lens with a little colour fringing, more vivid, lightly tinted, a light rim and a faint sheen from the top.

#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 panel;     // the glass in logical pixels
    float radius;
    float margin;   // the blurred backdrop reaches this far beyond the glass on every side
    float dark;
    float tint;     // how much the tint covers: MyCAD's clear glass 0.17 (dark 0.24), more where text is read on it
};
layout(binding = 1) uniform sampler2D source;

float sdRound(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

vec3 blurred(vec2 p)
{
    return texture(source, clamp((p + margin) / (panel + 2.0 * margin), 0.0, 1.0)).rgb;
}

void main()
{
    vec2 p = qt_TexCoord0 * panel;
    vec2 halfSize = panel * 0.5;
    vec2 rel = p - halfSize;
    float r = min(radius, min(halfSize.x, halfSize.y));
    float d = sdRound(rel, halfSize, r);
    vec2 n = normalize(vec2(sdRound(rel + vec2(1.0, 0.0), halfSize, r) - sdRound(rel - vec2(1.0, 0.0), halfSize, r),
                            sdRound(rel + vec2(0.0, 1.0), halfSize, r) - sdRound(rel - vec2(0.0, 1.0), halfSize, r)) + 1e-6);
    float inside = clamp(0.5 - d, 0.0, 1.0);
    bool isDark = dark > 0.5;

    // lens: towards the edge the backdrop is pulled inwards, red and blue a little apart
    float t = -d;
    float bend = 1.0 - smoothstep(0.0, 20.0, t);
    vec2 off = n * bend * bend * 18.0;
    vec3 col = vec3(blurred(p - off * 1.08).r, blurred(p - off).g, blurred(p - off * 0.92).b);
    // vibrancy, then the tint
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(l), col, 1.55);
    col = isDark ? mix(col, vec3(0.12, 0.12, 0.14), tint) : mix(col, vec3(1.0), tint);
    // light rim, brightest top left and bottom right
    float rim = 1.0 - smoothstep(0.0, 1.6, t);
    float spec = rim * (0.3 + 0.7 * abs(dot(n, normalize(vec2(-0.45, -1.0)))));
    col += spec * (isDark ? 0.32 : 0.55) * 1.3;
    // faint sheen from the top
    col += (1.0 - smoothstep(0.0, panel.y, p.y)) * (isDark ? 0.03 : 0.05);
    fragColor = vec4(min(col, 1.0), 1.0) * inside * qt_Opacity;
}
