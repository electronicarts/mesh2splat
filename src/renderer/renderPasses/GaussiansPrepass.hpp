///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "RenderPass.hpp"

class GaussiansPrepass : public IRenderPass {
public:
    ~GaussiansPrepass() = default;
    void execute(RenderContext& renderContext);
};