///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - GLTF Loader Header                  //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Types.hpp"
#include <string>

namespace mesh2splat {

/// Load a GLTF/GLB file and return a Scene
class GltfLoader {
public:
    GltfLoader() = default;
    ~GltfLoader() = default;
    
    /// Load a GLTF or GLB file
    /// @param filePath Path to the GLTF/GLB file
    /// @return Scene containing all meshes and materials
    Scene load(const std::string& filePath);
    
    /// Check if a file is a GLTF/GLB file based on extension
    static bool isGltfFile(const std::string& filePath);
    
    /// Get any warnings or info messages from the last load
    const std::vector<std::string>& getMessages() const { return messages_; }
    
    /// Get error message from last load operation
    std::string getErrorMessage() const { return errorMessage_; }
    
private:
    std::vector<std::string> messages_;
    std::string errorMessage_;
    
    /// Compute tangent vectors for faces that don't have them
    void computeTangents(Face& face);
    
    /// Compute bounding box for a mesh
    void computeBBox(Mesh& mesh);
};

} // namespace mesh2splat
