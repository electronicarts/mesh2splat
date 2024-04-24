#pragma once
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include "utils.hpp"
#include <triangle.h>

#define DEFAULT_MATERIAL_NAME "mm_default_material_mm"

std::tuple<std::vector<Mesh>, std::vector<glm::vec3>, std::vector<glm::vec2>, std::vector<glm::vec3>> parseObjFileToMeshes(const std::string& filename);

triangulateio prepareTriangleInput(const std::vector<glm::vec3>& vertices, const std::vector<Mesh>& meshes);

std::map<std::string, Material> parseMtlFile(const std::string& filename);