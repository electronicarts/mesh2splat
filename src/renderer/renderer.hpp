#pragma once
#include "../utils/utils.hpp"
#include "ioHandler.hpp"
#include "../imGuiUi/ImGuiUi.hpp"
#include "mesh2SplatHandler.hpp"
#include "../parsers.hpp"
#include "../utils/shaderUtils.hpp"
#include "../radixSort/RadixSort.hpp"
#include "renderPasses/RenderContext.hpp"
#include "RenderPasses.hpp"


#define MAX_GAUSSIANS_TO_SORT 5000000 

class Renderer
{
public:
	Renderer(GLFWwindow* window);
	~Renderer();

	void initialize();
    void renderFrame();        // Execute all enabled render passes
	void clearingPrePass(glm::vec4 clearColor); //TODO: hmmm
	
	void updateTransformations();
	static glm::vec3 computeCameraPosition();
	//TODO: For now not using this, will implement a render-pass based structure and change how the render-loop is implemented
	bool updateShadersIfNeeded(bool forceReload=false);
	unsigned int getGaussianCountFromIndirectBuffer();
	void setLastShaderCheckTime(double lastShaderCheckedTime);
	double getLastShaderCheckTime();
	RenderContext* getRenderContext();
	void enableRenderPass(std::string renderPassName);
	void setViewportResolutionForConversion(int resolutionTarget);
	void setStdDevFromImGui(float stdDev);
	void resetRendererViewportResolution();

private:
	
    std::map<std::string, std::unique_ptr<RenderPass>> renderPasses;
    std::vector<std::string> renderPassesOrder;

	GLFWwindow* rendererGlfwWindow;

	RenderContext renderContext;
	
	std::unordered_map<std::string, ShaderFileInfo> shaderFiles;
	//Todo make this into a map and store name->shaderInfo map
	std::vector<std::pair<std::string, GLenum>> converterShadersInfo;
	std::vector<std::pair<std::string, GLenum>> computeShadersInfo;
	std::vector<std::pair<std::string, GLenum>> radixSortPrePassShadersInfo;
	std::vector<std::pair<std::string, GLenum>> radixSortGatherPassShadersInfo;
	std::vector<std::pair<std::string, GLenum>> rendering3dgsShadersInfo;
	double lastShaderCheckTime;

};

