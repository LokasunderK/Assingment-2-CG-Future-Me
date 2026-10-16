#version 330 core

in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;
in vec4 FragPosLightSpace;
out vec4 colour;

uniform vec3 lightColour;
uniform vec3 lightDirection;
uniform vec3 viewPos;
uniform bool isLight;

uniform sampler2D texture_data;
uniform sampler2D shadowMap;
uniform bool useTexture;
uniform vec3 objectColour;
uniform bool isDepthPass;

float ShadowCalculation(vec4 fragPosLightSpace)
{
    // Perspective divide and map the light's NDC coordinates into [0, 1].
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    // Treat points outside the light frustum as unshadowed.
    if (projCoords.z > 1.0 || projCoords.z < 0.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    // A small depth bias prevents self-shadowing (shadow acne).
    const float bias = 0.003;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float shadow = 0.0;

    // 3x3 percentage-closer filtering softens shadow-map aliasing.
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float closestDepth = texture(shadowMap,
                projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias > closestDepth) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main()
{
    // Pass 1: Depth map generation only needs depth value (handled by GPU)
    if (isDepthPass)
    {
        return;
    }

    if (isLight)
    {
        colour = vec4(lightColour, 1.0);
        return;
    }

    vec3 baseColour = objectColour;
    if (useTexture)
    {
        vec4 texCol = texture(texture_data, TexCoord);
        if (texCol.a < 0.1)
        {
            baseColour = objectColour;
        }
        else
        {
            baseColour = texCol.rgb;
        }
    }

    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(lightDirection);
    vec3 viewDir = normalize(viewPos - FragPos);

    float diffuse = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float specular = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

    float shadow = ShadowCalculation(FragPosLightSpace);
    vec3 ambientTerm = 0.35 * baseColour;
    vec3 directTerm = (1.0 - shadow) *
        (0.65 * diffuse * lightColour * baseColour + 0.20 * specular * lightColour);
    colour = vec4(ambientTerm + directTerm, 1.0);
}