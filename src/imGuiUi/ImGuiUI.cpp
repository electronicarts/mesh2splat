#include "ImGuiUI.hpp"

ImGuiUI::ImGuiUI(float defaultResolutionIndex, int defaultFormat, float defaultGaussianStd, float defaultMesh2SPlatQuality)
    : resolutionIndex(defaultResolutionIndex),
      formatIndex(defaultFormat),
      gaussian_std(defaultGaussianStd),
      quality(defaultMesh2SPlatQuality),
      runConversionFlag(false),
      loadNewMesh(false),
      savePly(false),
      hasMeshBeenLoaded(false),
      loadNewPly(false),
      hasPlyBeenLoaded(false)
{
    //TODO: remove this and add a different debug default folder
    strcpy(meshFilePathBuffer, "C:\\Users\\sscolari\\Desktop\\dataset\\scifiHelmet\\scifiHelmet.glb");
    strcpy(plyFilePathBuffer, "C:\\Users\\sscolari\\Desktop\\halcyonFix\\halcyon\\Content\\Halcyon\\PlyTestAssets\\plushToyUp.ply");

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

    ImGui::InputText("Mesh File", meshFilePathBuffer, sizeof(meshFilePathBuffer));
    if (ImGui::Button("Load Mesh")) {
        loadNewMesh = true;
        std::string filePath(meshFilePathBuffer);
        if (!filePath.empty()) {
            size_t lastSlashPos = filePath.find_last_of("/\\");
            if (lastSlashPos != std::string::npos) {
                meshParentFolder = filePath.substr(0, lastSlashPos + 1);
            }
            else {
                meshParentFolder = "";
            }
        }
    }

    ImGui::InputText("Ply File", plyFilePathBuffer, sizeof(plyFilePathBuffer));
    if (ImGui::Button("Load Ply")) {
        loadNewPly = true;
        std::string filePath(plyFilePathBuffer);
        if (!filePath.empty()) {
            size_t lastSlashPos = filePath.find_last_of("/\\");
            if (lastSlashPos != std::string::npos) {
                plyParentFolder = filePath.substr(0, lastSlashPos + 1);
            }
            else {
                plyParentFolder = "";
            }
        }
    }

    ImGui::ColorEdit4("Background Color", &sceneBackgroundColor.x);
    ImGui::Combo("Render Mode", &renderIndex, renderLabels, IM_ARRAYSIZE(renderLabels));

    //TODO: right now std_dev is not updated in the actual gaussianBuffer, just during rendering. Need to consider this when exporting
    if (ImGui::SliderFloat("Gaussian Std Dev", &gaussian_std, minStd, maxStd, "%.2f"));

    if (ImGui::SliderFloat("Mesh2Splat quality", &quality, 0.0f, 1.0f, "%.2f")) {
        runConversionFlag = true;
    }
    ImGui::Combo("Export Format", &formatIndex, formatLabels, IM_ARRAYSIZE(formatLabels));

    ImGui::InputText("Save .PLY destination", destinationFilePathBuffer, sizeof(destinationFilePathBuffer));
    if (ImGui::Button("Save splat")) {
        savePly = true;
    }


    ImGui::End();

    renderGpuFrametime();
}

void ImGuiUI::renderGizmoUi(glm::mat4& glmViewMat, glm::mat4& glmProjMat, glm::mat4& glmModelMat)
{

    ImGui::Begin("Gizmo Control");

    static ImGuizmo::OPERATION currentOperation = ImGuizmo::TRANSLATE;
    static ImGuizmo::MODE currentMode = ImGuizmo::LOCAL;

    if (ImGui::RadioButton("Translate", currentOperation == ImGuizmo::TRANSLATE))
        currentOperation = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", currentOperation == ImGuizmo::ROTATE))
        currentOperation = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", currentOperation == ImGuizmo::SCALE))
        currentOperation = ImGuizmo::SCALE;

    if (currentOperation != ImGuizmo::SCALE)
    {
        if (ImGui::RadioButton("Local", currentMode == ImGuizmo::LOCAL))
            currentMode = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if (ImGui::RadioButton("World", currentMode == ImGuizmo::WORLD))
            currentMode = ImGuizmo::WORLD;
    }

    ImGui::End();

    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());

    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    float view[16], proj[16], model[16];

    std::memcpy(view,   glm::value_ptr(glmViewMat),     16 * sizeof(float));
    std::memcpy(proj,   glm::value_ptr(glmProjMat),     16 * sizeof(float));
    std::memcpy(model,  glm::value_ptr(glmModelMat),    16 * sizeof(float));

    bool manipulated = ImGuizmo::Manipulate(view, proj, currentOperation, currentMode, model);
    if (manipulated)
    {
        glmModelMat = glm::make_mat4(model);
    }

    if (ImGuizmo::IsOver())
        std::cout << "Gizmo is hovered this frame!\n";
    else
        std::cout << "Gizmo NOT hovered.\n";
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
    ImGui::Begin("Visible gaussians count:");
    ImGui::Text("N: %s", utils::formatWithCommas(gaussianCount).c_str());
    ImGui::End();
}


void ImGuiUI::postframe()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


bool ImGuiUI::shouldRunConversion() const { return runConversionFlag; } ;
bool ImGuiUI::shouldLoadNewMesh() const { return loadNewMesh; } ;
bool ImGuiUI::wasMeshLoaded() const { return hasMeshBeenLoaded; } ;
bool ImGuiUI::shouldLoadPly() const { return loadNewPly; } ;
bool ImGuiUI::wasPlyLoaded() const { return hasPlyBeenLoaded; };

bool ImGuiUI::shouldSavePly() const { return savePly; } ;
std::string ImGuiUI::getMeshFilePath() const { return std::string(meshFilePathBuffer); };
std::string ImGuiUI::getMeshFilePathParentFolder() const {return meshParentFolder;};
std::string ImGuiUI::getMeshFullFilePathDestination() const { return std::string(destinationFilePathBuffer); };

std::string ImGuiUI::getPlyFilePath() const { return std::string(plyFilePathBuffer); };
std::string ImGuiUI::getPlyFilePathParentFolder() const { return plyParentFolder; };

unsigned int ImGuiUI::getFormatOption() const { return formatOptions[formatIndex]; };
glm::vec4 ImGuiUI::getSceneBackgroundColor() const { return sceneBackgroundColor; };
float ImGuiUI::getGaussianStd() const { return gaussian_std; };
int ImGuiUI::getResolutionTarget() const { return static_cast<int>(minRes + quality * (maxRes - minRes)); };

//renderModeSelector
ImGuiUI::VisualizationOption ImGuiUI::selectedRenderMode() const { return renderOptions[renderIndex]; };

void ImGuiUI::setLoadNewMesh(bool shouldLoadNewMesh) { loadNewMesh = shouldLoadNewMesh; };
void ImGuiUI::setMeshLoaded(bool loaded) { hasMeshBeenLoaded = loaded; };

void ImGuiUI::setLoadNewPly(bool loadPly) { loadNewPly = loadPly; };
void ImGuiUI::setPlyLoaded(bool loadedPly) { hasPlyBeenLoaded = loadedPly; };

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






