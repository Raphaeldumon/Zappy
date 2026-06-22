#version 330

// Standard raylib vertex attributes (auto-bound by name).
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Standard raylib uniforms (auto-set: mvp every draw, matModel per model and
// to identity for the immediate-mode batch used by DrawCube).
uniform mat4 mvp;
uniform mat4 matModel;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;

void main()
{
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor    = vertexColor;
    // Use matModel (w=0 => direction) instead of a dedicated normal matrix:
    // the immediate-mode batch reliably sets matModel but not matNormal, and
    // our scales are near-uniform so skew is negligible.
    fragNormal   = normalize(vec3(matModel * vec4(vertexNormal, 0.0)));

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
