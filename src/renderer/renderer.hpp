#pragma once
#include "../utils/utils.hpp"
#include "ioHandler.hpp"

class Renderer
{
public:
	static glm::vec3 computeCameraPosition(float yaw, float pitch, float distance);
	GLuint compute_shader_dispatch(GLuint computeShaderProgram, GLuint* drawBuffers, GLuint gaussianBuffer, unsigned int resolutionTarget);
	void render_point_cloud(GLFWwindow* window, GLuint pointsVAO, GLuint gaussianBuffer, GLuint drawIndirectBuffer, GLuint renderShaderProgram, float std_gauss);

private:
	unsigned int getSplatBufferCount(GLuint counterBuffer);

};

