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
    uint meshId;
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
    mat4 culling_model;
    mat4 culling_view;
    mat4 culling_proj;
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
    float culling_p00;
	float culling_p11;
	float zNear;
	int display_mode;
    int culling_updating;
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

layout(set = 0, binding = 7, rgba32f) uniform writeonly image2D meshesDrawnDebugView;

layout(std430, binding = 8) buffer SphereProjectionDebugBuffer
{
	SphereProjectionDebugData data[];
} sphereProjectionDebugBuffer;

layout(std430, binding = 9) buffer IndirectBufferCountBuffer
{
	uint data;
} indirectBufferCountBuffer;

layout(std430, binding = 10) buffer PreviousFrameLODBuffer
{
	uint data[];
} previousFrameLODBuffer;

const vec4 color_mapping_5[5] = vec4[](vec4(1.0, 0.0, 0.0, 1.0),
                                       vec4(0.0, 1.0, 0.0, 1.0),
                                       vec4(0.0, 0.0, 1.0, 1.0),
                                       vec4(1.0, 1.0, 0.0, 1.0),
                                       vec4(0.0, 1.0, 1.0, 1.0));

const vec4 color_mapping_11[11] = vec4[](vec4(0.0, 0.0, 1.0, 1.0),
                                         vec4(0.0, 1.0, 0.0, 1.0),
                                         vec4(0.0, 1.0, 1.0, 1.0),
                                         vec4(1.0, 0.0, 0.0, 1.0),
                                         vec4(1.0, 0.0, 1.0, 1.0),
                                         vec4(1.0, 1.0, 0.0, 1.0),
                                         vec4(1.0, 1.0, 1.0, 1.0),
                                         vec4(220.0 / 255.0, 20.0 / 255.0, 60.0 / 255.0, 1.0),
                                         vec4(255.0 / 255.0, 165.0 / 255.0, 0.0 / 255.0, 1.0),
                                         vec4(255.0 / 255.0, 192.0 / 255.0, 203.0 / 255.0, 1.0),
                                         vec4(0.6, 0.6, 0.6, 1.0));

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

// Take a position in view space and returns:
//     uvec2(a, b)
//     a: The number of indices
//     b: The offset in to the index buffer
uvec2 meshLODCalculation(vec4 mvPos, vec4 aabb, bool new)
{
    uint lod_index = lodConfigData.data.length();

    if (new)
    {
        float max_size = max(aabb[0] - aabb[2], aabb[1] - aabb[3]);

        for(uint curr_lod_index = 0; curr_lod_index < 4; ++curr_lod_index)
        {
            float curr_lod_max_size = lodConfigData.data[curr_lod_index].maxDist;
            float next_lod_max_size = lodConfigData.data[curr_lod_index + 1].maxDist;
            if(max_size > curr_lod_max_size && max_size >= next_lod_max_size)
            {
                lod_index = curr_lod_index + 1;
                break;
            }
        }

        lod_index = min(lod_index, 4);
        previousFrameLODBuffer.data[gl_GlobalInvocationID.x] = lod_index;
    }
    else
    {
        float dist = length(mvPos);

        float lodConfigs[5] = float[5](0.0, 0.0, 2.2, 5.5, 50.0);

        for(uint curr_lod_index = 0; curr_lod_index < lodConfigData.data.length() - 1; ++curr_lod_index)
        {
            float curr_lod_max_dist = lodConfigs[curr_lod_index];
            float next_lod_max_dist = lodConfigs[curr_lod_index + 1];
            if(dist > curr_lod_max_dist && dist <= next_lod_max_dist)
            {
                lod_index = curr_lod_index + 1;
                break;
            }
        }

        lod_index = min(lod_index, lodConfigData.data.length() - 1);
    }

    return uvec2(lodConfigData.data[lod_index].size, lodConfigData.data[lod_index].offset);
}

// Takes a point in view space, a model radius, and a near and far frustum plane.
// Return whether the object is potentially in the frustum.
bool potentiallyInFrustum(vec3 mvPos, float radius, float near, float far)
{
    bool near_far_test = (-mvPos.z + radius > near) && (-mvPos.z - radius < far);

	float aspect_ratio = ubo.win_dim.x / ubo.win_dim.y;

    vec3 abs_y = normalize(vec3(0.0, abs(mvPos.y) - radius, abs(mvPos.z)));
    vec3 abs_x = normalize(vec3(abs(mvPos.x) - radius, 0.0, abs(mvPos.z)));
    float y_frustum_angle = radians(45.0 / 2.0);
    float x_frustum_angle = aspect_ratio * radians(45.0 / 2.0);
    bool frustum_test_y = dot(abs_y, vec3(0.0, 0.0, 1.0)) > cos(y_frustum_angle);
    bool frustum_test_x = dot(abs_x, vec3(0.0, 0.0, 1.0)) > cos(x_frustum_angle);

    return near_far_test && frustum_test_x && frustum_test_y;
}

// Early pass. Simply draws what was drawn last frame.
void early(vec4 mvPos)
{
    float radius = 0.351285 * modelScalesBuffer.data[gl_GlobalInvocationID.x];

    if (!drawnLastFrameBuffer.data[gl_GlobalInvocationID.x] ||
        !potentiallyInFrustum(mvPos.xyz, radius, 1.0, 250.0))
    {
		return;
    }

    uint drawBufferIdx = atomicAdd(indirectBufferCountBuffer.data, 1);

    indirectBuffer.data[drawBufferIdx].indexCount = lodConfigData.data[previousFrameLODBuffer.data[gl_GlobalInvocationID.x]].size;
    indirectBuffer.data[drawBufferIdx].instanceCount = 1;
    indirectBuffer.data[drawBufferIdx].firstIndex = lodConfigData.data[previousFrameLODBuffer.data[gl_GlobalInvocationID.x]].offset;
    indirectBuffer.data[drawBufferIdx].vertexOffset = 0;
    indirectBuffer.data[drawBufferIdx].firstInstance = 0;
    indirectBuffer.data[drawBufferIdx].meshId = gl_GlobalInvocationID.x;
}

