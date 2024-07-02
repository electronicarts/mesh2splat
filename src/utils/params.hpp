#pragma once
#define _USE_MATH_DEFINES


//-------------------- TEXTURE AND COLORS -------------------------------------------------------------------------------------------------------
#define RESOLUTION_TARGET						2048
#define METALLIC_ROUGHNESS_TEXTURE				"metallicRoughnessTexture"
#define BASE_COLOR_TEXTURE						"baseColorTexture"
#define NORMAL_TEXTURE							"normalTexture"
#define OCCLUSION_TEXTURE						"occlusionTexture"
#define EMISSIVE_TEXTURE						"emissiveTexture"
#define DEFAULT_PURPLE							glm::vec3(102.0f/255.0f, 51.0f/255.0f, 153.0f/255.0f) //RGB - 0 , ... , 255
#define SH_COEFF0								0.28209479177387814f


//-------------------- FLAGS --------------------------------------------------------------------------------------------------------------------
#define GPU_IMPL								1		//Set to 0 to try the CPU implementation
#define DRAW_UV_MAPPING							0		//Set to 1 to produce a .tga with the normalized UV mapping produced by XATLAS
#define DEBUG									1
#define VOLUMETRIC								0
#define PLY_FORMAT								3

//-------------------- INPUT DATA ---------------------------------------------------------------------------------------------------------------
#define OBJ_NAME								"mayanFigure"
#define MICROMESH_NAME							"microMesh6"
#define OBJ_FORMAT								".glb"


//-------------------- DEFAULTS -----------------------------------------------------------------------------------------------------------------
#define DEFAULT_MATERIAL_NAME					"mm_default_material"


//-------------------- 2D/3D GAUSSIAN PARAMS ----------------------------------------------------------------------------------------------------
#define PIXEL_SIZE_GAUSSIAN_RADIUS				1.0f	//In order to have good sum overlap at the corners, this ensures the densit at the corner of each pixel is .25%
#define EPSILON									1e-8	// Default Z axis scale


//-------------------- TESSELATION FACTOR -------------------------------------------------------------------------------------------------------
#define TESSELATION_LEVEL_FACTOR_MULTIPLIER		200		//increase for greater tesselation


//-------------------- DIRECTORIES --------------------------------------------------------------------------------------------------------------
#define BASE_MICROMESH_FOLDER					"C:/Users/sscolari/Desktop/dataset/" MICROMESH_NAME "/"
#define BASE_DATASET_FOLDER						"C:/Users/sscolari/Desktop/dataset/"  OBJ_NAME  "/"				

#define OUTPUT_GAUSSIAN_FORMAT					".ply"
#define GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_1		"C:/Users/sscolari/Desktop/halcyonUpdate/halcyon/Content/Halcyon/PlyTestAssets/Mesh2SplatOut1.ply"
#define GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_2		"C:/Users/sscolari/Desktop/outputGaussians/" OBJ_NAME OUTPUT_GAUSSIAN_FORMAT

#define INPUT_MESH_FILENAME						BASE_DATASET_FOLDER OBJ_NAME OBJ_FORMAT
#define INPUT_MICROMESH_FILENAME				BASE_MICROMESH_FOLDER MICROMESH_NAME OBJ_FORMAT