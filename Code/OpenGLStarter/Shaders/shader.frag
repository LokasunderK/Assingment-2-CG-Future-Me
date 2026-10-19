#version 330 core

in vec4 vCol;
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec4 fragPosLightSpace;

out vec4 colour;

uniform vec3 lightColour;
uniform vec3 lightPos;
uniform vec3 viewPos;

uniform sampler2D texture_data;
uniform sampler2D shadowMap;
uniform bool useTexture;
uniform vec3 objectColour;

vec3 ambientLight()
{
    float ambientStrength = 0.3;
    vec3 ambient = lightColour * ambientStrength;
    return ambient;
}

vec3 diffuseLight()
{
    float diffuseStrength = 0.5;
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = lightColour * diff * diffuseStrength;
    return diffuse;
}

vec3 specularLight()
{
    float specularStrength = 0.8;
    float shininess = 256.0;

    vec3 viewDir  = normalize(viewPos - FragPos);
    vec3 norm     = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);

    // Blinn-Phong
    vec3 halfDir = normalize((lightDir + viewDir) / 2.0);
    float spec = pow(max(dot(halfDir, norm), 0.0), shininess);

    vec3 specular = lightColour * spec * specularStrength;
    return specular;
}

float ShadowCalculation()
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = 0.005;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    if (projCoords.z > 1.0)
        shadow = 0.0;
    return shadow;
}

void main()
{
    vec4 baseColour;
    if (useTexture)
    {
        baseColour = texture(texture_data, TexCoord);
    }
    else
    {
        baseColour = vec4(objectColour, 1.0);
    }

    float shadow = ShadowCalculation();
    vec3 lighting = ambientLight() + (1.0 - shadow) * (diffuseLight() + specularLight());

    colour = baseColour * vec4(lighting, 1.0);
}