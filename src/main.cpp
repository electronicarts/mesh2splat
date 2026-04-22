///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "utils/normalizedUvUnwrapping.hpp"
#include "renderer/renderer.hpp"
#include "glewGlfwHandlers/glewGlfwHandler.hpp"
#include "renderer/guiRendererConcreteMediator.hpp"
#include <cfloat>

int main(int argc, char** argv) {
    GlewGlfwHandler glewGlfwHandler(glm::ivec2(1080, 720), "Mesh2Splat");
    
    Camera camera(
        glm::vec3(0.0f, 0.0f, 5.0f), 
        glm::vec3(0.0f, 1.0f, 0.0f), 
        -90.0f, 
        0.0f
    );  

    IoHandler ioHandler(glewGlfwHandler.getWindow(), camera);
    if(glewGlfwHandler.init() == -1) return -1;

    ioHandler.setupCallbacks();

    ImGuiUI ImGuiUI(0.65f, 0.5f); //TODO: give a meaning to these params
    ImGuiUI.initialize(glewGlfwHandler.getWindow());

    Renderer renderer(glewGlfwHandler.getWindow(), camera);
    renderer.initialize();
    GuiRendererConcreteMediator guiRendererMediator(renderer, ImGuiUI);
    
    // Set up F key to frame object based on scene bbox
    ioHandler.setFrameObjectCallback([&camera, &renderer]() {
        auto* ctx = renderer.getRenderContext();
        if (ctx->dataMeshAndGlMesh.empty()) return;
        
        // Compute combined bbox of all meshes
        glm::vec3 bboxMin(FLT_MAX), bboxMax(-FLT_MAX);
        for (auto& mesh : ctx->dataMeshAndGlMesh) {
            bboxMin = glm::min(bboxMin, mesh.first.bbox.min);
            bboxMax = glm::max(bboxMax, mesh.first.bbox.max);
        }
        camera.FrameObject(bboxMin, bboxMax);
    });

    float deltaTime = 0.0f; 
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(glewGlfwHandler.getWindow())) {
        
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();
        
        ioHandler.processInput(deltaTime);

        renderer.clearingPrePass(ImGuiUI.getSceneBackgroundColor());

        ImGuiUI.preframe();
        ImGuiUI.renderUI();
        
        guiRendererMediator.update();

        renderer.renderFrame();

        ImGuiUI.displayGaussianCounts(renderer.getTotalGaussianCount(), renderer.getVisibleGaussianCount());
        ImGuiUI.postframe();

        glfwSwapBuffers(glewGlfwHandler.getWindow());
    }

    glfwTerminate();

    return 0;
}

