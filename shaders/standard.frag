#version 450 core
#extension GL_ARB_separate_shader_objects : enable
#include "shading.inl" // Vulkan pre-compiled glsl allows include statements!

layout(location = 0) in vec3 W_fragNor;
layout(location = 1) in vec3 W_fragPos;

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform WorldInfo { 
    mat4 V;
    mat4 P;
} uWorld;

void main(){
    vec3 normal = normalize(W_fragNor); //see https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/normalize.xhtml
    vec3 color = normal; //normal-based coloring.
    
    color = mix(vec3(0.0), color, 0.4); //reduce the color values to 40% of their original value. see https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/mix.xhtml
    color = color + vec3(0.4); //add some intensity back

    fragColor = vec4(color, 1.0); //add an alpha value
}
