///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "utils.hpp"
#include "parsers/parsers.hpp"
#include "xatlas/xatlas.h"
#include "stb_image_write.h"

namespace uvUnwrapping
{
	void generateNormalizedUvCoordinatesPerMesh(int& uvSpaceWidth, int& uvSpaceHeight, std::vector<utils::Mesh>& meshes);

	void generateNormalizedUvCoordinatesPerFace(int& uvSpaceWidth, int& uvSpaceHeight, utils::Mesh& meshes);
}

