#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable
#extension GL_ARB_shader_draw_parameters: require

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
    float p00;
	float p11;
	float zNear;
	int display_mode;
} ubo;

layout(std140, binding = 2) readonly buffer ModelTranformsBuffer
{
	mat4 data[];
} modelTranformsBuffer;

struct VkDrawIndexedIndirectCommand
{
	uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
    uint meshId;
    uint pad1;
    uint pad2;
};

layout(std430, binding = 4) readonly buffer IndirectBuffer
{
	VkDrawIndexedIndirectCommand data[];
} indirectBuffer;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out float texture_on;
layout(location = 4) out float specularity;
layout(location = 5) flat out uint ID;

mat4 rotationMatrix(vec3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

void main() {
    // TODO: make the chickens spin!
    //mat4 rotMat = rotationMatrix(normalize(vec3(0.1, 0.2, 0.3)), 25.0);

    if (ubo.display_mode == 22)
    {
        gl_Position = ubo.proj * ubo.view * modelTranformsBuffer.data[indirectBuffer.data[gl_DrawIDARB].meshId] * vec4(normalize(inPosition) * 0.351285, 1.0);
    }
    else
    {
        gl_Position = ubo.proj * ubo.view * modelTranformsBuffer.data[indirectBuffer.data[gl_DrawIDARB].meshId] * vec4(inPosition, 1.0);
    }

    outNormal = inNormal;

    specularity = ubo.specular;

    fragColor = vec3(ubo.diffuse,ubo.diffuse,ubo.diffuse);

    fragTexCoord = inTexCoord;
    texture_on = int(ubo.texture_stage_on);
    ID = gl_DrawIDARB;
}