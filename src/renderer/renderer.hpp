#pragma once
#include "../utils/utils.hpp"
#include "ioHandler.hpp"
#include "../imGuiUi/ImGuiUi.hpp"
#include "mesh2SplatHandler.hpp"
#include "../parsers.hpp"
#include "../utils/shaderUtils.hpp"

class Renderer
{
public:
	Renderer();
	~Renderer();

	void initializeOpenGLState();
	static glm::vec3 computeCameraPosition(float yaw, float pitch, float distance);
	void run3dgsRenderingPass(GLFWwindow* window, GLuint pointsVAO, GLuint gaussianBuffer, GLuint drawIndirectBuffer, GLuint renderShaderProgram, float std_gauss);
	void renderLoop(GLFWwindow* window, ImGuiUI& gui);
	//TODO: For now not using this, will implement a render-pass based structure and change how the render-loop is implemented
	void recordRenderPass(); 
	bool updateShadersIfNeeded();
private:
	void clearingPrePass(glm::vec4 clearColor);
	unsigned int getSplatBufferCount(GLuint counterBuffer);
	Mesh2splatConverterHandler mesh2SplatConversionHandler;
	int normalizedUvSpaceWidth, normalizedUvSpaceHeight;
    std::vector<std::pair<Mesh, GLMesh>> dataMeshAndGlMesh;
    std::vector<Gaussian3D> gaussians_3D_list;
    std::map<std::string, TextureDataGl> textureTypeMap;

	GLuint pointsVAO = 0;
	GLuint converterShaderProgram;
	GLuint renderShaderProgram;
	GLuint computeShaderProgram;
	GLuint gaussianBuffer;
	GLuint drawIndirectBuffer;

	std::unordered_map<std::string, ShaderFileInfo> shaderFiles;
	std::vector<std::pair<std::string, GLenum>> converterShadersInfo;
	std::vector<std::pair<std::string, GLenum>> computeShadersInfo;
	std::vector<std::pair<std::string, GLenum>> rendering3dgsShadersInfo;
	double lastShaderCheckTime;

};

