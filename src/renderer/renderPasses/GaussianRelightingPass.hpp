///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "RenderPass.hpp"

class GaussianRelightingPass : public IRenderPass {
public:
    GaussianRelightingPass();
    ~GaussianRelightingPass() = default;
    void execute(RenderContext& renderContext);
private:
    GLuint m_fullscreenQuadVAO = 0;
    GLuint m_fullscreenQuadVBO = 0;
    GLuint m_fullscreenQuadEBO = 0;

};