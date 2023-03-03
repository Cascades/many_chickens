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
    vec4 top_down_model_bounds;
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
};

layout(std430, binding = 7) buffer SphereProjectionDebugBuffer
{
	SphereProjectionDebugData data[];
} sphereProjectionDebugBuffer;

layout(binding = 8, rgba32f) uniform writeonly image2D meshesDrawnDebugView;

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

    if ((abs(AABB.x) > 1) ||
        (abs(AABB.y) > 1) ||
        (abs(AABB.z) > 1) ||
        (abs(AABB.w) > 1)) {
      return false;
    }

    return true;
}

float linearizeDepth(float z, float n, float f)
{
    return (f * n)/(f * z - f - n * z);
}

uvec2 meshLODCalculation(vec4 mvPos)
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

    return uvec2(lodConfigData.data[lod_index].size, lodConfigData.data[lod_index].offset);
}

bool potentiallyInFrustum(vec3 mvPos, float radius, float near, float far)
{
    return (-mvPos.z + radius > near) && (-mvPos.z - radius < far);
}

void early(vec4 mvPos)
{
    float radius = 0.351285 * modelScalesBuffer.data[gl_GlobalInvocationID.x];

    if (!drawnLastFrameBuffer.data[gl_GlobalInvocationID.x] || !potentiallyInFrustum(mvPos.xyz, radius, 1.0, 250.0))
    {
        indirectBuffer.data[gl_GlobalInvocationID.x].indexCount = 0;
        indirectBuffer.data[gl_GlobalInvocationID.x].instanceCount = 0;
        indirectBuffer.data[gl_GlobalInvocationID.x].firstIndex = 0;
        indirectBuffer.data[gl_GlobalInvocationID.x].vertexOffset = 0;
        indirectBuffer.data[gl_GlobalInvocationID.x].firstInstance = 0;
		return;
    }

    uvec2 meshResults = meshLODCalculation(mvPos);

    indirectBuffer.data[gl_GlobalInvocationID.x].indexCount = meshResults[0];
    indirectBuffer.data[gl_GlobalInvocationID.x].instanceCount = 1;
    indirectBuffer.data[gl_GlobalInvocationID.x].firstIndex = meshResults[1];
    indirectBuffer.data[gl_GlobalInvocationID.x].vertexOffset = 0;
    indirectBuffer.data[gl_GlobalInvocationID.x].firstInstance = 0;
}

void late(vec4 mvPos)
{
	// Hard-coded chicken pos
    float radius = 0.351285 * modelScalesBuffer.data[gl_GlobalInvocationID.x];

    bool visible = true;

    visible = visible && potentiallyInFrustum(mvPos.xyz, radius, 1.0, 250.0);

    vec4 aabb;
    if (visible && getAxisAlignedBoundingBox(mvPos.xyz, radius, -ubo.zNear, ubo.proj, aabb))
    {
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
            if (originalDepth != 1234.0)
            {
                float linearlizedDepth = linearizeDepth(originalDepth, -1.0, -250.0);
                float depthSphere = -1 * (mvPos.z - radius - ubo.zNear) - 3.0;

                visible = visible && depthSphere - 0.0 <= linearlizedDepth;
            }
        }

        sphereProjectionDebugBuffer.data[gl_GlobalInvocationID.x].projectedAABB = aabb;
    }
    else
    {
        aabb = ((aabb + 1.0) * 0.5);
        sphereProjectionDebugBuffer.data[gl_GlobalInvocationID.x].projectedAABB = -aabb;
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
        uvec2 meshResults = meshLODCalculation(mvPos);

        indirectBuffer.data[gl_GlobalInvocationID.x].indexCount = mix(0, meshResults[0], visible);
        indirectBuffer.data[gl_GlobalInvocationID.x].instanceCount = mix(0, 1, visible);
        indirectBuffer.data[gl_GlobalInvocationID.x].firstIndex = mix(0, meshResults[1], visible);
        indirectBuffer.data[gl_GlobalInvocationID.x].vertexOffset = 0;
        indirectBuffer.data[gl_GlobalInvocationID.x].firstInstance = 0;
    }

    vec4 modelPos = modelTranformsBuffer.data[gl_GlobalInvocationID.x] * vec4(0.0, 0.0, 0.0, 1.0);
    vec2 modelXZ = modelPos.xz;
    modelXZ -= vec2(17.0, 0.0);
    modelXZ += vec2(5.0 + 1.0, 5.0 + 1.0);
    modelXZ /= vec2(10.0 + 2.0, 10.0 + 2.0);
    modelXZ *= vec2(100.0, 100.0);
    ivec2 debugMeshPos = ivec2(int(modelXZ[0]), int(modelXZ[1]));

    vec4 abc[5] = vec4[](vec4(1.0, 0.0, 0.0, 1.0), vec4(0.0, 1.0, 0.0, 1.0), vec4(0.0, 0.0, 1.0, 1.0), vec4(1.0, 1.0, 0.0, 1.0), vec4(0.0, 1.0, 1.0, 1.0));

    if (visible)
    {
        drawnLastFrameBuffer.data[gl_GlobalInvocationID.x] = true;
        imageStore(meshesDrawnDebugView, debugMeshPos, abc[gl_GlobalInvocationID.x % 5]);
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

    vec4 modelPos = modelTranformsBuffer.data[gl_GlobalInvocationID.x] * vec4(0.0, 0.0, 0.0, 1.0);
    vec4 mvPos = ubo.view * modelPos;
    mvPos = vec4(mvPos.xyz / mvPos.w, 1.0);

    if (ubo.display_mode == 25)
    {
        uvec2 meshResults = meshLODCalculation(mvPos);

        indirectBuffer.data[gl_GlobalInvocationID.x].indexCount = meshResults[0];
        indirectBuffer.data[gl_GlobalInvocationID.x].instanceCount = 1;
        indirectBuffer.data[gl_GlobalInvocationID.x].firstIndex = meshResults[1];
        indirectBuffer.data[gl_GlobalInvocationID.x].vertexOffset = 0;
        indirectBuffer.data[gl_GlobalInvocationID.x].firstInstance = 0;
    }
    else
    {
        if (EARLY)
        {
            early(mvPos);
        }
        else
        {
            late(mvPos);
        }
    }
}
