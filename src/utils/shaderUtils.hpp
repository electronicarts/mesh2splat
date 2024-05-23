#include "utils.hpp"
#include "../parsers.hpp"

#define VERTEX_SHADER_LOCATION "./src/shaders/vertex_shader.glsl" 
#define TESS_CONTROL_SHADER_LOCATION "./src/shaders/tess_control.glsl" 
#define TESS_EVAL_SHADER_LOCATION "./src/shaders/tess_evaluation.glsl" 
#define GEOM_SHADER_LOCATION "./src/shaders/geom_shader.glsl" 
#define EIGENDECOMPOSITION_SHADER_LOCATION "./src/shaders/eigendecomposition.glsl"


GLuint compileShader(const char* source, GLenum type);

GLuint createShaderProgram(unsigned int& transformFeedbackVertexStride);

void uploadTextures(const std::map<std::string, std::pair<unsigned char*, int>>& textureTypeMap, MaterialGltf material);

std::vector<GLMesh> uploadMeshesToOpenGL(const std::vector<Mesh>& meshes, float& medianArea, float& medianEdgeLength, float& medianPerimeter, float& meshSurfaceArea);

void setupTransformFeedback(size_t bufferSize, GLuint& feedbackBuffer, GLuint& feedbackVAO, GLuint& acBuffer, unsigned int totalStride);

//Make arguments into a struct its too many parameters to pass and not readable...
void performTessellationAndCapture(
	GLuint shaderProgram,		GLuint vao, 
	size_t vertexCount,			GLuint& targetTriangleEndgeLEngth, GLuint& acBuffer, 
	float medianTriangleArea,	float medianEdgeLength, 
	float medianPerimeter,		unsigned int textureSize, 
	float meshSurfaceArea,		glm::vec3 scale,
	int normalizedUVSpaceWidth, int normalizedUVSpaceHeight,
	glm::vec2 metalllicRoughnessFactors
);

void downloadMeshFromGPU(GLuint& feedbackBuffer, GLuint numberOfTesselatedTriangles, unsigned int elementStride);

std::string readShaderFile(const char* filePath);