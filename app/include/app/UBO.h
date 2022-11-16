#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
	glm::mat4 light;
	glm::mat4 lightVP;
	glm::vec4 Ka;
	glm::vec4 Kd;
	glm::vec4 Ks;
	glm::vec4 Ke;
	glm::vec2 win_dim;
	glm::float32 Ns;
	glm::float32 model_stage_on;
	glm::float32 texture_stage_on;
	glm::float32 lighting_stage_on;
	glm::float32 pcf_on;
	glm::float32 specular;
	glm::float32 diffuse;
	glm::float32 ambient;
	glm::float32 shadow_bias;
	glm::float32 p00;
	glm::float32 p11;
	glm::float32 zNear;
	glm::int32 display_mode;
};

struct ShadowUniformBufferObject
{
	glm::mat4 depthMVP;
};