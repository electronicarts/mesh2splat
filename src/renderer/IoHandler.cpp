///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "IoHandler.hpp"
#include <imgui.h>

bool IoHandler::mouseDragging = false;
bool IoHandler::firstMouse = true;
bool IoHandler::keys[1024] = { false };


IoHandler::IoHandler(GLFWwindow* window, Camera& cameraInstance)
    : window(window), camera(&cameraInstance) {
    setupCallbacks();
}

void IoHandler::setupCallbacks() {
    glfwSetWindowUserPointer(window, this);
}


void IoHandler::processInput(float deltaTime)
{
    bool forward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool backward = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool left = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
    bool right = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
    bool upMove = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
    bool downMove = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
    bool rotateLeftFrontVect = glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
    bool rotateRightFrontVect = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    bool boostSpeed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    bool slowSpeed = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;


    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard)
    {
        camera->ProcessKeyboard(deltaTime, forward, backward, left, right,
                               upMove, downMove,
                               rotateLeftFrontVect, rotateRightFrontVect, boostSpeed, slowSpeed
        );
    }

    if (!io.WantCaptureMouse)
    {
        bool rightMouseDown = io.MouseDown[1];
        if (rightMouseDown)
        {
            float xoffset = io.MouseDelta.x;
            float yoffset = -io.MouseDelta.y; // invert if you prefer typical "FPS" style

            camera->ProcessMouseMovement(xoffset, yoffset);
        }

        float scrollY = io.MouseWheel;
        if (scrollY != 0.0f)
        {
            camera->ProcessMouseScroll(scrollY);
        }
    }

}

