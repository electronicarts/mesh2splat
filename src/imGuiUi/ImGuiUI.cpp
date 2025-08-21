///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "ImGuiUI.hpp"

ImGuiUI::ImGuiUI(float defaultGaussianStd, float defaultMesh2SPlatQuality)
    : resolutionIndex(0),
      formatIndex(0),
      gaussian_std(defaultGaussianStd),
      quality(defaultMesh2SPlatQuality),
      runConversionFlag(false),
      loadNewMesh(false),
      savePly(false),
      hasMeshBeenLoaded(false),
      loadNewPly(false),
      hasPlyBeenLoaded(false)
{}

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

void ImGuiUI::renderFileSelectorWindow()
{
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 200), ImGuiCond_FirstUseEver);

    ImGui::Begin("File Selector");

    ImGui::SeparatorText("Input");

    if (ImGui::Button("Select file to load (.glb / .ply)")) {
        IGFD::FileDialogConfig config;
        config.path = ".";
        ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiCond_Always);
        ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".glb,.ply", config);
    }

    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) { 
            std::string file = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string parentFolder = ImGuiFileDialog::Instance()->GetCurrentPath().append("//");
            currentModelFormat = utils::getFileExtension(file);
            switch (currentModelFormat)
            {
            case utils::ModelFileExtension::GLB:
                meshFilePath = file;
                meshParentFolder = parentFolder;
                break;
            case utils::ModelFileExtension::PLY:
                plyFilePath = file;
                plyParentFolder = parentFolder;
                break;
            case utils::ModelFileExtension::NONE:
                break;
            }
            
        }

        ImGuiFileDialog::Instance()->Close();
    }

    switch (currentModelFormat)
    {
    case utils::ModelFileExtension::GLB:
        ImGui::Text("Selected Glb file: %s", meshFilePath.c_str());
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.60f, 0.20f, 1.0f)); 
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.75f, 0.30f, 1.0f)); 
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.50f, 0.15f, 1.0f)); 
        loadNewMesh = ImGui::Button("Convert Mesh to 3DGS");
        ImGui::PopStyleColor(3);

        break;
    case utils::ModelFileExtension::PLY:
        ImGui::Text("Selected Ply file: %s", plyFilePath.c_str());
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.60f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.75f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.50f, 0.15f, 1.0f));
        loadNewPly = ImGui::Button("Load 3DGS ply");
        ImGui::PopStyleColor(3);

        break;
    case utils::ModelFileExtension::NONE:
        break;
    }

    ImGui::SeparatorText("Output Folder");

    if (ImGui::Button("Select output folder")) {
        IGFD::FileDialogConfig config;
        config.path = ".";
        ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiCond_Always);
        ImGuiFileDialog::Instance()->OpenDialog(
            "ChooseFolderDlgKey",          
            "Choose Output Folder",        
            nullptr,                       
            config
        );
    }

    if (ImGuiFileDialog::Instance()->Display("ChooseFolderDlgKey"))
    {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string chosenFolder = ImGuiFileDialog::Instance()->GetCurrentPath();
            destinationFilePathFolder = chosenFolder + "\\";
        }

        // Close the dialog
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::SameLine();
    // Show which folder is chosen
    if (!destinationFilePathFolder.empty()) ImGui::Text("Selected folder: %s", destinationFilePathFolder.c_str());
    ImGui::SameLine();
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float comboWidth = std::max(180.0f, availableWidth * 0.25f);
    // Now set the next item's width to half the available region
    ImGui::InputText("##ExportFileName", outputFilename, sizeof(outputFilename));

    ImGui::SetNextItemWidth(comboWidth);
    ImGui::Combo("##Combobox", &formatIndex, formatLabels, IM_ARRAYSIZE(formatLabels));

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.60f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.75f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.50f, 0.15f, 1.0f));
    if (ImGui::Button("Save splat")) {
        savePly = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::End();
}

