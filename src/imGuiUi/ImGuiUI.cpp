#include "ImGuiUI.hpp"

ImGuiUI::ImGuiUI(float defaultResolutionIndex, int defaultFormat, float defaultGaussianStd, float defaultMesh2SPlatQuality)
    : resolutionIndex(defaultResolutionIndex),
      formatIndex(defaultFormat),
      gaussian_std(defaultGaussianStd),
      quality(defaultMesh2SPlatQuality),
      runConversionFlag(false),
      loadNewMesh(false),
      savePly(false),
      hasMeshBeenLoaded(false)
{
    //TODO: remove this and add a different debug default folder
    strcpy(filePathBuffer, "C:\\Users\\sscolari\\Desktop\\dataset\\scifiHelmet\\scifiHelmet.glb");
    destinationFilePathBuffer[0] = '\0';  // empty destination path initially
}

ImGuiUI::~ImGuiUI()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiUI::initialize(GLFWwindow* window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460"); // Use appropriate GLSL version
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

    ImGui::ColorEdit4("Background Color", &sceneBackgroundColor.x);


    //TODO: right now std_dev is not updated in the actual gaussianBuffer, just during rendering. Need to consider this when exporting
    if (ImGui::SliderFloat("Gaussian Std Dev", &gaussian_std, minStd, maxStd, "%.2f"));
    //{
    //    runConversionFlag = true;
    //};
    if (ImGui::SliderFloat("Mesh2Splat quality", &quality, 0.0f, 1.0f, "%.2f")) {
        runConversionFlag = true;
    }
    ImGui::Combo("Format", &formatIndex, formatLabels, IM_ARRAYSIZE(formatLabels));

    ImGui::InputText("Save .PLY destination", destinationFilePathBuffer, sizeof(destinationFilePathBuffer));
    if (ImGui::Button("Save splat")) {
        savePly = true;
    }

    ImGui::End();
}

void ImGuiUI::preframe()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiUI::displayGaussianCount(unsigned int gaussianCount)
{
    ImGui::Begin("Gaussian Count");
    ImGui::Text("N: %s", formatWithCommas(gaussianCount).c_str());
    ImGui::End();
}


void ImGuiUI::postframe()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


bool ImGuiUI::shouldRunConversion() { return runConversionFlag; };
bool ImGuiUI::shouldLoadNewMesh() { return loadNewMesh; };
bool ImGuiUI::wasMeshLoaded() { return hasMeshBeenLoaded; };
bool ImGuiUI::shouldSavePly() { return savePly; };

void ImGuiUI::setLoadNewMesh(bool shouldLoadNewMesh) { loadNewMesh = shouldLoadNewMesh; };
void ImGuiUI::setMeshLoaded(bool loaded) { hasMeshBeenLoaded = loaded; };
void ImGuiUI::setRunConversion(bool shouldRunConversionFlag) { runConversionFlag = shouldRunConversionFlag; };
void ImGuiUI::setShouldSavePly(bool shouldSavePly) { savePly = shouldSavePly; };

std::string ImGuiUI::getFilePath() { return std::string(filePathBuffer); };
std::string ImGuiUI::getFilePathParentFolder(){return parent_folder;};
std::string ImGuiUI::getFullFilePathDestination() { return std::string(destinationFilePathBuffer); };
int ImGuiUI::getFormatOption() { return formatOptions[formatIndex]; };
glm::vec4 ImGuiUI::getSceneBackgroundColor() { return sceneBackgroundColor; };


float ImGuiUI::getGaussianStd() { return gaussian_std; };
int ImGuiUI::getResolutionTarget() { return static_cast<int>(minRes + quality * (maxRes - minRes)); };


