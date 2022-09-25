#include "app/Model.h"

#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <glm/glm.hpp>

#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#endif

#include "app/Vertex.h"

void Model::loadModel(std::filesystem::path const & model_path) {	
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
                attrib.vertices[3 * index.vertex_index + 2] * 1.0
            };

            if (index.texcoord_index >= 0)
            {
                vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };

            }
            else
            {
                vertex.texCoord = { 0, 0 };
            }

            vertex.color = { 1.0f, 1.0f, 1.0f };


            vertex.norm = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]
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
}