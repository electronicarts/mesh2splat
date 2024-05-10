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

GLuint createShaderProgram() {
    const char* vertexShaderSource = R"glsl(
        #version 430 core
        layout(location = 0) in vec3 position;
        out vec4 vs_outPosition;  // Pass position as vec4

        void main() {
            vs_outPosition = vec4(position, 1.0);  // Ensure it's a vec4
        }
    )glsl";

    const char* tessControlShaderSource = R"glsl(
        #version 430 core
        layout(vertices = 3) out;

        in vec4 vs_outPosition[];
        out vec4 tcs_outPosition[];

        void main() {
            if (gl_InvocationID == 0) {
                gl_TessLevelInner[0] = 3.0;
                gl_TessLevelOuter[0] = 3.0;
                gl_TessLevelOuter[1] = 3.0;
                gl_TessLevelOuter[2] = 3.0;
            }
            tcs_outPosition[gl_InvocationID] = vs_outPosition[gl_InvocationID];  // Keep it as vec4
            gl_out[gl_InvocationID].gl_Position = vs_outPosition[gl_InvocationID];
        }
    )glsl";

    const char* tessEvaluationShaderSource = R"glsl(
        #version 430 core
        layout(triangles, equal_spacing, cw) in;

        in vec4 tcs_outPosition[];
        out vec3 outPosition;  // Assuming you want a vec3 for output

        void main() {
            vec3 p0 = gl_TessCoord.x * tcs_outPosition[0].xyz;
            vec3 p1 = gl_TessCoord.y * tcs_outPosition[1].xyz;
            vec3 p2 = gl_TessCoord.z * tcs_outPosition[2].xyz;
            outPosition = p0 + p1 + p2;  // Correctly using vec3
            gl_Position = vec4(outPosition, 1.0);
        }
    )glsl";

    GLuint vertexShader = compileShader(vertexShaderSource, GL_VERTEX_SHADER);
    GLuint tessControlShader = compileShader(tessControlShaderSource, GL_TESS_CONTROL_SHADER);
    GLuint tessEvaluationShader = compileShader(tessEvaluationShaderSource, GL_TESS_EVALUATION_SHADER);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, tessControlShader);
    glAttachShader(program, tessEvaluationShader);

    const GLchar* feedbackVaryings[] = { "outPosition" };  // the name of the varying to capture
    glTransformFeedbackVaryings(program, 1, feedbackVaryings, GL_INTERLEAVED_ATTRIBS);

    GLint success;
    GLchar infoLog[512];
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
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

void performTessellationAndCapture(GLuint shaderProgram, GLuint vao, size_t vertexCount) {
    // Use shader program and perform tessellation
    glUseProgram(shaderProgram);
    glBindVertexArray(vao);
    glEnable(GL_RASTERIZER_DISCARD);
    glBeginTransformFeedback(GL_TRIANGLES);
    glDrawArrays(GL_PATCHES, 0, vertexCount);
    glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);

    // Insert a fence sync object after tessellation
    GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush(); 
        
    GLenum wait = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 1e8); // Wait up to 0.1 second
    if (wait == GL_TIMEOUT_EXPIRED) {
        std::cerr << "Sync timeout expired." << std::endl;
    }
    glDeleteSync(sync); // Clean up the sync object
}

void downloadMeshFromGPU(size_t vertexCount, GLuint& feedbackBuffer) {
    glFinish();
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, feedbackBuffer);
    float* feedbackData = (float*)glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0,
        vertexCount * 3 * sizeof(float), GL_MAP_READ_BIT);

    // Check if mapping was successful
    if (feedbackData) {
        std::cout << "Vertex Positions:" << std::endl;
        for (size_t i = 0; i < vertexCount; ++i) {
            // Each vertex has three float values (x, y, z)
            float x = feedbackData[3 * i];     // X coordinate
            float y = feedbackData[3 * i + 1]; // Y coordinate
            float z = feedbackData[3 * i + 2]; // Z coordinate
            std::cout << "Vertex " << i + 1 << ": (" << x << ", " << y << ", " << z << ")" << std::endl;
        }
    }
    else {
        std::cerr << "Failed to map feedback buffer." << std::endl;
    }

    // Unmap the buffer after done reading
    glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
}
