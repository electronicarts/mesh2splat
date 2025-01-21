#include "ImGuiUI.hpp"

ImGuiUI::ImGuiUI(float defaultResolutionIndex, int defaultFormat, float defaultGaussianStd, float defaultMesh2SPlatQuality)
    : resolutionIndex(defaultResolutionIndex),
      formatIndex(defaultFormat),
      gaussian_std(defaultGaussianStd),
      quality(defaultMesh2SPlatQuality),
      runConversionFlag(false),
      loadNewMesh(false),
      savePly(false)
{
    strcpy(filePathBuffer, "C:\\Users\\sscolari\\Desktop\\dataset\\scifiHelmet\\scifiHelmet.glb");
    destinationFilePathBuffer[0] = '\0';  // empty destination path initially
}

ImGuiUI::~ImGuiUI()
{

}

void ImGuiUI::initialize()
{

}

void ImGuiUI::renderUI()
{
    ImGui::Begin("File Selector");

    ImGui::InputText("Mesh File", filePathBuffer, sizeof(filePathBuffer));
    if (ImGui::Button("Load Mesh")) {
        loadNewMesh = true;
        std::string filePath(filePathBuffer);
        if (!filePath.empty()) {
            size_t lastSlashPos = filePath.find_last_of("/\\");
            if (lastSlashPos != std::string::npos) {
                parent_folder = filePath.substr(0, lastSlashPos + 1);
            }
            else {
                parent_folder = "";
            }
        }
    }

    ImGui::Combo("Resolution", &resolutionIndex, resolutionLabels, IM_ARRAYSIZE(resolutionLabels));
    ImGui::Combo("Format", &formatIndex, formatLabels, IM_ARRAYSIZE(formatLabels));

    if (ImGui::Button("Run Conversion")) {
        runConversionFlag = true;
    }

    ImGui::SliderFloat("Gaussian Std", &gaussian_std, minStd, maxStd, "%.2f");
    if (ImGui::SliderFloat("Mesh2Splat quality", &quality, 0.0f, 1.0f, "%.2f")) {
        runConversionFlag = true;
    }

    ImGui::InputText("Save .PLY destination", destinationFilePathBuffer, sizeof(destinationFilePathBuffer));
    if (ImGui::Button("Save splat")) {
        savePly = true;
    }

    ImGui::End();

    // For example: handling loadNewMesh, runConversionFlag, savePly actions
    // You may want to call methods on other classes (e.g., mesh2splatConverter) based on these flags
}

bool ImGuiUI::shouldRunConversion() { return runConversionFlag; };
bool ImGuiUI::shouldLoadNewMesh() { return loadNewMesh; };
bool ImGuiUI::shouldSavePly() { return savePly; };

void ImGuiUI::setLoadNewMesh(bool shouldLoadNewMesh) { loadNewMesh = shouldLoadNewMesh; };
void ImGuiUI::setRunConversion(bool shouldRunConversionFlag) { runConversionFlag = shouldRunConversionFlag; };

std::string ImGuiUI::getFilePath() { return std::string(filePathBuffer); };
std::string ImGuiUI::getFilePathParentFolder(){return parent_folder;};
int ImGuiUI::getFormatOption() { return formatOptions[formatIndex]; };

float ImGuiUI::getGaussianStd() { return gaussian_std; };
int ImGuiUI::getResolutionTarget() { return static_cast<int>(minRes + quality * (maxRes - minRes)); };


