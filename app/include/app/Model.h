#pragma once
#include <filesystem>
#include <glm/glm.hpp>

#include "app/Vertex.h"

class Model
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

public:

    float Ns;
    float Ni;
    float d;
    float Tr;
    glm::vec3 Tf;
    float illum;
    glm::vec3 Ka;
    glm::vec3 Kd;
    glm::vec3 Ks;
    glm::vec3 Ke;
    float specular = 0.1f;
    float diffuse = 0.5f;
    float ambient = 0.2f;
	
	void loadModel(std::filesystem::path const & model_path);

	std::vector<Vertex> const& getVertices() const
	{
        return vertices;
	}

	std::vector<uint32_t> const& getIndices() const
	{
        return indices;
	}
};
