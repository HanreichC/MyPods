// MyPods
// License: GPL-3.0
// MyStyle Liquid Glass (from MyPods/MyCAD's "clear glass"): the blurred backdrop bent inwards towards the edge like
// a lens with a little colour fringing, more vivid, tinted by the glass level, a specular rim brightest top left and
// bottom right, the iOS 27 darkened outermost pixel, and a faint sheen from the top.
// Compile: qsb --qt6 -o glass.frag.qsb glass.frag   (CMake: qt_add_shaders)

#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 panel;         // the glass in logical pixels
    float radius;
    float margin;       // the blurred backdrop reaches this far beyond the glass on every side
    float dark;
    float tint;         // how much tintColor covers the backdrop: Theme.glassTintAmount()
    float lensZone;     // px from the edge in which the lens bends (thickness)
    float lensStrength; // px the backdrop is pulled in at the very edge (thickness)
    float edgeBlur;     // px the backdrop additionally blurs towards the edge (thickness)
    float chroma;       // red/blue split of the lens
    float saturation;   // vibrancy
    float rim;          // specular rim strength
    float edge;         // darkening of the outermost pixel (Theme.glassEdge.a)
    vec4 tintColor;     // Theme.glassTint
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

// the backdrop seen through the glass at p, shifted by off; red and blue a little apart (dispersion)
vec3 refracted(vec2 p, vec2 off)
{
    return vec3(blurred(p - off * (1.0 + chroma)).r, blurred(p - off).g, blurred(p - off * (1.0 - chroma)).b);
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
    float t = -d; // how far into the glass

    // lens like a rounded glass edge (quarter circle): the slope, and with it the refraction, stays gentle inside the
    // lens zone and turns steep at the very edge; there the curved glass also scatters, the backdrop blurs more
    float u = clamp(1.0 - t / lensZone, 0.0, 1.0);
    float k = 0.97 * u;
    vec2 off = n * (k / sqrt(1.0 - k * k) / 3.99) * lensStrength;
    vec3 col = refracted(p, off);
    float scatter = u * u * edgeBlur;
    if (scatter > 0.5) {
        for (int i = 0; i < 8; ++i) {
            float a = float(i) * 0.7853982;
            col += refracted(p + scatter * vec2(cos(a), sin(a)), off);
        }
        col /= 9.0;
    }

    // vibrancy, then the tint
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(l), col, saturation);
    col = mix(col, tintColor.rgb, tint);

    // the outermost pixel darkened, the specular rim just inside it
    float outer = 1.0 - smoothstep(0.0, 1.0, t);
    float band = (1.0 - outer) * (1.0 - smoothstep(1.0, 2.6, t));
    col += band * rim * (0.3 + 0.7 * abs(dot(n, normalize(vec2(-0.45, -1.0)))));
    col *= 1.0 - outer * edge;

    // faint sheen from the top
    col += (1.0 - smoothstep(0.0, panel.y, p.y)) * (dark > 0.5 ? 0.03 : 0.05);
    fragColor = vec4(min(col, 1.0), 1.0) * inside * qt_Opacity;
}
