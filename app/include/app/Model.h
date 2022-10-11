#pragma once
#include <filesystem>
#include <glm/glm.hpp>

#include "app/Vertex.h"

struct LodConfigData
{
    float maxDist;
    uint32_t offset;
    uint32_t size;
    const uint32_t padding = 0;
};

class Model
{
    constexpr static uint32_t total_lod_levels = 5;

	std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> lod_indices_offsets;
    std::vector<uint32_t> lod_indices_sizes;
    std::vector<float> lod_max_distances;

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

    constexpr uint32_t getTotalLodLevels() const
    {
        return total_lod_levels;
    }

    std::vector<LodConfigData> getLodConfigData() const
    {
        std::vector<LodConfigData> lodConfigDataTempVec;

        for (size_t i = 0; i < total_lod_levels; ++i)
        {
            lodConfigDataTempVec.emplace_back(lod_max_distances[i], lod_indices_offsets[i], lod_indices_sizes[i]);
        }

        return lodConfigDataTempVec;
    }

    std::vector<float>& getMaxDistances()
    {
        return lod_max_distances;
    }

    template<size_t lod_level>
    uint32_t getIndicesOffset() const
	{
        //return lod_indices[lod_level];
        return lod_indices_offsets[total_lod_levels - 1];
	}

    template<size_t lod_level>
    uint32_t const& getIndicesSize() const
    {
        //return lod_indices[lod_level];
        return lod_indices_sizes[total_lod_levels - 1];
    }
private:
    void generateLOD();
};
