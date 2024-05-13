#include "utils.hpp"

#define VERTEX_SHADER_LOCATION "./src/shaders/vertex_shader.vert" //"..\\shaders\\vertex_shader.vert"
#define TESS_CONTROL_SHADER_LOCATION "./src/shaders/tess_control.tesc" //"..\\shaders\\tess_control.tesc"
#define TESS_EVAL_SHADER_LOCATION "./src/shaders/tess_evaluation.tese" //"..\\shaders\\tess_evaluation.tese"

GLuint compileShader(const char* source, GLenum type);

GLuint createShaderProgram();

std::vector<GLMesh> uploadMeshesToOpenGL(const std::vector<Mesh>& meshes);

void setupTransformFeedback(size_t bufferSize, GLuint& feedbackBuffer, GLuint& feedbackVAO);

void performTessellationAndCapture(GLuint shaderProgram, GLuint vao, size_t vertexCount, const int targetTriangleEndgeLEngth, GLuint& numTessellatedTriangles);

void downloadMeshFromGPU(GLuint& feedbackBuffer, GLuint numberOfTesselatedTriangles);

std::string readShaderFile(const char* filePath);