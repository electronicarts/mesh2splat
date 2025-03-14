#pragma once
#include <string>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glm.hpp>
#include "../utils/utils.hpp"
#include "../../thirdParty/imguizmo/Imguizmo.hpp"
#include "../../thirdParty/ImGuiFileDialog/ImGuiFileDialog.h"


class ImGuiUI {
public:
    ImGuiUI(float defaultResolutionIndex, int defaultFormat, float defaultGaussianStd, float defaultMesh2SPlatQuality);
    ~ImGuiUI();

    void preframe();
    void initialize(GLFWwindow* window);
    void renderUI();
    void displayGaussianCounts(unsigned int gaussianCount, unsigned int visibleGaussianCount);
    void postframe();

    bool shouldRunConversion() const;
    bool shouldLoadNewMesh() const;
    bool shouldSavePly() const;
    bool wasMeshLoaded() const;
    bool shouldLoadPly() const;
    bool isLightingEnabled() const;
    bool isLightSelected() const;
    bool wasPlyLoaded() const;

    std::string getMeshFilePath() const;
    std::string getMeshFilePathParentFolder() const;
    std::string getMeshFullFilePathDestination() const;
    std::string getPlyFilePathParentFolder() const;
    std::string getPlyFilePath() const;


    float getGaussianStd() const;
    int getResolutionTarget() const;
    unsigned int getFormatOption() const;

    glm::vec4 getSceneBackgroundColor() const;

    void setLoadNewMesh(bool shouldLoadNewMesh);
    void setMeshLoaded(bool loaded);
    void setRunConversion(bool shouldRunConversionFlag);
    void setShouldSavePly(bool shouldSavePly);
    void setFrameMetrics(double gpuFrameTime);
    void setLoadNewPly(bool loadedPly);
    void setPlyLoaded(bool loadedPly);

    void renderGizmoUi(glm::mat4& glmViewMat, glm::mat4& glmProjMat, glm::mat4& glmModelMat);

    void renderGpuFrametime();
    void renderLightingSettings();
    float getLightIntensity() const;
    glm::vec3 getLightColor() const;
    

    enum class VisualizationOption
    {
        ALBEDO = 0,
        DEPTH = 1,
        NORMAL = 2,
        GEOMETRY = 3,
        OVERDRAW = 4,
        PBR = 5
    };
    
    ImGuiUI::VisualizationOption selectedRenderMode() const ;


private:
    int resolutionIndex;
    const int resolutionOptions[3] = { 1024, 2048, 4096 };
    const char* resolutionLabels[3] = { "1024", "2048", "4096" };
    
    int formatIndex;
    const unsigned int formatOptions[3] = { 0, 1, 2 };
    const char* formatLabels[3] = { "PLY Standard Format", "PLY PBR", "PLY Compressed PBR"};

    int renderIndex;
    const ImGuiUI::VisualizationOption renderOptions[6] = {
        ImGuiUI::VisualizationOption::ALBEDO,
        ImGuiUI::VisualizationOption::DEPTH,
        ImGuiUI::VisualizationOption::NORMAL,
        ImGuiUI::VisualizationOption::GEOMETRY,
        ImGuiUI::VisualizationOption::OVERDRAW,
        ImGuiUI::VisualizationOption::PBR
    };
    const char* renderLabels[6] = { "Albedo", "Depth", "Normals", "Geometry", "Overdraw", "PBR (metallic-roughness)"};

    float gaussian_std;
    float lightIntensity;
    float quality;
    bool runConversionFlag;
    bool loadNewMesh;
    bool loadNewPly;

    //Rendering flags
    bool hasRenderModeChanged;

    bool hasPlyBeenLoaded;
    bool hasMeshBeenLoaded;

    bool lightSelected;
    bool lightingEnabled;

    bool savePly;

    std::string meshFilePath;
    std::string meshParentFolder;

    std::string plyFilePath;
    std::string plyParentFolder;

    utils::ModelFileExtension currentModelFormat = utils::ModelFileExtension::NONE;

    std::string destinationFilePathFolder = "";
    char outputFilename[256] = "DefaultFilename.ply";


    const float minStd = 0.1f;
    const float maxStd = 2.0f;
    const float minLightIntensity = 0.0;
    const float maxLightIntensity = 1000.0;
    int maxRes = 1024; //TBH not sure what best value is here
    int minRes = 16;

    //Gpu timing data
    std::vector<float> frameTimeHistory = {0.0f};
    static constexpr size_t MAX_FRAME_HISTORY = 100;
    double gpuFrameTime = 0;
    float maxPlotTimeMs = 100.0f; 
    float targetFrameTimeThreshold = 16.6f; 

    glm::vec4 sceneBackgroundColor = { 0,0,0,1 };
    glm::vec3 lightColor = { 1,1,1 };
};
