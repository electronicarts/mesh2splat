#include "shaderUtils.hpp"

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

GLuint createShaderProgram(unsigned int& transformFeedbackVertexStride) {

    std::string vertexShaderSource          = readShaderFile(VERTEX_SHADER_LOCATION);
    std::string tessControlShaderSource     = readShaderFile(TESS_CONTROL_SHADER_LOCATION);
    std::string tessEvaluationShaderSource  = readShaderFile(TESS_EVAL_SHADER_LOCATION);
    std::string geomShaderSource            = readShaderFile(GEOM_SHADER_LOCATION);
    std::string fragmentShaderSource        = readShaderFile(FRAG_SHADER_LOCATION);


    GLuint vertexShader         = compileShader(vertexShaderSource.c_str(), GL_VERTEX_SHADER);
    GLuint tessControlShader    = compileShader(tessControlShaderSource.c_str(), GL_TESS_CONTROL_SHADER);
    GLuint tessEvaluationShader = compileShader(tessEvaluationShaderSource.c_str(), GL_TESS_EVALUATION_SHADER);
    GLuint geomShader           = compileShader(geomShaderSource.c_str(), GL_GEOMETRY_SHADER);
    GLuint fragmentShader       = compileShader(fragmentShaderSource.c_str(), GL_FRAGMENT_SHADER);


    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, tessControlShader);
    glAttachShader(program, tessEvaluationShader);
    glAttachShader(program, geomShader);
    glAttachShader(program, fragmentShader);

    //const GLchar* feedbackVaryings[] = { "GaussianPosition", "Scale", "Normal", "Quaternion", "Rgba"};  // the name of the varying to capture
    //glTransformFeedbackVaryings(program, 5, feedbackVaryings, GL_INTERLEAVED_ATTRIBS);
    //              GaussianPosition (3), Scale (3), Normal(3), Quaternion(4), Rgba(4)
    //transformFeedbackVertexStride = 3 + 3 + 3 + 4 + 4;

    GLint success;
    const int maxMsgLength = 512;
    GLchar infoLog[maxMsgLength];
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, maxMsgLength, NULL, infoLog);
        std::cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    return program;
}

float calcTriangleArea(glm::vec2 p1, glm::vec2 p2, glm::vec2 p3)
{
    float a = glm::length(p1 - p2);
    float b = glm::length(p1 - p3);
    float c = glm::length(p2 - p3);

    float p = (a + b + c) / 2.0;

    return sqrt(p * (p - a) * (p - b) * (p - c));
}

void findMedianFromFloatVector(std::vector<float>& vec, float& result)
{
    std::sort(vec.begin(), vec.end());

    size_t size = vec.size();
    if (size % 2 == 0) {
        result = (vec[size / 2 - 1] + vec[size / 2]) / 2;
    }
    else {
        result = vec[size / 2];
    }
}



void uploadTextures(std::map<std::string, std::pair<unsigned char*, int>>& textureTypeMap, MaterialGltf material)
{
    std::map<std::string, GLenum> textureUnits = {
        { BASE_COLOR_TEXTURE,           GL_TEXTURE0 },
        { NORMAL_TEXTURE,               GL_TEXTURE1 },
        { METALLIC_ROUGHNESS_TEXTURE,   GL_TEXTURE2 },
        { OCCLUSION_TEXTURE,            GL_TEXTURE3 },
        { EMISSIVE_TEXTURE,             GL_TEXTURE4 }
    };

    for (auto& textureType : textureTypeMap)
    {
        const std::string& textureName = textureType.first;
        unsigned char* textureData = textureType.second.first;

        if (textureUnits.find(textureName) != textureUnits.end())
        {
            GLuint texture;
            GLenum textureUnit = textureUnits[textureName];

            glActiveTexture(textureUnit);
            glGenTextures(1, &texture); 
            glBindTexture(GL_TEXTURE_2D, texture);

            int width, height;
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
            else if (textureName == OCCLUSION_TEXTURE)
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

            GLenum internalFormat = GL_RGB;
            GLenum format = GL_RGB;

            if (textureType.second.second == 4)
            {
                internalFormat = GL_RGBA;
                format = GL_RGBA;
            }

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
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            //TODO: this is trash, oh mamma mia this is really bad. Change it sooner than later.
            //Before assignment it was the Bpp and now it is the textureID........
            textureType.second.second = texture;

            GLenum error = glGetError();
            if (error != GL_NO_ERROR) {
                std::cerr << "OpenGL error: " << error << " for texture: " << textureName << std::endl;
            }
        }
    }
}


