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

#define PBSTR "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"
#define PBWIDTH 60

//https://stackoverflow.com/a/36315819
void printProgressBar(float percentage)
{
    int val = (int)(percentage * 100);
    int lpad = (int)(percentage * PBWIDTH);
    int rpad = PBWIDTH - lpad;
    printf("\r%3d%% [%.*s%*s]", val, lpad, PBSTR, rpad, "");
    fflush(stdout);
}

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
    auto parsedGltfModel = parseGltfFileToMesh(OUTPUT_FILENAME);
    
    std::vector<Mesh>       meshes          = std::get<0>(parsedGltfModel);
    std::vector<glm::vec3>  globalVertices  = std::get<1>(parsedGltfModel);
    std::vector<glm::vec2>  globalUvs       = std::get<2>(parsedGltfModel);
    std::vector<glm::vec3>  globalNormals   = std::get<3>(parsedGltfModel);

    printf("Parsed input mesh\n");

    printf(
        "[Total number of meshes: %zu]\n[Total number of vertices : %zu]\n[Total number of UVs : %zu]\n[Total number of normlas : %zu]\n", 
        meshes.size(), globalVertices.size(), globalUvs.size(), globalNormals.size()
    );
 
    std::vector<Gaussian3D> gaussians_3D_list;
    
    int j = 0;

    //TODO: I believe having an initial reference UV space would be ideal

    std::map<std::string, std::pair<std::vector<unsigned char>, int>> loadedImages;
    int t = 0;
    int totFaces = 0;
    for (const auto& mesh : meshes) 
    {
        totFaces += mesh.faces.size();
    }

    for (auto& mesh : meshes) 
    {
        j++;
        //TODO: I dont really like this, but it works

        unsigned char* meshTexture = NULL;
        int bpp = 0;

        if (mesh.material.baseColorTexture.path != EMPTY_TEXTURE)
        {
            std::pair<unsigned char*, int> textureAndBpp = loadImage(mesh.material.baseColorTexture.path, mesh.material.baseColorTexture.width, mesh.material.baseColorTexture.height);

            meshTexture = std::get<0>(textureAndBpp);
            bpp = std::get<1>(textureAndBpp);
        }
        else {
            mesh.material.baseColorTexture.width = MAX_TEXTURE_WIDTH;
            mesh.material.baseColorTexture.height = MAX_TEXTURE_WIDTH;
        }
        

        printf("%zu triangle faces for mesh number %d / %zu\n", mesh.faces.size(), j, meshes.size());
        for (const auto& triangleFace : mesh.faces) {
            
            printProgressBar((float)t / (float)totFaces);
            t++;

            // If texture for this face not yet loaded, do so
            //TODO: This is toooooooo slow but supports multiple different albedo textures per mesh
            /*
            if (loadedImages.find(materialGltf.baseColorTexture.path) == loadedImages.end())
            {
                std::vector<unsigned char> vecToFill;
                std::pair<bool, int> successAndBpp = loadImageIntoVector(materialGltf.baseColorTexture.path, materialGltf.baseColorTexture.width, materialGltf.baseColorTexture.height, vecToFill);
                bool success = std::get<0>(successAndBpp);
                int bpp = std::get<1>(successAndBpp);
                loadedImages.emplace(materialGltf.baseColorTexture.path, std::make_pair(vecToFill, bpp));
            } 
            */

            //TODO: obviously if there is no texture you need to do something otherwise wont rasterize shit and wont find correspondence in pixel
            //This will have to stay like this until I establish a common initial UV mapping; for now, as a requirement, the model needs to have a UV mapping
            

#if SHOULD_RASTERIZE
            // Compute the bounding box in UV space
            std::pair<glm::vec2, glm::vec2> minMaxUV = computeUVBoundingBox(triangleFace.uvIndices);

            glm::vec2 minUV = std::get<0>(minMaxUV);
            glm::vec2 maxUV = std::get<1>(minMaxUV);

            // Convert bounding box to pixel coordinates
            glm::ivec2 minPixel = uvToPixel(minUV, mesh.material.baseColorTexture.width-1, mesh.material.baseColorTexture.height-1);
            glm::ivec2 maxPixel = uvToPixel(maxUV, mesh.material.baseColorTexture.width-1, mesh.material.baseColorTexture.height-1);

            std::vector<std::tuple<glm::vec3, glm::vec3, glm::vec4, MaterialGltf>> positionsOnTriangleSurfaceAndRGBs;

            if (minPixel.x == maxPixel.x && minPixel.y == maxPixel.y)
            {                
                continue;
            }

            for (int y = minPixel.y; y <= maxPixel.y; ++y) {
                for (int x = minPixel.x; x <= maxPixel.x; ++x) {
                    glm::vec2 pixelUV = pixelToUV(glm::ivec2(x, y), mesh.material.baseColorTexture.width - 1, mesh.material.baseColorTexture.height - 1);

                    if (pointInTriangle(pixelUV, triangleFace.uvIndices[0], triangleFace.uvIndices[1], triangleFace.uvIndices[2])) {
                        float u, v, w;
                        computeBarycentricCoords(pixelUV, triangleFace.uvIndices[0], triangleFace.uvIndices[1], triangleFace.uvIndices[2], u, v, w);

                        glm::vec3 interpolatedPos =
                            triangleFace.vertexIndices[0] * u +
                            triangleFace.vertexIndices[1] * v +
                            triangleFace.vertexIndices[2] * w;

                        glm::vec3 interpolatedNormal = glm::normalize(
                            triangleFace.normalIndices[0] * u +
                            triangleFace.normalIndices[1] * v +
                            triangleFace.normalIndices[2] * w);

                        //glm::vec4 normalColor((interpolatedNormal + glm::vec3(1.0f, 1.0f, 1.0f)) / 2.0f, 1.0f);

                        if (meshTexture != NULL && bpp != 0) //use texture for rgba
                        {

                            glm::vec4 rgba_from_texture = rgbaAtPos(
                                mesh.material.baseColorTexture.width,
                                x, y,
                                meshTexture, bpp
                            );

                            //TODO: unify so that just pass material even when texture, the rgba is the albedo, no need for rgba slot
                            positionsOnTriangleSurfaceAndRGBs.push_back(std::make_tuple(interpolatedNormal, interpolatedPos, rgba_from_texture, mesh.material));
                        }
                        else { //use material for rgba

                            glm::vec4 rgba = mesh.material.baseColorFactor;
                            positionsOnTriangleSurfaceAndRGBs.push_back(std::make_tuple(interpolatedNormal, interpolatedPos, rgba, mesh.material));

                        }


                    }

                }
            }

            // Calculate σ based on the density and desired overlap, I derive this simple formula from 
            //TODO: Should find a better way to compute sigma and do it based on the size of the current triangle to tesselate
            //TODO: should base this on nyquist sampling rate: https://www.pbr-book.org/3ed-2018/Texture/Sampling_and_Antialiasing#FindingtheTextureSamplingRate

            float scale_factor_multiplier = .75f;
            float image_area = (mesh.material.baseColorTexture.width * mesh.material.baseColorTexture.height);
            float sigma = scale_factor_multiplier * sqrt(2.0f / image_area);
            std::pair<glm::vec4, glm::vec3> rotAndScale = getScaleRotationGaussian(sigma, triangleFace.vertexIndices, triangleFace.uvIndices);

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
                glm::vec3 interpolatedNormal    = std::get<0>(normalPosRgbDispl);
                glm::vec3 position              = std::get<1>(normalPosRgbDispl);
                glm::vec4 rgba                  = std::get<2>(normalPosRgbDispl);
                MaterialGltf material           = std::get<3>(normalPosRgbDispl);
                
                Gaussian3D gaussian_3d;
                gaussian_3d.position    = position;
                gaussian_3d.normal      = interpolatedNormal; 
                gaussian_3d.rotation    = rotation;
                gaussian_3d.scale       = scale;
                gaussian_3d.sh0         = getColor(glm::vec3(rgba.r, rgba.g, rgba.b));
                gaussian_3d.opacity     = rgba.a;
                gaussian_3d.material    = material; //TODO: still need to embed this info in the .ply

                gaussians_3D_list.push_back(gaussian_3d); 
            }

        }
                
    }
    
    printf("\nStarted writing to file\n");

    writeBinaryPLY(GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_1, gaussians_3D_list);
    writeBinaryPLY(GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_2, gaussians_3D_list); //And save it in output folder to have them a list of all outputs for later re-use
    
    printf("\nFinished writing to file\n");

    //_CrtDumpMemoryLeaks();

    return 0;
}

