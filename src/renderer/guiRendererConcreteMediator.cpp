#include "GuiRendererConcreteMediator.hpp"

void GuiRendererConcreteMediator::notify(EventType event)
{
    switch (event) {
        case EventType::LoadModel: {
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
            
            break;
        }
        case EventType::LoadPly: {
            if (renderer.getSceneManager().loadPly(imguiUI.getPlyFilePath()))
            {
                renderer.updateGaussianBuffer();
                renderer.setFormatType(1); //TODO: use an enum
                renderer.enableRenderPass(gaussiansPrePassName);
                renderer.enableRenderPass(radixSortPassName);
                renderer.enableRenderPass(gaussianSplattingPassName);
                renderer.resetRendererViewportResolution();

                imguiUI.setLoadNewPly(false);
                imguiUI.setPlyLoaded(true);
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
            
            break;
        }
        case EventType::CheckShaderUpdate: {
            renderer.updateShadersIfNeeded();
            renderer.setLastShaderCheckTime(glfwGetTime());
            
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

    if (imguiUI.shouldLoadNewMesh() && !imguiUI.getMeshFilePath().empty()) {
        notify(EventType::LoadModel);
    }

    if (imguiUI.shouldLoadPly() && !imguiUI.getPlyFilePath().empty()) {
        notify(EventType::LoadPly);
    }

    if (true) {
        notify(EventType::RunConversion);
    }

    if (imguiUI.wasMeshLoaded() || imguiUI.wasPlyLoaded()) {
        notify(EventType::EnableGaussianRendering);
    }

    double gpuFrameTime = renderer.getTotalGpuFrameTimeMs(); // Retrieve GPU frame time
    imguiUI.setFrameMetrics(gpuFrameTime);
}
