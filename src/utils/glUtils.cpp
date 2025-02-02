#include "glUtils.hpp"

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

    //TODO: probably could find way to better generalize this
    void initializeShaderFileMonitoring(
        std::unordered_map<std::string, ShaderFileInfo>& shaderFiles,
        std::vector<std::pair<std::string, GLenum>>& converterShadersInfo,
        std::vector<std::pair<std::string, GLenum>>& computeShadersInfo,
        std::vector<std::pair<std::string, GLenum>>& radixSortPrePassShadersInfo,
        std::vector<std::pair<std::string, GLenum>>& radixSortGatherPassShadersInfo,
        std::vector<std::pair<std::string, GLenum>>& rendering3dgsShadersInfo,
        std::vector<std::pair<std::string, GLenum>>& rendering3dgsComputePrepassShadersInfo) {

        shaderFiles["converterVert"]                = { fs::last_write_time(CONVERTER_VERTEX_SHADER_LOCATION), CONVERTER_VERTEX_SHADER_LOCATION };
        shaderFiles["converterGeom"]                = { fs::last_write_time(CONVERTER_GEOM_SHADER_LOCATION),   CONVERTER_GEOM_SHADER_LOCATION };
        shaderFiles["converterFrag"]                = { fs::last_write_time(CONVERTER_FRAG_SHADER_LOCATION),   CONVERTER_FRAG_SHADER_LOCATION };
    
        shaderFiles["readerCompute"]                = { fs::last_write_time(TRANSFORM_COMPUTE_SHADER_LOCATION),   TRANSFORM_COMPUTE_SHADER_LOCATION };
    
        //TODO: add postPass
        shaderFiles["radixSortPrepassCompute"]      = { fs::last_write_time(RADIX_SORT_PREPASS_SHADER_LOCATION),   RADIX_SORT_PREPASS_SHADER_LOCATION };
        shaderFiles["radixSortGatherCompute"]       = { fs::last_write_time(RADIX_SORT_GATHER_SHADER_LOCATION),   RADIX_SORT_GATHER_SHADER_LOCATION };
    

        shaderFiles["rendererComputePrepass"]       = { fs::last_write_time(RENDERER_PREPASS_COMPUTE_SHADER_LOCATION),   RENDERER_PREPASS_COMPUTE_SHADER_LOCATION };
        shaderFiles["renderer3dgsVert"]             = { fs::last_write_time(RENDERER_VERTEX_SHADER_LOCATION),   RENDERER_VERTEX_SHADER_LOCATION };
        shaderFiles["renderer3dgsFrag"]             = { fs::last_write_time(RENDERER_FRAGMENT_SHADER_LOCATION),   RENDERER_FRAGMENT_SHADER_LOCATION };

        converterShadersInfo = {
            { CONVERTER_VERTEX_SHADER_LOCATION,     GL_VERTEX_SHADER   },
            { CONVERTER_GEOM_SHADER_LOCATION,       GL_GEOMETRY_SHADER },
            { CONVERTER_FRAG_SHADER_LOCATION,       GL_FRAGMENT_SHADER }
        };

        computeShadersInfo = {
            { TRANSFORM_COMPUTE_SHADER_LOCATION,    GL_COMPUTE_SHADER }
        };

        radixSortPrePassShadersInfo = {
            { RADIX_SORT_PREPASS_SHADER_LOCATION,    GL_COMPUTE_SHADER }
        };

        radixSortGatherPassShadersInfo = {
            { RADIX_SORT_GATHER_SHADER_LOCATION,    GL_COMPUTE_SHADER }
        };


        
        rendering3dgsComputePrepassShadersInfo = {
            { RENDERER_PREPASS_COMPUTE_SHADER_LOCATION,      GL_COMPUTE_SHADER },
        };

        rendering3dgsShadersInfo = {
            { RENDERER_VERTEX_SHADER_LOCATION,      GL_VERTEX_SHADER },
            { RENDERER_FRAGMENT_SHADER_LOCATION,    GL_FRAGMENT_SHADER }
        };
    }

    bool shaderFileChanged(const ShaderFileInfo& info) {
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

        // Compile each shader and attach to the program
        for (auto& pair : shaderInfos) {
            std::string filePath = pair.first;
            GLenum shaderType = pair.second;
            std::string shaderSource = readShaderFile(filePath.c_str());
            GLuint shaderID = compileShader(shaderSource.c_str(), shaderType);
        
            // If shader compilation failed, clean up and return old program
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


//TODO: must return the ID for each texture
void generateTextures(MaterialGltf material, std::map<std::string, TextureDataGl>& textureTypeMap)
{
    std::map<std::string, GLenum> textureUnits = {
        { BASE_COLOR_TEXTURE,           GL_TEXTURE0 },
        { NORMAL_TEXTURE,               GL_TEXTURE1 },
        { METALLIC_ROUGHNESS_TEXTURE,   GL_TEXTURE2 },
        { AO_TEXTURE,                   GL_TEXTURE3 },
        { EMISSIVE_TEXTURE,             GL_TEXTURE4 }
    };

    for (auto& textureTypeMapEntry : textureTypeMap)
    {
        const std::string& textureName = textureTypeMapEntry.first;
        TextureDataGl& textureDataGl = textureTypeMapEntry.second;
        unsigned char* textureData = textureDataGl.textureData;
        
        auto unitIt = textureUnits.find(textureName);
        if (unitIt == textureUnits.end())
            continue;

        GLenum textureUnit = unitIt->second;

        // Delete existing texture if it exists
        if (textureDataGl.glTextureID != 0)
        {
            glDeleteTextures(1, &textureDataGl.glTextureID);
            textureDataGl.glTextureID = 0;
        }

        // Determine width and height based on the texture type
        int width = 0;
        int height = 0;

        if (textureName == BASE_COLOR_TEXTURE)
        {
            width = material.baseColorTexture.width;
            height = material.baseColorTexture.height;
        }
        else if (textureName == NORMAL_TEXTURE)
        {
            width = material.normalTexture.width;
            height = material.normalTexture.height;
        }
        else if (textureName == METALLIC_ROUGHNESS_TEXTURE)
        {
            width = material.metallicRoughnessTexture.width;
            height = material.metallicRoughnessTexture.height;
        }
        else if (textureName == AO_TEXTURE)
        {
            width = material.occlusionTexture.width;
            height = material.occlusionTexture.height;
        }
        else if (textureName == EMISSIVE_TEXTURE)
        {
            width = material.emissiveTexture.width;
            height = material.emissiveTexture.height;
        }
        else
        {
            continue;
        }

        // Skip if the material doesn't have this texture or data is invalid
        if (width <= 0 || height <= 0 || textureData == nullptr)
            continue;

        GLuint texture;
        glActiveTexture(textureUnit);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        textureDataGl.glTextureID = texture;

        GLenum internalFormat = textureDataGl.bpp == 4 ? GL_RGBA : GL_RGB;
        GLenum format = internalFormat;

        glTexImage2D(
            GL_TEXTURE_2D,
            0, internalFormat,
            width, height,
            0, format,
            GL_UNSIGNED_BYTE,
            textureData
        );

        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 4); // Reduced from 40 to a reasonable level

        
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

    void generateMeshesVBO(const std::vector<Mesh>& meshes, std::vector<std::pair<Mesh, GLMesh>>& DataMeshAndGlMesh) {
    
        DataMeshAndGlMesh.clear();
        DataMeshAndGlMesh.reserve(meshes.size());

        for (auto& mesh : meshes) {
            GLMesh glMesh;
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
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertexStride, (void*)0);
            glEnableVertexAttribArray(0);
            // Normal attribute
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertexStride, (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            // Tangent attribute
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, vertexStride, (void*)(6 * sizeof(float)));
            glEnableVertexAttribArray(2);
            // UV attribute
            glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, vertexStride, (void*)(10 * sizeof(float)));
            glEnableVertexAttribArray(3);
            // Normalized UV attribute
            glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, vertexStride, (void*)(12 * sizeof(float)));
            glEnableVertexAttribArray(4);
            // Scale attribute
            glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, vertexStride, (void*)(14 * sizeof(float)));
            glEnableVertexAttribArray(5);

            //Should use array indices for per face data such as rotation and scale or directly compute it in the shader, should actually do it in a compute shader and be done

            // Unbind VAO
            glBindVertexArray(0);

            // Add to list of GLMeshes
            DataMeshAndGlMesh.push_back(std::make_pair(mesh, glMesh));
        }

    }

    GLuint* setupFrameBuffer(GLuint& framebuffer, unsigned int width, unsigned int height)
    {
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        const int numberOfTextures = 5;
    
        GLuint* textures = new GLuint[numberOfTextures];
        glGenTextures(numberOfTextures, textures);

        for (int i = 0; i < numberOfTextures; ++i) {
          glBindTexture(GL_TEXTURE_2D, textures[i]);
          glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
          glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[i], 0);
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Incomplete framebuffer status" << std::endl;
            exit(1);
        }

        GLenum drawBuffers[numberOfTextures] = {
            GL_COLOR_ATTACHMENT0
        };
        glDrawBuffers(numberOfTextures, drawBuffers);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return textures; 
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

    void setUniform3f(GLuint shaderProgram, std::string uniformName, glm::vec3 uniformValue)
    {
        GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

        if (uniformLocation == -1) {
            std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
        }

        glUniform3f(uniformLocation, uniformValue[0], uniformValue[1], uniformValue[2]);
    }

    void setUniform2f(GLuint shaderProgram, std::string uniformName, glm::vec2 uniformValue)
    {
        GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

        if (uniformLocation == -1) {
            std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
        }

        glUniform2f(uniformLocation, uniformValue[0], uniformValue[1]);
    }

    void setUniform2i(GLuint shaderProgram, std::string uniformName, glm::vec2 uniformValue)
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

    void read3dgsDataFromSsboBuffer(GLuint& indirectDrawCommandBuffer, GLuint& gaussianBuffer, GaussianDataSSBO*& gaussians, unsigned int& gaussianCount)
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

        gaussians = static_cast<GaussianDataSSBO*>(
            glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, gaussianBufferSize, GL_MAP_READ_BIT));

    }


    //TODO: this one is a bit different
    //void setupGaussianBufferSsbo(unsigned int width, unsigned int height, GLuint* gaussianBuffer)
    //{
    //    glGenBuffers(1, &(*gaussianBuffer));
    //    glBindBuffer(GL_SHADER_STORAGE_BUFFER, *gaussianBuffer);
    //    //TODO: I will categorize this hardcoding issue of the number of output float4 params from the SSBO as: ISSUE6
    //    GLsizeiptr bufferSize = width * height * sizeof(glm::vec4) * 6;
    //    glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_DYNAMIC_DRAW);
    //    unsigned int bindingPos = 5; //TODO: SSBO binding pos, should not hardcode it and should depend on how many drawbuffers from the framebuffer I want to read from
    //    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPos, *gaussianBuffer);
    //    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    //}

    void fillGaussianBufferSsbo(GLuint& gaussianBuffer, std::vector<GaussianDataSSBO>& gaussians)
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
