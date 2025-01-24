#pragma once
#include "../utils/utils.hpp"
#include "ioHandler.hpp"
#include "../imGuiUi/ImGuiUi.hpp"
#include "mesh2SplatHandler.hpp"
#include "../parsers.hpp"
#include "../utils/shaderUtils.hpp"
#include "../radixSort/RadixSort.hpp"

#define MAX_GAUSSIANS_TO_SORT 5000000 

class Renderer
{
public:
	Renderer();
	~Renderer();

	void initializeOpenGLState();
	static glm::vec3 computeCameraPosition(float yaw, float pitch, float distance);
	void run3dgsRenderingPass(GLFWwindow* window, GLuint pointsVAO, GLuint gaussianBuffer, GLuint drawIndirectBuffer, GLuint renderShaderProgram, float std_gauss, int resolutionTarget);
	void renderLoop(GLFWwindow* window, ImGuiUI& gui);
	//TODO: For now not using this, will implement a render-pass based structure and change how the render-loop is implemented
	void recordRenderPass(); 
	bool updateShadersIfNeeded(bool forceReload=false);
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
	GLuint radixSortPrepassProgram;
	GLuint radixSortGatherProgram;

	GLuint gaussianBuffer;
	GLuint drawIndirectBuffer;

	GLuint keysBuffer;
	GLuint valuesBuffer;

	GLuint gaussianBufferSorted;

	
	std::unordered_map<std::string, ShaderFileInfo> shaderFiles;
	//Todo make this into a map and store name->shaderInfo map
	std::vector<std::pair<std::string, GLenum>> converterShadersInfo;
	std::vector<std::pair<std::string, GLenum>> computeShadersInfo;
	std::vector<std::pair<std::string, GLenum>> radixSortPrePassShadersInfo;
	std::vector<std::pair<std::string, GLenum>> radixSortGatherPassShadersInfo;
	std::vector<std::pair<std::string, GLenum>> rendering3dgsShadersInfo;
	double lastShaderCheckTime;

};

