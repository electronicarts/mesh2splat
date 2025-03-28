///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "RenderPass.hpp"

class GaussianSplattingPass : public IRenderPass {
public:
    GaussianSplattingPass(RenderContext& renderContext);
    ~GaussianSplattingPass() = default;
    void execute(RenderContext& renderContext);
private:
    GLuint quadVBO;
    GLuint quadEBO;
};