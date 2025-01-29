#include "glewGlfwHandler.hpp"

GlewGlfwHandler::GlewGlfwHandler(glm::ivec2 windowDimensions, std::string windowName)
{
    if (!glfwInit())
        exit(-1);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    //glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    this->window = glfwCreateWindow(windowDimensions.x, windowDimensions.y, windowName.c_str(), NULL, NULL);
    if (!this->window) {
        glfwTerminate();
        exit(-1);
    }
}

int GlewGlfwHandler::init()
{
    glfwMakeContextCurrent(this->window);
    //glfwSetMouseButtonCallback(this->window, mouse_button_callback);
    //glfwSetCursorPosCallback(this->window, cursor_position_callback);
    //glfwSetScrollCallback(this->window, scroll_callback);

    // Initialize GLEW
    glewExperimental = GL_TRUE;  // Enable modern OpenGL features
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        // GLEW failed to initialize
        fprintf(stderr, "Error initializing GLEW: %s\n", glewGetErrorString(err));
        glfwTerminate();
        return -1;
    }

    return 0;
}

GLFWwindow* GlewGlfwHandler::getWindow() { return window; };
