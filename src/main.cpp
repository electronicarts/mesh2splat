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
#include "renderer/renderer.hpp"



//Most of this code is really bad.
//Want to rewrite it all and make it OOP and cleaner. But dont know if I will have time during my internship here.
//Also, this will be abandoned and (maybe) ported to Halcyon, so dont even know if it is worth the time to polish it too much

static void CheckOpenGLError(const char* stmt, const char* fname, int line)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        printf("OpenGL error %08x, at %s:%i - for %s\n", err, fname, line, stmt);
        abort();
    }
}

#ifdef _DEBUG
    #define GL_CHECK(stmt) do { \
            stmt; \
            CheckOpenGLError(#stmt, __FILE__, __LINE__); \
        } while (0)
#else
    #define GL_CHECK(stmt) stmt
#endif

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

static void runConversion(const std::string& meshFilePath, const std::string& baseFolder,
                   int resolution, float gaussianStd,
                   GLuint converterShaderProgram, GLuint computeShaderProgram,
                   std::vector<std::pair<Mesh, GLMesh>>& dataMeshAndGlMesh,
                   int normalizedUvSpaceWidth, int normalizedUvSpaceHeight,
                   std::map<std::string, TextureDataGl>& textureTypeMap,
                   GLuint& gaussianBuffer, GLuint &drawIndirectBuffer) 
{
    for (auto& mesh : dataMeshAndGlMesh) {
        Mesh meshData = std::get<0>(mesh);
        GLMesh meshGl = std::get<1>(mesh);

        GLuint framebuffer;
        GLuint* drawBuffers = setupFrameBuffer(framebuffer, resolution, resolution);

        glViewport(0, 0, resolution, resolution); //Set this as gpu conversion is dependant on viewport
        
        performGpuConversion(
            converterShaderProgram, meshGl.vao,
            framebuffer, meshGl.vertexCount,
            normalizedUvSpaceWidth, normalizedUvSpaceHeight,
            textureTypeMap, meshData.material, resolution, gaussianStd
        );

        if (gaussianBuffer != 0) {
            glDeleteBuffers(1, &gaussianBuffer);
        }

        setupSsboForComputeShader(resolution, resolution, &gaussianBuffer);
        
        if (drawIndirectBuffer != 0) {
            glDeleteBuffers(1, &drawIndirectBuffer);
        }
        drawIndirectBuffer = compute_shader_dispatch(computeShaderProgram, drawBuffers, gaussianBuffer, resolution);

        // Cleanup framebuffer and drawbuffers
        const int numberOfTextures = 5;
        glDeleteTextures(numberOfTextures, drawBuffers); 
        glDeleteFramebuffers(1, &framebuffer);         
        delete[] drawBuffers;                            
        glDeleteFramebuffers(1, &framebuffer);
    }
}

static void savePlyVector(std::string outputFileLocation, std::vector<Gaussian3D> gaussians_3D_list, unsigned int FORMAT)
{
    switch (FORMAT)
    {
        case 1:
            writeBinaryPLY_standard_format(outputFileLocation, gaussians_3D_list);
            break;
    
        case 2:
            writePbrPLY(outputFileLocation, gaussians_3D_list);
            break;
    
        case 3:
            writeBinaryPLY_lit(outputFileLocation, gaussians_3D_list);
            break;
    
        default:
            writeBinaryPLY_standard_format(outputFileLocation, gaussians_3D_list);
            break;
    }
}

