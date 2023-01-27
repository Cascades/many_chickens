#version 450
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
    float p00;
	float p11;
	float zNear;
	int display_mode;
} ubo;

layout (input_attachment_index = 0, set = 0, binding = 1) uniform subpassInput inColor;
layout (input_attachment_index = 0, set = 0, binding = 2) uniform subpassInput inNormal;
layout (input_attachment_index = 0, set = 0, binding = 4) uniform subpassInput inDepth;
layout (set = 0, binding = 5) uniform sampler2D inShadowDepth;
layout (set = 0, binding = 6) uniform sampler2DShadow inShadowDepthPCF;
layout (set = 0, binding = 7) uniform sampler2D inDepthPyramid;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragcolor;

struct SphereProjectionDebugData
{
    vec4 projectedAABB;
    //vec4 depthData;
    //vec2 depthData;
    //vec2 depthLookUpCoord;
    //uint lodLevel;
};

layout(std430, binding = 8) buffer SphereProjectionDebugBuffer
{
	SphereProjectionDebugData data[];
} sphereProjectionDebugBuffer;

//float LinearizeDepth(float depth, float near_p, float far_p)
//{
//    float z = depth;// * 2.0 - 1.0; // Back to NDC 
//    return (2.0 * near_p * far_p) / (far_p + near_p - z * (far_p - near_p));
//}

float LinearizeDepth(float z, float n, float f) {
    //return ((f * n)/(z + (f / (n - f)))) * (1/(f - n));
    // equivelant to
    return (f * n)/(f * z - f - n * z);
}

vec4 position_from_depth(float depth)
{
    float z = subpassLoad(inDepth).r;

    vec4 clipSpace = vec4(inUV * 2.0 - 1.0, z, 1.0);
	vec4 viewSpace = inverse(ubo.proj) * clipSpace;
	viewSpace.xyz /= viewSpace.w;

    vec4 position = inverse(ubo.view) * vec4( viewSpace.xyz, 1.0 );

    return vec4(vec3(position), 1.0);
}

float calc_shadow_influence(vec4 position)
{
    
    return 1.0;
}

float insideBox(vec2 v, vec2 bottomLeft, vec2 topRight) {
    vec2 s = step(bottomLeft, v) - step(topRight, v);
    return s.x * s.y;   
}

