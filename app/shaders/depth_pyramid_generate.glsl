#version 450

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0, r32f) uniform writeonly image2D outImage;
layout(binding = 1) uniform sampler2D inImage;

layout(push_constant) uniform block
{
	vec2 frame_size;
};

void main()
{
	uvec2 pos = gl_GlobalInvocationID.xy;

	//float real_width = pow(2, ceil(log2(frame_size.x)));
	//float real_height = pow(2, ceil(log2(frame_size.y)));

	vec2 real_size = frame_size;//vec2(max(real_width, real_height));

	if (pos.x > frame_size.x || pos.y > frame_size.y)
	{
		imageStore(outImage, ivec2(pos), vec4(1234.0));
		return;
	}

	vec2 scaling_factor = real_size / frame_size;

	// Sampler is set up to do min reduction, so this computes the minimum depth of a 2x2 texel quad
	float depth = texture(inImage, (((vec2(pos) + vec2(0.5)) / real_size) * scaling_factor)).x;

	imageStore(outImage, ivec2(pos), vec4(depth));
}