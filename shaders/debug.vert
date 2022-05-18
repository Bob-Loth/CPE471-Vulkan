#version 450 core

layout(location = 0) in vec4 vertPos;
layout(location = 1) in vec4 vertNor;
layout(location = 2) in vec2 W_texCoord;

layout(location = 0) out vec3 W_fragNor;
layout(location = 1) out vec4 W_fragPos;
layout(location = 2) out vec2 texCoord;
layout(location = 3) out vec3 W_lightDir;

layout(binding = 0) uniform WorldInfo {
    mat4 V;
    mat4 P;
    vec4 lightPos;
} uWorld;

layout(binding = 1) uniform Transform{
    mat4 gltfModel;
    mat4 Model;
} uModel;

void main(){
    mat4 M = uModel.gltfModel * uModel.Model;
    texCoord = vec2(W_texCoord.x, -W_texCoord.y); //Vulkan, in its infinite wisdom, inverts the y-coordinate.
    W_fragPos = M * vertPos; // Fragment position in world space
    W_fragNor = mat3(M) * vertNor.xyz; // Fragment normal in world space
    
    W_lightDir = uWorld.lightPos.xyz - (M*vertPos).xyz; // light vector

    gl_Position = uWorld.P * uWorld.V * W_fragPos; // p*v*m
}
