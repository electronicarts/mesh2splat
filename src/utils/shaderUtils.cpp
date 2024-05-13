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

    GLuint vertexShader         = compileShader(vertexShaderSource.c_str(), GL_VERTEX_SHADER);
    GLuint tessControlShader    = compileShader(tessControlShaderSource.c_str(), GL_TESS_CONTROL_SHADER);
    GLuint tessEvaluationShader = compileShader(tessEvaluationShaderSource.c_str(), GL_TESS_EVALUATION_SHADER);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, tessControlShader);
    glAttachShader(program, tessEvaluationShader);

    const GLchar* feedbackVaryings[] = { "outPosition" };  // the name of the varying to capture
    glTransformFeedbackVaryings(program, 1, feedbackVaryings, GL_INTERLEAVED_ATTRIBS);

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

std::vector<GLMesh> uploadMeshesToOpenGL(const std::vector<Mesh>& meshes) {
    std::vector<GLMesh> glMeshes;
    glMeshes.reserve(meshes.size());

    for (auto& mesh : meshes) {
        GLMesh glMesh;
        std::vector<float> vertices;  // Buffer to hold all vertex data

        // Convert mesh data to a flat array of floats (position only for this example)
        for (const auto& face : mesh.faces) {
            for (int i = 0; i < 3; ++i) { // Assuming each face is a triangle
                vertices.push_back(face.pos[i].x);
                vertices.push_back(face.pos[i].y);
                vertices.push_back(face.pos[i].z);
            }
        }

        glMesh.vertexCount = vertices.size() / 3; // Number of vertices

        // Generate and bind VAO
        glGenVertexArrays(1, &glMesh.vao);
        glBindVertexArray(glMesh.vao);

        // Generate and bind VBO
        glGenBuffers(1, &glMesh.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, glMesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        // Setup vertex attribute pointers
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Unbind VAO
        glBindVertexArray(0);

        // Add to list of GLMeshes
        glMeshes.push_back(glMesh);
    }

    return glMeshes;
}


void setupTransformFeedback(size_t bufferSize, GLuint& feedbackBuffer, GLuint& feedbackVAO) {

    // Create a buffer for storing feedback
    glGenBuffers(1, &feedbackBuffer);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, feedbackBuffer);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, bufferSize * sizeof(float), NULL, GL_STATIC_READ);

    // Create a VAO for transform feedback
    glGenVertexArrays(1, &feedbackVAO);
    glBindVertexArray(feedbackVAO);
    glBindBuffer(GL_ARRAY_BUFFER, feedbackBuffer);
    glEnableVertexAttribArray(0);  // Assuming output is vec3 positions
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // Bind the buffer to the transform feedback binding point
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, feedbackBuffer);
}

void performTessellationAndCapture(GLuint shaderProgram, GLuint vao, size_t vertexCount, const int targetTriangleEdgeLength, GLuint& numTessellatedTriangles) {
    // Use shader program and perform tessellation
    glUseProgram(shaderProgram);

    GLint tessellationLevelLocation = glGetUniformLocation(shaderProgram, "targetEdgeLength");
    if (tessellationLevelLocation == -1) {
        std::cerr << "Could not find uniform tessellationLevel." << std::endl;
    }

    glUniform1i(tessellationLevelLocation, targetTriangleEdgeLength);

    // Prepare to capture the number of tessellated triangles
    GLuint query;
    glGenQueries(1, &query);

    glBindVertexArray(vao);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, query);

    glEnable(GL_RASTERIZER_DISCARD);
    glBeginTransformFeedback(GL_TRIANGLES);
    glDrawArrays(GL_PATCHES, 0, vertexCount);
    glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);

    // End the query to capture the number of tessellated triangles
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);

    glGetQueryObjectuiv(query, GL_QUERY_RESULT, &numTessellatedTriangles);

    glDeleteQueries(1, &query);

    // Insert a fence sync object after tessellation
    /*
    GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush(); 
        
    GLenum wait = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 1e8); // Wait up to 0.1 second
    if (wait == GL_TIMEOUT_EXPIRED) {
        std::cerr << "Sync timeout expired." << std::endl;
    }
    glDeleteSync(sync); // Clean up the sync object
    */
}

void saveToObjFile(const float* feedbackData, GLuint numberOfTesselatedTriangles, const std::string& filename) {
    std::ofstream objFile(filename);
    if (!objFile.is_open()) {
        std::cerr << "Failed to open the file for writing: " << filename << std::endl;
        return;
    }

    unsigned int numberOfTotalVertices = numberOfTesselatedTriangles * 3;

    objFile << "# OBJ file generated by application\n";
    objFile << "# Vertex count: " << numberOfTotalVertices << "\n";
    objFile << "# Triangle count: " << numberOfTesselatedTriangles << "\n";

    // Write each vertex position
    for (size_t i = 0; i < numberOfTotalVertices; ++i) {
        float x = feedbackData[3 * i];     // X coordinate
        float y = feedbackData[3 * i + 1]; // Y coordinate
        float z = feedbackData[3 * i + 2]; // Z coordinate
        objFile << "v " << x << " " << y << " " << z << "\n";
    }

    // Write face (triangle) definitions using 1-based indexing
    for (unsigned int i = 1; i <= numberOfTotalVertices; i += 3) {
        objFile << "f " << i << " " << i + 1 << " " << i + 2 << "\n";
    }

    objFile.close();
    std::cout << "Data successfully written to " << filename << std::endl;
}

void downloadMeshFromGPU(GLuint& feedbackBuffer, GLuint numTessellatedTriangles) {
    glFinish();  // Ensure all OpenGL commands are finished
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, feedbackBuffer);

    // Assuming each triangle gives 3 vertices and each vertex has 3 floats (x, y, z)
    float* feedbackData = (float*)glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0,
        numTessellatedTriangles * 3 * 3 * sizeof(float), GL_MAP_READ_BIT);

    if (feedbackData) {
        saveToObjFile(feedbackData, numTessellatedTriangles, "tessellated_mesh.obj");
    }
    else {
        std::cerr << "Failed to map feedback buffer." << std::endl;
    }

    glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
}

