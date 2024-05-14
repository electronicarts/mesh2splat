#include "utils.hpp"

#define VERTEX_SHADER_LOCATION "./src/shaders/vertex_shader.vert" //"..\\shaders\\vertex_shader.vert"
#define TESS_CONTROL_SHADER_LOCATION "./src/shaders/tess_control.tesc" //"..\\shaders\\tess_control.tesc"
#define TESS_EVAL_SHADER_LOCATION "./src/shaders/tess_evaluation.tese" //"..\\shaders\\tess_evaluation.tese"

GLuint compileShader(const char* source, GLenum type);

GLuint createShaderProgram();

std::vector<GLMesh> uploadMeshesToOpenGL(const std::vector<Mesh>& meshes, float& minTriangleArea, float& maxTriangleArea, float& medianArea);

void setupTransformFeedbackAndAtomicCounter(size_t bufferSize, GLuint& feedbackBuffer, GLuint& feedbackVAO, GLuint& acBuffer);

void performTessellationAndCapture(GLuint shaderProgram, GLuint vao, size_t vertexCount, GLuint& targetTriangleEndgeLEngth, GLuint& acBuffer, float minTriangleArea, float maxTriangleArea, float medianTriangleArea);

void downloadMeshFromGPU(GLuint& feedbackBuffer, GLuint numberOfTesselatedTriangles);

std::string readShaderFile(const char* filePath);