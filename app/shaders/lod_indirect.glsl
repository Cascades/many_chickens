#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(push_constant) uniform block
{
	bool EARLY;
};

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
    uint pad0;
    uint pad1;
    uint pad2;
};

layout(std140, binding = 1) writeonly buffer IndirectBuffer
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

layout(binding = 4) readonly buffer ModelScalesBuffer
{
	float data[];
} modelScalesBuffer;

layout (set = 0, binding = 5) uniform sampler2D inDepthPyramid;

layout(binding = 6) buffer DrawnLastFrameBuffer
{
	bool data[];
} drawnLastFrameBuffer;

struct SphereProjectionDebugData
{
    vec4 projectedAABB;
    //vec4 depthData;
    //vec2 depthData;
    //vec2 depthLookUpCoord;
    //uint lodLevel;
};

layout(std430, binding = 7) buffer SphereProjectionDebugBuffer
{
	SphereProjectionDebugData data[];
} sphereProjectionDebugBuffer;

vec3 project(mat4 P, vec3 Q)
{
    vec4 v = P * vec4(Q, 1.0);
    return v.xyz / v.w;
}

// 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere. Michael Mara, Morgan McGuire. 2013
bool getAxisAlignedBoundingBox(
    vec3 C, // camera-space sphere center
    float r, // sphere radius
    float nearZ, // near clipping plane position (negative)
    mat4 P,
    out vec4 AABB) // camera to screen space
{
    if (length(C) - r < nearZ)
    {
        return false;
    }

    vec2 cx = vec2(dot(vec3(1,0,0), C), C.z); // C in the a-z frame
    float tSquaredx = dot(cx, cx) - (r * r);
    vec2 vx = vec2(sqrt(tSquaredx), r) / length(cx); // (cos, sin) of angle theta between c and a tangent vector

    vec2 boundsx[2];
    boundsx[0] = mat2(vx.x, -vx.y, vx.y, vx.x) * cx * vx.x;
    boundsx[1] = mat2(vx.x, vx.y, -vx.y, vx.x) * cx * vx.x;

    // Transform back to camera space
    vec3 left = boundsx[1].x * vec3(1,0,0); left.z = boundsx[1].y;
    vec3 right = boundsx[0].x * vec3(1,0,0); right.z = boundsx[0].y;

    vec2 cy = vec2(dot(vec3(0,1,0), C), C.z); // C in the a-z frame
    float tSquaredy = dot(cy, cy) - (r * r);
    vec2 vy = vec2(sqrt(tSquaredy), r) / length(cy); // (cos, sin) of angle theta between c and a tangent vector

    vec2 boundsy[2]; // In the a-z reference frame
    boundsy[0] = mat2(vy.x, -vy.y, vy.y, vy.x) * cy * vy.x;
    boundsy[1] = mat2(vy.x, vy.y, -vy.y, vy.x) * cy * vy.x;

    // Transform back to camera space
    vec3 bottom = boundsy[1].x * vec3(0,1,0); bottom.z = boundsy[1].y;
    vec3 top = boundsy[0].x * vec3(0,1,0); top.z = boundsy[0].y;

    AABB = vec4(
        project(P, left).x,
        project(P, top).y,
        project(P, right).x,
        project(P, bottom).y
        );
    return true;
}

//float LinearizeDepth(float depth, float near_p, float far_p)
//{
//    float z = depth * 2.0 - 1.0; // Back to NDC 
//    return (2.0 * near_p * far_p) / (far_p + near_p - z * (far_p - near_p));
//}

float LinearizeDepth(float z, float n, float f) {
    //return ((f * n)/(z + (f / (n - f)))) * (1/(f - n));
    // equivelant to
    return (f * n)/(f * z - f - n * z);
}

void early()
{
    if (!drawnLastFrameBuffer.data[gl_GlobalInvocationID.x])
    {
        indirectBuffer.data[gl_GlobalInvocationID.x].indexCount = 0;
        indirectBuffer.data[gl_GlobalInvocationID.x].instanceCount = 0;
        indirectBuffer.data[gl_GlobalInvocationID.x].firstIndex = 0;
        indirectBuffer.data[gl_GlobalInvocationID.x].vertexOffset = 0;
        indirectBuffer.data[gl_GlobalInvocationID.x].firstInstance = 0;
		return;
    }

    vec4 modelPos = modelTranformsBuffer.data[gl_GlobalInvocationID.x] * vec4(0.0, 0.0, 0.0, 1.0);
    vec4 mvPos = ubo.view * modelPos;
    mvPos = vec4(mvPos.xyz / mvPos.w, 1.0);

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

    lod_index = min(lod_index, lodConfigData.data.length() - 1);

    uint indexCount = lodConfigData.data[lod_index].size;
    uint firstIndex = lodConfigData.data[lod_index].offset;
    uint instanceCount = 1;

    indirectBuffer.data[gl_GlobalInvocationID.x].indexCount = indexCount;
    indirectBuffer.data[gl_GlobalInvocationID.x].instanceCount = instanceCount;
    indirectBuffer.data[gl_GlobalInvocationID.x].firstIndex = firstIndex;
    indirectBuffer.data[gl_GlobalInvocationID.x].vertexOffset = 0;
    indirectBuffer.data[gl_GlobalInvocationID.x].firstInstance = 0;
}

