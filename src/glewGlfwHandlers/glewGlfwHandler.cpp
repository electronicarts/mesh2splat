///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "glewGlfwHandler.hpp"

GlewGlfwHandler::GlewGlfwHandler(glm::ivec2 windowDimensions, std::string windowName)
{
    if (!glfwInit())
        exit(-1);

    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    //glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    this->window = glfwCreateWindow(windowDimensions.x, windowDimensions.y, windowName.c_str(), NULL, NULL);
    if (!this->window) {
        glfwTerminate();
        exit(-1);
    }

    updateResize();

}

void GlewGlfwHandler::updateResize()
{
    glfwSetFramebufferSizeCallback(this->window, GlewGlfwHandler::framebuffer_size_callback);
}


int GlewGlfwHandler::init()
{
    glfwMakeContextCurrent(this->window);
    glfwSwapInterval(0);
    glewExperimental = GL_TRUE;  
    GLenum err = glewInit();

    if (err != GLEW_OK) {
        fprintf(stderr, "Error initializing GLEW: %s\n", glewGetErrorString(err));
        glfwTerminate();
        return -1;
    }

    return 0;
}

GLFWwindow* GlewGlfwHandler::getWindow() { return window; };
