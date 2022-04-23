#version 450 core

layout(location = 0) in vec4 vertPos;
layout(location = 1) in vec4 vertNor;

layout(location = 0) out vec3 W_fragNor;
layout(location = 1) out vec4 W_fragPos;

layout(binding = 0) uniform WorldInfo {
    mat4 View;
    mat4 Projection;
} uWorld;

layout(binding = 1) uniform Transform{
    mat4 Model;
} uModel;

void main(){
    W_fragPos = uModel.Model * vertPos; // Fragment position in world space
    W_fragNor = mat3(uModel.Model) * vertNor.xyz; // Fragment normal in world space

    gl_Position = uWorld.Projection * uWorld.View * W_fragPos; // p*v*m, order matters and is "evaluated" right-to-left
}