void ImGuiUI::renderPropertiesWindow()
{
    ImGui::SetNextWindowPos(ImVec2(600, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(450, 350), ImGuiCond_FirstUseEver);

    ImGui::Begin("Properties");

    ImGui::Combo("Property visualization", &renderIndex, renderLabels, IM_ARRAYSIZE(renderLabels));
    ImGui::Checkbox("Enable mesh-gaussian depth test (improves rendering performance)", &enableDepthTest);

    //TODO: right now std_dev is not updated in the actual gaussianBuffer, just during rendering. Need to consider this when exporting
    if (ImGui::SliderFloat("Gaussian Scale", &gaussian_std, minStd, maxStd, "%.2f"));

    ImGui::SeparatorText("Sampling density settings");

    if (ImGui::SliderFloat("Sampling density", &quality, 0.0f, 1.0f, "%.2f")) {
        runConversionFlag = true;
    }
    if (ImGui::Combo("(Max quality tweak)", &resolutionIndex, resolutionLabels, IM_ARRAYSIZE(resolutionLabels)))
    {
        maxRes = resolutionOptions[resolutionIndex];
        runConversionFlag = true;
    }

    ImGui::Dummy(ImVec2(0, 2.0f));
    ImGui::SeparatorText("##");
    ImGui::Dummy(ImVec2(0, 1.0f));


    ImGui::ColorEdit4("Background Color", &sceneBackgroundColor.x);

    ImGui::End();
}


void ImGuiUI::renderUI()
{
    renderFileSelectorWindow();
    renderPropertiesWindow();
    renderGpuFrametime();
    renderLightingSettings();
    renderBatchWindow();
}

void ImGuiUI::renderGizmoUi(glm::mat4& glmViewMat, glm::mat4& glmProjMat, glm::mat4& glmModelMat)
{
    ImGui::SetNextWindowPos(ImVec2(100, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("Gizmo Control");

    static ImGuizmo::OPERATION currentOperation = ImGuizmo::TRANSLATE;
    static ImGuizmo::MODE currentMode = ImGuizmo::LOCAL;

    ImGui::SeparatorText("Operation");
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
        ImGui::SeparatorText("Reference System");
        if (ImGui::RadioButton("Local", currentMode == ImGuizmo::LOCAL))
            currentMode = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if (ImGui::RadioButton("World", currentMode == ImGuizmo::WORLD))
            currentMode = ImGuizmo::WORLD;
    }

    if (isLightingEnabled())
    {
        ImGui::SeparatorText("Object selector");
        if (ImGui::RadioButton("Model", !lightSelected))
            lightSelected = false;
        ImGui::SameLine();
        if (ImGui::RadioButton("Light", lightSelected) && lightingEnabled)
            lightSelected = true;
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

}

void ImGuiUI::renderGpuFrametime()
{
    ImGui::SetNextWindowPos(ImVec2(20, 350), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 240), ImGuiCond_FirstUseEver);

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

void ImGuiUI::renderLightingSettings()
{
    ImGui::SetNextWindowPos(ImVec2(500, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 80), ImGuiCond_FirstUseEver);

    ImGui::Begin("Lighting");

    ImGui::Checkbox("Enable lighting", &lightingEnabled);
    
    if (lightingEnabled)
    {
        ImGui::SliderFloat("Light intensity", &lightIntensity, minLightIntensity, maxLightIntensity, "%2.0f");
        ImGui::ColorEdit3("Light Color", &lightColor.x);
    } else lightSelected = false;

    ImGui::End();
}

void ImGuiUI::renderBatchWindow()
{
    ImGui::SetNextWindowPos(ImVec2(20, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800, 320), ImGuiCond_FirstUseEver);
    ImGui::Begin("Batch Processing");

    // Folder chooser
    if (ImGui::Button("Select input folder")) {
        IGFD::FileDialogConfig cfg; cfg.path = ".";
        ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiCond_Always);
        ImGuiFileDialog::Instance()->OpenDialog("BatchFolderKey", "Choose Input Folder", nullptr, cfg);
    }

    ImGui::SameLine();
    ImGui::Checkbox("Include subfolders", &batchIncludeSubfolders);

    // Handle folder selection
    if (ImGuiFileDialog::Instance()->Display("BatchFolderKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string dir = ImGuiFileDialog::Instance()->GetCurrentPath();
            enqueueFolder(dir);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // Controls
    ImGui::Separator();
    if (!batchRunning) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f,0.60f,0.20f,1.0f));
        if (ImGui::Button("Start batch")) {
            batchRunning = !batchItems.empty();
            batchCancelRequested = false;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Clear list")) {
            batchItems.clear();
            batchProgress01 = 0.f;
            batchSelectedRow = -1;
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f,0.20f,0.20f,1.0f));
        if (ImGui::Button("Cancel")) {
            batchCancelRequested = true;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextUnformatted("(Running… conversions are dispatched by app loop)");
    }

    // Progress
    // Compute live progress = finished / total
    int done = 0, failed = 0, processing = 0;
    for (const auto& it : batchItems) {
        if (it.status == BatchItem::Status::Done) ++done;
        else if (it.status == BatchItem::Status::Failed) ++failed;
        else if (it.status == BatchItem::Status::Processing) ++processing;
    }
    int total = static_cast<int>(batchItems.size());
    float progress = total > 0 ? float(done + failed) / float(total) : 0.f;
    batchProgress01 = progress;

    ImGui::ProgressBar(batchProgress01, ImVec2(-1, 0),
        total ? (std::string(" ") + std::to_string(done) + "/" + std::to_string(total) + " done").c_str()
              : "0/0");

    // Table
    ImGui::Separator();
    if (ImGui::BeginTable("batch_table", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("File");
        ImGui::TableSetupColumn("Parent");
        ImGui::TableSetupColumn("Ext");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Output");
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)batchItems.size(); ++i) {
            auto& it = batchItems[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(std::filesystem::path(it.path).filename().string().c_str(), batchSelectedRow == i, ImGuiSelectableFlags_SpanAllColumns))
                batchSelectedRow = i;
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(it.parent.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(utils::modelFileExtensionEnumToString(it.ext).c_str()); // add toString helper if you don't have one
            ImGui::TableSetColumnIndex(3);
            {
                const char* s = it.status == BatchItem::Status::Queued ? "Queued" :
                                it.status == BatchItem::Status::Processing ? "Processing" :
                                it.status == BatchItem::Status::Done ? "Done" : "Failed";
                if (it.status == BatchItem::Status::Failed && !it.error.empty()) {
                    ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "Failed");
                    if (ImGui::IsItemHovered() && !it.error.empty())
                        ImGui::SetTooltip("%s", it.error.c_str());
                } else {
                    ImGui::TextUnformatted(s);
                }
            }
            ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted(it.outPath.c_str());
        }
        ImGui::EndTable();
    }

    // Row actions
    if (batchSelectedRow >= 0 && batchSelectedRow < (int)batchItems.size()) {
        ImGui::Separator();
        if (ImGui::Button("Remove selected")) {
            batchItems.erase(batchItems.begin() + batchSelectedRow);
            batchSelectedRow = -1;
        }
    }

    ImGui::End();
}


