///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "RenderPass.hpp"

class ConversionPass : public IRenderPass {
public:
    ~ConversionPass() = default;
    void execute(RenderContext &renderContext);

private:
    void conversion(RenderContext& renderContext, std::pair<utils::Mesh, utils::GLMesh>& mesh, GLuint dummyFramebuffer);
};