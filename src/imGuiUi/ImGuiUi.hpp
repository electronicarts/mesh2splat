#pragma once
#include <string>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glm.hpp>
#include "../utils/utils.hpp"

class ImGuiUI {
public:
    ImGuiUI(float defaultResolutionIndex, int defaultFormat, float defaultGaussianStd, float defaultMesh2SPlatQuality);
    ~ImGuiUI();

    void preframe();
    void initialize(GLFWwindow* window);
    void renderUI();
    void displayGaussianCount(unsigned int gaussianCount=0);
    void postframe();

    bool shouldRunConversion() const;
    bool shouldLoadNewMesh() const;
    bool shouldSavePly() const;
    bool wasMeshLoaded() const;
    bool shouldLoadPly() const;

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

    void renderGpuFrametime();

    enum class VisualizationOption
    {
        COLOR = 0,
        DEPTH = 1,
        NORMAL = 2,
        GEOMETRY = 3,
    };
    
    ImGuiUI::VisualizationOption selectedRenderMode() const ;


private:
    int resolutionIndex;
    const int resolutionOptions[4] = { 256, 512, 1000, 2048 };
    const char* resolutionLabels[4] = { "256", "512", "1024", "2048" };
    
    int formatIndex;
    const unsigned int formatOptions[3] = { 0, 1, 2 };
    const char* formatLabels[3] = { "PLY Standard Format", "PLY PBR", "PLY Compressed PBR"};

    int renderIndex;
    const ImGuiUI::VisualizationOption renderOptions[4] = {
        ImGuiUI::VisualizationOption::COLOR,
        ImGuiUI::VisualizationOption::DEPTH,
        ImGuiUI::VisualizationOption::NORMAL,
        ImGuiUI::VisualizationOption::GEOMETRY
    };
    const char* renderLabels[4] = { "Color", "Depth", "Normals", "Geometry"};

    float gaussian_std;
    float quality;
    bool runConversionFlag;
    bool loadNewMesh;
    bool loadNewPly;

    //Rendering flags
    bool hasRenderModeChanged;

    bool hasPlyBeenLoaded;
    bool hasMeshBeenLoaded;
    
    bool savePly;

    char meshFilePathBuffer[256];
    std::string meshParentFolder;

    char plyFilePathBuffer[256];
    std::string plyParentFolder;

    char destinationFilePathBuffer[256];

    const float minStd = 0.1f;
    const float maxStd = 3.0f;
    const int maxRes = 2048;
    const int minRes = 16;

    //Gpu timing data
    std::vector<float> frameTimeHistory = {0.0f};
    static constexpr size_t MAX_FRAME_HISTORY = 100;
    double gpuFrameTime = 0;
    float maxPlotTimeMs = 100.0f; 
    float targetFrameTimeThreshold = 16.6f; 

    glm::vec4 sceneBackgroundColor = { 0,0,0,1 };
};

