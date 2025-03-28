///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include <string>
#include <utility>
#include <any>
#include "event.hpp"

class IMediator {
public:
    virtual void notify(EventType event) = 0;
    virtual ~IMediator() = default;
};
