#pragma once
#include <string>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

class ImGuiUI {
public:
    ImGuiUI(float defaultResolutionIndex, int defaultFormat, float defaultGaussianStd, float defaultMesh2SPlatQuality);
    ~ImGuiUI();

    void preframe();
    void initialize(GLFWwindow* window);
    void renderUI();
    void postframe();

    bool shouldRunConversion();
    bool shouldLoadNewMesh();
    bool shouldSavePly();
    std::string getFilePath();
    std::string getFilePathParentFolder();
    float getGaussianStd();
    int getResolutionTarget();
    int getFormatOption();

    void setLoadNewMesh(bool shouldLoadNewMesh);
    void setRunConversion(bool shouldRunConversionFlag);

private:
    int resolutionIndex;
    const int resolutionOptions[4] = { 256, 512, 1000, 2048 };
    const char* resolutionLabels[4] = { "256", "512", "1024", "2048" };
    
    int formatIndex;
    const int formatOptions[2] = { 1, 2 };
    const char* formatLabels[2] = { "PLY Standard Format", "Pbr PLY" };

    float gaussian_std;
    float quality;
    bool runConversionFlag;
    bool loadNewMesh;
    bool savePly;

    char filePathBuffer[256];
    std::string parent_folder;
    char destinationFilePathBuffer[256];

    const float minStd = 0.1f;
    const float maxStd = 10.0f;
    const int maxRes = 2048;
    const int minRes = 16;
};

