#pragma once
#include "../utils/utils.hpp"
#include "../utils/shaderUtils.hpp"
#include "../utils/normalizedUvUnwrapping.hpp"

class Mesh2splatConverterHandler
{
public:
    void runConversion(const std::string& meshFilePath, const std::string& baseFolder,
        int resolution, float gaussianStd,
        GLuint converterShaderProgram, GLuint computeShaderProgram,
        std::vector<std::pair<Mesh, GLMesh>>& dataMeshAndGlMesh,
        int normalizedUvSpaceWidth, int normalizedUvSpaceHeight,
        std::map<std::string, TextureDataGl>& textureTypeMap,
        GLuint& gaussianBuffer, GLuint& drawIndirectBuffer);
    GLuint compute_shader_dispatch(GLuint computeShaderProgram, GLuint* drawBuffers, GLuint gaussianBuffer, unsigned int resolutionTarget);

    void performGpuConversion(
        GLuint shaderProgram, GLuint vao,
        GLuint framebuffer, size_t vertexCount,
        int normalizedUVSpaceWidth, int normalizedUVSpaceHeight,
        const std::map<std::string, TextureDataGl>& textureTypeMap, MaterialGltf material, unsigned int referenceResolution, float GAUSSIAN_STD
    );

    void prepareMeshAndUploadToGPU(std::string filename, std::string base_folder, std::vector<std::pair<Mesh, GLMesh>>& dataMeshAndGlMesh, int& uvSpaceWidth, int& uvSpaceHeight);


private:
    struct DrawArraysIndirectCommand {
        GLuint count;        // Number of vertices to draw.
        GLuint primCount;    // Number of instances.
        GLuint first;        // Starting index in the vertex buffer.
        GLuint baseInstance; // Base instance for instanced rendering.
    };
};
