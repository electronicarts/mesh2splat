///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "GuiRendererConcreteMediator.hpp"

void GuiRendererConcreteMediator::notify(EventType event)
{
    switch (event) {
        case EventType::LoadModel: {
            renderer.resetModelMatrices();
            renderer.getSceneManager().loadModel(imguiUI.getMeshFilePath(), imguiUI.getMeshFilePathParentFolder());
            renderer.gaussianBufferFromSize(imguiUI.getResolutionTarget() * imguiUI.getResolutionTarget());
            renderer.setFormatType(0); //TODO: use an enum
            renderer.setViewportResolutionForConversion(imguiUI.getResolutionTarget());
            renderer.enableRenderPass(conversionPassName);
            renderer.enableRenderPass(gaussiansPrePassName);
            renderer.enableRenderPass(radixSortPassName);
            renderer.enableRenderPass(gaussianSplattingPassName);

            imguiUI.setLoadNewMesh(false);
            imguiUI.setMeshLoaded(true);
            
            imguiUI.setPlyLoaded(false); //need to reset this
            break;
        }
        case EventType::LoadPly: {
            if (renderer.getSceneManager().loadPly(imguiUI.getPlyFilePath()))
            {
                renderer.resetModelMatrices();
                renderer.updateGaussianBuffer();
                renderer.setFormatType(1); //TODO: use an enum
                renderer.enableRenderPass(gaussiansPrePassName);
                renderer.enableRenderPass(radixSortPassName);
                renderer.enableRenderPass(gaussianSplattingPassName);
                renderer.enableRenderPass(gaussianSplattingRelightingPassName);

                renderer.resetRendererViewportResolution();

                imguiUI.setLoadNewPly(false);
                imguiUI.setPlyLoaded(true);
                

                imguiUI.setMeshLoaded(false); //need to reset this
            }
            break;
        }
        case EventType::RunConversion: {
            renderer.enableRenderPass(conversionPassName);
            renderer.setViewportResolutionForConversion(imguiUI.getResolutionTarget());
            imguiUI.setRunConversion(false);
            
            break;
        }
        case EventType::EnableGaussianRendering: {
            renderer.resetRendererViewportResolution();
            renderer.setStdDevFromImGui(imguiUI.getGaussianStd());
            renderer.setRenderMode(imguiUI.selectedRenderMode());
            renderer.enableRenderPass(gaussiansPrePassName);
            renderer.enableRenderPass(radixSortPassName);
            renderer.enableRenderPass(gaussianSplattingPassName);
        
            renderer.setLightingEnabled(imguiUI.isLightingEnabled());

            if (imguiUI.isLightingEnabled())
            {
                renderer.enableRenderPass(gaussianSplattingShadowsPassName);
                renderer.setLightIntensity(imguiUI.getLightIntensity());
                renderer.setLightColor(imguiUI.getLightColor());
            }

            if (renderer.getRenderContext()->format == 0 && imguiUI.getIsDepthTestEnabled())
            {
                renderer.enableRenderPass(depthPrepassName);
                renderer.setDepthTestEnabled(true);
            }
            else
            {
                renderer.setDepthTestEnabled(false);
            }
                        
            renderer.enableRenderPass(gaussianSplattingRelightingPassName);

            glm::mat4& modelM = imguiUI.isLightSelected() ? renderer.getRenderContext()->pointLightData.pointLightModel : renderer.getRenderContext()->modelMat;

            imguiUI.renderGizmoUi(
                renderer.getRenderContext()->viewMat,
                renderer.getRenderContext()->projMat,
                modelM
            );

            break;
        }
        case EventType::CheckShaderUpdate: {
            renderer.updateShadersIfNeeded();
            renderer.setLastShaderCheckTime(glfwGetTime());
            
            break;
        }
        case EventType::SavePLY: {
            renderer.getSceneManager().exportPly(imguiUI.getMeshFullFilePathDestination(), imguiUI.getFormatOption());
            imguiUI.setShouldSavePly(false);
            break;
        }
        case EventType::ResizedWindow: {

            renderer.deleteDepthTexture();
            renderer.createDepthTexture();
            renderer.deleteGBuffer();
            renderer.createGBuffer();

            break;
        }
        case EventType::UpdateTransforms: {
            renderer.updateTransformations();
            break;
        }
    }
}

