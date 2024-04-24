#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <tuple>
#include <math.h>
#include <array>
#include <algorithm>
#include <chrono>
#include <cstdlib> // For system()
#include <random>
#include <glm.hpp>
#include <set>
#include <stdint.h>
#define STB_IMAGE_IMPLEMENTATION
#define GLM_ENABLE_EXPERIMENTAL

#include <gtx/hash.hpp>
#include <gtx/quaternion.hpp>
#include <gtx/string_cast.hpp>

#include <unordered_set>
#include <Eigen/Dense>
#include <Eigen/Geometry>
#include "json.h"
#include "stb_image.hpp"
#include "parsers.hpp"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>

#define RASTERIZE 1
#define DEBUG 0
#define _USE_MATH_DEFINES
#define PI atan(1.0)*4;
#define EPSILON 1e-8
#define MAX_TEXTURE_WIDTH 1024
#define SH_COEFF0 0.28209479177387814f
#define DEFAULT_PURPLE glm::vec3(102, 51, 153); //0,...,255

#define OBJ_NAME "ship2"
#define OBJ_FORMAT ".obj"
#define TEXTURE_FORMAT ".jpg"
#define BASE_DATASET_FOLDER "C:\\Users\\sscolari\\Desktop\\dataset\\"  OBJ_NAME  "\\"

using json = nlohmann::json;

// Simple function to parse an OBJ file into meshes


bool file_exists(std::string fn) { return std::experimental::filesystem::exists(fn); }

// Function to check if a point is inside a triangle
bool pointInTriangle(const glm::vec2& pt, const glm::vec2& v1, const glm::vec2& v2, const glm::vec2& v3) {
    auto sign = [](const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3) {
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
        };

    float d1 = sign(pt, v1, v2);
    float d2 = sign(pt, v2, v3);
    float d3 = sign(pt, v3, v1);

    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(hasNeg && hasPos);
}

// https://ceng2.ktu.edu.tr/~cakir/files/grafikler/Texture_Mapping.pdf

// Function to calculate barycentric coordinates
bool computeBarycentricCoords(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, float& u, float& v, float& w) {
    glm::vec2 v0 = b - a, v1 = c - a, v2 = p - a;
    float d00 = glm::dot(v0, v0);
    float d01 = glm::dot(v0, v1);
    float d11 = glm::dot(v1, v1);
    float d20 = glm::dot(v2, v0);
    float d21 = glm::dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;
    if (denom == 0) return false; // Collinear or singular triangle
    float invDenom = 1.0f / denom;
    v = (d11 * d20 - d01 * d21) * invDenom;
    w = (d00 * d21 - d01 * d20) * invDenom;
    u = 1.0f - v - w;
    return true; // Successful computation
}

// Simplified function to run a subprocess and wait for it to finish (replaces Python's subprocess.run)

void runSubprocess(const std::string& command) {
    system(command.c_str());
}



void writeBinaryPLY(const std::string& filename, const std::vector<Gaussian3D>& gaussians) {
    std::ofstream file(filename, std::ios::binary | std::ios::out);

    // Write header in ASCII
    file << "ply\n";
    file << "format binary_little_endian 1.0\n";
    file << "element vertex " << gaussians.size() << "\n";
    
    file << "property float x\n";
    file << "property float y\n";
    file << "property float z\n";
    
    file << "property float nx\n";
    file << "property float ny\n";
    file << "property float nz\n";

    file << "property float f_dc_0\n";
    file << "property float f_dc_1\n";
    file << "property float f_dc_2\n";
    for (int i = 0; i < 45; i++)
    {
        file << "property float f_rest_" << i << "\n";
    }

    file << "property float opacity\n";

    file << "property float scale_0\n";
    file << "property float scale_1\n";
    file << "property float scale_2\n";

    file << "property float rot_0\n";
    file << "property float rot_1\n";
    file << "property float rot_2\n";
    file << "property float rot_3\n";

    file << "end_header\n";

    // Write vertex data in binary
    for (const auto& gaussian : gaussians) {
        //Mean
        file.write(reinterpret_cast<const char*>(&gaussian.position.x), sizeof(gaussian.position.x));
        file.write(reinterpret_cast<const char*>(&gaussian.position.y), sizeof(gaussian.position.y));
        file.write(reinterpret_cast<const char*>(&gaussian.position.z), sizeof(gaussian.position.z));
        //Normal
        file.write(reinterpret_cast<const char*>(&gaussian.normal.x), sizeof(gaussian.normal.x));
        file.write(reinterpret_cast<const char*>(&gaussian.normal.y), sizeof(gaussian.normal.y));
        file.write(reinterpret_cast<const char*>(&gaussian.normal.z), sizeof(gaussian.normal.z));
        //RGB
        file.write(reinterpret_cast<const char*>(&gaussian.sh0.x), sizeof(gaussian.sh0.x));
        file.write(reinterpret_cast<const char*>(&gaussian.sh0.y), sizeof(gaussian.sh0.y));
        file.write(reinterpret_cast<const char*>(&gaussian.sh0.z), sizeof(gaussian.sh0.z));

        // TODO: this takes up basically 65% of the space and I do not even need to use it
        float zeroValue = 0.0f; 
        for (int i = 0; i < 45; i++) {
            file.write(reinterpret_cast<const char*>(&zeroValue), sizeof(zeroValue));
        }

        //Opacity
        file.write(reinterpret_cast<const char*>(&gaussian.opacity), sizeof(gaussian.opacity));

        file.write(reinterpret_cast<const char*>(&gaussian.scale.x), sizeof(gaussian.scale.x));
        file.write(reinterpret_cast<const char*>(&gaussian.scale.y), sizeof(gaussian.scale.y));
        file.write(reinterpret_cast<const char*>(&gaussian.scale.z), sizeof(gaussian.scale.z));
        //Rotation
        file.write(reinterpret_cast<const char*>(&gaussian.rotation.x), sizeof(gaussian.rotation.x));
        file.write(reinterpret_cast<const char*>(&gaussian.rotation.y), sizeof(gaussian.rotation.y));
        file.write(reinterpret_cast<const char*>(&gaussian.rotation.z), sizeof(gaussian.rotation.z));
        file.write(reinterpret_cast<const char*>(&gaussian.rotation.w), sizeof(gaussian.rotation.w));
    }

    file.close();
}