int main(int argc, char** argv) {
    // Initialize GLFW and create a window...
    if (!glfwInit())
        return -1;
    GLFWwindow* window = glfwCreateWindow(720, 480, "Mesh2Splat", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    
    // Initialize GLEW
    glewExperimental = GL_TRUE;  // Enable modern OpenGL features
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        // GLEW failed to initialize
        fprintf(stderr, "Error initializing GLEW: %s\n", glewGetErrorString(err));
        glfwTerminate();
        return -1;
    }

    //SETUP IMGUI ---> TODO: separate imgui logic in a class
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460"); // Use appropriate GLSL version

    // Load shaders and meshes: todo: separate also this 
    printf("Creating shader program\n");
    GLuint converterShaderProgram           = createConverterShaderProgram();

    int normalizedUvSpaceWidth = 0, normalizedUvSpaceHeight = 0;
    std::vector<std::pair<Mesh, GLMesh>> dataMeshAndGlMesh;
    std::vector<Gaussian3D> gaussians_3D_list;
    std::map<std::string, TextureDataGl> textureTypeMap;

    GLuint pointsVAO;
    glGenVertexArrays(1, &pointsVAO);

    // Compile and link rendering shaders
    GLuint renderShaderProgram      = createRendererShaderProgram(); 
    GLuint computeShaderProgram     = createComputeShaderProgram(); 
    GLuint gaussianBuffer           = -1;
    GLuint drawIndirectBuffer       = -1;

    // Enable depth testing
    //glEnable(GL_DEPTH_TEST);

    static int resolutionIndex              = 3;  // Start with the highest resolution index (e.g., 2048)
    const int resolutionOptions[]           = { 256, 512, 1000, 2048 };
    const char* resolutionLabels[]          = { "256", "512", "1024", "2048" };

    static int formatIndex                  = 0;
    const int formatOptions[]               = { 1, 2};
    const char* formatLabels[]              = { "PLY Standard Format", "Pbr PLY" };
    static float pointSize                  = 5.0f;
    int resolutionTarget                    = resolutionOptions[resolutionIndex];
    int formatOption                        = formatOptions[formatIndex];
    bool wasConversionRunOnce               = false;
    static bool stillNeedtoLoadFirstMesh    = true;
    static char filePathBuffer[256]         = "C:\\Users\\sscolari\\Desktop\\dataset\\scifiHelmet\\scifiHelmet.glb"; //TODO: just for debug, remove this
    static float gaussian_std = 1.0f;  

    //-------RENDER LOOP--------
    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents();

        // Start new ImGui frame
        ImGui_ImplOpenGL3_NewFrame();

        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();


        // UI --> todo: separate logic
        {
            static char destinationFilePathBuffer[256] = "";

                
            const float  minStd = 0.1f;             
            const float  maxStd = 10.0f;  
            
            static float quality = 0.5f;  
            const int maxRes = 2048; 
            const int minRes = 16;

            static bool runConversionFlag = false;
            static bool loadNewMesh = false;
            static bool savePly = false;
            std::string parent_folder;

            ImGui::Begin("File Selector");

            // Always show the input field for the mesh file path
            ImGui::InputText("Mesh File", filePathBuffer, sizeof(filePathBuffer));

            if (ImGui::Button("Load Mesh")) {
                loadNewMesh = true;
            }

            // Resolution and format selection
            ImGui::Combo("Resolution", &resolutionIndex, resolutionLabels, IM_ARRAYSIZE(resolutionLabels));
            ImGui::Combo("Format", &formatIndex, formatLabels, IM_ARRAYSIZE(formatLabels));

            if (ImGui::Button("Run Conversion")) {
                runConversionFlag = true;
            }

            ImGui::SliderFloat("Gaussian Std", &gaussian_std, minStd, maxStd, "%.2f");


            if (ImGui::SliderFloat("Mesh2Splat quality", &quality, 0.0f, 1.0f, "%.2f"))
            {
                runConversionFlag = true;
            };


            ImGui::InputText("Save .PLY destination", destinationFilePathBuffer, sizeof(destinationFilePathBuffer));
            if (ImGui::Button("Save splat"))
            {
                savePly = true;
            }

            ImGui::End();

            // Check if we need to load a new mesh
            if (loadNewMesh) {
                std::string filePath(filePathBuffer);
                if (!filePath.empty()) {
                    size_t lastSlashPos = filePath.find_last_of("/\\");
                    if (lastSlashPos != std::string::npos) {
                        parent_folder = filePath.substr(0, lastSlashPos + 1);
                    } else {
                        parent_folder = "";
                    }

                    prepareMeshAndUploadToGPU(filePathBuffer, parent_folder, dataMeshAndGlMesh, normalizedUvSpaceWidth, normalizedUvSpaceHeight);

                    for (auto& mesh : dataMeshAndGlMesh)
                    {
                        Mesh meshData = mesh.first;
                        GLMesh meshGl = mesh.second;
                        printf("Loading textures into utility std::map\n");
                        loadAllTextureMapImagesIntoMap(meshData.material, textureTypeMap);
                        generateTextures( meshData.material, textureTypeMap );
                    }
                    stillNeedtoLoadFirstMesh = false;
                }
                loadNewMesh = false;  
            }

            if (runConversionFlag && !stillNeedtoLoadFirstMesh) {
                std::string meshFilePath(filePathBuffer);
                if (!meshFilePath.empty()) {
                    resolutionTarget = static_cast<int>(minRes + quality * (maxRes - minRes));
                    //Entry point for conversion code
                    runConversion(  
                        meshFilePath, parent_folder,
                        resolutionTarget, gaussian_std,
                        converterShaderProgram, computeShaderProgram,
                        dataMeshAndGlMesh, normalizedUvSpaceWidth, normalizedUvSpaceHeight,
                        textureTypeMap, gaussianBuffer, drawIndirectBuffer
                    );

                    wasConversionRunOnce = true;
                }
                runConversionFlag = false;
            }

            if (wasConversionRunOnce && savePly && destinationFilePathBuffer)
            {
                formatOption = formatOptions[formatIndex];
                std::vector<Gaussian3D> gaussian_3d_list;

                savePlyVector(std::string(destinationFilePathBuffer), gaussian_3d_list, formatOption);
            }
        }

        ImGui::Render();

        if (gaussianBuffer != static_cast<GLuint>(-1) && 
            drawIndirectBuffer != static_cast<GLuint>(-1) && 
            pointsVAO != 0 && 
            renderShaderProgram != 0) 
        {
            render_point_cloud(window, pointsVAO, gaussianBuffer, drawIndirectBuffer, renderShaderProgram, gaussian_std);
        }

        // Render ImGui on top
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup resources
    glDeleteVertexArrays(1, &pointsVAO);
    glDeleteBuffers(1, &gaussianBuffer);
    glDeleteProgram(renderShaderProgram);

    //Cleanup imgui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Terminate GLFW after rendering loop ends
    glfwTerminate();

    return 0;
}

