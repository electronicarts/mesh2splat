///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "glUtils.hpp"
#include "utils/shaderRegistry.hpp"

namespace glUtils 
{

    GLuint compileShader(const char* source, GLenum type) {
        GLint success;
        GLchar infoLog[512];
        GLuint shaderID = glCreateShader(type);
        glShaderSource(shaderID, 1, &source, NULL);
        glCompileShader(shaderID);

        glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shaderID, 512, NULL, infoLog);
            std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
        }

        return shaderID;
    }

    std::string readShaderFile(const char* filePath) {
        std::ifstream shaderFile;
        std::stringstream shaderStream;

        // Ensure ifstream objects can throw exceptions:
        shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

        try {
            shaderFile.open(filePath);
            shaderStream << shaderFile.rdbuf();
            shaderFile.close();
            return shaderStream.str();
        }
        catch (std::ifstream::failure& e) {
            std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ: " << e.what() << std::endl;
            exit(1);
        }
    }

    ShaderLocations shaderLocations;

    //Add new shader location here
    void initializeShaderLocations() {

        fs::path shadersBase = fs::path(__FILE__).parent_path() / "../shaders"; //Todo: rather move the shader files relative to the exe loc
    
        shaderLocations.converterVertexShaderLocation                   = (shadersBase / "conversion" / "converterVS.glsl").string();
        shaderLocations.converterGeomShaderLocation                     = (shadersBase / "conversion" / "converterGS.glsl").string();
        shaderLocations.eigenDecompositionShaderLocation                = (shadersBase / "conversion" / "eigendecomposition.glsl").string();
        shaderLocations.converterFragShaderLocation                     = (shadersBase / "conversion" / "converterFS.glsl").string();

        shaderLocations.transformComputeShaderLocation                  = (shadersBase / "rendering" / "frameBufferReaderCS.glsl").string();

        shaderLocations.radixSortPrepassShaderLocation                  = (shadersBase / "rendering" / "radixSortPrepass.glsl").string();
        shaderLocations.radixSortGatherShaderLocation                   = (shadersBase / "rendering" / "radixSortGather.glsl").string();

        shaderLocations.rendererPrepassComputeShaderLocation            = (shadersBase / "rendering" / "gaussianSplattingPrepassCS.glsl").string();

        shaderLocations.rendererVertexShaderLocation                    = (shadersBase / "rendering" / "gaussianSplattingVS.glsl").string();
        shaderLocations.rendererFragmentShaderLocation                  = (shadersBase / "rendering" / "gaussianSplattingPS.glsl").string();

        shaderLocations.rendererDeferredRelightingVertexShaderLocation  = (shadersBase / "rendering" / "gaussianSplattingDeferredVS.glsl").string();
        shaderLocations.rendererDeferredRelightingFragmentShaderLocation = (shadersBase / "rendering" / "gaussianSplattingDeferredPS.glsl").string();

        shaderLocations.shadowsPrepassComputeShaderLocation             = (shadersBase / "rendering" / "gaussianPointShadowMappingCS.glsl").string();
        shaderLocations.shadowsCubemapVertexShaderLocation              = (shadersBase / "rendering" / "gaussianPointLightCubeMapShadowVS.glsl").string();
        shaderLocations.shadowsCubemapFragmentShaderLocation            = (shadersBase / "rendering" / "gaussianPointLightCubeMapShadowPS.glsl").string();

        shaderLocations.depthPrepassVertexShaderLocation                = (shadersBase / "rendering" / "depthPrepassVS.glsl").string();
        shaderLocations.depthPrepassFragmentShaderLocation                = (shadersBase / "rendering" / "depthPrepassPS.glsl").string();
    }

    void initializeShaderFileMonitoring(ShaderRegistry& shaderRegistry)
    {
        // CONVERSION PASS
        shaderRegistry.registerShaderProgram(ShaderProgramTypes::ConverterProgram, {
            { shaderLocations.converterVertexShaderLocation, GL_VERTEX_SHADER },
            { shaderLocations.converterGeomShaderLocation, GL_GEOMETRY_SHADER },
            { shaderLocations.converterFragShaderLocation, GL_FRAGMENT_SHADER }
        });
        shaderRegistry.registerShaderProgram(ShaderProgramTypes::ComputeTransformProgram, {
            { shaderLocations.transformComputeShaderLocation, GL_COMPUTE_SHADER }
        });

        // SORTING
        shaderRegistry.registerShaderProgram(ShaderProgramTypes::RadixSortPrepassProgram, {
            { shaderLocations.radixSortPrepassShaderLocation, GL_COMPUTE_SHADER }
        });
        shaderRegistry.registerShaderProgram(ShaderProgramTypes::RadixSortGatherComputeProgram, {
            { shaderLocations.radixSortGatherShaderLocation, GL_COMPUTE_SHADER }
        });

        // 3DGS RENDERING
        shaderRegistry.registerShaderProgram(ShaderProgramTypes::PrepassFiltering3dgsProgram, {
            { shaderLocations.rendererPrepassComputeShaderLocation, GL_COMPUTE_SHADER }
        });

        shaderRegistry.registerShaderProgram(ShaderProgramTypes::Rendering3dgsProgram, {
            { shaderLocations.rendererVertexShaderLocation, GL_VERTEX_SHADER },
            { shaderLocations.rendererFragmentShaderLocation, GL_FRAGMENT_SHADER }
        });

        // DEFERRED LIGHTING PASS
        shaderRegistry.registerShaderProgram(ShaderProgramTypes::DeferredRelightingPassProgram, {
            { shaderLocations.rendererDeferredRelightingVertexShaderLocation, GL_VERTEX_SHADER },
            { shaderLocations.rendererDeferredRelightingFragmentShaderLocation, GL_FRAGMENT_SHADER }
        });

        // SHADOW PASS
        shaderRegistry.registerShaderProgram(ShaderProgramTypes::ShadowPrepassComputeProgram, {
            { shaderLocations.shadowsPrepassComputeShaderLocation, GL_COMPUTE_SHADER }
        });
        shaderRegistry.registerShaderProgram(ShaderProgramTypes::ShadowCubemapPassProgram, {
            { shaderLocations.shadowsCubemapVertexShaderLocation, GL_VERTEX_SHADER },
            { shaderLocations.shadowsCubemapFragmentShaderLocation, GL_FRAGMENT_SHADER }
        });

        //MESH DEPTH RENDERING
        shaderRegistry.registerShaderProgram(ShaderProgramTypes::DepthPrepassProgram, {
            { shaderLocations.depthPrepassVertexShaderLocation, GL_VERTEX_SHADER },
            { shaderLocations.depthPrepassFragmentShaderLocation, GL_FRAGMENT_SHADER }
        });

    }

    bool shaderFileChanged(const ShaderFileEditingInfo& info) {
        auto currentWriteTime = fs::last_write_time(info.filePath);
        return (currentWriteTime != info.lastWriteTime);
    }

    GLuint reloadShaderPrograms(
        const std::vector<std::pair<std::string, GLenum>>& shaderInfos,
        GLuint oldProgram)
    {
        GLuint newProgram = glCreateProgram();
        std::vector<GLuint> shaderIDs;
        shaderIDs.reserve(shaderInfos.size());

        for (auto& pair : shaderInfos) {
            std::string filePath = pair.first;
            GLenum shaderType = pair.second;
            std::string shaderSource = readShaderFile(filePath.c_str());
            GLuint shaderID = compileShader(shaderSource.c_str(), shaderType);
        
            if (!shaderID) {
                std::cerr << "Failed to compile shader: " << filePath << std::endl;
                for (GLuint id : shaderIDs) {
                    glDeleteShader(id);
                }
                glDeleteProgram(newProgram);
                return oldProgram;
            }

            glAttachShader(newProgram, shaderID);
            shaderIDs.push_back(shaderID);
        }

        glLinkProgram(newProgram);

        GLint success;
        glGetProgramiv(newProgram, GL_LINK_STATUS, &success);
        if (!success) {
            GLchar infoLog[512];
            glGetProgramInfoLog(newProgram, sizeof(infoLog), nullptr, infoLog);
            std::cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;

            for (GLuint shaderID : shaderIDs) {
                glDeleteShader(shaderID);
            }
            glDeleteProgram(newProgram);
            return oldProgram;
        }

        for (GLuint shaderID : shaderIDs) {
            glDeleteShader(shaderID);
        }

        if (oldProgram != 0) {
            glDeleteProgram(oldProgram);
        }

        return newProgram;
    }


    GLenum mapTextureTypeToUnit(const std::string &textureType)
    {
        if (textureType == "baseColorTexture")
            return GL_TEXTURE0;
        else if (textureType == "normalTexture")
            return GL_TEXTURE1;
        else if (textureType == "metallicRoughnessTexture")
            return GL_TEXTURE2;
        else if (textureType == "occlusionTexture")
            return GL_TEXTURE3;
        else if (textureType == "emissiveTexture")
            return GL_TEXTURE4;
        return GL_TEXTURE0; 
    }

    void generateTextures(std::map<std::string, std::map<std::string, utils::TextureDataGl>>& meshToTextureData)
    {
        // For each mesh entry in meshToTextureData
        for (auto &meshEntry : meshToTextureData)
        {
            const std::string& meshName = meshEntry.first;
            auto &textureMap = meshEntry.second; // map<string, TextureDataGl>

            // For each texture type in that mesh's texture map
            for (auto &texturePair : textureMap)
            {
                const std::string &textureType = texturePair.first;      
                utils::TextureDataGl &textureDataGl = texturePair.second;

                // If we want to pick a texture unit based on "textureType", do that here
                // If you want each mesh to have separate sets of texture units, you might do that differently.
                GLenum textureUnit = mapTextureTypeToUnit(textureType);

                // Delete existing texture if it exists
                if (textureDataGl.glTextureID != 0) {
                    glDeleteTextures(1, &textureDataGl.glTextureID);
                    textureDataGl.glTextureID = 0;
                }

                // Skip if no data
                if (textureDataGl.width <= 0 ||
                    textureDataGl.height <= 0 ||
                    textureDataGl.textureData.empty())
                {
                    continue;
                }

                // Now create and upload to GL
                GLuint texture;
                glActiveTexture(textureUnit);
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);

                textureDataGl.glTextureID = texture;

                GLenum internalFormat = (textureDataGl.channels == 4) ? GL_RGBA : GL_RGB;
                GLenum format = internalFormat;

                glTexImage2D(GL_TEXTURE_2D,
                             0,
                             internalFormat,
                             textureDataGl.width,
                             textureDataGl.height,
                             0,
                             format,
                             GL_UNSIGNED_BYTE,
                             textureDataGl.textureData.data());

                glGenerateMipmap(GL_TEXTURE_2D);

                // Set wrapping/filtering
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 4);

                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
    }

    void generateMeshesVBO(const std::vector<utils::Mesh>& meshes, std::vector<std::pair<utils::Mesh, utils::GLMesh>>& DataMeshAndGlMesh) {
    
        DataMeshAndGlMesh.clear();
        DataMeshAndGlMesh.reserve(meshes.size());

        for (auto& mesh : meshes) {
            utils::GLMesh glMesh;
            std::vector<float> vertices;  // Buffer to hold all vertex data
        
            // Convert mesh data to a flat array of floats (position only for this example)
            for (const auto& face : mesh.faces) {
                for (int i = 0; i < 3; ++i) { // Assuming each face is a triangle (and it must be as we are only reading .gltf/.glb files)
                    // Position
                    vertices.push_back(face.pos[i].x);
                    vertices.push_back(face.pos[i].y);
                    vertices.push_back(face.pos[i].z);

                    // Normal
                    vertices.push_back(face.normal[i].x);
                    vertices.push_back(face.normal[i].y);
                    vertices.push_back(face.normal[i].z);

                    // Tangent
                    vertices.push_back(face.tangent[i].x);
                    vertices.push_back(face.tangent[i].y);
                    vertices.push_back(face.tangent[i].z);
                    vertices.push_back(face.tangent[i].w);

                    // UV
                    vertices.push_back(face.uv[i].x);
                    vertices.push_back(face.uv[i].y);

                    // NORMALIZED UV
                    vertices.push_back(face.normalizedUvs[i].x);
                    vertices.push_back(face.normalizedUvs[i].y);

                    // Scale
                    vertices.push_back(face.scale.x);
                    vertices.push_back(face.scale.y);
                    vertices.push_back(face.scale.z);

                }
            
            }

            glMesh.vertexCount = vertices.size() / 17; // Number of vertices

            // 3 position, 3 normal, 4 tangent, 2 UV, 2 NORMALIZED UVs, 3 scale = 17
            size_t vertexStride = 17 * sizeof(float);

            // Generate and bind VAO
            glGenVertexArrays(1, &glMesh.vao);
            glBindVertexArray(glMesh.vao);

            // Generate and bind VBO
            glGenBuffers(1, &glMesh.vbo);
            glBindBuffer(GL_ARRAY_BUFFER, glMesh.vbo);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

            // Vertex attribute pointers
            // Position attribute
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (GLsizei)vertexStride, (void*)0);
            glEnableVertexAttribArray(0);
            // Normal attribute
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, (GLsizei)vertexStride, (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            // Tangent attribute
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, (GLsizei)vertexStride, (void*)(6 * sizeof(float)));
            glEnableVertexAttribArray(2);
            // UV attribute
            glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, (GLsizei)vertexStride, (void*)(10 * sizeof(float)));
            glEnableVertexAttribArray(3);
            // Normalized UV attribute
            glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, (GLsizei)vertexStride, (void*)(12 * sizeof(float)));
            glEnableVertexAttribArray(4);
            // Scale attribute
            glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, (GLsizei)vertexStride, (void*)(14 * sizeof(float)));
            glEnableVertexAttribArray(5);

            //Should use array indices for per face data such as rotation and scale or directly compute it in the shader, should actually do it in a compute shader and be done

            // Unbind VAO
            glBindVertexArray(0);

            // Add to list of GLMeshes
            DataMeshAndGlMesh.push_back(std::make_pair(mesh, glMesh));
        }

    }

    GLuint setupFrameBuffer(GLuint& framebuffer, unsigned int width, unsigned int height)
    {
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

        GLuint dummyRenderbuffer;
        glGenRenderbuffers(1, &dummyRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, dummyRenderbuffer);

        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, width, height);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, dummyRenderbuffer);

        // Check that the framebuffer is complete.
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Dummy framebuffer is not complete!" << std::endl;
            exit(1);
        }

        // Unbind the framebuffer (optional).
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        return dummyRenderbuffer;
    }

    void setupTransformFeedback(size_t bufferSize, GLuint& feedbackBuffer, GLuint& feedbackVAO, GLuint& acBuffer, unsigned int totalStride) {

        // Create a buffer for storing feedback
        glGenBuffers(1, &feedbackBuffer);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, feedbackBuffer);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, bufferSize * sizeof(float), NULL, GL_STATIC_READ);
    
        // Create a VAO for transform feedback
        glGenVertexArrays(1, &feedbackVAO);
        glBindVertexArray(feedbackVAO);

        glBindBuffer(GL_ARRAY_BUFFER, feedbackBuffer);
        //Gaussian mean (position) attribute
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, totalStride, (void*)0);
        // Scale attribute
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, totalStride, (void*)(3 * sizeof(float))); 
        // Normal attribute
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, totalStride, (void*)(6 * sizeof(float)));
        // Quaternion attribute
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, totalStride, (void*)(9 * sizeof(float)));
        // Rgba attribute
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, totalStride, (void*)(13 * sizeof(float)));
        // MetallicRoughness attribute
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, totalStride, (void*)(17 * sizeof(float)));

        glBindVertexArray(0);

        // Bind the buffer to the transform feedback binding point
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, feedbackBuffer);
    }

    template<typename T>
    void setUniform(GLuint shaderProgram, std::string uniformName, T uniformValue)
    {
        GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

        if (uniformLocation == -1) {
            std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
        }
        //TODO: I am open to a better solution
        if constexpr (std::is_same_v<T, float>) {
            glUniform1f(uniformLocation, uniformValue);
        }
        else if constexpr (std::is_same_v<T, int>) {
            glUniform1i(uniformLocation, uniformValue);
        }
        else if constexpr (std::is_same_v<T, glm::vec2>) {
            glUniform2fv(uniformLocation, 1, &uniformValue[0]);
        }
        else if constexpr (std::is_same_v<T, glm::vec3>) {
            glUniform3fv(uniformLocation, 1, &uniformValue[0]);
        }
        else if constexpr (std::is_same_v<T, glm::vec4>) {
            glUniform4fv(uniformLocation, 1, &uniformValue[0]);
        }
        else if constexpr (std::is_same_v<T, glm::mat4>) {
            glUniformMatrix4fv(uniformLocation, 1, GL_FALSE, &uniformValue[0][0]);
        }
        else {
            static_assert(sizeof(T) == 0, "setUniform: Unsupported uniform type.");
        }
    }

    void setUniform1f(GLuint shaderProgram, std::string uniformName, float uniformValue)
    {
        GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

        if (uniformLocation == -1) {
            std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
        }

        glUniform1f(uniformLocation, uniformValue);
    }

    //TODO: holy moly, the radix sort was being zeroes because I was setting an int rather than an unsigned int... 
    void setUniform1i(GLuint shaderProgram, std::string uniformName, int uniformValue)
    {
        GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

        if (uniformLocation == -1) {
            std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
        }

        glUniform1i(uniformLocation, uniformValue);
    }


    void setUniform1ui(GLuint shaderProgram, std::string uniformName, unsigned int uniformValue)
    {
        GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

        if (uniformLocation == -1) {
            std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
        }

        glUniform1ui(uniformLocation, uniformValue);
    }

    void setUniform1uiv(GLuint shaderProgram, std::string uniformName, unsigned int* uniformValue, int count)
    {
        GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

        if (uniformLocation == -1) {
            std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
        }

        glUniform1uiv(uniformLocation, count, &uniformValue[0]);
    }

    void setUniform3f(GLuint shaderProgram, std::string uniformName, glm::vec3 uniformValue)
    {
        GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

        if (uniformLocation == -1) {
            std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
        }

        glUniform3f(uniformLocation, uniformValue[0], uniformValue[1], uniformValue[2]);
    }

    void setUniform4f(GLuint shaderProgram, std::string uniformName, glm::vec4 uniformValue)
    {
        GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

        if (uniformLocation == -1) {
            std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
        }

        glUniform4f(uniformLocation, uniformValue[0], uniformValue[1], uniformValue[2], uniformValue[3]);
    }

    void setUniform2f(GLuint shaderProgram, std::string uniformName, glm::vec2 uniformValue)
    {
        GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

        if (uniformLocation == -1) {
            std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
        }

        glUniform2f(uniformLocation, uniformValue[0], uniformValue[1]);
    }

    void setUniform2i(GLuint shaderProgram, std::string uniformName, glm::ivec2 uniformValue)
    {
        GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

        if (uniformLocation == -1) {
            std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
        }

        glUniform2i(uniformLocation, uniformValue[0], uniformValue[1]);
    }


    void setUniformMat4(GLuint shaderProgram, std::string uniformName, glm::mat4 matrix)
    {
        GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

        if (uniformLocation == -1) {
            std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
        }

        glUniformMatrix4fv(uniformLocation, 1, GL_FALSE, &matrix[0][0]);
    }

    
    void setUniformMat4v(GLuint shaderProgram, std::string uniformName, std::vector<glm::mat4> matrices, unsigned int count)
    {
        GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

        if (uniformLocation == -1) {
            std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
        }

        glUniformMatrix4fv(uniformLocation, count, GL_FALSE, glm::value_ptr(matrices[0]));
    }

    void setTexture2D(GLuint shaderProgram, std::string textureUniformName, GLuint texture, unsigned int textureUnitNumber)
    {
        GLint uniformLocation = glGetUniformLocation(shaderProgram, textureUniformName.c_str());

        if (uniformLocation == -1) {
            std::cerr << "Could not find uniform: '" + textureUniformName + "'." << std::endl;
        }
        glActiveTexture(GL_TEXTURE0 + textureUnitNumber);
        glBindTexture(GL_TEXTURE_2D, texture);

        glUniform1i(uniformLocation, textureUnitNumber);
    }



    static void writeAttachmentToPNG(const std::vector<float>& pixels, int width, int height, const std::string& filename) {
        int channels = 4; // RGBA
        std::vector<unsigned char> imageData(width * height * channels);
        for (int i = 0; i < width * height * channels; ++i) {
            // Clamp values to [0,1] and convert to 0-255 range
            float val = glm::clamp(pixels[i], 0.0f, 1.0f);
            imageData[i] = static_cast<unsigned char>(val * 255.0f);
        }
        // Write PNG file (row stride = width * channels)
        stbi_write_png(filename.c_str(), width, height, channels, imageData.data(), width * channels);
    }

    void read3dgsDataFromSsboBuffer(GLuint& indirectDrawCommandBuffer, GLuint& gaussianBuffer, utils::GaussianDataSSBO*& gaussians, unsigned int& gaussianCount)
    {
        glFinish();
        //TODO: Should read this structs from where they are declared initally or common file, not repeating it like this
        struct DrawArraysIndirectCommand {
            GLuint count;        
            GLuint instanceCount;    
            GLuint first;        
            GLuint baseInstance;
        };

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, indirectDrawCommandBuffer);
        DrawArraysIndirectCommand* drawCmd = static_cast<DrawArraysIndirectCommand*>(
            glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(DrawArraysIndirectCommand), GL_MAP_READ_BIT)
        );
        if (!drawCmd) {
            std::cerr << "Failed to map drawCommandBuffer." << std::endl;
            return;
        }
        gaussianCount = drawCmd->instanceCount;
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        // Bind and map the Gaussian buffer to read vertex data
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, gaussianBuffer);
        size_t gaussianBufferSize = gaussianCount * sizeof(glm::vec4) * 6; //TODO: ISSUE6

        gaussians = static_cast<utils::GaussianDataSSBO*>(
            glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, gaussianBufferSize, GL_MAP_READ_BIT));

    }

    void fillGaussianBufferSsbo(GLuint& gaussianBuffer, std::vector<utils::GaussianDataSSBO>& gaussians)
    {
        glGenBuffers(1, &gaussianBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, gaussianBuffer);
        //TODO: I will categorize this hardcoding issue of the number of output float4 params from the SSBO as: ISSUE6
        GLsizeiptr bufferSize = gaussians.size() * sizeof(glm::vec4) * 6;
        glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, gaussians.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    void fillGaussianBufferSsbo(GLuint& gaussianBuffer, unsigned int size)
    {
        glGenBuffers(1, &gaussianBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, gaussianBuffer);
        //TODO: I will categorize this hardcoding issue of the number of output float4 params from the SSBO as: ISSUE6
        GLsizeiptr bufferSize = size * sizeof(glm::vec4) * 6;
        glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    void resetAtomicCounter(GLuint atomicCounterBuffer)
    {
        uint32_t zeroVal = 0;
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffer);
        glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(uint32_t), &zeroVal);
    }

}
