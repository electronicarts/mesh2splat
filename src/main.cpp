#include "./utils/normalizedUvUnwrapping.hpp"
#include "renderer/renderer.hpp"
#include "glewGlfwHandlers/glewGlfwHandler.hpp"
#include "renderer/guiRendererConcreteMediator.hpp"

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

    ImGuiUI ImGuiUI(3, 0, 0.1f, 0.5f); //TODO: give a meaning to these params
    ImGuiUI.initialize(glewGlfwHandler.getWindow());

    Renderer renderer(glewGlfwHandler.getWindow(), camera);
    renderer.initialize();
    GuiRendererConcreteMediator guiRendererMediator(renderer, ImGuiUI);

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

        ImGuiUI.renderGizmoUi(
            renderer.getRenderContext()->viewMat,
            renderer.getRenderContext()->projMat,
            renderer.getRenderContext()->modelMat
        );

        renderer.updateTransformations();

        renderer.renderFrame();

        ImGuiUI.displayGaussianCount(renderer.getVisibleGaussianCount());
        ImGuiUI.postframe();

        glfwSwapBuffers(glewGlfwHandler.getWindow());
    }

    glfwTerminate();

    return 0;
}

