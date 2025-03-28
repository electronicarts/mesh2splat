///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "utils/utils.hpp"
#include "ioHandler.hpp"
#include "imGuiUi/ImGuiUi.hpp"
#include "parsers/parsers.hpp"
#include "utils/glUtils.hpp"
#include "RadixSort.hpp"
#include "renderPasses/RenderContext.hpp"
#include "RenderPasses.hpp"
#include "utils/SceneManager.hpp"
#include "utils/ShaderRegistry.hpp"

class Renderer {
public:
	Renderer(GLFWwindow* window, Camera& cameraInstance);

	~Renderer();

	void initialize();
	void renderFrame();        
	void clearingPrePass(glm::vec4 clearColor); 
	void updateTransformations();

	//TODO: For now not using this, will implement a render-pass based structure and change how the render-loop is implemented
	bool updateShadersIfNeeded(bool forceReload = false);
	unsigned int getVisibleGaussianCount();
	unsigned int getTotalGaussianCount();
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
	void createGBuffer();
	void deleteGBuffer();
	void createDepthTexture();
	void deleteDepthTexture();
	void setDepthTestEnabled(bool depthTest);
	void setLightingEnabled(bool isEnabled);
	void setLightIntensity(float lightIntensity);
	void setLightColor(glm::vec3 lightColor);
	bool hasWindowSizeChanged();
	bool isWindowMinimized();


private:
	std::map<std::string, std::unique_ptr<IRenderPass>> renderPasses;
	std::vector<std::string> renderPassesOrder;

	GLFWwindow* rendererGlfwWindow;

	std::unique_ptr<SceneManager> sceneManager;
	RenderContext renderContext;

	double lastShaderCheckTime;

	double gpuFrameTimeMs;

	Camera& camera;

};