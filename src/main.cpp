#include <string>
#include <vector>
#include <tuple>
#include <math.h>
#include <glm.hpp>
#include <set>
#include <stdint.h>
#include <chrono>

#include "parsers.hpp"
#include "utils/gaussianShapesUtilities.hpp"
#include "gaussianComputations.hpp"
#include "utils/shaderUtils.hpp"
#include "utils/normalizedUvUnwrapping.hpp"

#define PBSTR "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"
#define PBWIDTH 60
#define GPU_IMPL 0

//https://stackoverflow.com/a/36315819
void printProgressBar(float percentage)
{
    int val = (int)(percentage * 100);
    int lpad = (int)(percentage * PBWIDTH);
    int rpad = PBWIDTH - lpad;
    printf("\r%3d%% [%.*s%*s]", val, lpad, PBSTR, rpad, "");
    fflush(stdout);
}

#if GPU_IMPL == 0
int main() {
    // Initialize GLFW and create a window...
    if (!glfwInit())
        return -1;
    GLFWwindow* window = glfwCreateWindow(640, 480, "Tessellation Example", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = GL_TRUE;  // Enable modern OpenGL features
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        // GLEW failed to initialize
        fprintf(stderr, "Error initializing GLEW: %s\n", glewGetErrorString(err));
        glfwTerminate();
        return -1;
    }

    // Load shaders and meshes
    GLuint shaderProgram = createShaderProgram();
    std::vector<Mesh> meshes = parseGltfFileToMesh(OUTPUT_FILENAME);
    int uvSpaceWidth, uvSpaceHeight;
    generateNormalizedUvCoordinatesPerFace(uvSpaceWidth, uvSpaceHeight, meshes);
    float scale_factor_multiplier = 0.75f;
    int image_area = uvSpaceWidth * uvSpaceHeight;
    float Sd_x = .5f / (float)uvSpaceWidth;
    float Sd_y = .5f / (float)uvSpaceHeight;

    for (auto& mesh : meshes)
    {
        for (auto& face : mesh.faces)
        {
            get3DGaussianQuaternionRotation(face.pos, face.rotation);
            get3DGaussianScale(Sd_x, Sd_y, face.pos, face.normalizedUvs, face.scale);
        }
    }


    //std::cout << glm::to_string(meshes[0].faces[0].uv[1]) << "\n" << std::endl;

    float medianArea, medianEdgeLength, medianPerimeter, meshSurfaceArea;
    std::vector<GLMesh> glMeshes = uploadMeshesToOpenGL(meshes, medianArea, medianEdgeLength, medianPerimeter, meshSurfaceArea);

    // Setup Transform Feedback (assuming each mesh could be expanded up to 10 times its original size)
    GLuint feedbackBuffer, feedbackVAO, acBuffer;
    GLuint tesselationLevel = 3;

    size_t estimatedMaxVertices = 0;
    for (const auto& glMesh : glMeshes) {
        size_t verticesPerPatch = 150;
        estimatedMaxVertices += glMesh.vertexCount * verticesPerPatch;
    }

    setupTransformFeedbackAndAtomicCounter(estimatedMaxVertices * 3 * sizeof(float), feedbackBuffer, feedbackVAO, acBuffer);  // 3 floats per vertex


    // Perform tessellation and capture the output
    for (const auto& glMesh : glMeshes) {
        GLuint numberOfTessellatedTriangles = 0;
        auto started = std::chrono::high_resolution_clock::now();
        performTessellationAndCapture(
            shaderProgram, glMesh.vao, glMesh.vertexCount, 
            numberOfTessellatedTriangles, acBuffer, medianArea, medianEdgeLength, medianPerimeter, uvSpaceWidth, meshSurfaceArea, meshes[0].faces[0].scale, uvSpaceWidth, uvSpaceHeight);
        auto done = std::chrono::high_resolution_clock::now();
        std::cout << "Tesselation execution time: " << std::chrono::duration_cast<std::chrono::milliseconds>(done - started).count() << "ms\n" << std::endl;
        // Download the tessellated mesh data for each mesh
        downloadMeshFromGPU(feedbackBuffer, numberOfTessellatedTriangles);  // Assuming 10x vertices after tessellation
    }

    // Cleanup
    glfwTerminate();
    return 0;
}
#else
int main() {

    printf("Parsing input mesh\n");
    std::vector<Mesh> meshes = parseGltfFileToMesh(OUTPUT_FILENAME); //TODO: Struct is more readable and leaves no space for doubt

    //generateNormalizedUvCoordinatesPerFace(uvSpaceWidth, uvSpaceHeight, meshes);
    printf("Parsed input meshes, total number meshes: %d \n", (unsigned int)meshes.size());
 
    std::vector<Gaussian3D> gaussians_3D_list; //TODO: Think if can allocate space instead of having the vector dynamic size
    gaussians_3D_list.reserve(MAX_TEXTURE_SIZE * MAX_TEXTURE_SIZE);
    //TODO: I believe having an initial reference UV space would be ideal

    //std::map<std::string, std::pair<std::vector<unsigned char>, int>> loadedImages;
    int t = 0;
    size_t totFaces = 0;
    for (const auto& mesh : meshes) 
    {
        totFaces += mesh.faces.size();
    }

    int meshNumber = 0;

    int uvSpaceWidth, uvSpaceHeight;

    generateNormalizedUvCoordinatesPerFace(uvSpaceWidth, uvSpaceHeight, meshes);

    for (auto& mesh : meshes) 
    {
        meshNumber++;
        //TODO: I dont really like this, but it works

        std::map<std::string, std::pair<unsigned char*, int>> textureTypeMap;

        loadAllTexturesIntoMap(mesh.material, textureTypeMap);


        // Calculate σ based on the density and desired overlap, I derived this simple formula
        //TODO: Should find a better way to compute sigma and do it based on the size of the current triangle to tesselate
        //TODO: should base this on nyquist sampling rate: https://www.pbr-book.org/3ed-2018/Texture/Sampling_and_Antialiasing#FindingtheTextureSamplingRate
        //TODO: do outside

        printf("\n%zu triangle faces for mesh number %d / %zu\n", mesh.faces.size(), meshNumber, meshes.size());
        //std::vector<std::tuple<glm::vec3, glm::vec3, glm::vec4, MaterialGltf>> positionsOnTriangleSurfaceAndRGBs;


        for (const auto& triangleFace : mesh.faces) {

            printProgressBar((float)t / (float)totFaces);
            t++;


            glm::vec4 rotation3D;

            get3DGaussianQuaternionRotation(triangleFace.pos, rotation3D);

            glm::vec3 scale3D;
            float Sd_x = .5f / (float)uvSpaceWidth; //setting it to 1 results in the best quality and least transparency issues
            float Sd_y = .5f / (float)uvSpaceHeight;
            get3DGaussianScale(Sd_x, Sd_y, triangleFace.pos, triangleFace.normalizedUvs, scale3D);

            //std::cout << "Scale: " << glm::to_string(scale3D) << std::endl;

            //TODO: obviously if there is no texture you need to do something otherwise wont rasterize anything and wont find correspondence in pixel
            //This will have to stay like this until I establish a common initial UV mapping; for now, as a requirement, the model needs to have a UV mapping
            
            //TODO: split load and rasterization
            // Compute the bounding box in UV space
            std::pair<glm::vec2, glm::vec2> minMaxUV = computeUVBoundingBox(triangleFace.uv); //TODO: struct struct struct

            glm::vec2 minUV = std::get<0>(minMaxUV);
            glm::vec2 maxUV = std::get<1>(minMaxUV);

            // Convert bounding box to pixel coordinates
            
            glm::ivec2 minPixel = uvToPixel(minUV, mesh.material.baseColorTexture.width, mesh.material.baseColorTexture.height);
            glm::ivec2 maxPixel = uvToPixel(maxUV, mesh.material.baseColorTexture.width, mesh.material.baseColorTexture.height);

            
            if (minPixel.x == maxPixel.x && minPixel.y == maxPixel.y)
            {                
                continue; //Too small of bounding box, it is degenerate
            }

            for (int y = minPixel.y; y <= maxPixel.y; ++y) {
                for (int x = minPixel.x; x <= maxPixel.x; ++x) {
                    glm::vec2 pixelUV = pixelToUV(glm::ivec2(x, y), mesh.material.baseColorTexture.width - 1, mesh.material.baseColorTexture.height - 1);

                    if (pointInTriangle(pixelUV, triangleFace.uv[0], triangleFace.uv[1], triangleFace.uv[2])) {
                        float u, v, w;
                        computeBarycentricCoords(pixelUV, triangleFace.uv[0], triangleFace.uv[1], triangleFace.uv[2], u, v, w);

                        glm::vec3 interpolatedPos =
                            triangleFace.pos[0] * u +
                            triangleFace.pos[1] * v +
                            triangleFace.pos[2] * w;

                        glm::vec3 interpolatedNormal = 
                            glm::normalize(
                                triangleFace.normal[0] * u +
                                triangleFace.normal[1] * v +
                                triangleFace.normal[2] * w
                            );
                        
                        glm::vec4 interpolatedTangent =
                            glm::normalize(
                                triangleFace.tangent[0] * u +
                                triangleFace.tangent[1] * v +
                                triangleFace.tangent[2] * w
                            );

                        glm::vec3 surfaceNormal = glm::normalize(glm::cross(triangleFace.pos[1] - triangleFace.pos[0], triangleFace.pos[2] - triangleFace.pos[0]));
                        glm::vec4 rgba(0.0f, 0.0f, 0.0f, 0.0f);
                        float metallicFactor;
                        float roughnessFactor;
                        glm::vec3 outputNormal;
                        //Building TBN matrix, because if I use the one per splat (which is currently basically per triangle), the normals will not be nicely interpolated
                        computeAndLoadTextureInformation(
                            textureTypeMap,
                            mesh.material,
                            x, y, rgba,
                            metallicFactor, roughnessFactor,
                            interpolatedNormal, outputNormal, interpolatedTangent
                        );

                        {
                            Gaussian3D gaussian_3d;
                            gaussian_3d.position = interpolatedPos;
                            gaussian_3d.normal = outputNormal;
                            gaussian_3d.rotation = rotation3D;
                            gaussian_3d.scale = scale3D;
                            gaussian_3d.sh0 = getColor(glm::vec3((rgba.r), (rgba.g), (rgba.b)));
                            gaussian_3d.opacity = (rgba.a);
                            gaussian_3d.material = mesh.material;
                            gaussian_3d.material.metallicFactor = metallicFactor;
                            gaussian_3d.material.roughnessFactor = roughnessFactor;

                            gaussians_3D_list.push_back(gaussian_3d);
                        }
                    }

                }
            }
        }
                
    }
    
    printf("\nStarted writing to file\n");
#ifdef GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_1
    writeBinaryPLY(GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_1, gaussians_3D_list);
#endif
#ifdef GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_2
    writeBinaryPLY(GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_2, gaussians_3D_list); 
#endif
    
    printf("\nFinished writing to file\n");

    return 0;
}
#endif
