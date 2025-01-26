#pragma once
#include "RenderPass.hpp"

class GaussianSplattingPass : public RenderPass {
public:
    ~GaussianSplattingPass() = default;
    void execute(RenderContext& renderContext);

};