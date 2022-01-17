#version 450 core
#include "shading.inl" // Vulkan pre-compiled glsl allows include statements!

layout(location = 0) in vec3 W_fragNor;
layout(location = 1) in vec3 W_fragPos;
layout(location = 2) in vec3 W_lightDir;

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


void main(){
    vec3 normal = normalize(W_fragNor);
    
    vec3 normVis = shadeByNormal(normal); // Function included from shading.inl

    // Hard coded diffuse lighting on the logo
    // Lab 10: Replace with full Blinn-Phong
    //const vec3 vulkanRed = vec3(0.266356, 0.009134, 0.01096);
    //float diffuse1 = shadeConstantDiffuse(normal, normalize(uAnimShade.diffuseData));
    //float diffuse2 = shadeConstantDiffuse(normal, normalize(vec3(.5, 1.0, -.5)));
    //vec3 shadeLogo = 0.5*vulkanRed + 1.5*vulkanRed*(diffuse1 + diffuse2);
    vec3 diffuse[8];
    for(int i = 0; i < 8; i++){
        diffuse[i] = 0.05 * uAnimShade.diffuseData.xyz * max(0.0,dot(normal, uWorld.lightPos[i].xyz));
    }
    vec3 diffuseCombined = vec3(0.0);
    for(int i = 0; i < 8; i++){
        diffuseCombined += diffuse[i];
    }
    // if shadeStyle == 1 shade it red instead of with normal based shading
    //vec3 color = mix(normVis, shadeLogo, float(uAnimShade.shadeStyle == 1));
    vec3 color = diffuseCombined;
    
    fragColor = vec4(color, 1.0);
}
