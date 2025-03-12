#pragma once
#include "utils.hpp"
#include "../parsers/parsers.hpp"

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


#define RENDERER_PREPASS_COMPUTE_SHADER_LOCATION "./src/shaders/rendering/gaussianSplattingPrepassCS.glsl" 

#define RENDERER_VERTEX_SHADER_LOCATION "./src/shaders/rendering/gaussianSplattingVS.glsl" 
#define RENDERER_FRAGMENT_SHADER_LOCATION "./src/shaders/rendering/gaussianSplattingPS.glsl" 

#define RENDERER_DEFERRED_RELIGHTING_VERTEX_SHADER_LOCATION "./src/shaders/rendering/gaussianSplattingDeferredVS.glsl" 
#define RENDERER_DEFERRED_RELIGHTING_FRAGMENT_SHADER_LOCATION "./src/shaders/rendering/gaussianSplattingDeferredPS.glsl"

#define SHADOWS_PREPASS_COMPUTE_SHADER_LOCATION "./src/shaders/rendering/gaussianPointShadowMappingCS.glsl" 
#define SHADOWS_CUBEMAP_VERTEX_SHADER_LOCATION "./src/shaders/rendering/gaussianPointLightCubeMapShadowVS.glsl" 
#define SHADOWS_CUBEMAP_FRAGMENT_SHADER_LOCATION "./src/shaders/rendering/gaussianPointLightCubeMapShadowPS.glsl" 



namespace glUtils
{ 

    GLuint compileShader(const char* source, GLenum type);

    void generateTextures(std::map<std::string, utils::TextureDataGl>& textureTypeMap);

    void generateMeshesVBO(const std::vector<utils::Mesh>& meshes, std::vector<std::pair<utils::Mesh, utils::GLMesh>>& DataMeshAndGlMesh);

    void setupTransformFeedback(size_t bufferSize, GLuint& feedbackBuffer, GLuint& feedbackVAO, GLuint& acBuffer, unsigned int totalStride);

    GLuint setupFrameBuffer(GLuint& framebuffer, unsigned int width, unsigned int height);
    //GLuint setupFrameBuffer(GLuint& framebuffer, unsigned int width, unsigned int height);

    void read3dgsDataFromSsboBuffer(GLuint& indirectDrawCommandBuffer, GLuint& gaussianBuffer, utils::GaussianDataSSBO*& gaussians, unsigned int& gaussianCount);
    
    void fillGaussianBufferSsbo(GLuint& gaussianBuffer, std::vector<utils::GaussianDataSSBO>& gaussians);
    void fillGaussianBufferSsbo(GLuint& gaussianBuffer, unsigned int size);
    
    void resetAtomicCounter(GLuint atomicCounterBuffer);

    template<typename T>
    void resizeAndBindToPosSSBO(unsigned int size, GLuint buffer, unsigned int bindingPos)
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
        GLsizeiptr bufferSize = size * sizeof(T);
        glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_STREAM_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPos, buffer);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    };

    std::string readShaderFile(const char* filePath);

    namespace fs = std::experimental::filesystem;

    struct ShaderFileInfo {
        fs::file_time_type lastWriteTime;
        std::string filePath;
    };

    //TODO: Wont scale ffs!!!
    void initializeShaderFileMonitoring(
        std::unordered_map<std::string, ShaderFileInfo>& shaderFiles,
        std::vector<std::pair<std::string, GLenum>>& converterShadersInfo,
        std::vector<std::pair<std::string, GLenum>>& computeShadersInfo,
        std::vector<std::pair<std::string, GLenum>>& radixSortPrePostShadersInfo,
        std::vector<std::pair<std::string, GLenum>>& radixSortGatherShadersInfo,
        std::vector<std::pair<std::string, GLenum>>& rendering3dgsShadersInfo,
        std::vector<std::pair<std::string, GLenum>>& rendering3dgsComputePrepassShadersInfo,
        std::vector<std::pair<std::string, GLenum>>& deferredRelightingShaderInfo,
        std::vector<std::pair<std::string, GLenum>>& shadowsComputeShaderInfo,
        std::vector<std::pair<std::string, GLenum>>& shadowsRenderCubemapShaderInfo
    );

    bool shaderFileChanged(const ShaderFileInfo& info);

    GLuint reloadShaderPrograms(
        const std::vector<std::pair<std::string, GLenum>>& shaderInfos,
        GLuint oldProgram);

    //TODO: Make template function for this and make these one generic
    template<typename T>
    void setUniform(GLuint shaderProgram, std::string uniformName, T uniformValue);

    void setUniform1f(GLuint shaderProgram, std::string uniformName, float uniformValue);
    void setUniform1i(GLuint shaderProgram, std::string uniformName, int uniformValue);
    void setUniform1ui(GLuint shaderProgram, std::string uniformName, unsigned int uniformValue);
    void setUniform1uiv(GLuint shaderProgram, std::string uniformName, unsigned int* uniformValue, int count);
    void setUniform3f(GLuint shaderProgram, std::string uniformName, glm::vec3 uniformValue);
    void setUniform2f(GLuint shaderProgram, std::string uniformName, glm::vec2 uniformValue);
    void setUniform2i(GLuint shaderProgram, std::string uniformName, glm::ivec2 uniformValue);
    void setUniformMat4(GLuint shaderProgram, std::string uniformName, glm::mat4 matrix);
    void setUniformMat4v(GLuint shaderProgram, std::string uniformName, std::vector<glm::mat4> matrices, unsigned int count);
    void setTexture2D(GLuint shaderProgram, std::string textureUniformName, GLuint texture, unsigned int textureUnitNumber);
}