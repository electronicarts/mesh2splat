///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "utils/glUtils.hpp"

class ShaderRegistry {
public:
    ShaderRegistry() = default;
    ~ShaderRegistry();

    void registerShaderProgram(const glUtils::ShaderProgramTypes, const std::vector<std::pair<std::string, GLenum>>& shaderPaths);

    bool reloadModifiedShaders(bool forceReload);

    GLuint getProgramID(const glUtils::ShaderProgramTypes);

private:

    std::vector<std::pair<std::string, GLenum>> getShaderPaths(const std::vector<glUtils::ShaderInfo>& shaders);

    std::unordered_map<std::string, std::shared_ptr<glUtils::ShaderFileInfo>> shaderFiles;
    std::unordered_map<glUtils::ShaderProgramTypes, glUtils::ShaderProgramInfo> shaderPrograms;
};
