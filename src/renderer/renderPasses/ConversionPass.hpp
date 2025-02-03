#pragma once
#include "RenderPass.hpp"

class ConversionPass : public IRenderPass {
public:
    ~ConversionPass() = default;
    void execute(RenderContext &renderContext);

private:
    void conversion(RenderContext renderContext, std::pair<Mesh, GLMesh> mesh, GLuint dummyFramebuffer);
    void aggregation(GLuint& computeShaderProgram, GLuint* drawBuffers, GLuint& gaussianBuffer, GLuint& drawIndirectBuffer, GLuint& atomicCounterBuffer, unsigned int resolutionTarget);
};