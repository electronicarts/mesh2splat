///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include <cstdint>
#include "tiny_gltf.h"
#include "stb_image.h"   
#include "stb_image_resize.h"
#include "stb_image_write.h"
#include "happly.h"
#include "utils/utils.hpp"

namespace parsers
{
	enum class DcMode : uint32_t
	{
		Current = 0,      // SH0 encoding (default)
		DirectLinear = 1, // Direct linear color
		DirectSrgb = 2    // Direct sRGB color
	};

	enum class OpacityMode : uint32_t
	{
		Current = 0,      // Use default behavior
		Raw = 1,          // Store opacity directly
		Logit = 2         // Apply inverse sigmoid
	};

	utils::TextureDataGl loadImageAndBpp(std::string texturePath, int& textureWidth, int& textureHeight);

	void writePbrPLY(const std::string& filename, std::vector<utils::GaussianDataSSBO>& gaussians, float scaleMultiplier);

	void writeBinaryPlyStandardFormat(const std::string& filename, const std::vector<utils::GaussianDataSSBO>& gaussians, float scaleMultiplier);

	void loadPlyFile(std::string plyFileLocation, std::vector<utils::GaussianDataSSBO>& gaussians, bool& hasPbr);

	void savePlyVector(std::string outputFileLocation, std::vector<utils::GaussianDataSSBO> gaussians_3D_list, unsigned int format, float scaleMultiplier);

	unsigned char* combineMetallicRoughness(const char* path1, const char* path2, int& width, int& height, int& channels);

	bool extractImageNames(const std::string& combinedName, std::string& path, std::string& name1, std::string& name2);
}


