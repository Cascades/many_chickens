#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 fragColor;
//layout(location = 1) in vec3 inNormal;
//layout(location = 2) in vec2 fragTexCoord;
//layout(location = 3) in flat float texture_on;
//layout(location = 4) in float specularity;
//layout(location = 5) flat in uint ID;

layout(location = 0) out vec4 outColor;
//layout(location = 1) out vec4 outNormal;

layout(std140, binding = 0) uniform UniformBufferObject {
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

layout(binding = 1) uniform sampler2D texSampler;

struct SphereProjectionDebugData
{
    vec4 projectedAABB;
    //vec4 depthData;
};

layout(std430, binding = 3) buffer SphereProjectionDebugBuffer
{
	SphereProjectionDebugData data[];
} sphereProjectionDebugBuffer;

void main() {
    outColor = fragColor;
    //outNormal = vec4(0.0, 1.0, 0.0, 1.0);

    /* if (ubo.display_mode == 23)
    {
        //outColor = vec4(vec3(float(sphereProjectionDebugBuffer.data[ID].lodLevel + 1) / 10.0), 1.0);
    }
    else
    {
        if (texture_on > 0)
        {
            outColor = vec4(fragColor * texture(texSampler, fragTexCoord).rgb, 1.0);
        }
        else
        {
            outColor = vec4(fragColor, 1.0);
        }
    }

    if (sphereProjectionDebugBuffer.data.length() < 6)
    {
        vec4 abc[5] = vec4[](vec4(1.0, 0.0, 0.0, 1.0), vec4(0.0, 1.0, 0.0, 1.0), vec4(0.0, 0.0, 1.0, 1.0), vec4(1.0, 1.0, 0.0, 1.0), vec4(0.0, 1.0, 1.0, 1.0));
        //outNormal.rgb = abc[ID].rgb;
    }
    else
    {
        outNormal.rgb = normalize(inNormal) * 0.5 + vec3(0.5);
    }

    outColor.a = specularity;*/
}