void ImGuiUI::preframe()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiUI::displayGaussianCounts(unsigned int gaussianCount, unsigned int visibleGaussianCount)
{
    ImGui::SetNextWindowPos(ImVec2(20, 250), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 80), ImGuiCond_FirstUseEver);
    ImGui::Begin("Object info");
    ImGui::Text("Total gaussian count: %s", utils::formatWithCommas(gaussianCount).c_str());
    ImGui::Text("Visible gaussian count: %s", utils::formatWithCommas(visibleGaussianCount).c_str());
    ImGui::End();
}


void ImGuiUI::postframe()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


bool ImGuiUI::shouldRunConversion() const { return runConversionFlag; } ;
bool ImGuiUI::shouldLoadNewMesh() const { return loadNewMesh; } ;
bool ImGuiUI::shouldBatchLoadNewMeshes() const { return batchLoadNewMeshes; } ;
bool ImGuiUI::wasMeshLoaded() const { return hasMeshBeenLoaded; } ;
bool ImGuiUI::shouldLoadPly() const { return loadNewPly; } ;
bool ImGuiUI::wasPlyLoaded() const { return hasPlyBeenLoaded; };

bool ImGuiUI::shouldSavePly() const { return savePly; } ;
std::string ImGuiUI::getMeshFilePath() const { return meshFilePath; };
std::string ImGuiUI::getMeshFilePathParentFolder() const {return meshParentFolder;};
std::string ImGuiUI::getMeshFullFilePathDestination() const {
    
    if (utils::getFileExtension(std::string(outputFilename)) == utils::ModelFileExtension::NONE)
    {
        return destinationFilePathFolder + "/" + std::string(outputFilename) + ".ply";
    }
    else if (utils::getFileExtension(std::string(outputFilename)) == utils::ModelFileExtension::PLY)
    {
        return destinationFilePathFolder + "/" + std::string(outputFilename);
    }
};

