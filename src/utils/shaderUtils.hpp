#include "utils.hpp"
#include "../parsers.hpp"


#define VERTEX_SHADER_LOCATION "./src/shaders/vertex_shader.glsl" 
#define TESS_CONTROL_SHADER_LOCATION "./src/shaders/tess_control.glsl" 
#define TESS_EVAL_SHADER_LOCATION "./src/shaders/tess_evaluation.glsl" 
#define GEOM_SHADER_LOCATION "./src/shaders/geom_shader.glsl" 
#define EIGENDECOMPOSITION_SHADER_LOCATION "./src/shaders/eigendecomposition.glsl"
#define FRAG_SHADER_LOCATION "./src/shaders/fragment_shader.glsl" 



GLuint compileShader(const char* source, GLenum type);

GLuint createShaderProgram(unsigned int& transformFeedbackVertexStride);

void uploadTextures(std::map<std::string, std::pair<unsigned char*, int>>& textureTypeMap, MaterialGltf material);

std::vector<GLMesh> uploadMeshesToOpenGL(const std::vector<Mesh>& meshes);

void setupTransformFeedback(size_t bufferSize, GLuint& feedbackBuffer, GLuint& feedbackVAO, GLuint& acBuffer, unsigned int totalStride);

void setupFrameBuffer(GLuint& framebuffer, unsigned int width, unsigned int height);


//Make arguments into a struct its too many parameters to pass and not readable...
void performTessellationAndCapture(
    GLuint shaderProgram, GLuint vao,
    GLuint framebuffer, size_t vertexCount,
    GLuint& numGaussiansGenerated, GLuint& acBuffer,
    int normalizedUVSpaceWidth, int normalizedUVSpaceHeight,
    const std::map<std::string, std::pair<unsigned char*, int>>& textureTypeMap
);

void downloadMeshFromGPU(std::vector<Gaussian3D>& gaussians_3D_list, GLuint& framebuffer, unsigned int width, unsigned int height);

std::string readShaderFile(const char* filePath);