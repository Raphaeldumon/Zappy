#version 330

// Blinn-Phong with one fixed directional "sun" plus a cool sky ambient.
// Applied to every loaded model (players, resources); the skybox and the
// batched floor keep their own paths.

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

uniform sampler2D texture0; // material diffuse map
uniform vec4 colDiffuse;    // DrawModelEx tint (team glow comes through here)
uniform vec3 viewPos;       // camera eye, for the specular term

out vec4 finalColor;

// Sun from high south-east; warm light, cool ambient — cheap "space daylight".
const vec3 kSunDir = normalize(vec3(-0.35, -1.0, -0.45));
const vec3 kSunColor = vec3(1.00, 0.96, 0.88);
const vec3 kAmbient = vec3(0.42, 0.44, 0.52);
const float kSpecStrength = 0.22;
const float kShininess = 24.0;

void main()
{
    vec4 texel = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    if (texel.a < 0.05)
        discard;

    vec3 n = normalize(fragNormal);
    vec3 l = -kSunDir;

    float diff = max(dot(n, l), 0.0);

    vec3 v = normalize(viewPos - fragPosition);
    vec3 h = normalize(l + v);
    float spec = pow(max(dot(n, h), 0.0), kShininess) * kSpecStrength * diff;

    vec3 lit = texel.rgb * (kAmbient + kSunColor * diff) + kSunColor * spec;
    finalColor = vec4(lit, texel.a);
}
