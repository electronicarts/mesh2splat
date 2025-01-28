#include "IoHandler.hpp"
#include "renderer.hpp"

//TODO: yeah, these are globals for now :D
float yaw = -90.0f, pitch = 0.0f;
double lastMouseX = 320.0f, lastMouseY = 240.0f;  
bool firstMouse = true;
float cameraRadius = 5.0f; 
float distance = 5.0f;
bool mouseDraggingForRotation;
bool mouseDraggingForPanning;
float panSpeed = 0.01;
glm::vec3 cameraTarget = glm::vec3(0,0,0);

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                mouseDraggingForRotation = true;
                glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
            } else if (action == GLFW_RELEASE) {
                mouseDraggingForRotation = false;
            }
        }
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                mouseDraggingForPanning = true;
                glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
            } else if (action == GLFW_RELEASE) {
                mouseDraggingForPanning = false;
            }
        }
    }
}

// Cursor position callback to update yaw and pitch during dragging
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        if (mouseDraggingForRotation) {
            double dx = xpos - lastMouseX;
            double dy = ypos - lastMouseY;

            // Sensitivity factors for smoother control
            float sensitivity = 0.1f;
            yaw += static_cast<float>(dx) * sensitivity;
            pitch += static_cast<float>(dy) * sensitivity;

            // Clamp pitch to avoid flipping the camera
            if (pitch > 89.0f)  pitch = 89.0f;
            if (pitch < -89.0f) pitch = -89.0f;

            lastMouseX = xpos;
            lastMouseY = ypos;
        }

        if (mouseDraggingForPanning)
        {
            double currentRightMouseX = xpos;
            double currentRightMouseY = ypos;


            double dx = currentRightMouseX - lastMouseX;
            double dy = currentRightMouseY - lastMouseY;

            // Calculate camera axes for panning
            glm::vec3 camPos = Renderer::computeCameraPosition();
            glm::vec3 front = glm::normalize(cameraTarget - camPos);
            glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
            glm::vec3 cameraRight = glm::normalize(glm::cross(front, worldUp));
            glm::vec3 cameraUp = glm::normalize(glm::cross(cameraRight, front));

            cameraTarget += -static_cast<float>(dx) * panSpeed * cameraRight
                + static_cast<float>(dy) * panSpeed * cameraUp;

            lastMouseX = currentRightMouseX;
            lastMouseY = currentRightMouseY;
        }
    }
}

// Scroll callback to adjust camera distance (zoom)
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    float zoomSpeed = 0.15f;
    distance -= static_cast<float>(yoffset) * zoomSpeed;
    
    // Clamp distance to avoid extreme zoom
    if(distance < 0.1f)  distance = 0.1f;
    if(distance > 250.0f) distance = 250.0f;
}