#pragma once
#include "RenderPass.hpp"
#include "../../radixSort/RadixSort.hpp"

class RadixSortPass : public RenderPass {
public:
    ~RadixSortPass() = default;
    void execute(RenderContext& renderContext);

private:
    
    unsigned int computeKeyValuesPre(RenderContext& renderContext);
    
    void sort(RenderContext& renderContext, unsigned int validCount);
    
    void gatherPost(RenderContext& renderContext, unsigned int validCount);
};