//vertexIndices, uvIndices, normalIndices
// Function to find a 3D position from UV coordinates, this is probably the main bottleneck, as complexity grows with more complex 3D models.
//Need to think of possible smarter ways to do this
/*
std::tuple<glm::vec3, std::string, glm::vec3, std::array<glm::vec3, 3>, std::array<glm::vec2, 3>> find3DPositionFromUV(const std::vector<Mesh>& meshes, const glm::vec2& targetUv, const std::vector<glm::vec3>& globalVertices, const std::vector<glm::vec2>& globalUvs) {
    for (const auto& mesh : meshes) {
        for (const auto& face : mesh.faces) {
            std::vector<int> vertexIndices = std::get<0>(face);
            std::vector<int> uvIndices = std::get<1>(face);
            std::vector<int> normalIndices = std::get<2>(face);

            if (face.first.size() != 3 || face.second.size() != 3) {
                continue; // Skip non-triangular faces
            }

            std::array<glm::vec2, 3> triangleUvs = { globalUvs[face.second[0]], globalUvs[face.second[1]], globalUvs[face.second[2]] };
            std::array<glm::vec3, 3> triangleVertices = { globalVertices[face.first[0]], globalVertices[face.first[1]], globalVertices[face.first[2]] };

            //std::cout << "rad angle: " << rad_angle << std::endl;
            if (pointInTriangle(targetUv, triangleUvs[0], triangleUvs[1], triangleUvs[2])) {
                float u;
                float v;
                float w;
                computeBarycentricCoords(targetUv, triangleUvs[0], triangleUvs[1], triangleUvs[2], u, v, w);
                
                glm::vec3 interpolatedPos =
                    globalVertices[face.first[0]] * u +
                    globalVertices[face.first[1]] * v +
                    globalVertices[face.first[2]] * w;

                glm::vec3 normal = glm::cross(globalVertices[face.first[1]] - globalVertices[face.first[0]], globalVertices[face.first[2]] - globalVertices[face.first[0]]);

                return { interpolatedPos, mesh.name,  glm::normalize(normal), triangleVertices, triangleUvs };
            }
        }
    }
    //std::cout << "3D point not found for given UV" << std::endl;
    return { glm::vec3(0,0,0), "NotFound", glm::vec3(0,0,0), {}, {} }; // Return an empty result if no matching face is found
}
*/

