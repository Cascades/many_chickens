#pragma once
#include <filesystem>
#include <format>
#include <iostream>
#include <unordered_map>
#include <glm/glm.hpp>

#include <tiny_obj_loader.h>

#include <meshoptimizer.h>

#include "app/Vertex.h"

struct LodConfigData
{
    float maxDist;
    uint32_t offset;
    uint32_t size;
    const uint32_t padding = 0;
};

template<uint32_t total_lod_levels>
class Model
{
    //constexpr static uint32_t total_lod_levels = 5;

	std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> lod_indices_offsets;
    std::vector<uint32_t> lod_indices_sizes;
    std::vector<float> lod_max_distances;

    size_t max_meshlets;
    std::vector<meshopt_Meshlet> meshlets;
    std::vector<unsigned int> meshlet_vertices;
    std::vector<unsigned char> meshlet_triangles;
    std::vector<meshopt_Bounds> meshlet_bounds;

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

	void loadModel(std::filesystem::path const & model_path)
    {
        tinyobj::ObjReaderConfig reader_config;
        //reader_config.mtl_search_path = "..\\assets"; // Path to material files

        tinyobj::ObjReader reader;

        if (!reader.ParseFromFile(model_path.generic_string(), reader_config)) {
            if (!reader.Error().empty()) {
                std::cerr << "TinyObjReader: " << reader.Error();
            }
            exit(1);
        }

        if (!reader.Warning().empty()) {
            std::cout << "TinyObjReader: " << reader.Warning();
        }

        auto& attrib = reader.GetAttrib();
        auto& shapes = reader.GetShapes();
        auto& materials = reader.GetMaterials();

        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                Vertex vertex{};

                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0] * 1.0,
                    attrib.vertices[3 * index.vertex_index + 1] * 1.0,
                    attrib.vertices[3 * index.vertex_index + 2] * 1.0,
                    1.0
                };

                if (index.texcoord_index >= 0)
                {
                    vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    attrib.texcoords[2 * index.texcoord_index + 1]
                    };

                }
                else
                {
                    vertex.texCoord = { 0, 0 };
                }

                vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };


                vertex.norm = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2],
                    0.0
                };

                vertex.norm = glm::normalize(vertex.norm);

                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }

            if (!materials.empty())
            {
                Ns = materials[shape.mesh.material_ids[0]].shininess;
                Ni = 1.0f;
                d = materials[shape.mesh.material_ids[0]].dissolve;
                Tr = 1.0f - d;
                Tf = glm::vec3(1.0f, 1.0f, 1.0f);
                illum = static_cast<float>(materials[shape.mesh.material_ids[0]].illum);
                Ka = glm::vec3(materials[shape.mesh.material_ids[0]].ambient[0],
                    materials[shape.mesh.material_ids[0]].ambient[1],
                    materials[shape.mesh.material_ids[0]].ambient[2]);
                Kd = glm::vec3(materials[shape.mesh.material_ids[0]].diffuse[0],
                    materials[shape.mesh.material_ids[0]].diffuse[1],
                    materials[shape.mesh.material_ids[0]].diffuse[2]);
                Ks = glm::vec3(materials[shape.mesh.material_ids[0]].specular[0],
                    materials[shape.mesh.material_ids[0]].specular[1],
                    materials[shape.mesh.material_ids[0]].specular[2]);
                Ke = glm::vec3(materials[shape.mesh.material_ids[0]].emission[0],
                    materials[shape.mesh.material_ids[0]].emission[1],
                    materials[shape.mesh.material_ids[0]].emission[2]);
            }
            else
            {
                Ns = 0.0;
                Ni = 1.0f;
                d = 0.0;
                Tr = 1.0f - d;
                Tf = glm::vec3(1.0f, 1.0f, 1.0f);
                illum = 0.0;
                Ka = glm::vec3(0.2, 0.2, 0.2);
                Kd = glm::vec3(0.7, 0.7, 0.7);
                Ks = glm::vec3(0.2, 0.2, 0.2);
                Ke = glm::vec3(0.0, 0.0, 0.0);
            }
        }

        float maxDist = 0.0f;
        for (auto const& vertex : vertices)
        {
            maxDist = std::max(maxDist, glm::length(vertex.pos));
        }
        std::cout << "MAX DISTTTTTT: " << maxDist << std::endl;

        generateLOD();
        generateMeshlets();
    }

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

    std::vector<meshopt_Meshlet> const& getMeshlets() const
    {
        return meshlets;
    }

    std::vector<unsigned int> const& getMeshletVertices() const
    {
        return meshlet_vertices;
    }

    std::vector<unsigned char> const& getMeshletIndices() const
    {
        return meshlet_triangles;
    }

