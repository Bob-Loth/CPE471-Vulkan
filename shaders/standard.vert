#version 450 core

layout(location = 0) in vec4 vertPos;
layout(location = 1) in vec4 vertNor;

layout(location = 0) out vec3 W_fragNor;
layout(location = 1) out vec4 W_fragPos;
layout(location = 2) out vec3 W_lightDir;

layout(binding = 0) uniform WorldInfo {
    mat4 V;
    mat4 P;
    vec3 lightPos;
} uWorld;

layout(binding = 1) uniform Transform{
    mat4 Model;
} uModel;

void main(){

    W_fragPos = uModel.Model * vertPos; // Fragment position in world space
    W_fragNor = mat3(uModel.Model) * vertNor.xyz; // Fragment normal in world space
    
    W_lightDir = uWorld.lightPos - (uModel.Model*vertPos).xyz; // light vector

    gl_Position = uWorld.P * uWorld.V * W_fragPos; // p*v*m
}