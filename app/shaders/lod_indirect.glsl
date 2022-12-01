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
};

layout(binding = 7) buffer SphereProjectionDebugBuffer
{
	SphereProjectionDebugData data[];
} sphereProjectionDebugBuffer;

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
    aabb = aabb.xwzy * vec4(-0.5f, 0.5f, -0.5f, 0.5f) + vec4(0.5f); // clip space -> uv space

    return true;
}

void getBoundsForAxis(
    vec3 a, // Bounding axis (camera space)
    vec3 C, // Sphere center (camera space)
    float r, // Sphere radius
    vec3 projCenter, 
    float nearZ, // Near clipping plane (negative)
    out vec3 L, // Tangent point (camera space)
    out vec3 U) // Tangent point (camera space)
{
    vec2 c = vec2(dot(a, C), C.z); // C in the a-z frame

    vec2 bounds[2]; // In the a-z reference frame

    float tSquared = dot(c, c) - (r * r);
    bool cameraInsideSphere = (tSquared <= 0);

    // (cos, sin) of angle theta between c and a tangent vector
    vec2 v = cameraInsideSphere ? vec2(0.0f, 0.0f) : vec2(sqrt(tSquared), r) / c.length();
    //sphereProjectionDebugBuffer.data[gl_GlobalInvocationID.x].projectedAABB = vec4(v, v);

    // Does the near plane intersect the sphere?
    bool clipSphere = (c.y + r >= nearZ);

    // Square root of the discriminant; NaN (and unused)
    // if the camera is in the sphere
    float k = sqrt((r * r) - pow((nearZ - c.y), 2));

    for (int i = 0; i < 2; ++i)
    {
        if (!cameraInsideSphere)
        {
            bounds[i] = mat2(v.x, -v.y,
                             v.y, v.x) * c * v.x;
        }

        bool clipBound = cameraInsideSphere || (bounds[i].y > nearZ);

        if (clipSphere && clipBound)
        {
            bounds[i] = vec2(projCenter.x + k, nearZ);
        }

        // Set up for the lower bound
        v.y = -v.y; k = -k;
    }

    // Transform back to camera space
    L = bounds[1].x * a; L.z = bounds[1].y;
    U = bounds[0].x * a; U.z = bounds[0].y;
}


vec3 project(mat4 P, vec3 Q)
{
    vec4 v = P * vec4(Q, 1.0f);
    return v.xyz / v.w;
}

vec3 project(mat4 P, vec4 Q)
{
    vec4 v = P * Q;
    return v.xyz / v.w;
}

vec4 getAxisAlignedBoundingBox(
    vec3 C, // camera-space sphere center
    float r, // sphere radius
    float nearZ, // near clipping plane position (negative)
    mat4 P) // camera to screen space
{
    // Points on edges
    vec3 right;
    vec3 left;
    vec3 top;
    vec3 bottom;

    getBoundsForAxis(vec3(1,0,0), C, r, project(P, C), nearZ, left, right);
    getBoundsForAxis(vec3(0,1,0), C, r, project(P, C), nearZ, bottom, top);

    float projRight = project(P, right).x;
    float projLeft = project(P, left).x;
    float projTop = project(P, top).y;
    float projBottom = project(P, bottom).y;

    return vec4(
        max(projLeft, projRight),
        min(projBottom, projTop),
        min(projLeft, projRight),
        max(projBottom, projTop)).xwzy;
}

void main()
{
    if(gl_GlobalInvocationID.x >= indirectBuffer.data.length())
    {
        return;
    }

    // Hard-coded chicken pos
    vec4 modelPos = modelTranformsBuffer.data[gl_GlobalInvocationID.x] * vec4(0.0, 0.0, 0.0, 1.0);
    vec4 mvPos = ubo.view * modelPos;
    mvPos = vec4(mvPos.xyz / mvPos.w, 1.0);

    float radius = 0.351285 * modelScalesBuffer.data[gl_GlobalInvocationID.x];

    bool visible = true;

    float xyRatio = ubo.win_dim.x / ubo.win_dim.y;

    vec4 aabb = getAxisAlignedBoundingBox(mvPos.xyz, radius, -ubo.zNear, ubo.proj);
    /*bool projectionSuccess = projectSphere(mvPos.xyz, radius, ubo.zNear, ubo.p00, -ubo.p11, aabb);
    if (projectionSuccess)
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

        //visible = visible && depthSphere > depth;
    }*/
    sphereProjectionDebugBuffer.data[gl_GlobalInvocationID.x].projectedAABB = aabb;

    //drawnLastFrameBuffer.data[gl_GlobalInvocationID.x] = visible;

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