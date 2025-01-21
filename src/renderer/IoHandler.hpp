#pragma once
#include "../utils/utils.hpp"

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

extern float yaw, pitch;
extern double lastMouseX, lastMouseY;  
extern bool firstMouse;
extern float cameraRadius; 
extern float distance;
extern bool mouseDraggingForRotation;
extern bool mouseDraggingForPanning;
extern float panSpeed;
extern glm::vec3 cameraTarget;