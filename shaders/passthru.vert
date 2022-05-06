#version 450 core

layout(location = 0) in vec4 vertPos;
layout(location = 1) in vec4 vertCol;

layout(location = 0) out vec4 fragVtxColor;

//the final transform for this node in the gltf graph. Identity matrix for .obj files.
layout(binding = 0) uniform Transform{
    mat4 Model;
} uModel;

void main(){
    gl_Position = uModel.Model * vertPos;
    // Flip Y-axis because Vulkan uses an inverted Y coordinate system
    gl_Position.y = -gl_Position.y;
    fragVtxColor = vertCol;
}