#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 lightDir;
uniform vec3 viewPos;
uniform vec4 uColor;
uniform sampler2D uTexture;

void main()
{
    vec3 texColor = texture(uTexture, TexCoords).rgb;
    // Ambient
    vec3 ambient = 1.2 * texColor;

    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightDir - FragPos);
    float diff = max(dot(norm, -lightDir), 0.0);
    vec3 diffuse = diff * texColor;

    // Specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = vec3(0.5) * spec;

    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}
