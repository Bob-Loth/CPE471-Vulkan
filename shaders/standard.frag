#version 450 core
#include "shading.inl" // Vulkan pre-compiled glsl allows include statements!

layout(location = 0) in vec3 W_fragNor;
layout(location = 1) in vec3 W_fragPos;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 W_lightDir;


layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform WorldInfo { 
    mat4 V;
    mat4 P;
    vec4 lightPos[8];
} uWorld;

layout(binding = 2) uniform AnimShadeData {
    vec4 diffuseData;
    vec4 ambientData;
    vec4 specularData;
    float shininess;
} uAnimShade;

layout(binding = 3) uniform sampler2D texSampler;


void main(){
    vec4 texColor = texture(texSampler, texCoord);

    float brightnessCoefficient = 0.05f;
    vec3 normal = normalize(W_fragNor);

    vec3 diffuse[8];
    vec3 specular[8];
    vec3 H[8];
    vec3 viewDir = normalize(vec3(uWorld.V * vec4(W_fragPos, 1.0f)));
    
    for(int i = 0; i < 8; i++){
        diffuse[i] = brightnessCoefficient * uAnimShade.diffuseData.xyz * max(0.0f,dot(normal, uWorld.lightPos[i].xyz));
        H[i] = normalize(normalize(uWorld.lightPos[i].xyz + viewDir));
        specular[i] = uAnimShade.specularData.xyz * pow(max(dot(H[i],normal),0.0),uAnimShade.shininess);
    }
    vec3 diffuseCombined = vec3(0.0f);
    vec3 specularCombined = vec3(0.0f);
    for(int i = 0; i < 8; i++){
        diffuseCombined += diffuse[i];
        specularCombined += specular[i];
    }
    
    vec3 color = diffuseCombined + specularCombined + vec3(uAnimShade.ambientData);
    
    fragColor = vec4(texColor * vec4(color, 1.0));
}
