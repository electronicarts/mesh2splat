#pragma once

#include "utils/utils.hpp"

TextureDataGl loadImageAndBpp(std::string texturePath, int& textureWidth, int& textureHeight);

void loadAllTextureMapImagesIntoMap(MaterialGltf& material, std::map<std::string, TextureDataGl>& textureTypeMap);

std::vector<Mesh> parseGltfFileToMesh(const std::string& filename, std::string base_folder);

void writePbrPLY(const std::string& filename, const std::vector<Gaussian3D>& gaussians);

void writeBinaryPLY_lit(const std::string& filename, const std::vector<Gaussian3D>& gaussians);

void writeBinaryPLY_standard_format(const std::string& filename, const std::vector<Gaussian3D>& gaussians);

void savePlyVector(std::string outputFileLocation, std::vector<Gaussian3D> gaussians_3D_list, unsigned int format);


