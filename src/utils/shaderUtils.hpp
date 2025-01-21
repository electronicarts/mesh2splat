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

#define RENDERER_VERTEX_SHADER_LOCATION "./src/shaders/rendering/gaussianSplattingVS.glsl" 
#define RENDERER_FRAGMENT_SHADER_LOCATION "./src/shaders/rendering/gaussianSplattingPS.glsl" 

#define TRANSFORM_COMPUTE_SHADER_LOCATION "./src/shaders/rendering/frameBufferReaderCS.glsl" 


GLuint compileShader(const char* source, GLenum type);

GLuint createConverterShaderProgram();

GLuint createRendererShaderProgram();

GLuint createComputeShaderProgram();

void generateTextures(MaterialGltf material, std::map<std::string, TextureDataGl>& textureTypeMap);

void generateMeshesVBO(const std::vector<Mesh>& meshes, std::vector<std::pair<Mesh, GLMesh>>& DataMeshAndGlMesh);

void setupTransformFeedback(size_t bufferSize, GLuint& feedbackBuffer, GLuint& feedbackVAO, GLuint& acBuffer, unsigned int totalStride);

GLuint* setupFrameBuffer(GLuint& framebuffer, unsigned int width, unsigned int height);

//Make arguments into a struct its too many parameters to pass and not readable...

void retrieveMeshFromFrameBuffer(std::vector<Gaussian3D>& gaussians_3D_list, GLuint& framebuffer, unsigned int width, unsigned int height, bool print, bool check);

void setupSsbo(unsigned int width, unsigned int height, GLuint* gaussianBuffer);

std::string readShaderFile(const char* filePath);

//Make template function for this and make these one generic
void setUniform1f(GLuint shaderProgram, std::string uniformName, float uniformValue);
void setUniform1i(GLuint shaderProgram, std::string uniformName, unsigned int uniformValue);
void setUniform3f(GLuint shaderProgram, std::string uniformName, glm::vec3 uniformValue);
void setUniform2f(GLuint shaderProgram, std::string uniformName, glm::vec2 uniformValue);
void setUniformMat4(GLuint shaderProgram, std::string uniformName, glm::mat4 matrix);
void setTexture2D(GLuint shaderProgram, std::string textureUniformName, GLuint texture, unsigned int textureUnitNumber);