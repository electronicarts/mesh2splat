#include <string>
#include <vector>
#include <tuple>
#include <math.h>
#include <glm.hpp>
#include <set>
#include <stdint.h>
#include <chrono>

#include "parsers.hpp"
#include "utils/gaussianShapesUtilities.hpp"
#include "gaussianComputations.hpp"
#include "utils/shaderUtils.hpp"
#include "utils/normalizedUvUnwrapping.hpp"
#include "utils/argparser.hpp"
#include "renderer/renderer.hpp"
#include "renderer/mesh2SplatHandler.hpp"
#include "renderer/IoHandler.hpp"
#include "imGuiUi/ImGuiUi.hpp"


int main(int argc, char** argv) {
    if (!glfwInit())
        return -1;

    GLFWwindow* window = glfwCreateWindow(1080, 720, "Mesh2Splat", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Initialize GLEW
    glewExperimental = GL_TRUE;  // Enable modern OpenGL features
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        // GLEW failed to initialize
        fprintf(stderr, "Error initializing GLEW: %s\n", glewGetErrorString(err));
        glfwTerminate();
        return -1;
    }

    Renderer simpleRenderer;
    ImGuiUI ImGuiUI(3, 0, 0.1f, 0.5f);
    ImGuiUI.initialize(window);
    simpleRenderer.renderLoop(window, ImGuiUI);

    return 0;
}

