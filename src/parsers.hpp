#pragma once

#include "utils/utils.hpp"

unsigned char* loadImage(std::string texturePath, int& textureWidth, int& textureHeight);

//std::tuple<std::vector<Mesh>, std::vector<glm::vec3>, std::vector<glm::vec2>, std::vector<glm::vec3>> parseObjFileToMeshes(const std::string& filename);

std::map<std::string, Material> parseMtlFile(const std::string& filename);

std::tuple<std::vector<Mesh>, std::vector<glm::vec3>, std::vector<glm::vec2>, std::vector<glm::vec3>> parseGltfFileToMeshAndGlobalData(const std::string& filename);

void writeBinaryPLY(const std::string& filename, const std::vector<Gaussian3D>& gaussians);