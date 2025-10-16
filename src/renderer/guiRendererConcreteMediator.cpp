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
            if (renderer.getSceneManager().loadPly(imguiUI.getMeshFilePath()))
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
        case EventType::ExportSplats: {
            renderer.getSceneManager().exportSplats(imguiUI.getMeshFullFilePathDestination(), imguiUI.getFormatOption());
            imguiUI.setShouldExportSplats(false);
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

    if(!renderer.isWindowMinimized())
    { 
        if (renderer.hasWindowSizeChanged())
        {
            notify(EventType::ResizedWindow);
        }

        if (imguiUI.shouldLoadNewMesh() && !imguiUI.getMeshFilePath().empty()) {
            notify(EventType::LoadModel);
        }

        if (imguiUI.shouldLoadPly() && !imguiUI.getMeshFilePath().empty()) {
            notify(EventType::LoadPly);
        }

        if (imguiUI.wasMeshLoaded() || imguiUI.wasPlyLoaded()) {
            notify(EventType::EnableGaussianRendering);
        }

        if (imguiUI.shouldRunConversion() && imguiUI.wasMeshLoaded()) {
            notify(EventType::RunConversion);
        }

        if (imguiUI.shouldExportSplats()) {
            notify(EventType::ExportSplats);
        }

        notify(EventType::UpdateTransforms);
    }
    
    double gpuFrameTime = renderer.getTotalGpuFrameTimeMs(); // Retrieve GPU frame time
    imguiUI.setFrameMetrics(gpuFrameTime);
}
