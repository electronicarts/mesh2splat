#pragma once
#define _CRTDBG_MAP_ALLOC  
#include <stdlib.h>  
#include <crtdbg.h>  

#include <string>
#include <vector>
#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include <map>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include "../../thirdParty/stb_image_write.h"
#include <limits>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
#define EMPTY_TEXTURE "empty_texture"
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/string_cast.hpp>
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
    glm::vec4 baseColorFactor;              // Name of material, default is white
    TextureInfo baseColorTexture;           // Texture for the base color
    TextureInfo normalTexture;              // Normal map
    TextureInfo metallicRoughnessTexture;   // Contains the metalness value in the "blue" color channel, and the roughness value in the "green" color channel
    TextureInfo occlusionTexture;           // Texture for occlusion mapping
    TextureInfo emissiveTexture;            // Texture for emissive mapping
    float metallicFactor;                   // Metallic-Roughness map
    float roughnessFactor;                  // Metallic-Roughness map
    float occlusionStrength;                // Strength of occlusion effect
    float normalScale;                      // Scale of normal map
    glm::vec3 emissiveFactor;               // Emissive color factor

    MaterialGltf() : name("Default"), baseColorFactor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)),
        baseColorTexture(TextureInfo()), normalTexture(TextureInfo()), metallicRoughnessTexture(TextureInfo()),
        occlusionTexture(TextureInfo()), emissiveTexture(TextureInfo()),
        metallicFactor(1.0f), roughnessFactor(1.0f), occlusionStrength(1.0f), normalScale(1.0f), emissiveFactor(glm::vec3(1.0f, 1.0f, 1.0f)) {}

    MaterialGltf(const std::string& name, const glm::vec4& baseColorFactor) :
        name(name), baseColorFactor(baseColorFactor),
        baseColorTexture(TextureInfo()), normalTexture(TextureInfo()), metallicRoughnessTexture(TextureInfo()),
        occlusionTexture(TextureInfo()), emissiveTexture(TextureInfo()),
        metallicFactor(1.0f), roughnessFactor(1.0f), occlusionStrength(1.0f), normalScale(1.0f), emissiveFactor(glm::vec3(1.0f, 1.0f, 1.0f)) {}

    MaterialGltf(const std::string& name, const glm::vec4& baseColorFactor, const TextureInfo& baseColorTexture,
        const TextureInfo& normalTexture, const TextureInfo& metallicRoughnessTexture, const TextureInfo& occlusionTexture,
        const TextureInfo& emissiveTexture, float metallicFactor, float roughnessFactor, float occlusionStrength, float normalScale,
        glm::vec3 emissiveFactor) : name(name), baseColorFactor(baseColorFactor), baseColorTexture(baseColorTexture),
        normalTexture(normalTexture), metallicRoughnessTexture(metallicRoughnessTexture), occlusionTexture(occlusionTexture),
        emissiveTexture(emissiveTexture), metallicFactor(metallicFactor), roughnessFactor(roughnessFactor), occlusionStrength(occlusionStrength),
        normalScale(normalScale), emissiveFactor(emissiveFactor) {}
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
    glm::vec3 pos[3];
    glm::vec2 uv[3];
    glm::vec2 normalizedUvs[3]; //Resulting from xatlas
    glm::vec3 normal[3];
    glm::vec4 tangent[3];
    glm::vec3 scale;
    glm::vec4 rotation;
};

struct Mesh { //TODO: 
    std::string name;
    std::vector<Face> faces; // Tuple of vertex indices, uv indices and normalIndices
    MaterialGltf material;

    Mesh(const std::string& name = "Unnamed") : name(name) {}
};


struct GLMesh {
    GLuint vao;
    GLuint vbo;
    size_t vertexCount;
};

struct TextureDataGl {
    unsigned char* textureData;
    unsigned int bpp;
    unsigned int glTextureID;

    TextureDataGl(unsigned char* textureData, unsigned int bpp, unsigned int glTextureID) : textureData(textureData), bpp(bpp), glTextureID(glTextureID){}
    TextureDataGl(unsigned char* textureData, unsigned int bpp) : textureData(textureData), bpp(bpp), glTextureID(0){}
};

bool file_exists(std::string fn);

bool pointInTriangle(const glm::vec2& pt, const glm::vec2& v1, const glm::vec2& v2, const glm::vec2& v3);

bool computeBarycentricCoords(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, float& u, float& v, float& w);

glm::vec3 getColor(glm::vec3 color);

glm::vec3 floatToVec3(const float val);

int inline sign(float x);

glm::vec2 pixelToUV(const glm::ivec2& pixel, int textureWidth, int textureHeight);

glm::ivec2 uvToPixel(const glm::vec2& uv, int textureWidth, int textureHeight);

std::pair<glm::vec2, glm::vec2> computeUVBoundingBox(const glm::vec2* triangleUVs);

//https://www.nayuki.io/res/srgb-transform-library/srgb-transform.c
float linear_to_srgb_float(float x); //Assumes 0,...,1 range 

//https://www.nayuki.io/res/srgb-transform-library/srgb-transform.c
float srgb_to_linear_float(float x); //Assumes 0,...,1 range 

glm::vec4 rgbaAtPos(const int width, int X, int Y, unsigned char* rgb_image, const int bpp);

float displacementAtPos(const int width, int X, int Y, unsigned char* displacement_image);

float computeTriangleAreaUV(const glm::vec2& uv1, const glm::vec2& uv2, const glm::vec2& uv3);

void convert_xyz_to_cube_uv(float x, float y, float z, int* index, float* u, float* v);

void convert_cube_uv_to_xyz(int index, float u, float v, float* x, float* y, float* z);

void computeAndLoadTextureInformation(
    std::map<std::string, std::pair<unsigned char*, int>>& textureTypeMap,
    MaterialGltf& material, const int x, const int y,
    glm::vec4& rgba,
    float& metallicFactor, float& roughnessFactor,
    glm::vec3& interpolatedNormal, glm::vec3& outputNormal, glm::vec4& interpolatedTangent
);
