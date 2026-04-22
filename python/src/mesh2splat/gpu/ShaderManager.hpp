///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - Shader Manager Header               //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <memory>
#include <unordered_map>

namespace mesh2splat {

/// OpenGL shader program manager for the GPU converter
class ShaderManager {
public:
    ShaderManager();
    ~ShaderManager();
    
    /// Initialize shaders (must be called with valid GL context)
    /// @return true if successful
    bool initialize();
    
    /// Check if shaders are initialized
    bool isInitialized() const;
    
    /// Get the converter shader program ID
    unsigned int getConverterProgram() const;
    
    /// Use the converter shader program
    void useConverterProgram();
    
    /// Set uniform values
    void setUniform(const std::string& name, int value);
    void setUniform(const std::string& name, float value);
    void setUniform(const std::string& name, float x, float y);
    void setUniform(const std::string& name, float x, float y, float z);
    void setUniform(const std::string& name, float x, float y, float z, float w);
    
    /// Get any error message
    std::string getErrorMessage() const;
    
    /// Cleanup resources
    void cleanup();
    
    // Non-copyable
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Embedded shader sources (compatible with OpenGL 4.1/macOS)
namespace shaders {

/// Vertex shader source
extern const char* converterVS;

/// Geometry shader source  
extern const char* converterGS;

/// Fragment shader source
extern const char* converterFS;

} // namespace shaders

} // namespace mesh2splat
