#version 330

// Input vertex attributes
in vec3 vertexPosition;

// Input uniform values (raylib auto-binds matView / matProjection)
uniform mat4 matView;
uniform mat4 matProjection;

// Direction handed to the fragment shader (the cube-local position == view ray)
out vec3 fragPosition;

void main()
{
    fragPosition = vertexPosition;

    // Drop the translation part of the view matrix so the box is always
    // centred on the camera (an infinitely distant background).
    mat4 rotView = mat4(mat3(matView));
    vec4 clipPos = matProjection * rotView * vec4(vertexPosition, 1.0);

    // z = w pushes every fragment to the far plane (depth 1.0).
    gl_Position = clipPos.xyww;
}
