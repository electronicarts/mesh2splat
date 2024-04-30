#pragma once
#define _CRTDBG_MAP_ALLOC  
#include <stdlib.h>  
#include <crtdbg.h>  

#include <string>
#include <vector>
#include <glm.hpp>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
#define EMPTY_TEXTURE "empty_texture"
#include "params.hpp"

struct Material {
    glm::vec3 ambient;       // Ka
    glm::vec3 diffuse;       // Kd
    glm::vec3 specular;      // Ks
    float specularExponent;  // Ns
    float transparency;      // d or Tr
    float opticalDensity;    // Ni
    std::string diffuseMap;  // map_Kd, texture map

    Material() : ambient(0.0f), diffuse(0.0f), specular(0.0f), specularExponent(0.0f), transparency(1.0f), opticalDensity(1.0f) {}
};

struct TextureInfo {
    std::string path;
    int texCoordIndex; // Texture coordinate set index used by this texture
    unsigned char* texture;
    int width, height;

    TextureInfo(const std::string& path = EMPTY_TEXTURE, int texCoordIndex = 0, unsigned char* texture = nullptr, int width = 0, int height = 0) : path(path), texCoordIndex(texCoordIndex), texture(texture), width(width), height(height) {}
};

struct MaterialGltf {
    std::string name;
    glm::vec4 baseColorFactor; // Default: white
    TextureInfo baseColorTexture; // Texture for the base color
    TextureInfo normalTexture; // Normal map
    float metallicFactor; // Metallic-Roughness map
    float roughnessFactor; // Metallic-Roughness map

    MaterialGltf() : name("Default"), baseColorFactor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)),
        baseColorTexture(TextureInfo()), normalTexture(TextureInfo()), metallicFactor(0.0f), roughnessFactor(0.0f) {}

    MaterialGltf(const std::string name, const glm::vec4 baseColorFactor) : 
        name(name), baseColorFactor(baseColorFactor), 
        baseColorTexture(TextureInfo()), normalTexture(TextureInfo()), metallicFactor(0.0f), roughnessFactor(0.0f) {}

    MaterialGltf(const std::string& name, const glm::vec4& baseColorFactor, const TextureInfo& baseColorTexture = TextureInfo(),
        const TextureInfo& normalTexture = TextureInfo(), const float metallicFactor = 0.0f, const float roughnessFactor = 0.0f)
        : name(name), baseColorFactor(baseColorFactor), baseColorTexture(baseColorTexture),
        normalTexture(normalTexture), metallicFactor(metallicFactor), roughnessFactor(roughnessFactor) {}

};

struct Gaussian3D {
    Gaussian3D(glm::vec3 position, glm::vec3 normal, glm::vec3 scale, glm::vec4 rotation, glm::vec3 RGB, float opacity, MaterialGltf material)
        : position(position), normal(normal), scale(scale), rotation(rotation), sh0(RGB), opacity(opacity), material(material) {};
    Gaussian3D() : position(NULL), normal(NULL), scale(NULL), rotation(NULL), sh0(NULL), opacity(NULL), material(MaterialGltf()) {};
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 scale;
    glm::vec4 rotation;
    glm::vec3 sh0;
    float opacity;
    MaterialGltf material;
};

struct Face {
    std::vector<glm::vec3> vertexIndices;
    std::vector<glm::vec2> uvIndices;
    std::vector<glm::vec3> normalIndices;

    Face(const std::vector<glm::vec3>& verts, const std::vector<glm::vec2>& uvs, const std::vector<glm::vec3>& norms)
        : vertexIndices(verts), uvIndices(uvs), normalIndices(norms) {}
};

class Mesh {
public:
    std::string name;
    std::vector<Face> faces; // Tuple of vertex indices, uv indices and normalIndices
    MaterialGltf material;

    Mesh(const std::string& name = "Unnamed") : name(name) {}
};


bool file_exists(std::string fn);

bool pointInTriangle(const glm::vec2& pt, const glm::vec2& v1, const glm::vec2& v2, const glm::vec2& v3);

bool computeBarycentricCoords(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, float& u, float& v, float& w);

glm::vec3 getColor(glm::vec3 color);

glm::vec3 floatToVec3(const float val);

int inline sign(float x);

glm::vec2 pixelToUV(const glm::ivec2& pixel, int textureWidth, int textureHeight);

glm::ivec2 uvToPixel(const glm::vec2& uv, int textureWidth, int textureHeight);

std::pair<glm::vec2, glm::vec2> computeUVBoundingBox(const std::vector<glm::vec2>& triangleUVs);

//https://www.nayuki.io/res/srgb-transform-library/srgb-transform.c
float linear_to_srgb_float(float x); //Assumes 0,...,1 range 

//https://www.nayuki.io/res/srgb-transform-library/srgb-transform.c
float srgb_to_linear_float(float x); //Assumes 0,...,1 range 

glm::vec4 rgbaAtPos(const int width, int X, int Y, unsigned char* rgb_image, const int bpp);

float displacementAtPos(const int width, int X, int Y, unsigned char* displacement_image);

float computeTriangleAreaUV(const glm::vec2& uv1, const glm::vec2& uv2, const glm::vec2& uv3);

void convert_xyz_to_cube_uv(float x, float y, float z, int* index, float* u, float* v);

void convert_cube_uv_to_xyz(int index, float u, float v, float* x, float* y, float* z);