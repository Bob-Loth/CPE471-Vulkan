#version 450 core
#include "shading.inl" // Vulkan pre-compiled glsl allows include statements!

const uint LIGHTS = 8;
const uint TEXTURE_ARRAY_SIZE = 16;
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

layout(binding = 3) uniform sampler2D texSampler[TEXTURE_ARRAY_SIZE];


void main()
{
	vec4 texColor = texture(texSampler[uAnimShade.textureIndex + 1], texCoord);
	
	fragColor = texColor;
}
