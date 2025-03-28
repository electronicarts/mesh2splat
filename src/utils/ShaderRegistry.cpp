///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "utils/ShaderRegistry.hpp"

ShaderRegistry::~ShaderRegistry() {
    for (auto& [name, program] : shaderPrograms) {
        if (glIsProgram(program.programID)) {
            glDeleteProgram(program.programID);
        }
    }
}

void ShaderRegistry::registerShaderProgram(const glUtils::ShaderProgramTypes programType, const std::vector<std::pair<std::string, GLenum>>& shaderPaths) {
    glUtils::ShaderProgramInfo program;
    program.programType = programType;

    //shared ptr done to avoid duplication of ShaderFileInfo objects when multiple shader programs share the same shader file
    for (const auto& [path, type] : shaderPaths) {
        if (shaderFiles.find(path) == shaderFiles.end()) {
            shaderFiles[path] = std::make_shared<glUtils::ShaderFileInfo>(glUtils::ShaderFileInfo{ path, fs::last_write_time(path) });
        }
        program.shaders.push_back({ type, shaderFiles[path] });
    }

    shaderPrograms[programType] = std::move(program);
}

bool ShaderRegistry::reloadModifiedShaders(bool forceReload = false) {
    bool reloadedAny = false;
    for (auto& [programType, program] : shaderPrograms) {
        bool shouldReload = forceReload || std::any_of(program.shaders.begin(), program.shaders.end(),
            [](const glUtils::ShaderInfo& shader) {
                return fs::last_write_time(shader.fileInfo->filePath) > shader.fileInfo->lastModified;
            });

        if (shouldReload) {
            for (auto& shader : program.shaders) {
                shader.fileInfo->lastModified = fs::last_write_time(shader.fileInfo->filePath);
            }

            program.programID = glUtils::reloadShaderPrograms(
                getShaderPaths(program.shaders),
                program.programID
            );

            reloadedAny = true;
        }
    }
    return reloadedAny;
}

GLuint ShaderRegistry::getProgramID(const glUtils::ShaderProgramTypes programType) {
    return shaderPrograms[programType].programID;
}

std::vector<std::pair<std::string, GLenum>> ShaderRegistry::getShaderPaths(const std::vector<glUtils::ShaderInfo>& shaders) {
    std::vector<std::pair<std::string, GLenum>> paths;
    for (const auto& shader : shaders) {
        paths.emplace_back(shader.fileInfo->filePath, shader.shaderType);
    }
    return paths;
}
