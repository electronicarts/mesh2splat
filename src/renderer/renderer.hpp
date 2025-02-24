#pragma once
#include "../utils/utils.hpp"
#include "ioHandler.hpp"
#include "../imGuiUi/ImGuiUi.hpp"
#include "../parsers/parsers.hpp"
#include "../utils/glUtils.hpp"
#include "../radixSort/RadixSort.hpp"
#include "renderPasses/RenderContext.hpp"
#include "RenderPasses.hpp"
#include "../utils/SceneManager.hpp"

class Renderer {
public:
	Renderer(GLFWwindow* window, Camera& cameraInstance);

	~Renderer();

	void initialize();
	void renderFrame();        // Execute all enabled render passes
	void clearingPrePass(glm::vec4 clearColor); //TODO: hmmm
	void updateTransformations();

	//TODO: For now not using this, will implement a render-pass based structure and change how the render-loop is implemented
	bool updateShadersIfNeeded(bool forceReload = false);
	unsigned int getVisibleGaussianCount();
	void setLastShaderCheckTime(double lastShaderCheckedTime);
	double getLastShaderCheckTime();
	RenderContext* getRenderContext();
	void enableRenderPass(std::string renderPassName);
	void setViewportResolutionForConversion(int resolutionTarget);
	void setFormatType(unsigned int format);

	void setStdDevFromImGui(float stdDev);
	void resetRendererViewportResolution();
	SceneManager& getSceneManager();
	double getTotalGpuFrameTimeMs() const;
	void updateGaussianBuffer();
	void gaussianBufferFromSize(unsigned int size);
	void setRenderMode(ImGuiUI::VisualizationOption renderMode);
	void resetModelMatrices();



private:
	std::map<std::string, std::unique_ptr<IRenderPass>> renderPasses;
	std::vector<std::string> renderPassesOrder;

	GLFWwindow* rendererGlfwWindow;

	std::unique_ptr<SceneManager> sceneManager;
	RenderContext renderContext;

	std::unordered_map<std::string, glUtils::ShaderFileInfo> shaderFiles;
	//Todo make this into a map and store name->shaderInfo map
	std::vector<std::pair<std::string, GLenum>> converterShadersInfo;
	std::vector<std::pair<std::string, GLenum>> computeShadersInfo;
	std::vector<std::pair<std::string, GLenum>> radixSortPrePassShadersInfo;
	std::vector<std::pair<std::string, GLenum>> radixSortGatherPassShadersInfo;
	std::vector<std::pair<std::string, GLenum>> rendering3dgsComputePrepassShadersInfo;
	std::vector<std::pair<std::string, GLenum>> rendering3dgsShadersInfo;

	double lastShaderCheckTime;

	double gpuFrameTimeMs;

	Camera& camera;

};