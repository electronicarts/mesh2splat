#pragma once
#define DEBUG 1
#define _USE_MATH_DEFINES
#define EPSILON 1e-8
#define MAX_TEXTURE_SIZE 2048
#define SH_COEFF0 0.28209479177387814f
#define DEFAULT_PURPLE glm::vec3(102.0f/255.0f, 51.0f/255.0f, 153.0f/255.0f) //RGB - 0 , ... , 255

#define METALLIC_ROUGHNESS_TEXTURE				"metallicRoughnessTexture"
#define BASE_COLOR_TEXTURE						"baseColorTexture"
#define NORMAL_TEXTURE							"normalTexture"
#define OCCLUSION_TEXTURE						"occlusionTexture"
#define EMISSIVE_TEXTURE						"emissiveTexture"

#define GPU_IMPL								1		//Set to 0 to try the CPU implementation
#define DRAW_UV_MAPPING							0		//Set to 1 to produce a .tga with the normalized UV mapping produced by XATLAS

#define OBJ_NAME								"scifiHelmet"
#define OBJ_FORMAT								".glb"
#define OUTPUT_GAUSSIAN_FORMAT					".ply"
#define DEFAULT_MATERIAL_NAME					"mm_default_material"

#define PIXEL_SIZE_GAUSSIAN_RADIUS				.2f // 0.20,...,0.25 seems to be the sweet round
													//maybe for some meshes with particular shapes and UV mappings .5f or .75f can give better results 

#define TESSELATION_LEVEL_FACTOR_MULTIPLIER		10

#define BASE_DATASET_FOLDER						"C:/Users/sscolari/Desktop/dataset/"  OBJ_NAME  "/"					

#define GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_1		"C:/Users/sscolari/Desktop/halcyon/Content/GaussianSplatting/Mesh2SplatOut.ply"
#define GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_2		"C:/Users/sscolari/Desktop/outputGaussians/" OBJ_NAME OUTPUT_GAUSSIAN_FORMAT

#define OUTPUT_FILENAME							BASE_DATASET_FOLDER OBJ_NAME OBJ_FORMAT