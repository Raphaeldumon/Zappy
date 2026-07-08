#version 330

// Billboard d'astre (Luan = soleil, Palasse = lune). emissive est HDR pour
// nourrir le bloom ; circleMask découpe les textures à fond opaque (palasse).

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec3 emissive;
uniform float bodyAlpha;
uniform int circleMask;

out vec4 finalColor;

void main()
{
    vec4 t = texture(texture0, fragTexCoord);
    float a = t.a;
    if (circleMask == 1)
    {
        float d = length(fragTexCoord - vec2(0.5));
        a *= 1.0 - smoothstep(0.44, 0.5, d);
    }
    a *= bodyAlpha;
    if (a < 0.01)
        discard;
    finalColor = vec4(t.rgb * emissive, a);
}