std::vector<GLMesh> uploadMeshesToOpenGL(const std::vector<Mesh>& meshes) {
    
    std::vector<GLMesh> glMeshes;
    glMeshes.reserve(meshes.size());

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

        glMesh.vertexCount = vertices.size() / 3; // Number of vertices

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
        // UV attribute
        glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, vertexStride, (void*)(12 * sizeof(float)));
        glEnableVertexAttribArray(4);
        // Scale attribute
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, vertexStride, (void*)(14 * sizeof(float)));
        glEnableVertexAttribArray(5);

        //Should use array indices for per face data such as rotation and scale or directly compute it in the shader, should actually do it in a compute shader and be done

        // Unbind VAO
        glBindVertexArray(0);

        // Add to list of GLMeshes
        glMeshes.push_back(glMesh);
    }

    return glMeshes;
}

void setupFrameBuffer(GLuint& framebuffer, unsigned int width, unsigned int height)
{
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    const int numberOfTextures = 5;
    GLuint textures[numberOfTextures];
    glGenTextures(numberOfTextures, textures);

    for (int i = 0; i < numberOfTextures; ++i) {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, textures[i], 0);
    }

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Incomplete framebuffer status" << std::endl;
        exit(1);
    }

    GLenum drawBuffers[numberOfTextures] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };
    glDrawBuffers(numberOfTextures, drawBuffers);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

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

//TODO: declare in .hpp both these funcs
static void setUniform1f(GLuint shaderProgram, std::string uniformName, float uniformValue)
{
    GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

    if (uniformLocation == -1) {
        std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
    }

    glUniform1f(uniformLocation, uniformValue);
}
//Also you...
static void setUniform1i(GLuint shaderProgram, std::string uniformName, unsigned int uniformValue)
{
    GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

    if (uniformLocation == -1) {
        std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
    }

    glUniform1i(uniformLocation, uniformValue);
}

static void setUniform3f(GLuint shaderProgram, std::string uniformName, glm::vec3 uniformValue)
{
    GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

    if (uniformLocation == -1) {
        std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
    }

    glUniform3f(uniformLocation, uniformValue[0], uniformValue[1], uniformValue[2]);
}

static void setUniform2f(GLuint shaderProgram, std::string uniformName, glm::vec2 uniformValue)
{
    GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

    if (uniformLocation == -1) {
        std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
    }

    glUniform2f(uniformLocation, uniformValue[0], uniformValue[1]);
}


static void setUniformMat4(GLuint shaderProgram, std::string uniformName, glm::mat4 matrix)
{
    GLint uniformLocation = glGetUniformLocation(shaderProgram, uniformName.c_str());

    if (uniformLocation == -1) {
        std::cerr << "Could not find uniform: '" + uniformName + "'." << std::endl;
    }

    glUniform4fv(uniformLocation, 1, glm::value_ptr(matrix));
}

static void setTexture(GLuint shaderProgram, std::string textureUniformName, GLuint texture, unsigned int textureUnitNumber)
{
    GLint uniformLocation = glGetUniformLocation(shaderProgram, textureUniformName.c_str());

    if (uniformLocation == -1) {
        std::cerr << "Could not find uniform: '" + textureUniformName + "'." << std::endl;
    }
    glActiveTexture(GL_TEXTURE0 + textureUnitNumber);
    glBindTexture(GL_TEXTURE_2D, texture);

    glUniform1i(uniformLocation, textureUnitNumber);
}

