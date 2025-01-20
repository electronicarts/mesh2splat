#pragma once
#include "../utils/utils.hpp"

extern float yaw;
extern float pitch;
extern float lastX;
extern float lastY;
extern bool firstMouse;
extern float cameraRadius;


void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow * window, double xoffset, double yoffset);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);

glm::vec3 computeCameraPosition();

GLuint compute_shader_dispatch(GLuint computeShaderProgram, GLuint* drawBuffers, GLuint gaussianBuffer, unsigned int resolutionTarget);

void render_point_cloud(GLFWwindow* window, GLuint pointsVAO, GLuint gaussianBuffer, GLuint drawIndirectBuffer, GLuint renderShaderProgram, float std_gauss);

