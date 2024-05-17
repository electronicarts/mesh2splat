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

GLuint createShaderProgram() {

    std::string vertexShaderSource          = readShaderFile(VERTEX_SHADER_LOCATION);
    std::string tessControlShaderSource     = readShaderFile(TESS_CONTROL_SHADER_LOCATION);
    std::string tessEvaluationShaderSource  = readShaderFile(TESS_EVAL_SHADER_LOCATION);
    std::string geomShaderSource            = readShaderFile(GEOM_SHADER_LOCATION);

    GLuint vertexShader         = compileShader(vertexShaderSource.c_str(), GL_VERTEX_SHADER);
    GLuint tessControlShader    = compileShader(tessControlShaderSource.c_str(), GL_TESS_CONTROL_SHADER);
    GLuint tessEvaluationShader = compileShader(tessEvaluationShaderSource.c_str(), GL_TESS_EVALUATION_SHADER);
    GLuint geomShader           = compileShader(geomShaderSource.c_str(), GL_GEOMETRY_SHADER);


    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, tessControlShader);
    glAttachShader(program, tessEvaluationShader);
    glAttachShader(program, geomShader);

    const GLchar* feedbackVaryings[] = { "GaussianPosition", "Scale", "Normal", "Quaternion" };  // the name of the varying to capture
    glTransformFeedbackVaryings(program, 4, feedbackVaryings, GL_INTERLEAVED_ATTRIBS);

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

float calcTriangleArea(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3)
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

std::vector<GLMesh> uploadMeshesToOpenGL(const std::vector<Mesh>& meshes, float& minTriangleArea, float& maxTriangleArea, float& medianArea, float& medianEdgeLength, float& medianPerimeter) {
    std::vector<GLMesh> glMeshes;
    glMeshes.reserve(meshes.size());
    minTriangleArea = std::numeric_limits<float>::max();
    maxTriangleArea = std::numeric_limits<float>::min();
    std::vector<float> areas;
    std::vector<float> edgeLengths;
    std::vector<float> perimeters;
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
            }
            float area = calcTriangleArea(face.pos[0], face.pos[1], face.pos[2]);
            areas.push_back(area);
            
            float edge1_length = glm::length(face.pos[1] - face.pos[0]);
            float edge2_length = glm::length(face.pos[2] - face.pos[0]);
            float edge3_length = glm::length(face.pos[2] - face.pos[1]);
            
            edgeLengths.push_back(edge1_length);
            edgeLengths.push_back(edge2_length);
            edgeLengths.push_back(edge3_length);
            perimeters.push_back(edge1_length + edge2_length + edge3_length);

            if (area > maxTriangleArea) maxTriangleArea = area;
            else if (area < minTriangleArea) minTriangleArea = area;
        }

        glMesh.vertexCount = vertices.size() / 3; // Number of vertices

        // 3 position, 3 normal, 4 tangent, 2 UV = 12
        size_t vertexStride = 12 * sizeof(float);

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

        // Unbind VAO
        glBindVertexArray(0);

        // Add to list of GLMeshes
        glMeshes.push_back(glMesh);
    }

    findMedianFromFloatVector(areas, medianArea);
    findMedianFromFloatVector(edgeLengths, medianEdgeLength);
    findMedianFromFloatVector(perimeters, medianPerimeter);

    return glMeshes;
}


void setupTransformFeedbackAndAtomicCounter(size_t bufferSize, GLuint& feedbackBuffer, GLuint& feedbackVAO, GLuint& acBuffer) {

    // Create a buffer for storing feedback
    glGenBuffers(1, &feedbackBuffer);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, feedbackBuffer);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, bufferSize * sizeof(float), NULL, GL_STATIC_READ);

    // Create a VAO for transform feedback
    glGenVertexArrays(1, &feedbackVAO);
    glBindVertexArray(feedbackVAO);

    //gaussianPos(vec3) + scale(vec3) + normal(vec3) + tangent(vec4) + uv(vec2)
    GLsizei totalStride = 3 + 3 + 3 + 4;

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

