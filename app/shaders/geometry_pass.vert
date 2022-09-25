#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable

layout(std140, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 light;
    mat4 lightVP;
	vec4 Ka;
	vec4 Kd;
	vec4 Ks;
	vec4 Ke;
    vec2 win_dim;
    float Ns;
	float model_stage_on;
	float texture_stage_on;
	float lighting_stage_on;
    float pcf_on;
    float specular;
	float diffuse;
	float ambient;
    float shadow_bias;
	int display_mode;
} ubo;

layout(std140, binding = 2) readonly buffer ModelTranformsBuffer
{
	mat4 data[1000];
} modelTranformsBuffer;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out float texture_on;
layout(location = 4) out float specularity;

void main() {
    gl_Position = ubo.proj * ubo.view * modelTranformsBuffer.data[gl_InstanceIndex] * vec4(inPosition, 1.0);

    outNormal = inNormal;

    specularity = ubo.specular;

    fragColor = vec3(ubo.diffuse,ubo.diffuse,ubo.diffuse);

    fragTexCoord = inTexCoord;
    texture_on = int(ubo.texture_stage_on);
}