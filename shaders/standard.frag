#version 450 core
#extension GL_ARB_separate_shader_objects : enable
#include "shading.inl" // Vulkan pre-compiled glsl allows include statements!

const uint LIGHTS = 8;

//enums
const uint BLINN_PHONG     = 0;
const uint NORMAL_MAP      = 1;
const uint TEXTURE_MAP     = 2;
const uint TEXTURED_FLAT   = 3;
const uint TEXTURED_SHADED = 4;


layout(location = 0) in vec3 W_fragNor;
layout(location = 1) in vec3 W_fragPos;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 W_lightDir;


layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform WorldInfo { 
    mat4 V;
    mat4 P;
    vec4 lightPos[LIGHTS];
} uWorld;

layout(binding = 2) uniform AnimShadeData {
    vec4 diffuseData;
    vec4 ambientData;
    vec4 specularData;
    float shininess;
    uint shadingLayer;
    uint textureIndex;
} uAnimShade;

layout(binding = 3) uniform sampler2D texSampler[16];


void main(){
    vec4 texColor = texture(texSampler[uAnimShade.textureIndex], texCoord);

    float brightnessCoefficient = 0.4f / LIGHTS;
    vec3 normal = normalize(W_fragNor);

    vec3 diffuse[LIGHTS];
    vec3 specular[LIGHTS];
    vec3 H[LIGHTS];
    vec3 viewDir = normalize(vec3(uWorld.V * vec4(W_fragPos, 1.0f)));
    
    for(int i = 0; i < LIGHTS; i++){
        diffuse[i] = brightnessCoefficient * uAnimShade.diffuseData.xyz * max(0.0f,dot(normal, uWorld.lightPos[i].xyz));
        H[i] = normalize(normalize(uWorld.lightPos[i].xyz + viewDir));
        specular[i] = uAnimShade.specularData.xyz * pow(max(dot(H[i],normal),0.0),uAnimShade.shininess);
    }
    vec3 diffuseCombined = vec3(0.0f);
    vec3 specularCombined = vec3(0.0f);
    for(int i = 0; i < LIGHTS; i++){
        diffuseCombined += diffuse[i];
        specularCombined += specular[i];
    }
    
    vec3 color = diffuseCombined + specularCombined + vec3(uAnimShade.ambientData);
    //note: don't do this normally. 
    //As for why, the above code is only necessary for BLINN_PHONG and TEXTURED_SHADED modes, but it runs anyway.
    //This might be able to be "fixed" by moving it into a function in shading.inl, but a much cleaner and extensible
    //future version of this code will separate these into their own shader modules.
    switch(uAnimShade.shadingLayer){
        case BLINN_PHONG:
            fragColor = vec4(color, 1.0);
            break;
        case NORMAL_MAP:
            fragColor = vec4(normal, 1.0); //post-model transformation
            break;
        case TEXTURE_MAP:
            fragColor = vec4(texCoord, 0.0, 1.0); //y is reversed as compared to OpenGL. This is corrected(?) in the vertex shader.
            break;
        case TEXTURED_FLAT:
            fragColor = texColor;
            break;
        case TEXTURED_SHADED:
            fragColor = texColor * vec4(color, 1.0);
            break;
    }
        
    
        
    //
}
