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
#include "utils/argparser.hpp"

//Most of this code is really bad.
//Want to rewrite it all and make it OOP and cleaner. But dont know if I will have time during my internship here.
//Also, this will be abandoned and (maybe) ported to Halcyon, so dont even know if it is worth the time to polish it too much

#if GPU_IMPL

glm::mat4 createModelMatrix(const glm::vec3& center, const glm::vec3& normal, const glm::vec3& tangent, const glm::vec3& scale) {
    glm::vec3 r1 = glm::normalize(tangent);
    glm::vec3 r2 = glm::normalize(glm::cross(r1, normal));
    glm::vec3 r3 = glm::normalize(normal);

    glm::mat4 rotationMatrix = glm::mat4(
        glm::vec4(r1, 0.0f),
        glm::vec4(r2, 0.0f),
        glm::vec4(r3, 0.0f),
        glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
    );

    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), center);
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

    return translationMatrix * rotationMatrix * scaleMatrix;
}

static void prepareMeshAndUploadToGPU(std::string filename, std::string base_folder, std::vector<std::pair<Mesh, GLMesh>>& dataMeshAndGlMesh, int& uvSpaceWidth, int& uvSpaceHeight)
{
    std::vector<Mesh> meshes = parseGltfFileToMesh(filename, base_folder);

    printf("Parsed gltf mesh file\n");

    printf("Generating normalized UV coordinates (XATLAS)\n");
    generateNormalizedUvCoordinatesPerFace(uvSpaceWidth, uvSpaceHeight, meshes);

    printf("Loading mesh into OPENGL buffers\n");
    uploadMeshesToOpenGL(meshes, dataMeshAndGlMesh);
}


