///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once

enum class EventType {
    LoadModel,
    LoadPly,
    ViewDepth,
    RunConversion,
    SavePLY,
    EnableGaussianRendering,
    CheckShaderUpdate,
    ResizedWindow
};