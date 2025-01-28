#pragma once
#include "RenderContext.hpp"

#define MAX_GAUSSIANS_TO_SORT 5000000 

class IRenderPass {
public:
    virtual ~IRenderPass() = default;
    virtual void execute(RenderContext& context) = 0;
    virtual bool isEnabled() const { return isPassEnabled; }
    virtual void setIsEnabled(bool isPassEnabled) { this->isPassEnabled = isPassEnabled; };
    
    struct DrawElementsIndirectCommand {
        GLuint count;        
        GLuint instanceCount;
        GLuint first;       
        GLuint baseVertex;
        GLuint baseInstance; 
    };

private:
    bool isPassEnabled;

};