//I think this function can be optimized a lot, I am passing and rearranging vector info too many times, need to make it more concise
static Eigen::Matrix<double, 3, 2>  computeUv3DJacobian(const std::array<glm::vec3, 3> verticesTriangle3D, const std::array<glm::vec2, 3> verticesTriangleUV) {
    //3Ds
    const glm::vec3& pos0 = verticesTriangle3D[0];
    const glm::vec3& pos1 = verticesTriangle3D[1];
    const glm::vec3& pos2 = verticesTriangle3D[2];
    //UVs
    const glm::vec2& uv0 = verticesTriangleUV[0];
    const glm::vec2& uv1 = verticesTriangleUV[1];
    const glm::vec2& uv2 = verticesTriangleUV[2];

    Eigen::Vector3d P0(pos0.x, pos0.y, pos0.z), P1(pos1.x, pos1.y, pos1.z), P2(pos2.x, pos2.y, pos2.z);     // 3D positions of the triangle's vertices
    Eigen::Vector2d UV0(uv0.x, uv0.y), UV1(uv1.x, uv1.y), UV2(uv2.x, uv2.y);                                // UV coordinates of the triangle's vertices

    // Compute edge vectors in UV space
    Eigen::Vector2d edgeUV1 = UV1 - UV0;
    Eigen::Vector2d edgeUV2 = UV2 - UV0;

    // Compute edge vectors in 3D space
    Eigen::Vector3d edge3D1 = P1 - P0;
    Eigen::Vector3d edge3D2 = P2 - P0;

    Eigen::Matrix2d UVMatrix;
    UVMatrix << 
        edgeUV1[0], edgeUV2[0],
        edgeUV1[1], edgeUV2[1];

    Eigen::Matrix<double, 3, 2> VMatrix;
    VMatrix << 
        edge3D1[0], edge3D2[0],
        edge3D1[1], edge3D2[1],
        edge3D1[2], edge3D2[2];
    
    return VMatrix * UVMatrix.inverse(); //This is the Jacobian matrix
}

glm::vec3 getColor(glm::vec3 color)
{
    glm::vec3 col = color - glm::vec3(0.5f) / SH_COEFF0;
    return glm::vec3(col.x, col.y, col.z);
}

glm::vec3 floatToVec3(const float val)
{
    return glm::vec3(val, val, val);
}

int inline sign(float x) {
    return (x > 0) ? 1 : ((x < 0) ? -1 : 0);
}

std::vector<std::pair<glm::vec3, float>> getSortedEigenvectorEigenvalues(Eigen::Matrix3d covMat3d)
{
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eigensolver(covMat3d);

    Eigen::Vector3d eigenvalues = eigensolver.eigenvalues();
    Eigen::Matrix3d eigenvectors = eigensolver.eigenvectors();

    std::vector<std::pair<glm::vec3, float>> eigenPairs;

    for (int i = 0; i < eigenvalues.size(); i++) {
        float eigenvalue = eigenvalues[i];
        glm::vec3 eigenvector(eigenvectors(0, i), eigenvectors(1, i), eigenvectors(2, i));
        eigenPairs.push_back(std::make_pair(eigenvector, eigenvalue));
    }

    //sorted in ascending order, largest eigenvector is at the end with its eigenvalue
    std::sort(eigenPairs.begin(), eigenPairs.end(),
        [](const std::pair<glm::vec3, float>& a, const std::pair<glm::vec3, float>& b) {
            return a.second < b.second;
        }
    );

    return eigenPairs;
}

std::vector<Gaussian3D> drawLine(glm::vec3 initialPos, glm::vec3 finalPos, glm::vec3 color, float isotropicScale, float opacity = 1.0f)
{
    std::vector<glm::vec3> interpolatedPositions;
    for (int i = 0; i < 30; i++)
    {
        float alpha = ((float)i) / 100.0f;
        interpolatedPositions.push_back(floatToVec3(1 - alpha) * initialPos + floatToVec3(alpha) * finalPos);
    }

    std::vector<Gaussian3D> normalGaussians;

    for (glm::vec3& position : interpolatedPositions)
    {
        normalGaussians.push_back(
            Gaussian3D(
                position,
                glm::vec3(0.0f, 0.0f, 1.0f), //Not used anyway
                log(glm::vec3(isotropicScale, isotropicScale, isotropicScale)),
                glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), //Gaussian is isotropic so not used
                getColor(color),
                1.0f,
                Material()
            )
        );
    }
    return normalGaussians;
}

std::vector<Gaussian3D> createEncompassingTriangle(std::array<glm::vec3, 3> positions, glm::vec3 color, float isotropicScale, float opacity=1.0f)
{
    std::vector<Gaussian3D> triangleGaussians;

    for (glm::vec3& position : positions)
    {
        triangleGaussians.push_back(
            Gaussian3D(
                position,
                glm::vec3(0.0f, 0.0f, 1.0f), //Not used anyway
                log(glm::vec3(isotropicScale, isotropicScale, isotropicScale)),
                glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), //Gaussian is isotropic so not used
                color,
                1.0f,
                Material()
            )
        );
    }
    return triangleGaussians;
}



