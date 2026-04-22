///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once

//Base 
#include "renderPasses/RenderPass.hpp"

//Render Passes
#include "renderPasses/ConversionPass.hpp"
#include "renderPasses/GaussiansPrepass.hpp"
#include "renderPasses/RadixSortPass.hpp"
#include "renderPasses/GaussianSplattingPass.hpp"
#include "renderPasses/GaussianRelightingPass.hpp"
#include "renderPasses/GaussianShadowPass.hpp"
#include "renderPasses/DepthPrepass.hpp"
#include "renderPasses/MeshRenderPass.hpp"


inline const std::string conversionPassName                  = "conversion";
inline const std::string depthPrepassName                    = "depthPrepass";
inline const std::string meshRenderPassName                  = "meshRender";
inline const std::string gaussiansPrePassName                = "gaussianPrepass";
inline const std::string radixSortPassName                   = "radixSort";
inline const std::string gaussianSplattingPassName           = "gaussianSplatting";
inline const std::string gaussianSplattingRelightingPassName = "gaussianSplattingDeferredLighting";
inline const std::string gaussianSplattingShadowsPassName    = "gaussianSplattingShadows";
