#pragma once
#include "RenderContext.hpp"

class RenderPass {
public:
    virtual ~RenderPass() = default;
    virtual void execute(RenderContext& context) = 0;
    virtual bool isEnabled() const { return isPassEnabled; }
    virtual void setIsEnabled(bool isPassEnabled) { this->isPassEnabled = isPassEnabled; };
    
    struct DrawArraysIndirectCommand {
        GLuint count;        
        GLuint instanceCount;
        GLuint first;        
        GLuint baseInstance; 
    };

private:
    bool isPassEnabled;

};