//Expects to receive 24 positions, 4 per each of the 6 faces
std::vector<Gaussian3D> drawCube(std::vector<glm::vec3> positions, glm::vec3 color, float isotropicScale, float opacity = 1.0f)
{
    std::vector<std::vector<glm::vec3>> faces;
    for (int i = 0; i < 6; i++)
    {
        faces.push_back(std::vector<glm::vec3>(positions.begin() + i * 4, positions.end() + (i*4) + 4));
    }
    
    std::vector<Gaussian3D> cubeEdges;

    for (std::vector<glm::vec3> face : faces)
    {
        for (int i = 0; i < 4; i++)
        {
            auto edge = drawLine(face[i], face[(i+1)%4], color, isotropicScale, opacity);
            cubeEdges.insert(cubeEdges.end(), edge.begin(), edge.end());
        }
        
    }

    return cubeEdges;
}

glm::vec2 pixelToUV(const glm::ivec2& pixel, int textureWidth, int textureHeight) {
    // Convert pixel coordinates to normalized UV coordinates by dividing
    // by the texture dimensions. Cast to float to ensure floating-point division.
    float u = (static_cast<float>(pixel.x) / static_cast<float>(textureWidth));
    float v = (static_cast<float>(pixel.y) / static_cast<float>(textureHeight));
    
    return glm::vec2(u, v);
}

glm::ivec2 uvToPixel(const glm::vec2& uv, int textureWidth, int textureHeight) {
    // Convert back to pixel coordinates
    int x = static_cast<int>(uv.x * textureWidth);
    int y = static_cast<int>(uv.y * textureHeight);

    // Clamp the coordinates to ensure they're within the texture bounds
    // This accounts for potential rounding errors that might place the pixel outside the texture
    x = std::max(0, std::min(x, textureWidth));
    y = std::max(0, std::min(y, textureHeight));

    return glm::ivec2(x, y);
}


std::pair<glm::vec2, glm::vec2> computeUVBoundingBox(const std::vector<glm::vec2>& triangleUVs) {
    if (triangleUVs.empty()) {
        return { glm::vec2(0, 0), glm::vec2(0, 0) };
    }

    float minU = triangleUVs[0].x, maxU = triangleUVs[0].x;
    float minV = triangleUVs[0].y, maxV = triangleUVs[0].y;

    for (const auto& uv : triangleUVs) {
        minU = std::min(minU, uv.x);
        maxU = std::max(maxU, uv.x);

        minV = std::min(minV, uv.y);
        maxV = std::max(maxV, uv.y);
    }

    // Clamp values to [0, 1] 
    minU = std::max(minU, 0.0f);
    maxU = std::min(maxU, 1.0f);
    minV = std::max(minV, 0.0f);
    maxV = std::min(maxV, 1.0f);

    return { glm::vec2(minU, minV), glm::vec2(maxU, maxV) };
}

//https://www.nayuki.io/res/srgb-transform-library/srgb-transform.c
//Assumes 0,...,1 range 
float linear_to_srgb_float(float x) {
    if (x <= 0.0f)
        return 0.0f;
    else if (x >= 1.0f)
        return 1.0f;
    else if (x < 0.0031308f)
        return x * 12.92f;
    else
        return powf(x, 1.0f / 2.4f) * 1.055f - 0.055f;
}


//https://www.nayuki.io/res/srgb-transform-library/srgb-transform.c
//Assumes 0,...,1 range 
float srgb_to_linear_float(float x) {
    if (x <= 0.0f)
        return 0.0f;
    else if (x >= 1.0f)
        return 1.0f;
    else if (x < 0.04045f)
        return x / 12.92f;
    else
        return powf((x + 0.055f) / 1.055f, 2.4f);
}

//https://stackoverflow.com/a/48235560
glm::vec4 rgbaAtPos(const int width, int X, int Y, unsigned char* rgb_image)
{
    unsigned bytePerPixel = 3;
    unsigned char* pixelOffset = rgb_image + (X + width * Y) * bytePerPixel;
    
    float r = ((static_cast<float>(pixelOffset[0]) / 255.0f));
    float g = ((static_cast<float>(pixelOffset[1]) / 255.0f));
    float b = ((static_cast<float>(pixelOffset[2]) / 255.0f));
    //float a = bytePerPixel >= 4 ? (float)pixelOffset[3] : 255.0f;
    float a = 1.0f;

    return { r, g, b, a};
}

float displacementAtPos(const int width, int X, int Y, unsigned char* displacement_image)
{
    unsigned bytePerPixel = 1;
    unsigned char* pixelOffset = displacement_image + (X + width * Y) * bytePerPixel;
    //from my understanding a negative value is 0.0, if at 148 it means no displacement and at 255, full displacement
    return ((static_cast<float>(pixelOffset[0] - 128) / 255.0f));
}

float computeTriangleAreaUV(const glm::vec2& uv1, const glm::vec2& uv2, const glm::vec2& uv3) {
    float area = 0.5f * std::abs(uv1.x * (uv2.y - uv3.y) +
        uv2.x * (uv3.y - uv1.y) +
        uv3.x * (uv1.y - uv2.y));
    return area;
}