void GuiRendererConcreteMediator::update()
{
    double currentTime = glfwGetTime();
    if (currentTime - renderer.getLastShaderCheckTime() > 1.0) {
        notify(EventType::CheckShaderUpdate);
    }

    bool windowVisible = !renderer.isWindowMinimized();

    const bool batchActive      = imguiUI.isBatchRunning();
    const bool batchHasWork     = (currentJob != nullptr) || imguiUI.hasBatchWork();

    if (windowVisible && batchActive) {
        // If batch says it's running but there's nothing to do, end it now.
        if (!batchHasWork) {
            imguiUI.cancelBatch();
            // reset mediator-side state too
            currentJob = nullptr;
            batchSubstate = BatchSubstate::Idle;
        } else {
            // === Normal batch flow ===
            switch (batchSubstate) {
                case BatchSubstate::Idle: {
                    if (imguiUI.hasBatchWork()) {
                        if (ImGuiUI::BatchItem* job = imguiUI.popNextBatchItem()) {
                            startBatchJob(job, imguiUI);
                        }
                    }
                    break;
                }
                case BatchSubstate::Converting: {
                    ++framesSinceDispatch;
                    if (framesSinceDispatch >= 1) { // bump to 2 if you ever see racey exports
                        batchSubstate = BatchSubstate::Exporting;
                    }
                    break;
                }
                case BatchSubstate::Exporting: {
                    try {
                        const unsigned int fmt = imguiUI.getFormatOption();
                        renderer.getSceneManager().exportPly(currentJob->outPath, fmt);
                        finishBatchJobSuccess(imguiUI);
                    } catch (const std::exception& e) {
                        finishBatchJobFail(imguiUI, e.what());
                    } catch (...) {
                        finishBatchJobFail(imguiUI, "Unknown error");
                    }
                    break;
                }
                case BatchSubstate::Loading:
                    break;
            }

            // keep transforms valid, skip visualization
            if (renderer.hasWindowSizeChanged()) notify(EventType::ResizedWindow);
            notify(EventType::UpdateTransforms);

            // only EARLY-RETURN if batch is truly active (work still pending)
            double gpuFrameTime = renderer.getTotalGpuFrameTimeMs();
            imguiUI.setFrameMetrics(gpuFrameTime);
            return;
        }
    }

    if(windowVisible && !batchActive)
    { 
        if (renderer.hasWindowSizeChanged())
        {
            notify(EventType::ResizedWindow);
        }

        if (imguiUI.shouldLoadNewMesh() && !imguiUI.getMeshFilePath().empty()) {
            notify(EventType::LoadModel);
        }

        if (imguiUI.shouldLoadPly() && !imguiUI.getPlyFilePath().empty()) {
            notify(EventType::LoadPly);
        }

        if (imguiUI.wasMeshLoaded() || imguiUI.wasPlyLoaded()) {
            notify(EventType::EnableGaussianRendering);
        }

        if (imguiUI.shouldRunConversion() && imguiUI.wasMeshLoaded()) {
            notify(EventType::RunConversion);
        }

        if (imguiUI.shouldSavePly()) {
            notify(EventType::SavePLY);
        }

        notify(EventType::UpdateTransforms);
    }
    
    double gpuFrameTime = renderer.getTotalGpuFrameTimeMs(); // Retrieve GPU frame time
    imguiUI.setFrameMetrics(gpuFrameTime);
}

//TODO: as you can see batchItem should NOT be part of the ImGuiUI, this is poor SWE

static bool isGlb(utils::ModelFileExtension e) { return e == utils::ModelFileExtension::GLB; }
static bool isPly(utils::ModelFileExtension e) { return e == utils::ModelFileExtension::PLY; }

void GuiRendererConcreteMediator::startBatchJob(ImGuiUI::BatchItem* job, ImGuiUI& ui) {
    currentJob = job;
    framesSinceDispatch = 0;
    batchSubstate = BatchSubstate::Loading;

    renderer.resetModelMatrices();
    renderer.setFormatType(0); 

    if (isGlb(job->ext)) {
        renderer.getSceneManager().loadModel(job->path, job->parent);
        renderer.gaussianBufferFromSize(ui.getResolutionTarget() * ui.getResolutionTarget());
        renderer.setViewportResolutionForConversion(ui.getResolutionTarget());
        renderer.enableRenderPass(conversionPassName);
        batchSubstate = BatchSubstate::Converting;
    } else {
        finishBatchJobFail(ui, "Unsupported extension");
    }
}


void GuiRendererConcreteMediator::finishBatchJobSuccess(ImGuiUI& ui) {
    ui.markBatchItemDone(currentJob->path);
    currentJob = nullptr;
    batchSubstate = BatchSubstate::Idle;
}

void GuiRendererConcreteMediator::finishBatchJobFail(ImGuiUI& ui, const std::string& what) {
    ui.markBatchItemFailed(currentJob->path, what);
    currentJob = nullptr;
    batchSubstate = BatchSubstate::Idle;
}
