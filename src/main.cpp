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
#include "renderer/mesh2SplatHandler.hpp"
#include "renderer/IoHandler.hpp"
#include "imGuiUi/ImGuiUi.hpp"

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
    Renderer simpleRenderer;
    Mesh2splatConverterHandler mesh2splatConverter;
    ImGuiUI ImGuiUI(3, 0, 0.1f, 0.5f);
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

    int normalizedUvSpaceWidth = 0, normalizedUvSpaceHeight = 0;
    std::vector<std::pair<Mesh, GLMesh>> dataMeshAndGlMesh;
    std::vector<Gaussian3D> gaussians_3D_list;
    std::map<std::string, TextureDataGl> textureTypeMap;

    GLuint pointsVAO;
    glGenVertexArrays(1, &pointsVAO);

    // Compile and link rendering shaders
    GLuint converterShaderProgram           = createConverterShaderProgram();
    GLuint renderShaderProgram      = createRendererShaderProgram(); 
    GLuint computeShaderProgram     = createComputeShaderProgram(); 
    GLuint gaussianBuffer           = -1;
    GLuint drawIndirectBuffer       = -1;

    //-------RENDER LOOP-------- TODO: integrate in renderer, make it oop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        ImGuiUI.renderUI();
        if (ImGuiUI.shouldLoadNewMesh()) {
                mesh2splatConverter.prepareMeshAndUploadToGPU(ImGuiUI.getFilePath(), ImGuiUI.getFilePathParentFolder(), dataMeshAndGlMesh, normalizedUvSpaceWidth, normalizedUvSpaceHeight);
                for (auto& mesh : dataMeshAndGlMesh)
                {
                    Mesh meshData = mesh.first;
                    GLMesh meshGl = mesh.second;
                    printf("Loading textures into utility std::map\n");
                    loadAllTextureMapImagesIntoMap(meshData.material, textureTypeMap);
                    generateTextures( meshData.material, textureTypeMap );
                }
                std::string meshFilePath(ImGuiUI.getFilePath());
                //First conversion so when loading model it already visualizes it
                if (!meshFilePath.empty()) {
                    mesh2splatConverter.runConversion(
                        meshFilePath, ImGuiUI.getFilePathParentFolder(),
                        ImGuiUI.getResolutionTarget(), ImGuiUI.getGaussianStd(),
                        converterShaderProgram, computeShaderProgram,
                        dataMeshAndGlMesh, normalizedUvSpaceWidth, normalizedUvSpaceHeight,
                        textureTypeMap, gaussianBuffer, drawIndirectBuffer
                    );
                }
           
            ImGuiUI.setLoadNewMesh(false);
        }

        if (ImGuiUI.shouldRunConversion()) {
            std::string meshFilePath(ImGuiUI.getFilePath());
            if (!meshFilePath.empty()) {
                //Entry point for conversion code
                mesh2splatConverter.runConversion(  
                    meshFilePath, ImGuiUI.getFilePathParentFolder(),
                    ImGuiUI.getResolutionTarget(), ImGuiUI.getGaussianStd(),
                    converterShaderProgram, computeShaderProgram,
                    dataMeshAndGlMesh, normalizedUvSpaceWidth, normalizedUvSpaceHeight,
                    textureTypeMap, gaussianBuffer, drawIndirectBuffer
                );
            }
            ImGuiUI.setRunConversion(false);
        }

        if (ImGuiUI.shouldSavePly() && ImGuiUI.getFilePathParentFolder().size() > 0)
        {
            std::vector<Gaussian3D> gaussian_3d_list;
            savePlyVector(std::string(ImGuiUI.getFilePathParentFolder()), gaussian_3d_list, ImGuiUI.getFormatOption());
        }

        ImGui::Render();
        
        simpleRenderer.render_point_cloud(window, pointsVAO, gaussianBuffer, drawIndirectBuffer, renderShaderProgram, ImGuiUI.getGaussianStd());

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

