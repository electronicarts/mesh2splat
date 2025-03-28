///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "utils.hpp"
#include "parsers/parsers.hpp"

class ShaderRegistry; //forward decl

namespace glUtils
{ 
    //Register here new shader locations
    struct ShaderLocations
    {
        std::string converterVertexShaderLocation;
        std::string converterGeomShaderLocation;
        std::string eigenDecompositionShaderLocation;
        std::string converterFragShaderLocation;

        std::string transformComputeShaderLocation;

        std::string radixSortPrepassShaderLocation;
        std::string radixSortGatherShaderLocation;

        std::string rendererPrepassComputeShaderLocation;

        std::string rendererVertexShaderLocation;
        std::string rendererFragmentShaderLocation;

        std::string rendererDeferredRelightingVertexShaderLocation;
        std::string rendererDeferredRelightingFragmentShaderLocation;

        std::string shadowsPrepassComputeShaderLocation;
        std::string shadowsCubemapVertexShaderLocation;
        std::string shadowsCubemapFragmentShaderLocation;

        std::string depthPrepassVertexShaderLocation;
        std::string depthPrepassFragmentShaderLocation;
    };

    extern ShaderLocations shaderLocations;

    //Register here new shader programs
    enum ShaderProgramTypes
    {
        ConverterProgram,
        ComputeTransformProgram,
        RadixSortPrepassProgram,
        RadixSortGatherComputeProgram,
        PrepassFiltering3dgsProgram,
        Rendering3dgsProgram,
        DeferredRelightingPassProgram,
        ShadowPrepassComputeProgram,
        ShadowCubemapPassProgram,
        DepthPrepassProgram,
    };

    void initializeShaderLocations();

    GLuint compileShader(const char* source, GLenum type);

    void generateTextures(std::map<std::string, std::map<std::string, utils::TextureDataGl>>& meshToTextureData);

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

    struct ShaderFileEditingInfo {
        fs::file_time_type lastWriteTime;
        std::string filePath;
    };

    struct ShaderFileInfo {
        std::string filePath;
        fs::file_time_type lastModified;
    };

    struct ShaderInfo {
        GLenum shaderType;
        std::shared_ptr<ShaderFileInfo> fileInfo;
    };

    struct ShaderProgramInfo {
        ShaderProgramTypes programType;
        std::vector<ShaderInfo> shaders;
        GLuint programID; 
    };

    void initializeShaderFileMonitoring(ShaderRegistry& shaderRegistry);

    bool shaderFileChanged(const ShaderFileEditingInfo& info);

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
    void setUniform4f(GLuint shaderProgram, std::string uniformName, glm::vec4 uniformValue);
    void setUniform3f(GLuint shaderProgram, std::string uniformName, glm::vec3 uniformValue);
    void setUniform2f(GLuint shaderProgram, std::string uniformName, glm::vec2 uniformValue);
    void setUniform2i(GLuint shaderProgram, std::string uniformName, glm::ivec2 uniformValue);
    void setUniformMat4(GLuint shaderProgram, std::string uniformName, glm::mat4 matrix);
    void setUniformMat4v(GLuint shaderProgram, std::string uniformName, std::vector<glm::mat4> matrices, unsigned int count);
    void setTexture2D(GLuint shaderProgram, std::string textureUniformName, GLuint texture, unsigned int textureUnitNumber);
}

namespace std {
    template<>
    struct hash<glUtils::ShaderProgramTypes> {
        inline size_t operator()(const glUtils::ShaderProgramTypes& s) const noexcept {
            return hash<int>()(static_cast<int>(s));
        }
    };
}