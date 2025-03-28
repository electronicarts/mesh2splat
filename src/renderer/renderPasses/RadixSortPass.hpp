///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "RenderPass.hpp"
#include "RadixSort.hpp"

class RadixSortPass : public IRenderPass {
public:
    ~RadixSortPass() = default;
    void execute(RenderContext& renderContext);

private:
    
    unsigned int computeKeyValuesPre(RenderContext& renderContext);
    
    void sort(RenderContext& renderContext, unsigned int validCount);
    
    void gatherPost(RenderContext& renderContext, unsigned int validCount);
};