void late()
{
	// Hard-coded chicken pos
    vec4 modelPos = modelTranformsBuffer.data[gl_GlobalInvocationID.x] * vec4(0.0, 0.0, 0.0, 1.0);
    vec4 mvPos = ubo.view * modelPos;
    mvPos = vec4(mvPos.xyz / mvPos.w, 1.0);

    float radius = 0.351285 * modelScalesBuffer.data[gl_GlobalInvocationID.x];

    bool visible = true;

    float xyRatio = ubo.win_dim.x / ubo.win_dim.y;

    vec4 aabb;
    bool projectionSuccess = getAxisAlignedBoundingBox(mvPos.xyz, radius, -ubo.zNear, ubo.proj, aabb);
    if (projectionSuccess)
    {
        sphereProjectionDebugBuffer.data[gl_GlobalInvocationID.x].projectedAABB = aabb;

        vec2 frame_size = ubo.win_dim;

        float real_width = pow(2, ceil(log2(frame_size.x)));
	    float real_height = pow(2, ceil(log2(frame_size.y)));

	    vec2 real_size = vec2(max(real_width, real_height));

	    vec2 scaling_factor = real_size / frame_size;
        // transform to (0, 1)
        aabb = ((aabb + 1.0) * 0.5);

        vec2 lodLookupCoord = vec2(aabb.xy + aabb.zw) * 0.5;

        if (lodLookupCoord.x < 0.0 || lodLookupCoord.x > 1.0 ||
            lodLookupCoord.y < 0.0 || lodLookupCoord.y > 1.0)
        {
            sphereProjectionDebugBuffer.data[gl_GlobalInvocationID.x].projectedAABB = vec4(1.0);
        }
        else
        {
            // Sampler is set up to do min reduction, so this computes the minimum depth of a 2x2 texel quad
            float width = (aabb[0] - aabb[2]) * real_width;
            float height = (aabb[1] - aabb[3]) * real_height;
            float level = floor(log2(max(width, height)));
            float originalDepth = textureLod(inDepthPyramid, lodLookupCoord / scaling_factor, level).x;
            float linearlizedDepth = LinearizeDepth(originalDepth, -0.001, -250.0);
            float depthSphere = -1 * (mvPos.z - radius - ubo.zNear);

            visible = visible && depthSphere < linearlizedDepth;
        }

        sphereProjectionDebugBuffer.data[gl_GlobalInvocationID.x].projectedAABB = aabb;
    }
    else
    {
        sphereProjectionDebugBuffer.data[gl_GlobalInvocationID.x].projectedAABB = vec4(1.0);
    }

    if ((!visible) || drawnLastFrameBuffer.data[gl_GlobalInvocationID.x])
    {
        indirectBuffer.data[gl_GlobalInvocationID.x].indexCount = 0;
        indirectBuffer.data[gl_GlobalInvocationID.x].instanceCount = 0;
        indirectBuffer.data[gl_GlobalInvocationID.x].firstIndex = 0;
        indirectBuffer.data[gl_GlobalInvocationID.x].vertexOffset = 0;
        indirectBuffer.data[gl_GlobalInvocationID.x].firstInstance = 0;
    }
    else
    {
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

        lod_index = min(lod_index, lodConfigData.data.length() - 1);

        uint indexCount = mix(0, lodConfigData.data[lod_index].size, visible);
        uint firstIndex = mix(0, lodConfigData.data[lod_index].offset, visible);
        uint instanceCount = mix(0, 1, visible);

        indirectBuffer.data[gl_GlobalInvocationID.x].indexCount = indexCount;
        indirectBuffer.data[gl_GlobalInvocationID.x].instanceCount = instanceCount;
        indirectBuffer.data[gl_GlobalInvocationID.x].firstIndex = firstIndex;
        indirectBuffer.data[gl_GlobalInvocationID.x].vertexOffset = 0;
        indirectBuffer.data[gl_GlobalInvocationID.x].firstInstance = 0;
    }

    if (visible)
    {
        drawnLastFrameBuffer.data[gl_GlobalInvocationID.x] = true;
    }
    else
    {
        drawnLastFrameBuffer.data[gl_GlobalInvocationID.x] = false;
    }
}

void main()
{
    if(gl_GlobalInvocationID.x >= indirectBuffer.data.length())
    {
        return;
    }

    if (ubo.display_mode == 25)
    {
        vec4 modelPos = modelTranformsBuffer.data[gl_GlobalInvocationID.x] * vec4(0.0, 0.0, 0.0, 1.0);
        vec4 mvPos = ubo.view * modelPos;
        mvPos = vec4(mvPos.xyz / mvPos.w, 1.0);

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

        lod_index = min(lod_index, lodConfigData.data.length() - 1);

        indirectBuffer.data[gl_GlobalInvocationID.x].indexCount = lodConfigData.data[lod_index].size;
        indirectBuffer.data[gl_GlobalInvocationID.x].instanceCount = 1;
        indirectBuffer.data[gl_GlobalInvocationID.x].firstIndex = lodConfigData.data[lod_index].offset;
        indirectBuffer.data[gl_GlobalInvocationID.x].vertexOffset = 0;
        indirectBuffer.data[gl_GlobalInvocationID.x].firstInstance = 0;
    }
    else
    {
        if (EARLY)
        {
            early();
            //sphereProjectionDebugBuffer.data[gl_GlobalInvocationID.x].projectedAABB = vec4(123.0, 456.0, 789.0, 123.0);
        }
        else
        {
            late();
            //sphereProjectionDebugBuffer.data[gl_GlobalInvocationID.x].projectedAABB = vec4(111.0, 222.0, 333.0, 444.0);
        }
    }
}