void performTessellationAndCapture(
    GLuint shaderProgram, GLuint vao,
    GLuint framebuffer, size_t vertexCount,
    GLuint& numGaussiansGenerated, GLuint& acBuffer,
    int normalizedUVSpaceWidth, int normalizedUVSpaceHeight,
    const std::map<std::string, std::pair<unsigned char*, int>>& textureTypeMap
) {
    // Use shader program and perform tessellation
    glUseProgram(shaderProgram);

    //-------------------------------SET UNIFORMS-------------------------------   
    setUniform1i(shaderProgram, "tesselationFactorMultiplier", TESSELATION_LEVEL_FACTOR_MULTIPLIER);

    //Textures
    if (textureTypeMap.find(BASE_COLOR_TEXTURE) != textureTypeMap.end())
    {
        setTexture(shaderProgram, "albedoTexture", textureTypeMap.at(BASE_COLOR_TEXTURE).second,                        0);
    }
    if (textureTypeMap.find(NORMAL_TEXTURE) != textureTypeMap.end())
    {
        setTexture(shaderProgram, "normalTexture", textureTypeMap.at(NORMAL_TEXTURE).second,                            1);
    }
    if (textureTypeMap.find(METALLIC_ROUGHNESS_TEXTURE) != textureTypeMap.end())
    {
        setTexture(shaderProgram, "metallicRoughnessTexture", textureTypeMap.at(METALLIC_ROUGHNESS_TEXTURE).second,     2);
    }
    if (textureTypeMap.find(OCCLUSION_TEXTURE) != textureTypeMap.end())
    {
        setTexture(shaderProgram, "occlusionTexture", textureTypeMap.at(OCCLUSION_TEXTURE).second,                      3);
    }
    if (textureTypeMap.find(EMISSIVE_TEXTURE) != textureTypeMap.end())
    {
        setTexture(shaderProgram, "emissiveTexture", textureTypeMap.at(EMISSIVE_TEXTURE).second,                        4);
    }

    //--------------------------------------------------------------------------
    //Query for true conversion time

    GLuint queryID;
    glGenQueries(1, &queryID);

    glBindVertexArray(vao);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBeginQuery(GL_TIME_ELAPSED, queryID);

    glDrawArrays(GL_PATCHES, 0, vertexCount);

    glEndQuery(GL_TIME_ELAPSED);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GLuint64 elapsed_time;
    glGetQueryObjectui64v(queryID, GL_QUERY_RESULT, &elapsed_time);

    // Convert nanoseconds to milliseconds
    double elapsed_time_ms = elapsed_time / 1000000.0f;

    std::cout << "Draw call time: " << elapsed_time_ms << " ms" << std::endl;

}

void readAndSaveToPly(const float* feedbackData, GLuint numberOfGeneratedGaussians, const std::string& filename, unsigned int stride) {
    std::ofstream objFile(filename);
    if (!objFile.is_open()) {
        std::cerr << "Failed to open the file for writing: " << filename << std::endl;
        return;
    }
    std::vector<Gaussian3D> gaussians_3D_list; //TODO: Think if can allocate space instead of having the vector dynamic size
    gaussians_3D_list.reserve(MAX_TEXTURE_SIZE * MAX_TEXTURE_SIZE);

    // Write each vertex position
    for (size_t i = 0; i < numberOfGeneratedGaussians; ++i) {
        float pos_x = feedbackData[stride * i + 0];         
        float pos_y = feedbackData[stride * i + 1];     
        float pos_z = feedbackData[stride * i + 2];     

        float scale_x = feedbackData[stride * i + 3];   
        float scale_y = feedbackData[stride * i + 4];
        float scale_z = feedbackData[stride * i + 5];

        float normal_x = feedbackData[stride * i + 6];
        float normal_y = feedbackData[stride * i + 7];
        float normal_z = feedbackData[stride * i + 8];

        float quaternion_x = feedbackData[stride * i + 9];
        float quaternion_y = feedbackData[stride * i + 10];
        float quaternion_z = feedbackData[stride * i + 11];
        float quaternion_w = feedbackData[stride * i + 12];

        float rgba_r = feedbackData[stride * i + 13];
        float rgba_g = feedbackData[stride * i + 14];
        float rgba_b = feedbackData[stride * i + 15];
        float rgba_a = feedbackData[stride * i + 16];

        float metallic = feedbackData[stride * i + 17];
        float roughness = feedbackData[stride * i + 18];

        Gaussian3D gauss;
        MaterialGltf material;

        material.metallicFactor = metallic;
        material.roughnessFactor = roughness;
        
        gauss.material      = material;
        gauss.normal        = glm::vec3(normal_x, normal_y, normal_z);
        gauss.opacity       = 1.0f;// rgba_a;
        gauss.position      = glm::vec3(pos_x, pos_y, pos_z);
        gauss.rotation      = glm::vec4(quaternion_x, quaternion_y, quaternion_z, quaternion_w); //Try swizzling these around, its always a mess....
        gauss.scale         = glm::vec3(scale_x, scale_y, scale_z);
        gauss.sh0           = getColor(glm::vec3((rgba_r), (rgba_g), (rgba_b)));

        gaussians_3D_list.push_back(gauss);
    }

    std::cout << "Writing ply to" << filename << std::endl;

    writeBinaryPLY(filename, gaussians_3D_list);

    std::cout << "Data successfully written to " << filename << std::endl;
}

