#pragma once
#include "utils.hpp"
#include "../parsers/parsers.hpp"

namespace glUtils
{ 
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

    #define TRANSFORM_COMPUTE_SHADER_LOCATION "./src/shaders/rendering/frameBufferReaderCS.glsl" 

    #define RADIX_SORT_PREPASS_SHADER_LOCATION "./src/shaders/rendering/radixSortPrepass.glsl" 
    #define RADIX_SORT_GATHER_SHADER_LOCATION "./src/shaders/rendering/radixSortGather.glsl" 

    #define RENDERER_VERTEX_SHADER_LOCATION "./src/shaders/rendering/gaussianSplattingVS.glsl" 
    #define RENDERER_FRAGMENT_SHADER_LOCATION "./src/shaders/rendering/gaussianSplattingPS.glsl" 



    GLuint compileShader(const char* source, GLenum type);

    void generateTextures(MaterialGltf material, std::map<std::string, TextureDataGl>& textureTypeMap);

    void generateMeshesVBO(const std::vector<Mesh>& meshes, std::vector<std::pair<Mesh, GLMesh>>& DataMeshAndGlMesh);

    void setupTransformFeedback(size_t bufferSize, GLuint& feedbackBuffer, GLuint& feedbackVAO, GLuint& acBuffer, unsigned int totalStride);

    GLuint* setupFrameBuffer(GLuint& framebuffer, unsigned int width, unsigned int height);

    void read3dgsDataFromSsboBuffer(GLuint& indirectDrawCommandBuffer, GLuint& gaussianBuffer, GaussianDataSSBO*& gaussians, unsigned int& gaussianCount);

    void setupGaussianBufferSsbo(unsigned int width, unsigned int height, GLuint* gaussianBuffer);
    void setupSortedBufferSsbo(unsigned int size, GLuint sortedBuffer, unsigned int bindingPos);
    void setupKeysBufferSsbo(unsigned int size, GLuint keysBuffer, unsigned int bindingPos);
    void setupValuesBufferSsbo(unsigned int size, GLuint valuesBuffer, unsigned int bindingPos);

    std::string readShaderFile(const char* filePath);

    namespace fs = std::experimental::filesystem;

    struct ShaderFileInfo {
        fs::file_time_type lastWriteTime;
        std::string filePath;
    };

    //TODO: Wont scale, use some data structure
    void initializeShaderFileMonitoring(
        std::unordered_map<std::string, ShaderFileInfo>& shaderFiles,
        std::vector<std::pair<std::string, GLenum>>& converterShadersInfo,
        std::vector<std::pair<std::string, GLenum>>& computeShadersInfo,
        std::vector<std::pair<std::string, GLenum>>& radixSortPrePostShadersInfo,
        std::vector<std::pair<std::string, GLenum>>& radixSortGatherShadersInfo,
        std::vector<std::pair<std::string, GLenum>>& rendering3dgsShadersInfo
    );

    bool shaderFileChanged(const ShaderFileInfo& info);

    GLuint reloadShaderPrograms(
        const std::vector<std::pair<std::string, GLenum>>& shaderInfos,
        GLuint oldProgram);

    //TODO: Make template function for this and make these one generic
    void setUniform1f(GLuint shaderProgram, std::string uniformName, float uniformValue);
    void setUniform1i(GLuint shaderProgram, std::string uniformName, int uniformValue);
    void setUniform1ui(GLuint shaderProgram, std::string uniformName, unsigned int uniformValue);
    void setUniform3f(GLuint shaderProgram, std::string uniformName, glm::vec3 uniformValue);
    void setUniform2f(GLuint shaderProgram, std::string uniformName, glm::vec2 uniformValue);
    void setUniform2i(GLuint shaderProgram, std::string uniformName, glm::vec2 uniformValue);
    void setUniformMat4(GLuint shaderProgram, std::string uniformName, glm::mat4 matrix);
    void setTexture2D(GLuint shaderProgram, std::string textureUniformName, GLuint texture, unsigned int textureUnitNumber);
}