void performTessellationAndCapture(GLuint shaderProgram, GLuint vao, size_t vertexCount, GLuint& numGaussiansGenerated, GLuint& acBuffer, float minTriangleArea, float maxTriangleArea, float medianTriangleArea, float medianEdgeLength, float medianPerimeter, unsigned int textureSize) {
    // Use shader program and perform tessellation
    glUseProgram(shaderProgram);

    //-------------------------------SET UNIFORMS-------------------------------   
    setUniform1f(shaderProgram, "minTriangleArea", minTriangleArea);
    setUniform1f(shaderProgram, "maxTriangleArea", maxTriangleArea);
    setUniform1f(shaderProgram, "medianTriangleArea", medianTriangleArea);
    setUniform1f(shaderProgram, "medianEdgeLength", medianEdgeLength);
    setUniform1f(shaderProgram, "medianPerimeter", medianPerimeter);
    setUniform1i(shaderProgram, "textureSize", textureSize);
    //--------------------------------------------------------------------------

    // Prepare to capture the number of tessellated triangles
    GLuint query;
    glGenQueries(1, &query);

    glBindVertexArray(vao);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, query);

    glEnable(GL_RASTERIZER_DISCARD);
    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_PATCHES, 0, vertexCount);
    glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);

    // End the query to capture the number of tessellated triangles
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);

    glGetQueryObjectuiv(query, GL_QUERY_RESULT, &numGaussiansGenerated);

    glDeleteQueries(1, &query);
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
        float pos_x = feedbackData[stride * i];         
        float pos_y = feedbackData[stride * i + 1];     
        float pos_z = feedbackData[stride * i + 2];     

        float scale_x = feedbackData[stride * i + 3];   
        float scale_y = feedbackData[stride * i + 4];   
        float scale_z = feedbackData[stride * i + 5];

        float normal_x = feedbackData[stride * i + 6];
        float normal_y = feedbackData[stride * i + 7];
        float normal_z = feedbackData[stride * i + 8];

        float quaternion_x = 1.0f;//feedbackData[stride * i + 9];
        float quaternion_y = 1.0f; //feedbackData[stride * i + 10];
        float quaternion_z = 1.0f; //feedbackData[stride * i + 11];
        float quaternion_w = 1.0f; //feedbackData[stride * i + 12];

        /*
        std::cout << "Gaussian number " << i << "\n" << std::endl;
        std::cout << "pos_x: " << pos_x << "\n" << std::endl;
        std::cout << "pos_y: " << pos_y << "\n" << std::endl;
        std::cout << "pos_z: " << pos_z << "\n" << std::endl;

        std::cout << "scale_x: " << scale_x << "\n" << std::endl;
        std::cout << "scale_y: " << scale_y << "\n" << std::endl;
        std::cout << "scale_z: " << scale_z << "\n" << std::endl;

        std::cout << "normal_x: " << normal_x << "\n" << std::endl;
        std::cout << "normal_y: " << normal_y << "\n" << std::endl;
        std::cout << "normal_z: " << normal_z << "\n" << std::endl;

        std::cout << "quaternion_x: " << quaternion_x << "\n" << std::endl;
        std::cout << "quaternion_y: " << quaternion_y << "\n" << std::endl;
        std::cout << "quaternion_z: " << quaternion_z << "\n" << std::endl;
        std::cout << "quaternion_w: " << quaternion_w << "\n" << std::endl;
        */

        Gaussian3D gauss;
        gauss.material = MaterialGltf();
        gauss.normal = glm::vec3(normal_x, normal_y, normal_z);
        gauss.opacity = 1.0f;
        gauss.position = glm::vec3(pos_x, pos_y, pos_z);
        gauss.rotation = glm::vec4(quaternion_x, quaternion_y, quaternion_z, quaternion_w); //Try swizzling these around, its always a mess....
        gauss.scale = glm::vec3(scale_x, scale_y, scale_z);
        gauss.sh0 = DEFAULT_PURPLE;
        gaussians_3D_list.push_back(gauss);
    }

    std::cout << "Writing ply to" << filename << std::endl;

    writeBinaryPLY(filename, gaussians_3D_list);

    std::cout << "Data successfully written to " << filename << std::endl;
}

void downloadMeshFromGPU(GLuint& feedbackBuffer, GLuint numGeneratedGaussians) {
    glFinish();  // Ensure all OpenGL commands are finished
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, feedbackBuffer);

    unsigned int stride = 3 + 3 + 3 + 4;

    float* feedbackData = (float*)glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0,
        numGeneratedGaussians * stride * sizeof(float), GL_MAP_READ_BIT);
    
    //float* feedbackData = (float*)glMapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, GL_READ_ONLY);

    printf("Num generated gaussians: %d\n", numGeneratedGaussians);

    printf("Saving to ply\n");
    if (feedbackData) { 
        readAndSaveToPly(feedbackData, numGeneratedGaussians, GAUSSIAN_OUTPUT_MODEL_DEST_FOLDER_1, stride);
    }
    else {
        std::cerr << "Failed to map feedback buffer." << std::endl;
    }

    glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
}

