#version 330

// Same interface as lighting.vs, but the model matrix arrives per instance
// as a vertex attribute (raylib DrawMeshInstanced). Pairs with lighting.fs.
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in mat4 instanceTransform;

uniform mat4 mvp; // view * projection when instancing

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec3 fragNormal;
out vec4 fragColor;

void main()
{
    fragPosition = vec3(instanceTransform * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    // No per-instance inverse-transpose: fine for rotations + the near-uniform
    // scales the resource models use; the fragment shader renormalizes.
    fragNormal = normalize(vec3(instanceTransform * vec4(vertexNormal, 0.0)));
    fragColor = vertexColor;

    gl_Position = mvp * instanceTransform * vec4(vertexPosition, 1.0);
}
