#include "app/Model.h"

#include <filesystem>
#include <format>
#include <iostream>
#include <unordered_map>
#include <glm/glm.hpp>

#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#endif

#include <meshoptimizer.h>

#include "app/Vertex.h"
