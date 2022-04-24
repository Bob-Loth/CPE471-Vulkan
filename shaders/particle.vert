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
    mat4 Model;
} uModel;


void main()
{
	// Billboarding: set the upper 3x3 to be the identity matrix
    texCoord = vec2(W_texCoord.x, -W_texCoord.y);
    W_fragPos = uModel.Model * vertPos; // Fragment position in world space
    W_fragNor = mat3(uModel.Model) * vertNor.xyz; // Fragment normal in world space

    W_lightDir = uWorld.lightPos.xyz - (uModel.Model*vertPos).xyz;

	gl_Position = uWorld.P * uWorld.V * vec4(vertPos.xyz, 1.0);
}