void main() 
{
    vec4 tmpOutFragColor;

	if(ubo.display_mode == 0)
	{
		outFragcolor = vec4(subpassLoad(inNormal).rgb, 1.0);
	}
	else if(ubo.display_mode == 1)
	{
        float z = LinearizeDepth(subpassLoad(inDepth).r, -0.001, -250.0) / (250.0);
		outFragcolor = vec4(z, z, z,  1.0);
	}
	else if(ubo.display_mode == 2)
	{
		outFragcolor = vec4(subpassLoad(inColor).a, subpassLoad(inColor).a, subpassLoad(inColor).a, 1.0);
	}
	else if(ubo.display_mode == 3)
	{
		outFragcolor = vec4(subpassLoad(inColor).rgb, 1.0);
	}
    else if(ubo.display_mode == 4)
	{
        float depth_val = LinearizeDepth(texture(inShadowDepth, inUV).r, 0.001, 250.0) / 250.0;
		outFragcolor = vec4(depth_val, depth_val, depth_val, 1.0);
	}
    else if(ubo.display_mode == 5)
	{
        outFragcolor = position_from_depth(subpassLoad(inDepth).r);
	}
    else if(ubo.display_mode >= 6 && ubo.display_mode < 20)
	{
        vec2 frame_size = ubo.win_dim;

        float real_width = pow(2, ceil(log2(frame_size.x)));
	    float real_height = pow(2, ceil(log2(frame_size.y)));

	    vec2 real_size = vec2(max(real_width, real_height));

	    vec2 scaling_factor = real_size / frame_size;

        float depth_val = LinearizeDepth(textureLod(inDepthPyramid, inUV / scaling_factor, ubo.display_mode - 7).r, 0.001, 250.0) / 250.0;
		outFragcolor = vec4(depth_val, depth_val, depth_val, 1.0);
	}
	else
	{
        vec4 position = position_from_depth(subpassLoad(inDepth).r);

        vec3 normal = (subpassLoad(inNormal).rgb - vec3(0.5)) / 0.5;

		if(ubo.model_stage_on > 0)
        {
            if(ubo.lighting_stage_on > 0)
            {
                vec4 shadow_clip_space = ubo.lightVP * vec4(position.xyz, 1.0);
                vec4 shadow_NDC = shadow_clip_space / shadow_clip_space.w;
                shadow_NDC.xy = shadow_NDC.xy * 0.5 + 0.5;

                float shadow = 1.0;

                if(ubo.pcf_on > 0.5)
                {
                    shadow = texture(inShadowDepthPCF, shadow_NDC.xyz - vec3(0.0, 0.0, 0.00001)).r;
                }
                else
                {
                    float closest_dist = texture(inShadowDepth, shadow_NDC.xy).r;

                    if(shadow_NDC.z > closest_dist + 0.00001)
                    {
                        outFragcolor = vec4(clamp(ubo.Ke.xyz + subpassLoad(inColor).rgb * (ubo.ambient * ubo.Ka.xyz), vec3(0.0), vec3(1.0)), 1.0);
                        tmpOutFragColor = vec4(clamp(ubo.Ke.xyz + subpassLoad(inColor).rgb * (ubo.ambient * ubo.Ka.xyz), vec3(0.0), vec3(1.0)), 1.0);
                        return;
                    }
                }

                vec3 frag_pos = position.xyz;
                vec3 normal_dir = normalize((mat3(ubo.model) * normal).xyz);
                vec3 light_pos = (ubo.light * vec4(-2.5, 0.0, 0.0, 1.0)).xyz;
                vec3 light_dir = normalize(frag_pos - light_pos);

                float ambient = ubo.ambient;
                float diffuse = ubo.diffuse * max(0.0, dot(normal_dir, -light_dir)) * shadow;
            
                float specular = 0.0;
                if(diffuse != 0.0)
                {
                    vec3 camera_dir = normalize(frag_pos - vec3(-2.0, 0.0, 0.0));
                    vec3 reflection_dir = normalize(reflect(light_dir, normal_dir));

                    float spec_val = pow(max(dot(reflection_dir, -camera_dir), 0.0), ubo.Ns);
                    specular = clamp(subpassLoad(inColor).a * spec_val, 0.0, 1.0) * shadow;
                }

                outFragcolor = vec4(clamp(ubo.Ke.xyz + subpassLoad(inColor).rgb * (ambient * ubo.Ka.xyz + diffuse * ubo.Kd.xyz + specular * ubo.Ks.xyz), vec3(0.0), vec3(1.0)), 1.0);
                tmpOutFragColor = vec4(clamp(ubo.Ke.xyz + subpassLoad(inColor).rgb * (ambient * ubo.Ka.xyz + diffuse * ubo.Kd.xyz + specular * ubo.Ks.xyz), vec3(0.0), vec3(1.0)), 1.0);
             }
             else
             {
                outFragcolor = vec4(subpassLoad(inColor).rgb, 1.0);
                tmpOutFragColor = vec4(subpassLoad(inColor).rgb, 1.0);
             }
        }
        else
        {
            outFragcolor = vec4(0.0);
            tmpOutFragColor = vec4(0.0);
        }
	}

    if(ubo.display_mode >= 20 && ubo.display_mode < 23)
	{
        vec2 clipSpace = vec2(inUV * 2.0 - 1.0);

        /*float currentExtraRVal = 0.0f;

        for (uint i = 0; i < sphereProjectionDebugBuffer.data.length(); ++i)
        {
            bool inBox = clipSpace.x < sphereProjectionDebugBuffer.data[i].projectedAABB[0] &&
                         clipSpace.x > sphereProjectionDebugBuffer.data[i].projectedAABB[2] &&
                         clipSpace.y < sphereProjectionDebugBuffer.data[i].projectedAABB[1] &&
                         clipSpace.y > sphereProjectionDebugBuffer.data[i].projectedAABB[3];
            bool tooFarInBox = clipSpace.x < sphereProjectionDebugBuffer.data[i].projectedAABB[0] - 0.002 &&
                               clipSpace.x > sphereProjectionDebugBuffer.data[i].projectedAABB[2] + 0.002 &&
                               clipSpace.y < sphereProjectionDebugBuffer.data[i].projectedAABB[1] - 0.002 &&
                               clipSpace.y > sphereProjectionDebugBuffer.data[i].projectedAABB[3] + 0.002;
            if(inBox && !tooFarInBox)
            {
                currentExtraRVal = 0.7;
            }
        }
        
        outFragcolor = vec4(
            min(1.0, tmpOutFragColor.x + currentExtraRVal),
            tmpOutFragColor.y,
            tmpOutFragColor.z,
            tmpOutFragColor.w);*/
        
        float currentExtraRVal = 0.0f;

        for (uint i = 0; i < sphereProjectionDebugBuffer.data.length(); ++i)
        {
            float lod_level = int(sphereProjectionDebugBuffer.data[i].projectedAABB[3]);
            float pixel_size = pow(2.0, lod_level);
            bool inBox = inUV.x > sphereProjectionDebugBuffer.data[i].projectedAABB[0] &&
                         inUV.x < sphereProjectionDebugBuffer.data[i].projectedAABB[2] &&
                         inUV.y > sphereProjectionDebugBuffer.data[i].projectedAABB[1] &&
                         inUV.y < sphereProjectionDebugBuffer.data[i].projectedAABB[3];
            if(inBox)
            {
                currentExtraRVal = 0.7;
            }
        }
        
        outFragcolor = vec4(
            min(1.0, tmpOutFragColor.x + currentExtraRVal),
            tmpOutFragColor.y,
            tmpOutFragColor.z,
            tmpOutFragColor.w);
	}

}