std::string ImGuiUI::getPlyFilePath() const { return std::string(plyFilePath); };
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
    this->gpuFrameTime = static_cast<float>(gpuFrameTime);
    
    // Rolling buffer as vector
    if(frameTimeHistory.size() >= MAX_FRAME_HISTORY) {
        frameTimeHistory.erase(frameTimeHistory.begin());
    }

    frameTimeHistory.push_back(this->gpuFrameTime);
}

bool ImGuiUI::isLightSelected() const { return lightSelected; };
bool ImGuiUI::isLightingEnabled() const { return lightingEnabled; };
float ImGuiUI::getLightIntensity() const { return lightIntensity; };
glm::vec3 ImGuiUI::getLightColor() const { return lightColor; };

void ImGuiUI::setEnableDepthTest(bool depthTest) { enableDepthTest = depthTest; }
bool ImGuiUI::getIsDepthTestEnabled() const { return enableDepthTest; }

//TODO: batching utility code, I think refactor is needed to move batching logic to separate file, for later refactor pass
void ImGuiUI::enqueueFolder(const std::string& dir)
{
    namespace fs = std::filesystem;
    if (!fs::exists(dir)) return;

    auto addEntry = [&](const fs::directory_entry& e){
        if (!isSupportedMesh(e)) return;
        BatchItem bi;
        bi.path   = e.path().string();
        bi.parent = e.path().parent_path().string();
        bi.ext    = extFromPath(bi.path);

        // reuse destinationFilePathFolder + outputFilename + format
        // If user hasn't chosen an explicit filename, auto-derive from input stem.
        std::string stem = e.path().stem().string();
        const bool explicitName = std::string(outputFilename).size() > 0;
        const bool hasExt = utils::getFileExtension(std::string(outputFilename)) != utils::ModelFileExtension::NONE;
        std::string finalName = explicitName ? std::string(outputFilename) : (stem + ".ply");
        if (!hasExt && explicitName) finalName += ".ply";

        const std::filesystem::path inputPath = e.path();
        const std::string inputStem = inputPath.stem().string();

        // Batch output dir: if user picked one, use it; else (fallback) the input’s parent
        std::filesystem::path outDir = destinationFilePathFolder.empty()
            ? inputPath.parent_path()
            : std::filesystem::path(destinationFilePathFolder);

        // IMPORTANT: always per-row filename from input stem
        std::filesystem::path outPath = outDir / (inputStem + ".ply");

        bi.outPath = utils::makeUniquePath(outPath);

        batchItems.push_back(std::move(bi));
    };

    if (batchIncludeSubfolders) {
        for (auto& e : fs::recursive_directory_iterator(dir)) addEntry(e);
    } else {
        for (auto& e : fs::directory_iterator(dir)) addEntry(e);
    }
}

bool ImGuiUI::hasBatchWork() const
{
    if (batchCancelRequested) return false;
    for (const auto& it : batchItems)
        if (it.status == BatchItem::Status::Queued) return true;
    return false;
}

bool ImGuiUI::isBatchRunning() const { return batchRunning && !batchCancelRequested; }

ImGuiUI::BatchItem* ImGuiUI::popNextBatchItem()
{
    if (batchCancelRequested) return nullptr;
    for (auto& it : batchItems) {
        if (it.status == BatchItem::Status::Queued) {
            it.status = BatchItem::Status::Processing;
            return &it; // return pointer to live storage
        }
    }
    // Nothing left
    if (batchRunning) batchRunning = false;
    return nullptr;
}

void ImGuiUI::markBatchItemDone(const std::string& path)
{
    //TODO: use a map for O(1)
    for (auto& it : batchItems) {
        if (it.path == path && it.status == BatchItem::Status::Processing) {
            it.status = BatchItem::Status::Done;
            break;
        }
    }
}

void ImGuiUI::markBatchItemFailed(const std::string& path, const std::string& err)
{
    for (auto& it : batchItems) {
        if (it.path == path && it.status == BatchItem::Status::Processing) {
            it.status = BatchItem::Status::Failed;
            it.error = err;
            break;
        }
    }
}

void ImGuiUI::cancelBatch()
{
    batchCancelRequested = true;
    batchRunning = false; // UI no longer dispatches
}


const std::vector<ImGuiUI::BatchItem>& ImGuiUI::getBatchItems() const { return batchItems; }



