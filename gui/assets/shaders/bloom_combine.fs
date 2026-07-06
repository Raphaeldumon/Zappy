#version 330

// Bloom pass 2 of 2: the full-res scene plus the half-res glow target,
// bilinearly upscaled. Two taps per screen pixel.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0; // the full-res scene render texture
uniform sampler2D glowTex;  // the half-res bloom target

out vec4 finalColor;

const float kStrength = 0.85; // how much bloom is added back

void main()
{
    vec3 base = texture(texture0, fragTexCoord).rgb;
    vec3 glow = texture(glowTex, fragTexCoord).rgb;
    finalColor = vec4(base + glow * kStrength, 1.0) * fragColor;
}
