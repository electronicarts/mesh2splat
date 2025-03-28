///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#define _USE_MATH_DEFINES

//-------------------- TEXTURE AND COLORS -------------------------------------------------------------------------------------------------------
#define MAX_RESOLUTION_TARGET					2048
#define RESOLUTION_TARGET_STR					"_512"
#define METALLIC_ROUGHNESS_TEXTURE				"metallicRoughnessTexture"
#define BASE_COLOR_TEXTURE						"baseColorTexture"
#define NORMAL_TEXTURE							"normalTexture"
#define AO_TEXTURE								"occlusionTexture"
#define EMISSIVE_TEXTURE						"emissiveTexture"
#define SH_COEFF0								0.28209479177387814f

//-------------------- FLAGS --------------------------------------------------------------------------------------------------------------------
#define DRAW_UV_MAPPING							0		//Set to 1 to produce a .tga with the normalized UV mapping produced by XATLAS
#define VOLUMETRIC								0

//-------------------- INPUT DATA ---------------------------------------------------------------------------------------------------------------
#define OBJ_NAME								"scifiHelmet"
#define MICROMESH_NAME							"microMesh6"
#define OBJ_FORMAT								".glb"

