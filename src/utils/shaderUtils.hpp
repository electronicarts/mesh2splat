#include "utils.hpp"
#include "../parsers.hpp"


#define CONVERTER_VERTEX_SHADER_LOCATION "./src/shaders/conversion/vertex_shader.glsl" 
#define CONVERTER_TESS_CONTROL_SHADER_LOCATION "./src/shaders/conversion/tess_control.glsl" 
#define CONVERTER_TESS_EVAL_SHADER_LOCATION "./src/shaders/conversion/tess_evaluation.glsl" 
#define CONVERTER_GEOM_SHADER_LOCATION "./src/shaders/conversion/geom_shader.glsl" 
#define EIGENDECOMPOSITION_SHADER_LOCATION "./src/shaders/conversion/eigendecomposition.glsl"
#define CONVERTER_FRAG_SHADER_LOCATION "./src/shaders/conversion/fragment_shader.glsl" 

#define VOLUMETRIC_SURFACE_VERTEX_SHADER_LOCATION "./src/shaders/volumetric/volumetric_vertex_shader.glsl" 
#define VOLUMETRIC_SURFACE_GEOM_SHADER_LOCATION "./src/shaders/volumetric/volumetric_geometry_shader.glsl"
#define VOLUMETRIC_SURFACE_FRAGMENT_SHADER_LOCATION "./src/shaders/volumetric/volumetric_fragment_shader.glsl" 
#define VOLUMETRIC_SURFACE_COMPUTE_SHADER_LOCATION "./src/shaders/volumetric/compute_shader.glsl" 



GLuint compileShader(const char* source, GLenum type);

GLuint createConverterShaderProgram();

GLuint createVolumetricSurfaceShaderProgram();

GLuint createVolumetricSurfaceShaderProgram();

void uploadTextures(std::map<std::string, TextureDataGl>& textureTypeMap, MaterialGltf material);

void uploadMeshesToOpenGL(const std::vector<Mesh>& meshes, std::vector<std::pair<Mesh, GLMesh>>& DataMeshAndGlMesh);

void setupTransformFeedback(size_t bufferSize, GLuint& feedbackBuffer, GLuint& feedbackVAO, GLuint& acBuffer, unsigned int totalStride);

GLuint* setupFrameBuffer(GLuint& framebuffer, unsigned int width, unsigned int height);

//Make arguments into a struct its too many parameters to pass and not readable...
void performGpuConversion(
    GLuint shaderProgram, GLuint vao,
    GLuint framebuffer, size_t vertexCount,
    int normalizedUVSpaceWidth, int normalizedUVSpaceHeight,
    const std::map<std::string, TextureDataGl>& textureTypeMap,
    MaterialGltf material, unsigned int referenceResolution, float GAUSSIAN_STD
);

void generateVolumetricSurface(
    GLuint shaderProgram, GLuint vao,
    glm::mat4* modelMatrices, GLuint ssbo, GLuint counterBuffer,
    size_t vertexCount, const unsigned int numberOfMicroMeshes,
    int normalizedUVSpaceWidth, int normalizedUVSpaceHeight,
    const std::map<std::string, TextureDataGl>& textureTypeMap, MaterialGltf material
);

void retrieveMeshFromFrameBuffer(std::vector<Gaussian3D>& gaussians_3D_list, GLuint& framebuffer, unsigned int width, unsigned int height, bool print, bool check);

std::string readShaderFile(const char* filePath);

void setupAtomicCounter(GLuint& counterBuffer);

struct Gaussian3D_ssbo {
    float GaussianPosition[3];
    float Scale[3];
    float UV[2];
    float Normal[3];
    float Quaternion[4];
    float Rgba[4];
};

// Generate and allocate the SSBO
GLuint generateSSBO(GLuint& ssbo);

void readBackSSBO(std::vector<Gaussian3D>& gaussians, GLuint ssbo, GLuint counterBuffer);

GLuint readAtomicCounterValue(GLuint counterBuffer);