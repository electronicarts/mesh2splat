#include "utils.hpp"
#include "../parsers.hpp"


#define VERTEX_SHADER_LOCATION "./src/shaders/vertex_shader.glsl" 
#define TESS_CONTROL_SHADER_LOCATION "./src/shaders/tess_control.glsl" 
#define TESS_EVAL_SHADER_LOCATION "./src/shaders/tess_evaluation.glsl" 
#define GEOM_SHADER_LOCATION "./src/shaders/geom_shader.glsl" 
#define EIGENDECOMPOSITION_SHADER_LOCATION "./src/shaders/eigendecomposition.glsl"
#define FRAG_SHADER_LOCATION "./src/shaders/fragment_shader.glsl" 



GLuint compileShader(const char* source, GLenum type);

GLuint createConverterShaderProgram();

void uploadTextures(std::map<std::string, TextureDataGl>& textureTypeMap, MaterialGltf material);

std::vector<std::pair<Mesh, GLMesh>> uploadMeshesToOpenGL(const std::vector<Mesh>& meshes);

void setupTransformFeedback(size_t bufferSize, GLuint& feedbackBuffer, GLuint& feedbackVAO, GLuint& acBuffer, unsigned int totalStride);

void setupFrameBuffer(GLuint& framebuffer, unsigned int width, unsigned int height);


//Make arguments into a struct its too many parameters to pass and not readable...
void performGpuConversion(
    GLuint shaderProgram, GLuint vao,
    GLuint framebuffer, size_t vertexCount,
    int normalizedUVSpaceWidth, int normalizedUVSpaceHeight,
    const std::map<std::string, TextureDataGl>& textureTypeMap
);

void retrieveMeshFromFrameBuffer(std::vector<Gaussian3D>& gaussians_3D_list, GLuint& framebuffer, unsigned int width, unsigned int height);

std::string readShaderFile(const char* filePath);