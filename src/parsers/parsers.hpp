///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
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
		Current = 0,
		DirectLinear = 1,
		DirectSrgb = 2
	};
	enum class OpacityMode : uint32_t
	{
		Current = 0,
		Raw = 1,
		Logit = 2
	};
	utils::TextureDataGl loadImageAndBpp(std::string texturePath, int& textureWidth, int& textureHeight);

	void writePbrPLY(const std::string& filename, std::vector<utils::GaussianDataSSBO>& gaussians, float scaleMultiplier, DcMode dcMode, OpacityMode opacityMode);

	void writeBinaryPlyStandardFormat(const std::string& filename, const std::vector<utils::GaussianDataSSBO>& gaussians, float scaleMultiplier, DcMode dcMode, OpacityMode opacityMode);

	void loadPlyFile(std::string plyFileLocation, std::vector<utils::GaussianDataSSBO>& gaussians);

	void saveSplatVector(std::string outputFileLocation, std::vector<utils::GaussianDataSSBO> gaussians_3D_list, unsigned int format, float scaleMultiplier, DcMode dcMode, OpacityMode opacityMode);

	unsigned char* combineMetallicRoughness(const char* path1, const char* path2, int& width, int& height, int& channels);

	bool extractImageNames(const std::string& combinedName, std::string& path, std::string& name1, std::string& name2);
}


