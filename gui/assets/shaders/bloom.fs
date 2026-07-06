#version 330

// Single-pass bloom over the 3D scene render target: bright pixels are
// blurred with a fixed 13-tap kernel and added back. The UI is drawn after
// this pass, so text stays crisp.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0; // the scene render texture
uniform vec2 resolution;    // render-texture size in pixels

out vec4 finalColor;

const float kThreshold = 0.62;  // luminance where glow starts
const float kStrength = 0.85;   // how much bloom is added back
const float kSpread = 2.2;      // sample ring radius, in texels

vec3 brightPart(vec2 uv)
{
    vec3 c = texture(texture0, uv).rgb;
    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
    return c * smoothstep(kThreshold, 1.0, lum);
}

void main()
{
    vec2 texel = kSpread / resolution;
    vec3 base = texture(texture0, fragTexCoord).rgb;

    // Centre + two rings of 6 (hex pattern) — soft enough, one pass.
    vec3 glow = brightPart(fragTexCoord) * 0.20;
    for (int i = 0; i < 6; ++i)
    {
        float a = 6.2831853 * float(i) / 6.0;
        vec2 dir = vec2(cos(a), sin(a));
        glow += brightPart(fragTexCoord + dir * texel) * 0.09;
        glow += brightPart(fragTexCoord + dir * texel * 2.5) * 0.045;
    }

    finalColor = vec4(base + glow * kStrength, 1.0) * fragColor;
}
