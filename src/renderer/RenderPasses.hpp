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


static std::string conversionPassName					= "conversion";
static std::string depthPrepassName						= "depthPrepass";
static std::string gaussiansPrePassName					= "gaussianPrepass";
static std::string radixSortPassName					= "radixSort";
static std::string gaussianSplattingPassName			= "gaussianSplatting";
static std::string gaussianSplattingRelightingPassName	= "gaussianSplattingDeferredLighting";
static std::string gaussianSplattingShadowsPassName		= "gaussianSplattingShadows";
