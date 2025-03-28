///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#version 450 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

out vec2 fragUV;

void main()
{
    fragUV = aUV;

    gl_Position = vec4(aPos, 0.0, 1.0);
}
