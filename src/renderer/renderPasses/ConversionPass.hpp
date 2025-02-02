#pragma once
#include "RenderPass.hpp"

class ConversionPass : public IRenderPass {
public:
    ~ConversionPass() = default;
    void execute(RenderContext &renderContext);

private:
    void conversion(
        GLuint shaderProgram, GLuint vao,
        GLuint framebuffer, size_t vertexCount,
        int normalizedUVSpaceWidth, int normalizedUVSpaceHeight,
        const std::map<std::string, TextureDataGl>& textureTypeMap, MaterialGltf material, unsigned int referenceResolution, float GAUSSIAN_STD,
        GLuint gaussianBuffer, GLuint atomicCounter
    );
    void aggregation(GLuint& computeShaderProgram, GLuint* drawBuffers, GLuint& gaussianBuffer, GLuint& drawIndirectBuffer, GLuint& atomicCounterBuffer, unsigned int resolutionTarget);
};