std::pair<glm::vec4, glm::vec3> getScaleRotationGaussian(const float sigma2d, std::vector<glm::vec3> verticesTriangle3D, std::vector<glm::vec2> verticesTriangleUVs)
{
    //Building CovMat2D
    //float rho = 0.0f; //Pearson Corr. Coeff. (PCC)
    //float covariance = rho * abs(sigma2d) * abs(sigma2d);
    float covariance = 0.0f;
    Eigen::Matrix2d covMat2d;
    float sigma2 = sigma2d * sigma2d;
    covMat2d <<
        sigma2,         covariance,                     //Row 1
        covariance,     sigma2;              //Row 2

    //---------------Computing 3d Cov Matrix--------------------

    std::array<glm::vec3, 3> verticesTriangle3DArray = {
        verticesTriangle3D[0],
        verticesTriangle3D[1],
        verticesTriangle3D[2]
    };

    std::array<glm::vec2, 3> verticesTriangleUVsArray = {
        verticesTriangleUVs[0],
        verticesTriangleUVs[1],
        verticesTriangleUVs[2]
    };

    Eigen::Matrix<double, 3, 2> J = computeUv3DJacobian(verticesTriangle3DArray, verticesTriangleUVsArray);

    Eigen::Matrix3d covMat3d = J * covMat2d * J.transpose();

    std::vector<std::pair<glm::vec3, float>> eigenPairs = getSortedEigenvectorEigenvalues(covMat3d);

    //Computing and setting scale values from the eigenvalues
    float SD_x = sqrt(eigenPairs[2].second); //sqrt of eigenvalues..., jacobian by normal dot should yield 0 (to check)
    float SD_y = sqrt(eigenPairs[1].second);
    float SD_z = (std::min(SD_x, SD_y) / 2.0f);

    glm::vec3 x = glm::normalize(eigenPairs[2].first);
    glm::vec3 y = glm::normalize(eigenPairs[1].first);
    glm::vec3 z = glm::normalize(glm::cross(x, y));
    
    //TODO: for now I do this. I know it would make sense to directly use the computed normal. 
    // but I dont know if they are exactly identical and do not want to risk having a non-orthonormal basis.

    glm::mat3 rotMat = { x, y, z };

    glm::quat q = glm::quat_cast(rotMat);

    return std::make_pair(glm::vec4(q.w, q.x, q.y, q.z), glm::vec3(log(SD_x), log(SD_y), log(SD_z)));
}

void drawNormal(std::vector<glm::vec3> triangleVertices, glm::vec3 normal, std::vector<Gaussian3D>& gaussians_3D_list)
{
    glm::vec3 center = (1.0f / 3.0f) * (triangleVertices[0] + triangleVertices[1] + triangleVertices[2]);
    std::vector<Gaussian3D> normalLine = drawLine(center, center+(normal * 5.0f), glm::vec3(0.294f, 0.706f, 0.89f), 0.01f);
    gaussians_3D_list.insert(gaussians_3D_list.end(), normalLine.begin(), normalLine.end());
}

unsigned char* loadImage(std::string texturePath, int& textureWidth, int& textureHeight)
{
    int bpp;
    int bytes_per_pixel = 3;
    unsigned char* image = stbi_load(texturePath.c_str(), &textureWidth, &textureHeight, &bpp, bytes_per_pixel);

    std::cout << "Image: " << texturePath << "  width: " << textureWidth << "  height: " << textureHeight << " " << bpp << std::endl;

    std::string resized_texture_name_location = BASE_DATASET_FOLDER + std::string("resized_texture") + TEXTURE_FORMAT;
    float aspect_ratio = (float)textureHeight / (float)textureWidth;

    if (textureWidth > MAX_TEXTURE_WIDTH)
    {
        // Specify new width and height

        int new_width = MAX_TEXTURE_WIDTH;
        int new_height = static_cast<int>(new_width * aspect_ratio);

        // Allocate memory for the resized image
        unsigned char* resized_data = (unsigned char*)malloc(new_width * new_height * bytes_per_pixel);

        // Resize the image
        stbir_resize_uint8(image, textureWidth, textureHeight, 0, resized_data, new_width, new_height, 0, bytes_per_pixel);

        // Save the resized image
        stbi_write_png(resized_texture_name_location.c_str(), new_width, new_height, bytes_per_pixel, resized_data, new_width * bytes_per_pixel);
        image = stbi_load(resized_texture_name_location.c_str(), &textureWidth, &textureHeight, &bpp, bytes_per_pixel);

        
        std::cout << "Image: " << resized_texture_name_location << "  width: " << textureWidth << "  height: " << textureHeight << " " << bpp << std::endl;

    }

    return image;
}

