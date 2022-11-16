#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(std140, binding = 0) readonly buffer ModelTranformsBuffer
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
};

layout(binding = 1) writeonly buffer IndirectBuffer
{
	VkDrawIndexedIndirectCommand data[];
} indirectBuffer;

layout(std140, binding = 2) uniform UniformBufferObject {
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

struct LodConfigData
{
    float maxDist;
    uint offset;
    uint size;
    uint padding;
};

layout(std140, binding = 3) readonly buffer LodConfigBuffer
{
	LodConfigData data[];
} lodConfigData;

layout(std140, binding = 4) readonly buffer ModelScalesBuffer
{
	float data[];
} modelScalesBuffer;

layout (set = 0, binding = 5) uniform sampler2D inDepthPyramid;

// 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere. Michael Mara, Morgan McGuire. 2013
bool projectSphere(vec3 C, float r, float znear, float P00, float P11, out vec4 aabb)
{
    if (C.z < r + znear)
        return false;

    vec2 cx = -C.xz;
    vec2 vx = vec2(sqrt(dot(cx, cx) - r * r), r);
    vec2 minx = mat2(vx.x, vx.y, -vx.y, vx.x) * cx;
    vec2 maxx = mat2(vx.x, -vx.y, vx.y, vx.x) * cx;

    vec2 cy = -C.yz;
    vec2 vy = vec2(sqrt(dot(cy, cy) - r * r), r);
    vec2 miny = mat2(vy.x, vy.y, -vy.y, vy.x) * cy;
    vec2 maxy = mat2(vy.x, -vy.y, vy.y, vy.x) * cy;

    aabb = vec4(minx.x / minx.y * P00, miny.x / miny.y * P11, maxx.x / maxx.y * P00, maxy.x / maxy.y * P11);
    aabb = aabb.xwzy * vec4(0.5f, -0.5f, 0.5f, -0.5f) + vec4(0.5f); // clip space -> uv space

    return true;
}

void main()
{
    if(gl_GlobalInvocationID.x >= indirectBuffer.data.length())
    {
        return;
    }

    // Hard-coded chicken pos
    vec4 modelPos = modelTranformsBuffer.data[gl_GlobalInvocationID.x] * vec4(-0.0248936, 0.046164, -0.0155488, 1.0);
    vec4 mvPos = ubo.view * modelPos;

    float radius = 0.52 * modelScalesBuffer.data[gl_GlobalInvocationID.x];

    bool visible = true;
    vec4 aabb;
    if (projectSphere(mvPos.xyz, radius, ubo.zNear, ubo.p00, ubo.p11, aabb))
    {
        float width = (aabb.z - aabb.x) * 2048;
        float height = (aabb.w - aabb.y) * 2048;

        float level = floor(log2(max(width, height)));

        vec2 frame_size = ubo.win_dim;

        float real_width = pow(2, ceil(log2(frame_size.x)));
	    float real_height = pow(2, ceil(log2(frame_size.y)));

	    vec2 real_size = vec2(max(real_width, real_height));

	    vec2 scaling_factor = real_size / frame_size;

        // Sampler is set up to do min reduction, so this computes the minimum depth of a 2x2 texel quad
        float depth = textureLod(inDepthPyramid, ((aabb.xy + aabb.zw) * 0.5) / scaling_factor, level).x;
        float depthSphere = ubo.zNear / (mvPos.z - radius);

        visible = visible && depthSphere > depth;
    }

    float dist = length(mvPos);

    uint lod_index = lodConfigData.data.length();

    for(uint curr_lod_index = 0; curr_lod_index < lodConfigData.data.length() - 1; ++curr_lod_index)
    {
        float curr_lod_max_dist = lodConfigData.data[curr_lod_index].maxDist;
        float next_lod_max_dist = lodConfigData.data[curr_lod_index + 1].maxDist;
        if(dist > curr_lod_max_dist && dist <= next_lod_max_dist)
        {
            lod_index = curr_lod_index + 1;
            break;
        }
    }

    uint indexCount = mix(0, lodConfigData.data[lod_index].size, visible);
    uint firstIndex = mix(0, lodConfigData.data[lod_index].offset, visible);
    uint instanceCount = mix(0, 1, visible);

    indirectBuffer.data[gl_GlobalInvocationID.x].indexCount = indexCount;
    indirectBuffer.data[gl_GlobalInvocationID.x].instanceCount = instanceCount;
    indirectBuffer.data[gl_GlobalInvocationID.x].firstIndex = firstIndex;
    indirectBuffer.data[gl_GlobalInvocationID.x].vertexOffset = 0;
    indirectBuffer.data[gl_GlobalInvocationID.x].firstInstance = 0;
}