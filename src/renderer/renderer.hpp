#pragma once
#include "../utils/utils.hpp"
#include "ioHandler.hpp"
#include "../imGuiUi/ImGuiUi.hpp"
#include "mesh2SplatHandler.hpp"
#include "../parsers.hpp"

class Renderer
{
public:
	Renderer();
	~Renderer();

	static glm::vec3 computeCameraPosition(float yaw, float pitch, float distance);
	void runPointCloudRenderingPass(GLFWwindow* window, GLuint pointsVAO, GLuint gaussianBuffer, GLuint drawIndirectBuffer, GLuint renderShaderProgram, float std_gauss);
	void renderLoop(GLFWwindow* window, ImGuiUI& gui);
	//For now not using this, will implement a simil scene graph setup later
	void recordRenderPass();
private:
	void clearingPrePass();
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

	struct DrawArraysIndirectCommand {
        GLuint count;        
        GLuint instanceCount;    
        GLuint first;        
        GLuint baseInstance; 
    };

};

