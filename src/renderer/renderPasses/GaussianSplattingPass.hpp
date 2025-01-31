#pragma once
#include "RenderPass.hpp"

class GaussianSplattingPass : public IRenderPass {
public:
    ~GaussianSplattingPass() = default;
    void execute(RenderContext& renderContext);
private:
};