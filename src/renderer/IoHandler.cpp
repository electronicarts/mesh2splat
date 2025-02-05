#include "IoHandler.hpp"
#include <imgui.h>

bool IoHandler::mouseDragging = false;
bool IoHandler::firstMouse = true;
double IoHandler::lastX = 0.0;
double IoHandler::lastY = 0.0;
bool IoHandler::keys[1024] = { false };


IoHandler::IoHandler(GLFWwindow* window, Camera& cameraInstance)
    : window(window), camera(&cameraInstance) {
    setupCallbacks();
}

void IoHandler::setupCallbacks() {
    glfwSetWindowUserPointer(window, this);
    glfwSetMouseButtonCallback(window, IoHandler::mouseButtonCallback);
    glfwSetCursorPosCallback(window, IoHandler::cursorPositionCallback);
    glfwSetScrollCallback(window, IoHandler::scrollCallback);
    glfwSetKeyCallback(window, IoHandler::keyCallback);
}

void IoHandler::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    IoHandler* handler = static_cast<IoHandler*>(glfwGetWindowUserPointer(window));

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                mouseDragging = true;
                glfwGetCursorPos(window, &lastX, &lastY);
            }
            else if (action == GLFW_RELEASE) {
                mouseDragging = false;
                firstMouse = true;
            }
        }
    }
}

void IoHandler::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
    IoHandler* handler = static_cast<IoHandler*>(glfwGetWindowUserPointer(window));

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse && mouseDragging) {
        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = static_cast<float>(xpos - lastX);
        float yoffset = static_cast<float>(lastY - ypos); 

        lastX = xpos;
        lastY = ypos;

        handler->camera->ProcessMouseMovement(xoffset, yoffset);
    }
}

void IoHandler::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    IoHandler* handler = static_cast<IoHandler*>(glfwGetWindowUserPointer(window));

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        handler->camera->ProcessMouseScroll(static_cast<float>(yoffset));
    }
}

void IoHandler::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS)
        keys[key] = true;
    else if (action == GLFW_RELEASE)
        keys[key] = false;
}

void IoHandler::processInput(float deltaTime) {

    bool forward = keys[GLFW_KEY_W];
    bool backward = keys[GLFW_KEY_S];
    bool left = keys[GLFW_KEY_A];
    bool right = keys[GLFW_KEY_D];
    bool upMove = keys[GLFW_KEY_E];
    bool downMove = keys[GLFW_KEY_Q];
    bool rotateLeftFrontVect = keys[GLFW_KEY_T];
    bool rotateRightFrontVect = keys[GLFW_KEY_R];


    camera->ProcessKeyboard(deltaTime, forward, backward, left, right, upMove, downMove, rotateLeftFrontVect, rotateRightFrontVect);
}
