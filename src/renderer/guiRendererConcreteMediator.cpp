#include "GuiRendererConcreteMediator.hpp"

void GuiRendererConcreteMediator::notify(EventType event)
{
    switch (event) {
        case EventType::LoadModel: {
            renderer.getSceneManager().loadModel(imguiUI.getFilePath(), imguiUI.getFilePathParentFolder());
            renderer.setViewportResolutionForConversion(imguiUI.getResolutionTarget());
            renderer.enableRenderPass("conversion");
            imguiUI.setLoadNewMesh(false);
            imguiUI.setMeshLoaded(true);
            break;
        }
        case EventType::RunConversion: {
            renderer.enableRenderPass("conversion");
            renderer.setViewportResolutionForConversion(imguiUI.getResolutionTarget());
            imguiUI.setRunConversion(false);
            break;
        }
        case EventType::EnableGaussianRendering: {
            renderer.resetRendererViewportResolution();
            renderer.setStdDevFromImGui(imguiUI.getGaussianStd());
            renderer.enableRenderPass("radixSort");
            renderer.enableRenderPass("gaussianSplatting");
            break;
        }
        case EventType::CheckShaderUpdate: {
            renderer.updateShadersIfNeeded();
            renderer.setLastShaderCheckTime(glfwGetTime());
        }
    }
}

void GuiRendererConcreteMediator::update()
{
    double currentTime = glfwGetTime();
    if (currentTime - renderer.getLastShaderCheckTime() > 1.0) {
        notify(EventType::CheckShaderUpdate);
    }

    if (imguiUI.shouldLoadNewMesh() && !imguiUI.getFilePath().empty()) {
        notify(EventType::LoadModel);
    }

    if (imguiUI.shouldRunConversion()) {
        notify(EventType::RunConversion);
    }

    if (imguiUI.wasMeshLoaded()) {
        notify(EventType::EnableGaussianRendering);
    }


    double gpuFrameTime = renderer.getTotalGpuFrameTimeMs(); // Retrieve GPU frame time
    imguiUI.setFrameMetrics(gpuFrameTime);
}