// Late pass.
// * Draws what is in view and not already drawn by the early pass.
// * Marks all items drawn this from (early + late passes).
void late(vec4 mvPos)
{
	// Hard-coded chicken pos
    float radius = 0.351285 * modelScalesBuffer.data[gl_GlobalInvocationID.x];

    bool visible = true;

    visible = visible && potentiallyInFrustum(mvPos.xyz, radius, 1.0, 250.0);

    uint level = 0;

    // screen space axis aligned bounding box of mesh
    vec4 aabb;
    // this is statement only passes is the mesh being processed can be projected to the screen in full.
    if (visible && bool(ubo.culling_updating) && getAxisAlignedBoundingBox(mvPos.xyz, radius, -ubo.zNear, ubo.culling_proj, aabb))
    {
        // transform to (0, 1)
        aabb = ((aabb + 1.0) * 0.5);

        // screen space width and height of our AABB
        float width = (aabb[0] - aabb[2]) *  ubo.win_dim.x;
        float height = (aabb[1] - aabb[3]) *  ubo.win_dim.y;
        // mip level of our input depth pyramid texture
        level = uint(floor(log2(max(width, height))));
        // Sample each corner of our AABB in the depth pyramid
        float originalDepth = textureLod(inDepthPyramid, vec2(aabb[2], aabb[3]), level).x;
        originalDepth = max(originalDepth, textureLod(inDepthPyramid, vec2(aabb[0], aabb[3]), level).x);
        originalDepth = max(originalDepth, textureLod(inDepthPyramid, vec2(aabb[2], aabb[1]), level).x);
        originalDepth = max(originalDepth, textureLod(inDepthPyramid, vec2(aabb[0], aabb[1]), level).x);

        // convert our sampled depth to view space
        float linearlizedDepth = linearizeDepth(originalDepth, -1.0, -250.0) - ubo.zNear;
        // mesh's closest depth in view space
        float depthSphere = (-mvPos.z - (radius * 1.0) - ubo.zNear);

        visible = visible && depthSphere <= linearlizedDepth;

        sphereProjectionDebugBuffer.data[gl_GlobalInvocationID.x].projectedAABB = aabb;
    }
    else
    {
        aabb = ((aabb + 1.0) * 0.5);
        sphereProjectionDebugBuffer.data[gl_GlobalInvocationID.x].projectedAABB = -aabb;
        level = 10;
    }

    if (!bool(ubo.culling_updating))
    {
        visible = drawnLastFrameBuffer.data[gl_GlobalInvocationID.x];
    }
    else
    {
        uvec2 meshResults = meshLODCalculation(mvPos, aabb, true);
        if (visible && !drawnLastFrameBuffer.data[gl_GlobalInvocationID.x])
        {
            // For indirect draw buffer compaction we keep a count of all meshes which have been
            // drawn.
            uint drawBufferIdx = atomicAdd(indirectBufferCountBuffer.data, 1);

            indirectBuffer.data[drawBufferIdx].indexCount = mix(0, meshResults[0], visible);
            indirectBuffer.data[drawBufferIdx].instanceCount = mix(0, 1, visible);
            indirectBuffer.data[drawBufferIdx].firstIndex = mix(0, meshResults[1], visible);
            indirectBuffer.data[drawBufferIdx].vertexOffset = 0;
            indirectBuffer.data[drawBufferIdx].firstInstance = 0;
            indirectBuffer.data[drawBufferIdx].meshId = gl_GlobalInvocationID.x;
        }
    }

    vec4 modelPos = modelTranformsBuffer.data[gl_GlobalInvocationID.x] * vec4(0.0, 0.0, 0.0, 1.0);
    vec2 modelXZ = modelPos.xz;
    modelXZ -= vec2(17.0, 0.0);
    float boundRadius = 5.0;
    modelXZ += vec2(boundRadius + 1.0, boundRadius + 1.0);
    modelXZ /= vec2(boundRadius * 2.0 + 2.0, boundRadius * 2.0 + 2.0);
    modelXZ *= vec2(100.0 - radius, 100.0 - radius);
    ivec2 debugMeshPos = ivec2(int(modelXZ[0]), int(modelXZ[1]));

    if (visible)
    {
        drawnLastFrameBuffer.data[gl_GlobalInvocationID.x] = true;
        // simply for debugging to ImGui's top down view
        if (sphereProjectionDebugBuffer.data.length() < 6)
        {
            imageStore(meshesDrawnDebugView, debugMeshPos, color_mapping_5[gl_GlobalInvocationID.x % 5]);
        }
        else
        {
            imageStore(meshesDrawnDebugView, debugMeshPos, color_mapping_11[uint(level)]);
        }
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
    vec4 mvPos = ubo.culling_view * modelPos;
    mvPos = vec4(mvPos.xyz / mvPos.w, 1.0);

    if (ubo.display_mode == 25)
    {
        uvec2 meshResults = meshLODCalculation(mvPos, vec4(1.0), false);

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