int main(int argc, char** argv) {
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

    //BASIC ARG PARSING
    InputParser input(argc, argv);

    int RESOLUTION                      = RESOLUTION_TARGET;                    //DEFAULT VALUES
    std::string MESH_FILE_LOCATION      = INPUT_MESH_FILENAME;                  //...
    std::string OUTPUT_FILE_LOCATION    = GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_1;  //...
    std::string BASE_FOLDER             = BASE_DATASET_FOLDER;                  //...
    float GAUSSIAN_STD                  = PIXEL_SIZE_GAUSSIAN_RADIUS;           //...
    int FORMAT                          = PLY_FORMAT;                           // 1 standard PLY, 2 modified ply, 3 litsplat compatible
    
    if (input.cmdOptionExists("-r"))
    {
        RESOLUTION = stoi(input.getCmdOption("-r"));
    }
    if (input.cmdOptionExists("-f"))
    {
        MESH_FILE_LOCATION = input.getCmdOption("-f");
        size_t lastBackslashPos = MESH_FILE_LOCATION.find_last_of('\\');
        if (lastBackslashPos != std::string::npos) {
            BASE_FOLDER = MESH_FILE_LOCATION.substr(0, lastBackslashPos + 1);
        } 
    }
    if(input.cmdOptionExists("-o"))
    {
        OUTPUT_FILE_LOCATION = input.getCmdOption("-o");
    }
    if (input.cmdOptionExists("-p"))
    {
        GAUSSIAN_STD = stoi(input.getCmdOption("-p"));
    }
    if (input.cmdOptionExists("-v"))
    {
        FORMAT = stoi(input.getCmdOption("-v"));
    }
    
    glViewport(0, 0, RESOLUTION, RESOLUTION);

    // Load shaders and meshes
    printf("Creating shader program\n");
    GLuint converterShaderProgram           = createConverterShaderProgram();

#if VOLUMETRIC
    GLuint volumetricShaderProgram          = createVolumetricSurfaceShaderProgram();
#endif

    printf("Parsing gltf mesh file\n");
    int normalizedUvSpaceWidth = 0, normalizedUvSpaceHeight = 0;
    std::vector<std::pair<Mesh, GLMesh>> dataMeshAndGlMesh;
    prepareMeshAndUploadToGPU(MESH_FILE_LOCATION, BASE_FOLDER, dataMeshAndGlMesh, normalizedUvSpaceWidth, normalizedUvSpaceHeight);

#if VOLUMETRIC
    int normalizedUvSpaceWidthMicro = 0, normalizedUvSpaceHeightMicro = 0;
    std::vector<std::pair<Mesh, GLMesh>> dataMicromeshAndGlMesh;
    //This is wrong, need to change base folder
    prepareMeshAndUploadToGPU(INPUT_MICROMESH_FILENAME, BASE_FOLDER, dataMicromeshAndGlMesh, normalizedUvSpaceWidthMicro, normalizedUvSpaceHeightMicro);
#endif

    std::vector<Gaussian3D> gaussians_3D_list;

    GLuint atomicCounter;
    setupAtomicCounter(atomicCounter);

    for (auto& mesh : dataMeshAndGlMesh)
    {
        Mesh meshData = std::get<0>(mesh);
        GLMesh meshGl = std::get<1>(mesh);

        printf("Loading textures into utility std::map\n");
        std::map<std::string, TextureDataGl> textureTypeMap;
        loadAllTexturesIntoMap(meshData.material, textureTypeMap);

        printf("Loading textures into OPENGL texture buffers\n");
        uploadTextures(textureTypeMap, meshData.material);

        printf("Setting up framebuffer and textures\n");
        GLuint framebuffer;
        setupFrameBuffer(framebuffer, RESOLUTION, RESOLUTION);

        //------------------PASS 1------------------
        
        performGpuConversion(
            converterShaderProgram, meshGl.vao,
            framebuffer, meshGl.vertexCount,
            normalizedUvSpaceWidth, normalizedUvSpaceHeight, 
            textureTypeMap, meshData.material, RESOLUTION, GAUSSIAN_STD
        );

        /*
        * Unfortunately the SSBO for some reason is not filled fully and is missing a certain amount of splats
        GLuint ssbo;
        generateSSBO(ssbo);

        readBackSSBO(gaussians_3D_list, ssbo, atomicCounter);
        */
        retrieveMeshFromFrameBuffer(gaussians_3D_list, framebuffer, RESOLUTION, RESOLUTION, false, true);
        

#if VOLUMETRIC
        //------------------PASS 2------------------
        //Do this to decrease the interpolation level in the rasterizer, creat util function for this.
        //TODO: now its fucked up and does not work, fix it?
        glViewport(0, 0, 64, 64);

        int size = meshData.faces.size();

        glm::mat4* modelMatrices = new glm::mat4[size];

        for (int i = 0; i < size; i++)
        {
            Face face = meshData.faces[i];
            glm::vec3 center = (face.pos[0] + face.pos[1] + face.pos[2]) / 3.0f;
            glm::vec3 r1 = glm::normalize(face.pos[0] - face.pos[1]);
            glm::vec3 normal = glm::normalize((face.normal[0] + face.normal[1] + face.normal[2]) / 3.0f);
            glm::vec3 r2 = glm::normalize(glm::cross(r1, normal));
            float factor = 10;
            float minScaleFactor = std::min(glm::length(face.pos[2] - face.pos[0]), std::min(glm::length(face.pos[1] - face.pos[0]), glm::length(face.pos[1] - face.pos[2])));
            glm::mat4 modelMatrix = createModelMatrix(center, normal, r2, glm::vec3(1, 1, 1));
            modelMatrices[i] = modelMatrix;
        }
        
        GLuint ssbo;
        generateSSBO(ssbo);

        generateVolumetricSurface(
            volumetricShaderProgram, dataMicromeshAndGlMesh[0].second.vao,
            modelMatrices, ssbo, atomicCounter,
            dataMicromeshAndGlMesh[0].second.vertexCount, size, 
            normalizedUvSpaceWidthMicro, normalizedUvSpaceHeightMicro,
            textureTypeMap, dataMicromeshAndGlMesh[0].first.material
        );

        readBackSSBO(gaussians_3D_list, ssbo, atomicCounter);


        glViewport(0, 0, RESOLUTION_TARGET, RESOLUTION_TARGET);
             
        //Free map
        std::map<std::string, TextureDataGl>::iterator it;
        for (it = textureTypeMap.begin(); it != textureTypeMap.end(); it++) free(it->second.textureData);
        
        
        glDeleteFramebuffers(1, &ssbo);
#endif      
        glDeleteFramebuffers(1, &framebuffer);
    }

    //Write to file
    std::cout << "Writing ply to                ->  " << OUTPUT_FILE_LOCATION << std::endl;

    switch (FORMAT)
    {
    case 1:
        writeBinaryPLY_standard_format(OUTPUT_FILE_LOCATION, gaussians_3D_list);
        break;

    case 2:
        writeBinaryPLY(OUTPUT_FILE_LOCATION, gaussians_3D_list);
        break;

    case 3:
        writeBinaryPLY_lit(OUTPUT_FILE_LOCATION, gaussians_3D_list);
        break;

    default:
        writeBinaryPLY_standard_format(OUTPUT_FILE_LOCATION, gaussians_3D_list);
        break;

    }
    std::cout << "Data successfully written to  ->  " << OUTPUT_FILE_LOCATION << std::endl;

    // Cleanup
    glfwTerminate();

    return 0;
}

#else

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
