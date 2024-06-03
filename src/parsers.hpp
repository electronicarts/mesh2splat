#pragma once

#include "utils/utils.hpp"

TextureDataGl loadImageAndBpp(std::string texturePath, int& textureWidth, int& textureHeight);

void loadAllTexturesIntoMap(MaterialGltf& material, std::map<std::string, TextureDataGl>& textureTypeMap);

std::vector<Mesh> parseGltfFileToMesh(const std::string& filename);

void writeBinaryPLY(const std::string& filename, const std::vector<Gaussian3D>& gaussians);

