#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Standard raylib material uniforms.
uniform sampler2D texture0;   // diffuse map (1x1 white for untextured DrawCube)
uniform vec4 colDiffuse;      // material/draw tint

// Our lighting uniforms.
uniform vec3 lightDir;        // normalized direction TO the light
uniform vec4 lightColor;
uniform vec4 ambient;
uniform vec3 viewPos;

out vec4 finalColor;

void main()
{
    vec4 base   = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 normal = normalize(fragNormal);
    vec3 l      = normalize(lightDir);

    float ndotl = max(dot(normal, l), 0.0);
    vec3  diffuse = lightColor.rgb * ndotl;

    // Soft specular highlight.
    vec3  viewD    = normalize(viewPos - fragPosition);
    vec3  reflectD = reflect(-l, normal);
    float spec     = pow(max(dot(viewD, reflectD), 0.0), 16.0) * ndotl;
    vec3  specular = lightColor.rgb * spec * 0.25;

    vec3 lit = base.rgb * (ambient.rgb + diffuse) + specular;
    finalColor = vec4(lit, base.a);
}