private:
    void generateLOD()
    {
        std::array<std::vector<uint32_t>, total_lod_levels> lod_indices;
        lod_indices[0] = indices;

        size_t index_count = lod_indices[0].size();
        std::vector<unsigned int> remap(index_count); // allocate temporary memory for the remap table
        size_t vertex_count = meshopt_generateVertexRemap(remap.data(), lod_indices[0].data(), index_count, vertices.data(), vertices.size(), sizeof(Vertex));

        meshopt_remapIndexBuffer(lod_indices[0].data(), lod_indices[0].data(), index_count, remap.data());
        meshopt_remapVertexBuffer(vertices.data(), vertices.data(), vertex_count, sizeof(Vertex), remap.data());

        std::cout << "| lod_level | threshold | target_index_count | target_error | new_index_count | lod_error |" << std::endl;

        for (size_t lod_level = 0; lod_level < static_cast<float>(lod_indices.size()) - 1; ++lod_level)
        {
            size_t lod_index = lod_indices.size() - 1 - lod_level;

            float threshold_lower_bound = 3000.0f / static_cast<float>(index_count);
            float threshold = threshold_lower_bound + (((1.0f / static_cast<float>(lod_indices.size())) * static_cast<float>(lod_level)) * (1.0f - threshold_lower_bound));
            size_t target_index_count = size_t(index_count * threshold);
            float target_error = 0.02f;

            lod_indices[lod_index].resize(lod_indices[0].size());
            float lod_error = 0.f;
            lod_indices[lod_index].resize(
                meshopt_simplify(
                    lod_indices[lod_index].data(),
                    lod_indices[0].data(),
                    lod_indices[0].size(),
                    &(vertices[0].pos.x),
                    vertex_count,
                    sizeof(Vertex),
                    target_index_count,
                    target_error,
                    0,
                    &lod_error));

            std::cout << std::format("| {:^9} | {:.7f} | {:>18} | {:.10f} | {:>15} | {:.7f} |",
                lod_index,
                threshold,
                target_index_count,
                target_error,
                lod_indices[lod_index].size(),
                lod_error) << std::endl;
        }

        /*
        std::cout << std::format("| {:^9} | {:.7f} | {:>18} | {:.10f} | {:>15} | {:.7f} |",
            0,
            1.0f,
            index_count,
            0.0f,
            index_count,
            0.0f) << std::endl;
        */
        indices.clear();

        float cuur_max_dist = 8.0f;

        for (auto const& curr_lod_indices : lod_indices)
        {
            lod_indices_offsets.push_back(indices.size());
            lod_indices_sizes.push_back(curr_lod_indices.size());
            lod_max_distances.push_back(cuur_max_dist);

            cuur_max_dist += 2.0f;

            indices.insert(indices.end(), curr_lod_indices.begin(), curr_lod_indices.end());
        }

        lod_max_distances.back() = 50.0f;

        std::cout << "total indices: " << indices.size() << std::endl;;
    }

    void generateMeshlets()
    {
        const size_t max_vertices = 64;
        const size_t max_triangles = 64;
        const float cone_weight = 0.0f;

        max_meshlets = meshopt_buildMeshletsBound(indices.size(), max_vertices, max_triangles);
        meshlets.resize(max_meshlets);
        meshlet_vertices.resize(max_meshlets * max_vertices);
        meshlet_triangles.resize(max_meshlets * max_triangles * 3);

        const auto lodConfigData = getLodConfigData();

        size_t meshlet_count = meshopt_buildMeshlets(meshlets.data(),
            meshlet_vertices.data(),
            meshlet_triangles.data(),
            &indices[lodConfigData[0].offset],
            lodConfigData[0].size,
            &vertices[0].pos.x,
            vertices.size(),
            sizeof(Vertex),
            max_vertices,
            max_triangles,
            cone_weight);

        const meshopt_Meshlet& last = meshlets[meshlet_count - 1];

        meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
        meshlet_triangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
        meshlets.resize(meshlet_count);
        meshlet_bounds.resize(meshlets.size());

        size_t meshletCount = 0;
        for (const auto& m : meshlets)
        {
            meshlet_bounds[meshletCount] = meshopt_computeMeshletBounds(&meshlet_vertices[m.vertex_offset],
                &meshlet_triangles[m.triangle_offset],
                m.triangle_count,
                &vertices[0].pos.x,
                vertices.size(),
                sizeof(Vertex));
            ++meshletCount;
        }
    }
};
