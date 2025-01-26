#pragma once
#include "RenderPass.hpp"

class ConversionPass : public RenderPass {
public:
    ~ConversionPass() = default;
    void execute(RenderContext &renderContext);

private:
    void conversion(
        GLuint shaderProgram, GLuint vao,
        GLuint framebuffer, size_t vertexCount,
        int normalizedUVSpaceWidth, int normalizedUVSpaceHeight,
        const std::map<std::string, TextureDataGl>& textureTypeMap, MaterialGltf material, unsigned int referenceResolution, float GAUSSIAN_STD
    );
    void aggregation(GLuint& computeShaderProgram, GLuint* drawBuffers, GLuint& gaussianBuffer, GLuint& drawIndirectBuffer, unsigned int resolutionTarget);
};