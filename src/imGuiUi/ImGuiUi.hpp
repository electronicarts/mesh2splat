///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include <string>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>
#include <glm/glm.hpp>
#include "utils/utils.hpp"
#include "Imguizmo.hpp"
#include "ImGuiFileDialog.h"


class ImGuiUI {
public:
    ImGuiUI(float defaultGaussianStd, float defaultMesh2SPlatQuality);
    ~ImGuiUI();

    void preframe();
    void initialize(GLFWwindow* window);
    void renderUI();
    void displayGaussianCounts(unsigned int gaussianCount, unsigned int visibleGaussianCount);
    void postframe();

    bool shouldRunConversion() const;
    bool shouldLoadNewMesh() const;
    bool shouldExportSplats() const;
    bool wasMeshLoaded() const;
    bool shouldLoadPly() const;
    bool isLightingEnabled() const;
    bool isLightSelected() const;
    bool wasPlyLoaded() const;

    std::string getMeshFilePath() const;
    std::string getMeshFilePathParentFolder() const;
    std::string getMeshFullFilePathDestination() const;


    float getGaussianStd() const;
    int getResolutionTarget() const;
    unsigned int getFormatOption() const;

    glm::vec4 getSceneBackgroundColor() const;

    void setLoadNewMesh(bool shouldLoadNewMesh);
    void setMeshLoaded(bool loaded);
    void setRunConversion(bool shouldRunConversionFlag);
    void setShouldExportSplats(bool shouldSavePly);
    void setFrameMetrics(double gpuFrameTime);
    void setLoadNewPly(bool loadedPly);
    void setPlyLoaded(bool loadedPly);

    void renderGizmoUi(glm::mat4& glmViewMat, glm::mat4& glmProjMat, glm::mat4& glmModelMat);
    void renderFileSelectorWindow();
    void renderPropertiesWindow();
    void renderGpuFrametime();
    void renderLightingSettings();
    float getLightIntensity() const;
    glm::vec3 getLightColor() const;
    
    void setEnableDepthTest(bool depthTest);
    bool getIsDepthTestEnabled() const;


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
    int resolutionIndex = 0;
    const int resolutionOptions[3] = { 1024, 2048, 4096 };
    const char* resolutionLabels[3] = { "1024", "2048", "4096" };

	// index into formatLabels
    int formatIndex = 0;
    const char* formatLabels[3] = { "PLY Standard Format", "PLY PBR", "PLY Compressed PBR"};

    int renderIndex = 0;
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
    float lightIntensity = 0;
    float quality;
    bool runConversionFlag = false;
    bool loadNewMesh = false;
    bool loadNewPly = false;

    //Rendering flags
    bool hasRenderModeChanged = false;

    bool hasPlyBeenLoaded = false;
    bool hasMeshBeenLoaded = false;

    bool lightSelected = false;
    bool lightingEnabled = false;

    bool exportSplats = false;

    bool enableDepthTest = false;

    std::string meshFilePath;
    std::string meshParentFolder;

    utils::ModelFileExtension currentModelFormat = utils::ModelFileExtension::NONE;

    std::string destinationFilePathFolder = "";
	// without extension
    char outputFilename[256] = "DefaultFilename";


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
