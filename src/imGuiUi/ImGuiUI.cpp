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

    renderGpuFrametime();

}

void ImGuiUI::renderGpuFrametime()
{

    ImGui::Begin("Performance");
    ImGui::Text("Frame Time: %.3f ms", gpuFrameTime);
    float current_max = *std::max_element(frameTimeHistory.begin(), frameTimeHistory.end());
    float display_max = std::max(current_max, maxPlotTimeMs);
    const float history_length = 5.0f; //seconds
    const float plot_height = 100.0f;

    ImGui::PlotLines(
        "Frame Times",
        frameTimeHistory.data(),
        frameTimeHistory.size(),
        0,
        nullptr,
        0.0f,                   
        display_max,            
        ImVec2(0, plot_height)
    );

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 plot_pos = ImGui::GetItemRectMin();
    ImVec2 plot_size = ImGui::GetItemRectSize();
    float target_y = plot_pos.y + plot_size.y - (targetFrameTimeThreshold / display_max) * plot_size.y;

    draw_list->AddLine(
        ImVec2(plot_pos.x, target_y),
        ImVec2(plot_pos.x + plot_size.x, target_y),
        IM_COL32(255, 0, 0, 100), 2.0f
    );
    
    // Axis labels
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%.1f ms", display_max);
    ImGui::SameLine(0, ImGui::GetContentRegionAvail().x - 60);
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%.1f ms", 0.0f);
    
    ImGui::Separator();
    ImGui::SliderFloat("Max Scale", &maxPlotTimeMs, 16.6f, 100.0f, "%.1f ms");
    ImGui::SliderFloat("Target Line", &targetFrameTimeThreshold, 8.3f, 50.0f, "%.1f ms");
    
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
void ImGuiUI::setFrameMetrics(double gpuFrameTime) {
    this->gpuFrameTime = static_cast<float>(gpuFrameTime);
    
    // Rolling buffer as vector
    if(frameTimeHistory.size() >= MAX_FRAME_HISTORY) {
        frameTimeHistory.erase(frameTimeHistory.begin());
    }

    frameTimeHistory.push_back(this->gpuFrameTime);
}

std::string ImGuiUI::getFilePath() { return std::string(filePathBuffer); };
std::string ImGuiUI::getFilePathParentFolder(){return parent_folder;};
std::string ImGuiUI::getFullFilePathDestination() { return std::string(destinationFilePathBuffer); };
int ImGuiUI::getFormatOption() { return formatOptions[formatIndex]; };
glm::vec4 ImGuiUI::getSceneBackgroundColor() { return sceneBackgroundColor; };


float ImGuiUI::getGaussianStd() { return gaussian_std; };
int ImGuiUI::getResolutionTarget() { return static_cast<int>(minRes + quality * (maxRes - minRes)); };


