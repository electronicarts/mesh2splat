#pragma once
#include "../utils/Camera.hpp"

class IoHandler {
public:
    IoHandler(GLFWwindow* window, Camera& camera);
    void setupCallbacks();
    void processInput(float deltaTime);

private:
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    Camera* camera; 
    GLFWwindow* window;

    // Internal state
    static bool mouseDragging;
    static bool firstMouse;
    static bool keys[1024];
};
