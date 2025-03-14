#pragma once
#include "RenderPass.hpp"

class ConversionPass : public IRenderPass {
public:
    ~ConversionPass() = default;
    void execute(RenderContext &renderContext);

private:
    void conversion(RenderContext& renderContext, std::pair<utils::Mesh, utils::GLMesh>& mesh, GLuint dummyFramebuffer);
};