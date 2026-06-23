#version 330

// Interpolated view direction from the vertex shader
in vec3 fragPosition;

// raylib binds the material's diffuse map to texture0
uniform sampler2D texture0;

out vec4 finalColor;

// Map a 3D direction to equirectangular (lat/long) UVs.
// invAtan = (1/2pi, 1/pi); +0.5 recenters to [0,1].
const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(clamp(v.y, -1.0, 1.0)));
    uv *= invAtan;
    uv += 0.5;
    // raylib textures store row 0 at the top, so flip V to put the sky up top.
    uv.y = 1.0 - uv.y;
    return uv;
}

void main()
{
    vec2 uv = SampleSphericalMap(normalize(fragPosition));
    vec3 color = texture(texture0, uv).rgb;
    finalColor = vec4(color, 1.0);
}