void downloadMeshFromGPU(std::vector<Gaussian3D>& gaussians_3D_list, GLuint& framebuffer, unsigned int width, unsigned int height) {
    glFinish();  // Ensure all OpenGL commands are finished
    unsigned int frameBufferStride = 4;

    std::vector<float> pixels0(width * height * frameBufferStride); // For RGBA32F
    std::vector<float> pixels1(width * height * frameBufferStride); // For RGBA32F
    std::vector<float> pixels2(width * height * frameBufferStride); // For RGBA32F
    std::vector<float> pixels3(width * height * frameBufferStride); // For RGBA32F
    std::vector<float> pixels4(width * height * frameBufferStride); // For RGBA32F


    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, pixels0.data());

    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, pixels1.data());

    glReadBuffer(GL_COLOR_ATTACHMENT2);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, pixels2.data());

    glReadBuffer(GL_COLOR_ATTACHMENT3);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, pixels3.data());

    glReadBuffer(GL_COLOR_ATTACHMENT4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, pixels4.data());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    for (int i = 0; i < width * height; ++i) {
        // Extract data from the first texture
        float GaussianPosition_x    = pixels0[frameBufferStride * i + 0];
        float GaussianPosition_y    = pixels0[frameBufferStride * i + 1];
        float GaussianPosition_z    = pixels0[frameBufferStride * i + 2];
        float Scale_xy              = pixels0[frameBufferStride * i + 3];

        // Extract data from the second texture
        float Scale_z   = pixels1[frameBufferStride * i + 0];
        float Normal_x  = pixels1[frameBufferStride * i + 1];
        float Normal_y  = pixels1[frameBufferStride * i + 2];
        float Normal_z  = pixels1[frameBufferStride * i + 3];

        // Extract data from the third texture
        float Quaternion_x  = pixels2[frameBufferStride * i + 0];
        float Quaternion_y  = pixels2[frameBufferStride * i + 1];
        float Quaternion_z  = pixels2[frameBufferStride * i + 2];
        float Quaternion_w  = pixels2[frameBufferStride * i + 3];

        float Rgba_r = pixels3[frameBufferStride * i + 0];
        float Rgba_g = pixels3[frameBufferStride * i + 1];
        float Rgba_b = pixels3[frameBufferStride * i + 2];
        float Rgba_a = pixels3[frameBufferStride * i + 3];

        float metallic  = pixels4[frameBufferStride * i + 0];
        float roughness = pixels4[frameBufferStride * i + 1];

        if (GaussianPosition_x == 0.0f && GaussianPosition_y == 0.0f && GaussianPosition_z == 0.0f && Scale_xy == 0.0f)
        {
            continue;
        }

       // std::cout << Normal_x << " " << Normal_y << " " << Normal_z << "\n" << std::endl;

        Gaussian3D gauss;
        MaterialGltf material;

        material.metallicFactor = metallic;
        material.roughnessFactor = roughness;

        gauss.material = material;
        gauss.normal = glm::vec3(Normal_x, Normal_y, Normal_z);
        gauss.opacity = Rgba_a;
        gauss.position = glm::vec3(GaussianPosition_x, GaussianPosition_y, GaussianPosition_z);
        gauss.rotation = glm::vec4(Quaternion_x, Quaternion_y, Quaternion_z, Quaternion_w); //Try swizzling these around, its always a mess....
        gauss.scale = glm::vec3(Scale_xy, Scale_xy, Scale_z);
        gauss.sh0 = getColor(glm::vec3(Rgba_r, Rgba_g, Rgba_b));
        gaussians_3D_list.push_back(gauss);
    }

}

