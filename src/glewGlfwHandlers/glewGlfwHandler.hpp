///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "renderer/IoHandler.hpp"
#include "utils/utils.hpp"  

class GlewGlfwHandler
{
public:
	GlewGlfwHandler(glm::ivec2 windowDimensions, std::string windowName);
	~GlewGlfwHandler() {};
	int init();
	void updateResize();
	
	static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
	{
		glViewport(0, 0, width, height);
	}

	GLFWwindow* getWindow();
private:
	GLFWwindow* window;
};