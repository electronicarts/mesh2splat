#include "utils.hpp"

GLuint compileShader(const char* source, GLenum type);

GLuint createShaderProgram();

std::vector<GLMesh> uploadMeshesToOpenGL(const std::vector<Mesh>& meshes);

void setupTransformFeedback(size_t bufferSize, GLuint& feedbackBuffer, GLuint& feedbackVAO);

void performTessellationAndCapture(GLuint shaderProgram, GLuint vao, size_t vertexCount);

void downloadMeshFromGPU(size_t vertexCount, GLuint& feedbackBuffer);