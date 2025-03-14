#pragma once
#include "../../thirdParty/tiny_gltf.h"
#include "../../thirdParty/stb_image.h"   
#include "../../thirdParty/stb_image_resize.h"
#include "../../thirdParty/stb_image_write.h"
#include "../../thirdParty/happly.h"
#include "../utils/utils.hpp"

namespace parsers
{
	utils::TextureDataGl loadImageAndBpp(std::string texturePath, int& textureWidth, int& textureHeight);

	void writePbrPLY(const std::string& filename, std::vector<utils::GaussianDataSSBO>& gaussians, float scaleMultiplier);

	void writeBinaryPlyStandardFormat(const std::string& filename, const std::vector<utils::GaussianDataSSBO>& gaussians, float scaleMultiplier);

	void loadPlyFile(std::string plyFileLocation, std::vector<utils::GaussianDataSSBO>& gaussians);

	void savePlyVector(std::string outputFileLocation, std::vector<utils::GaussianDataSSBO> gaussians_3D_list, unsigned int format, float scaleMultiplier);

	unsigned char* combineMetallicRoughness(const char* path1, const char* path2, int& width, int& height, int& channels);

	bool extractImageNames(const std::string& combinedName, std::string& path, std::string& name1, std::string& name2);
}


