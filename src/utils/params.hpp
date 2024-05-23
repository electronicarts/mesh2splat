#pragma once
#define EIGEN_BASED_GAUSSIANS_ROTATION 0
#define DEBUG 1
#define _USE_MATH_DEFINES
#define EPSILON 1e-8
#define MAX_TEXTURE_SIZE 2048
#define SH_COEFF0 0.28209479177387814f
#define DEFAULT_PURPLE glm::vec3(102.0f/255.0f, 51.0f/255.0f, 153.0f/255.0f) //RGB - 0 , ... , 255

#define METALLIC_ROUGHNESS_TEXTURE "metallicRoughnessTexture"
#define BASE_COLOR_TEXTURE "baseColorTexture"
#define NORMAL_TEXTURE "normalTexture"
#define OCCLUSION_TEXTURE "occlusionTexture"
#define EMISSIVE_TEXTURE "emissiveTexture"

#define GPU_IMPL 1

#define OBJ_NAME					"scifiHelmet"
#define OBJ_FORMAT					".glb"
#define OUTPUT_GAUSSIAN_FORMAT		".ply"
#define DEFAULT_MATERIAL_NAME		"mm_default_material"

#define EXPECTED_MAX_VERTICES_PER_PATCH 500
#define PIXEL_SIZE_GAUSSIAN_RADIUS .25f
#define TESSELATION_LEVEL_FACTOR_MULTIPLIER 250

#define BASE_DATASET_FOLDER						"C:/Users/sscolari/Desktop/dataset/"  OBJ_NAME  "/"					

#define GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_1		"C:/Users/sscolari/Desktop/halcyon/Content/GaussianSplatting/Mesh2SplatOut.ply"
#define GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_2		"C:/Users/sscolari/Desktop/outputGaussians/" OBJ_NAME OUTPUT_GAUSSIAN_FORMAT

#define OUTPUT_FILENAME							BASE_DATASET_FOLDER OBJ_NAME OBJ_FORMAT