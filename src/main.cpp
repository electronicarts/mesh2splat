#include <string>
#include <vector>
#include <tuple>
#include <math.h>
#include <glm.hpp>
#include <set>
#include <stdint.h>

#include "parsers.hpp"
#include "utils/gaussianShapesUtilities.hpp"
#include "gaussianComputations.hpp"


int main() {
    //I am investigating a memory leak
    //_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    //_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
    //_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    //_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDOUT);
    //_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    //_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDOUT);
    
    /*
    if (_CrtDumpMemoryLeaks()) {
        std::cout << "Memory leaks!\n";
    }
    else {
        std::cout << "No leaks\n";
    }
    */

    printf("Parsing input mesh\n");
    auto parsedGltfModel = parseGltfFileToMeshAndGlobalData(OUTPUT_FILENAME);
    
    std::vector<Mesh>       meshes          = std::get<0>(parsedGltfModel);
    std::vector<glm::vec3>  globalVertices  = std::get<1>(parsedGltfModel);
    std::vector<glm::vec2>  globalUvs       = std::get<2>(parsedGltfModel);
    std::vector<glm::vec3>  globalNormals   = std::get<3>(parsedGltfModel);

    printf("Parsed input mesh\n");

    printf(
        "[Total number of meshes: %zu]\n[Total number of vertices : %zu]\n[Total number of UVs : %zu]\n[Total number of normlas : %zu]\n", 
        meshes.size(), globalVertices.size(), globalUvs.size(), globalNormals.size()
    );

    int rgbTextureWidth = 0, rgbTextureHeight = 0;
    int normalTextureWidth = 0,normalTextureHeight = 0;
    int displacementTextureWidth = 0, displacementTextureHeight = 0;

    unsigned char 
        *rgb_image = nullptr, 
        *displacement_map = nullptr, 
        *normal_map = nullptr;

    bool rgb_exists = false;

    //TODO: handle this fucking better
    if (file_exists(ALBEDO_TEXTURE_MAP_FILENAME))
    {
        rgb_image   = loadImage(ALBEDO_TEXTURE_MAP_FILENAME, rgbTextureWidth, rgbTextureHeight);
        rgb_exists  = true;
    } 
    else
    {
        rgbTextureWidth     = MAX_TEXTURE_WIDTH;
        rgbTextureHeight    = MAX_TEXTURE_WIDTH;
    }

    std::vector<Gaussian3D> gaussians_3D_list;
    float image_area = (rgbTextureWidth * rgbTextureHeight);
    int j = 0;

    for (const auto& mesh : meshes) {
        j++;
        printf("%zu triangle faces for mesh number %d / %zu\n", mesh.faces.size(), j, meshes.size());
        for (const auto& triangleFace : mesh.faces) {

            std::vector<glm::vec3> triangleVertices;
            std::vector<glm::vec2> triangleUVs;
            std::vector<glm::vec3> triangleNormals;
            MaterialGltf materialGltf = triangleFace.material; //even if none it defaults to default material

            //All the following is texture related computation
            std::vector<int> vertexIndices  = triangleFace.vertexIndices;
            std::vector<int> uvIndices      = triangleFace.uvIndices;
            std::vector<int> normalIndices  = triangleFace.normalIndices;

            for (int i = 0; i < 3; ++i) {
                triangleVertices.push_back(globalVertices[vertexIndices[i]]);
                triangleUVs.push_back(globalUvs[uvIndices[i]]);
                triangleNormals.push_back(globalNormals[normalIndices[i]]);
            }
#if SHOULD_RASTERIZE
            // Compute the bounding box in UV space
            std::pair<glm::vec2, glm::vec2> minMaxUV = computeUVBoundingBox(triangleUVs);

            glm::vec2 minUV = std::get<0>(minMaxUV);
            glm::vec2 maxUV = std::get<1>(minMaxUV);

            // Convert bounding box to pixel coordinates
            glm::ivec2 minPixel = uvToPixel(minUV, rgbTextureWidth - 1, rgbTextureHeight - 1);
            glm::ivec2 maxPixel = uvToPixel(maxUV, rgbTextureWidth - 1, rgbTextureHeight - 1);

            std::vector<std::tuple<glm::vec3, glm::vec3, glm::vec4, MaterialGltf>> positionsOnTriangleSurfaceAndRGBs;

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
                            //TODO: unify so that just pass material even when texture, the rgba is the albedo, no need for rgba slot
                            positionsOnTriangleSurfaceAndRGBs.push_back(std::make_tuple(interpolatedNormal, interpolatedPos, rgba, MaterialGltf("mat", rgba)));
                        }
                        else {
                            glm::vec4 rgba = materialGltf.baseColorFactor;
                            positionsOnTriangleSurfaceAndRGBs.push_back(std::make_tuple(interpolatedNormal, interpolatedPos, rgba, materialGltf));

                        }
                        

                    }

                }
            }

            // Calculate σ based on the density and desired overlap, I derive this simple formula from 
            //TODO: Should find a better way to compute sigma and do it based on the size of the current triangle to tesselate
            float scale_factor_multiplier = .75f;
            float sigma = scale_factor_multiplier * sqrt(2.0f / image_area);

            std::pair<glm::vec4, glm::vec3> rotAndScale = getScaleRotationGaussian(sigma, triangleVertices, triangleUVs);

            glm::vec4 rotation = std::get<0>(rotAndScale);
            glm::vec3 scale = std::get<1>(rotAndScale);

#else
            //TODO: STILL FIXING THIS AND DOES NOT WORK WELL FOR NOW
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

            for (auto& normalPosRgbDispl : positionsOnTriangleSurfaceAndRGBs)
            {
                glm::vec3 interpolatedNormal = std::get<0>(normalPosRgbDispl);
                glm::vec4 rgba = std::get<2>(normalPosRgbDispl);
                MaterialGltf material = std::get<3>(normalPosRgbDispl);
                
                Gaussian3D gaussian_3d;
                gaussian_3d.normal      = interpolatedNormal; 
                gaussian_3d.rotation    = rotation;
                gaussian_3d.scale       = scale;
                gaussian_3d.sh0         = getColor(glm::vec3(linear_to_srgb_float(rgba.r), linear_to_srgb_float(rgba.g), linear_to_srgb_float(rgba.b)));
                gaussian_3d.opacity     = rgba.a;
                gaussian_3d.position    = std::get<1>(normalPosRgbDispl);
                gaussian_3d.material    = material;

                gaussians_3D_list.push_back(gaussian_3d); 
            }

        }
                
    }
    
    printf("Started writing to file\n");

    writeBinaryPLY(GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_1, gaussians_3D_list);
    writeBinaryPLY(GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_2, gaussians_3D_list); //And save it in output folder to have them a list of all outputs for later re-use
    
    printf("Finished writing to file\n");

    //_CrtDumpMemoryLeaks();

    return 0;
}

