///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "utils/Camera.hpp"
#include <functional>

class IoHandler {
public:
    IoHandler(GLFWwindow* window, Camera& camera);
    void setupCallbacks();
    void processInput(float deltaTime);
    
    // Set callback for frame object (F key) - provides bbox min/max
    void setFrameObjectCallback(std::function<void()> callback) { frameObjectCallback = callback; }

private:
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    Camera* camera; 
    GLFWwindow* window;
    
    std::function<void()> frameObjectCallback;

    // Internal state
    static bool mouseDragging;
    static bool firstMouse;
    static bool keys[1024];
};
