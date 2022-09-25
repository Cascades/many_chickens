#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in flat float texture_on;
layout(location = 4) in float specularity;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;

layout(binding = 1) uniform sampler2D texSampler;

void main() {
    if(texture_on > 0)
    {
        outColor = vec4(fragColor * texture(texSampler, fragTexCoord).rgb, 1.0);
    }
    else
    {
        outColor = vec4(fragColor, 1.0);
    }

    outNormal.rgb = normalize(inNormal) * 0.5 + vec3(0.5);
    outColor.a = specularity;
}