void printMaterials(const std::map<std::string, Material>& materials) {
    for (const auto& pair : materials) {
        const std::string& name = pair.first;
        const Material& mat = pair.second;
        std::cout << "Material Name: " << name << std::endl
            << "  Ambient: (" << mat.ambient.x << ", " << mat.ambient.y << ", " << mat.ambient.z << ")" << std::endl
            << "  Diffuse: (" << mat.diffuse.x << ", " << mat.diffuse.y << ", " << mat.diffuse.z << ")" << std::endl
            << "  Specular: (" << mat.specular.x << ", " << mat.specular.y << ", " << mat.specular.z << ")" << std::endl
            << "  Specular Exponent: " << mat.specularExponent << std::endl
            << "  Transparency: " << mat.transparency << std::endl
            << "  Optical Density: " << mat.opticalDensity << std::endl
            << "  Diffuse Map: " << mat.diffuseMap << std::endl << std::endl;
    }
}

int main() {
    // Example call to subprocess (simplified)
    //folder, obj and texture should have same name (easier)

    //Needs to be already triangulated
    //std::string runProcessCommand = "TriangulateObj.exe " + baseDatasetFolder + objName + extension;
    //runSubprocess(runProcessCommand);
    //std::string output_filename = baseDatasetFolder + objName + ".triangulated" + extension;
    std::string output_filename = BASE_DATASET_FOLDER + std::string(OBJ_NAME) + OBJ_FORMAT;
    
    auto result = parseObjFileToMeshes(output_filename);
    std::vector<Mesh> meshes = std::get<0>(result);
    std::vector<glm::vec3> globalVertices = std::get<1>(result);
    std::vector<glm::vec2> globalUvs = std::get<2>(result);
    std::vector<glm::vec3> globalNormals = std::get<3>(result);

    std::string base_color_texturePath = BASE_DATASET_FOLDER + std::string(OBJ_NAME) + TEXTURE_FORMAT;
    //std::string normal_texturePath = BASE_DATASET_FOLDER + std::string("normal") + TEXTURE_FORMAT;
    std::string displacement_texturePath = BASE_DATASET_FOLDER + std::string("displacement") + TEXTURE_FORMAT;
    std::string material_path = BASE_DATASET_FOLDER + std::string(OBJ_NAME) + ".mtl";

    std::map<std::string, Material> materials;
    if (file_exists(material_path)) {
        materials = parseMtlFile(material_path);
    }
    else if(!file_exists(material_path) || materials.size() == 0) {
        materials = { { DEFAULT_MATERIAL_NAME , Material() } };
    }

    std::cout << meshes.size() << " " << globalVertices.size() << " " << globalUvs.size() << std::endl;

    int rgbTextureWidth, rgbTextureHeight;
    int normalTextureWidth, normalTextureHeight;
    int displacementTextureWidth, displacementTextureHeight;
    unsigned char* rgb_image = nullptr;
    unsigned char* displacement_map;
    unsigned char* normal_map;

    bool rgb_exists = false;
    //TODO: handle this fucking better, it is a disgrace how bad cases are handled tbh
    if (file_exists(base_color_texturePath))
    {
        rgb_image = loadImage(base_color_texturePath, rgbTextureWidth, rgbTextureHeight);
        rgb_exists = true;
    } 
    else
    {
        rgbTextureWidth = MAX_TEXTURE_WIDTH;
        rgbTextureHeight = MAX_TEXTURE_WIDTH;
        if (!file_exists(material_path))
        {
            throw std::exception();
        }
    }

    /*
    if (file_exists(displacement_texturePath))
    {
        displacement_map = loadImage(displacement_texturePath, displacementTextureWidth, displacementTextureHeight);
    }
    else
    {
        //using rgbTextureWidth and rgbTextureWidth as if we are here it means no exception was thrown and these two vars have a value
        displacementTextureWidth = rgbTextureWidth;
        displacementTextureHeight = rgbTextureHeight;

        displacement_map = new unsigned char[rgbTextureWidth * rgbTextureHeight]; // 1 byte per pixel
        memset(displacement_map, 0, rgbTextureWidth * rgbTextureHeight); // Set all pixels to 0
    }
    */

    //unsigned char* normal_map = loadImage(normal_texturePath, normalTextureWidth, normalTextureHeight);

    std::vector<Gaussian3D> gaussians_3D_list;
    float image_area = (rgbTextureWidth * rgbTextureHeight);
        
    glm::vec3 lightDir = glm::normalize(glm::vec3(0, -1, 0));

    for (const auto& mesh : meshes) {
        std::cout << mesh.faces.size() << " num triangles" << std::endl;
        for (const auto& triangleFace : mesh.faces) {
            
            std::vector<glm::vec3> triangleVertices;
            std::vector<glm::vec2> triangleUVs;
            std::vector<glm::vec3> triangleNormals;
            Material material = materials[triangleFace.materialName]; //even if none it defaults to default material
            //All the following is texture related computation
            std::vector<int> vertexIndices = triangleFace.vertexIndices;
            std::vector<int> uvIndices = triangleFace.uvIndices;
            std::vector<int> normalIndices = triangleFace.normalIndices;

            for (int i = 0; i < 3; ++i) {
                triangleVertices.push_back(globalVertices[vertexIndices[i]]);
                triangleUVs.push_back(globalUvs[uvIndices[i]]);
                triangleNormals.push_back(globalNormals[normalIndices[i]]);
            }
#if RASTERIZE
            // Compute the bounding box in UV space
            std::pair<glm::vec2, glm::vec2> minMaxUV = computeUVBoundingBox(triangleUVs);

            glm::vec2 minUV = std::get<0>(minMaxUV);
            glm::vec2 maxUV = std::get<1>(minMaxUV);

            // Convert bounding box to pixel coordinates
            glm::ivec2 minPixel = uvToPixel(minUV, rgbTextureWidth - 1, rgbTextureHeight - 1);
            glm::ivec2 maxPixel = uvToPixel(maxUV, rgbTextureWidth - 1, rgbTextureHeight - 1);

            std::vector<std::tuple<glm::vec3, glm::vec3, glm::vec4, Material>> positionsOnTriangleSurfaceAndRGBs;

            // Rasterize the triangle within the bounding box
            //TODO: need to write a totally different logic for objects that use materials
            for (int y = minPixel.y; y <= maxPixel.y; ++y) {
                for (int x = minPixel.x; x <= maxPixel.x; ++x) {
                      
                    glm::vec2 pixelUV = pixelToUV(glm::ivec2(x, y), rgbTextureWidth - 1, rgbTextureHeight - 1);
                                           
                    if (pointInTriangle(pixelUV, triangleUVs[0], triangleUVs[1], triangleUVs[2])) {
                        float u, v, w;
                        computeBarycentricCoords(pixelUV, triangleUVs[0], triangleUVs[1], triangleUVs[2], u, v, w);

                        glm::vec3 interpolatedPos =
                            triangleVertices[0] * u +
                            triangleVertices[1] * v +
                            triangleVertices[2] * w;

                        glm::vec3 interpolatedNormal = glm::normalize(
                            triangleNormals[0] * u +
                            triangleNormals[1] * v +
                            triangleNormals[2] * w);

                        glm::vec4 normalColor((interpolatedNormal + glm::vec3(1.0f, 1.0f, 1.0f)) / 2.0f, 1.0f);

                        if (rgb_exists)
                        {
                            //glm::vec4 rgba = normalColor;
                            glm::vec4 rgba = rgbaAtPos(rgbTextureWidth, x, (rgbTextureHeight - 1) - y, rgb_image);
                            //TODO: Insert displacement offset retrieval and normal mapping here
                            //float displacementValue = displacementAtPos(displacementTextureWidth, x, (displacementTextureHeight - 1) - y, displacement_map) / 5.0f;

                            positionsOnTriangleSurfaceAndRGBs.push_back(std::make_tuple(interpolatedNormal, interpolatedPos, rgba, Material()));
                        }
                        else {
                            //glm::vec4 rgba = normalColor;
                            glm::vec4 rgba = glm::vec4(glm::vec3(material.diffuse.r, material.diffuse.g, material.diffuse.b), 1.0f);
                            positionsOnTriangleSurfaceAndRGBs.push_back(std::make_tuple(interpolatedNormal, interpolatedPos, rgba, material));

                        }
                        

                    }

                }
            }

            // Calculate σ based on the density and desired overlap, I derive this simple formula from 
            //TODO: Should find a better way to compute sigma and do it based on the size of the current triangle to tesselate
            float scale_factor_multiplier = .5f;
            float sigma = scale_factor_multiplier * sqrt(2.0f / image_area);

            glm::vec3 normal =
                glm::normalize(glm::cross(triangleVertices[1] - triangleVertices[0], triangleVertices[2] - triangleVertices[0]));

            std::pair<glm::vec4, glm::vec3> rotAndScale = getScaleRotationGaussian(sigma, triangleVertices, triangleUVs);
            glm::vec4 rotation = std::get<0>(rotAndScale);
            glm::vec3 scale = std::get<1>(rotAndScale);

#else
            //https://arxiv.org/pdf/2402.01459.pdf
            glm::vec3 r1 = glm::normalize(glm::cross((triangleVertices[1] - triangleVertices[0]), (triangleVertices[2] - triangleVertices[0])));
            glm::vec3 m = (triangleVertices[0] + triangleVertices[1] + triangleVertices[2]) / 3.0f;
            glm::vec3 r2 = glm::normalize(triangleVertices[0] - m);
            //To obtain r3 we first get it as triangleVertices[1] - m and then orthonormalize it with respect to r1 and r2
            glm::vec3 initial_r3 = triangleVertices[1] - m; //DO NOT NORMALIZE THIS
            // Gram-Schmidt Orthonormalization to find r3
            // Project initial_r3 onto r1
            glm::vec3 proj_r1 = (glm::dot(initial_r3, r1) / glm::length2(r1)) * r1;

            // Project r3_minus_r1 onto r2
            glm::vec3 proj_r2 = (glm::dot(initial_r3, r2) / glm::length2(r2)) * r2;
            //TODO: try out some other combinations and reason why it is not working properly
            // Subtract the projection from r3_minus_r1 to get the orthogonal component
            glm::vec3 r3 = initial_r3 - proj_r1 - proj_r2;

            glm::mat3 rotMat = { r1, r2, glm::normalize(r3) };

            glm::quat q = glm::quat_cast(rotMat);
            glm::vec4 rotation(q.w, q.x, q.y, q.z);

            float s2 = glm::length(m - triangleVertices[0]);
            float s3 = glm::dot(triangleVertices[1], r3);
            float s1 = std::min(s2, s3) / 10.0f;
            glm::vec3 scale(log(s1), log(s2), log(s3));

            std::vector<std::tuple<glm::vec3, glm::vec3, glm::vec4, Material>> positionsOnTriangleSurfaceAndRGBs;
            if (rgb_exists)
            {
                glm::vec2 mUv = (triangleUVs[0] + triangleUVs[1] + triangleUVs[2]) / 3.0f;
                glm::ivec2 uvPos = uvToPixel(mUv, rgbTextureWidth - 1, rgbTextureHeight - 1);
                glm::vec4 rgba = rgbaAtPos(rgbTextureWidth, uvPos.x, (rgbTextureHeight - 1) - uvPos.y, rgb_image);
                positionsOnTriangleSurfaceAndRGBs.push_back(std::make_tuple(r1, m, rgba, material));
                
            } else {
                glm::vec4 rgba = glm::vec4(material.diffuse, 1.0f);
                positionsOnTriangleSurfaceAndRGBs.push_back(std::make_tuple(r1, m, rgba, material));
            }

#endif

            
            //float scaleRatio = scale.x / scale.y;

            //DEBUG NORMAL DRAWING
            //drawNormal(triangleVertices, normal, gaussians_3D_list);
            //---------------
            //float lambert = glm::dot(gaussian_3d.normal, lightDir);
            //float ambientTerm = 1.0f;
            //glm::vec4 shadedColor = rgba * (lambert + ambientTerm);

            for (auto& normalPosRgbDispl : positionsOnTriangleSurfaceAndRGBs)
            {
                glm::vec3 interpolatedNormal = std::get<0>(normalPosRgbDispl);
                glm::vec4 rgba = std::get<2>(normalPosRgbDispl);
                Material material = std::get<3>(normalPosRgbDispl);
                
                Gaussian3D gaussian_3d;
                gaussian_3d.normal = interpolatedNormal; 
                gaussian_3d.rotation = rotation;
                gaussian_3d.scale = scale;
                gaussian_3d.sh0 = getColor(glm::vec3(linear_to_srgb_float(rgba.r), linear_to_srgb_float(rgba.g), linear_to_srgb_float(rgba.b)));
                gaussian_3d.opacity = rgba.a;
                gaussian_3d.position = std::get<1>(normalPosRgbDispl); //+(gaussian_3d.normal * z_displacement);
                gaussian_3d.material = material;

                gaussians_3D_list.push_back(gaussian_3d); 
            }
            triangleUVs.clear();
            triangleVertices.clear();
    
        }
                
    }
    
    std::cout << "Started writing to file" << std::endl;

    /*
    gaussians_3D_list.push_back(Gaussian3D(
        glm::vec3(0.0f, -10.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(2.0f, 2.0f, 2.0f),
        gaussians_3D_list[gaussians_3D_list.size() - 1].rotation,
        glm::vec3(1.0f, 0.0f, 0.0f),
        1.0f, 
        Material()
    ));
    */
    writeBinaryPLY("C:\\Users\\sscolari\\Desktop\\halcyon\\Content\\GaussianSplatting\\Mesh2SplatOut.ply", gaussians_3D_list);
    
    //And save it in output folder to have them all to go back later
    std::string destOutput = "C:\\Users\\sscolari\\Desktop\\outputGaussians\\" + std::string(OBJ_NAME) + ".ply";
    writeBinaryPLY(destOutput, gaussians_3D_list);

    std::cout << "Finished writing to file" << std::endl;
    return 0;
}