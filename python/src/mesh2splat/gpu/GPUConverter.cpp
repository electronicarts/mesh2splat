///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - GPU Converter Implementation        //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "GPUConverter.hpp"
#include "HeadlessContext.hpp"
#include "ShaderManager.hpp"
#include <chrono>
#include <sstream>
#include <cstring>

// Use our GL loader which handles platform differences
#include "GLLoader.hpp"

namespace mesh2splat {

//------------------------------------------------------------------------------
// GPU Resources for rendering
//------------------------------------------------------------------------------

struct GPUMeshData {
    GLuint vao = 0;
    GLuint vbo = 0;
    size_t vertexCount = 0;
};

struct GPUTextureData {
    GLuint textureId = 0;
    bool valid = false;
};

struct FramebufferData {
    GLuint fbo = 0;
    GLuint textures[6] = {0}; // Position, Color, Scale, Normal, Rotation, PBR
    int width = 0;
    int height = 0;
};

//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------

class GPUConverter::Impl {
public:
    std::unique_ptr<HeadlessContext> context_;
    std::unique_ptr<ShaderManager> shaders_;
    FramebufferData framebuffer_;
    std::string errorMessage_;
    bool initialized_ = false;
    
    ~Impl() {
        cleanup();
    }
    
    bool initialize() {
        // Create headless context
        context_ = std::make_unique<HeadlessContext>(4, 1);
        if (!context_->isValid()) {
            errorMessage_ = "Failed to create headless context: " + context_->getErrorMessage();
            return false;
        }
        
        // Make context current
        if (!context_->makeCurrent()) {
            errorMessage_ = "Failed to make context current";
            return false;
        }
        
        // Initialize shaders
        shaders_ = std::make_unique<ShaderManager>();
        if (!shaders_->initialize()) {
            errorMessage_ = "Failed to initialize shaders: " + shaders_->getErrorMessage();
            return false;
        }
        
        initialized_ = true;
        return true;
    }
    
    void cleanup() {
        if (context_ && context_->isValid()) {
            context_->makeCurrent();
            
            // Cleanup framebuffer
            if (framebuffer_.fbo != 0) {
                glDeleteFramebuffers(1, &framebuffer_.fbo);
                glDeleteTextures(6, framebuffer_.textures);
                framebuffer_ = {};
            }
            
            // Cleanup shaders
            if (shaders_) {
                shaders_->cleanup();
            }
            
            context_->release();
        }
        initialized_ = false;
    }
    
    bool createFramebuffer(int width, int height) {
        // Validate dimensions to prevent integer overflow and excessive memory usage
        // Max 8192x8192 is reasonable for most GPUs and prevents overflow in pixelCount calculation
        constexpr int kMinResolution = 1;
        constexpr int kMaxResolution = 8192;
        
        if (width < kMinResolution || height < kMinResolution) {
            errorMessage_ = "Framebuffer dimensions must be at least 1x1";
            return false;
        }
        if (width > kMaxResolution || height > kMaxResolution) {
            errorMessage_ = "Framebuffer dimensions exceed maximum of 8192x8192";
            return false;
        }
        
        // Delete old framebuffer if exists
        if (framebuffer_.fbo != 0) {
            glDeleteFramebuffers(1, &framebuffer_.fbo);
            glDeleteTextures(6, framebuffer_.textures);
        }
        
        framebuffer_.width = width;
        framebuffer_.height = height;
        
        // Create framebuffer
        glGenFramebuffers(1, &framebuffer_.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_.fbo);
        
        // Create 6 render targets (float textures for precision)
        glGenTextures(6, framebuffer_.textures);
        
        GLenum attachments[6] = {
            GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
            GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5
        };
        
        for (int i = 0; i < 6; i++) {
            glBindTexture(GL_TEXTURE_2D, framebuffer_.textures[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachments[i], GL_TEXTURE_2D, framebuffer_.textures[i], 0);
        }
        
        // Set draw buffers
        glDrawBuffers(6, attachments);
        
        // Check completeness
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            errorMessage_ = "Framebuffer incomplete";
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }
    
    GPUMeshData uploadMesh(const Mesh& mesh) {
        GPUMeshData data;
        
        // Calculate vertex count (3 vertices per face)
        data.vertexCount = mesh.faces.size() * 3;
        if (data.vertexCount == 0) return data;
        
        // Vertex layout: position(3) + normal(3) + tangent(4) + uv(2) + normalizedUv(2) + scale(3) = 17 floats
        const int floatsPerVertex = 17;
        std::vector<float> vertices(data.vertexCount * floatsPerVertex);
        
        size_t idx = 0;
        for (const auto& face : mesh.faces) {
            for (int v = 0; v < 3; v++) {
                // Position
                vertices[idx++] = face.positions[v].x;
                vertices[idx++] = face.positions[v].y;
                vertices[idx++] = face.positions[v].z;
                // Normal
                vertices[idx++] = face.normals[v].x;
                vertices[idx++] = face.normals[v].y;
                vertices[idx++] = face.normals[v].z;
                // Tangent
                vertices[idx++] = face.tangents[v].x;
                vertices[idx++] = face.tangents[v].y;
                vertices[idx++] = face.tangents[v].z;
                vertices[idx++] = face.tangents[v].w;
                // UV
                vertices[idx++] = face.uvs[v].x;
                vertices[idx++] = face.uvs[v].y;
                // Normalized UV (from xatlas, or fallback to regular UVs)
                vertices[idx++] = face.uvs[v].x;
                vertices[idx++] = face.uvs[v].y;
                // Scale (placeholder, computed in GS)
                vertices[idx++] = 1.0f;
                vertices[idx++] = 1.0f;
                vertices[idx++] = 1.0f;
            }
        }
        
        // Create VAO and VBO
        glGenVertexArrays(1, &data.vao);
        glGenBuffers(1, &data.vbo);
        
        glBindVertexArray(data.vao);
        glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        
        // Setup vertex attributes
        const int stride = floatsPerVertex * sizeof(float);
        size_t offset = 0;
        
        // Position (location 0)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offset);
        glEnableVertexAttribArray(0);
        offset += 3 * sizeof(float);
        
        // Normal (location 1)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offset);
        glEnableVertexAttribArray(1);
        offset += 3 * sizeof(float);
        
        // Tangent (location 2)
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)offset);
        glEnableVertexAttribArray(2);
        offset += 4 * sizeof(float);
        
        // UV (location 3)
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)offset);
        glEnableVertexAttribArray(3);
        offset += 2 * sizeof(float);
        
        // Normalized UV (location 4)
        glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, stride, (void*)offset);
        glEnableVertexAttribArray(4);
        offset += 2 * sizeof(float);
        
        // Scale (location 5)
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, stride, (void*)offset);
        glEnableVertexAttribArray(5);
        
        glBindVertexArray(0);
        
        return data;
    }
    
    void deleteMeshData(GPUMeshData& data) {
        if (data.vao != 0) {
            glDeleteVertexArrays(1, &data.vao);
            glDeleteBuffers(1, &data.vbo);
            data = {};
        }
    }
    
    GPUTextureData uploadTexture(const TextureData& tex) {
        GPUTextureData data;
        if (tex.empty()) return data;
        
        glGenTextures(1, &data.textureId);
        glBindTexture(GL_TEXTURE_2D, data.textureId);
        
        GLenum format = GL_RGBA;
        if (tex.channels == 1) format = GL_RED;
        else if (tex.channels == 2) format = GL_RG;
        else if (tex.channels == 3) format = GL_RGB;
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.width, tex.height, 0, 
                     format, GL_UNSIGNED_BYTE, tex.data.data());
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        data.valid = true;
        return data;
    }
    
    void deleteTexture(GPUTextureData& data) {
        if (data.textureId != 0) {
            glDeleteTextures(1, &data.textureId);
            data = {};
        }
    }
    
    ConversionResult convert(const Scene& scene, const ConversionOptions& options) {
        ConversionResult result;
        result.usedBackend = Backend::GPU;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        if (!initialized_) {
            result.success = false;
            result.errorMessage = "GPU converter not initialized";
            return result;
        }
        
        try {
            ContextGuard guard(*context_);
            if (!guard.isValid()) {
                result.success = false;
                result.errorMessage = "Failed to make GL context current";
                return result;
            }
            
            // Create framebuffer at requested resolution
            if (!createFramebuffer(options.resolution, options.resolution)) {
                result.success = false;
                result.errorMessage = errorMessage_;
                return result;
            }
            
            // Count total triangles
            for (const auto& mesh : scene.meshes) {
                result.totalTriangles += mesh.faces.size();
            }
            
            if (options.verbose) {
                std::ostringstream oss;
                oss << "GPU processing " << scene.meshes.size() << " meshes with "
                    << result.totalTriangles << " triangles at resolution " << options.resolution
                    << " using " << (options.rasterizationMode == RasterizationMode::UV ? "UV" : "Projection")
                    << " mode";
                result.messages.push_back(oss.str());
            }
            
            // Process each mesh
            for (const auto& mesh : scene.meshes) {
                if (mesh.faces.empty()) continue;
                
                // Upload mesh data
                GPUMeshData meshData = uploadMesh(mesh);
                
                // Upload textures
                GPUTextureData albedoTex = uploadTexture(mesh.material.baseColorTexture);
                GPUTextureData normalTex = uploadTexture(mesh.material.normalTexture);
                GPUTextureData metalRoughTex = uploadTexture(mesh.material.metallicRoughnessTexture);
                
                // H16 fix: RAII guard to ensure GPU resources are cleaned up on exception
                auto cleanupResources = [&]() {
                    deleteMeshData(meshData);
                    deleteTexture(albedoTex);
                    deleteTexture(normalTex);
                    deleteTexture(metalRoughTex);
                };
                
                try {
                
                // Bind framebuffer and clear
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_.fbo);
                glViewport(0, 0, framebuffer_.width, framebuffer_.height);
                
                // Clear all attachments to zero (we'll use alpha=0 to detect empty pixels)
                float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                for (int i = 0; i < 6; i++) {
                    glClearBufferfv(GL_COLOR, i, clearColor);
                }
                
                // Use shader
                shaders_->useConverterProgram();
                
                // Set uniforms
                shaders_->setUniform("u_bboxMin", mesh.bbox.min.x, mesh.bbox.min.y, mesh.bbox.min.z);
                shaders_->setUniform("u_bboxMax", mesh.bbox.max.x, mesh.bbox.max.y, mesh.bbox.max.z);
                shaders_->setUniform("u_rasterizationMode", 
                    options.rasterizationMode == RasterizationMode::UV ? 0 : 1);
                shaders_->setUniform("u_materialFactor", 
                    mesh.material.baseColorFactor.r,
                    mesh.material.baseColorFactor.g,
                    mesh.material.baseColorFactor.b,
                    mesh.material.baseColorFactor.a);
                
                // Bind textures
                shaders_->setUniform("hasAlbedoMap", albedoTex.valid ? 1 : 0);
                shaders_->setUniform("hasNormalMap", normalTex.valid ? 1 : 0);
                shaders_->setUniform("hasMetallicRoughnessMap", metalRoughTex.valid ? 1 : 0);
                
                if (albedoTex.valid) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, albedoTex.textureId);
                    shaders_->setUniform("albedoTexture", 0);
                }
                if (normalTex.valid) {
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, normalTex.textureId);
                    shaders_->setUniform("normalTexture", 1);
                }
                if (metalRoughTex.valid) {
                    glActiveTexture(GL_TEXTURE2);
                    glBindTexture(GL_TEXTURE_2D, metalRoughTex.textureId);
                    shaders_->setUniform("metallicRoughnessTexture", 2);
                }
                
                // Draw
                glBindVertexArray(meshData.vao);
                glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(meshData.vertexCount));
                glBindVertexArray(0);
                
                // Read back results from all render targets
                readbackGaussians(result, options);
                
                } catch (...) {
                    cleanupResources();
                    throw;
                }
                
                // Cleanup mesh and textures (normal path)
                cleanupResources();
            }
            
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            
            result.totalGaussians = result.gaussians.size();
            result.success = true;
            
            if (options.verbose) {
                std::ostringstream oss;
                oss << "Generated " << result.totalGaussians << " gaussians via GPU";
                result.messages.push_back(oss.str());
            }
            
        } catch (const std::exception& e) {
            result.success = false;
            result.errorMessage = std::string("GPU conversion failed: ") + e.what();
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        result.conversionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        
        return result;
    }
    
    void readbackGaussians(ConversionResult& result, const ConversionOptions& options) {
        int w = framebuffer_.width;
        int h = framebuffer_.height;
        // H13 fix: use size_t to prevent int overflow on large resolutions
        size_t pixelCount = static_cast<size_t>(w) * static_cast<size_t>(h);
        
        // Read all 6 render targets
        std::vector<float> positionData(pixelCount * 4);
        std::vector<float> colorData(pixelCount * 4);
        std::vector<float> scaleData(pixelCount * 4);
        std::vector<float> normalData(pixelCount * 4);
        std::vector<float> rotationData(pixelCount * 4);
        std::vector<float> pbrData(pixelCount * 4);
        
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_.fbo);
        
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, positionData.data());
        
        glReadBuffer(GL_COLOR_ATTACHMENT1);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, colorData.data());
        
        glReadBuffer(GL_COLOR_ATTACHMENT2);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, scaleData.data());
        
        glReadBuffer(GL_COLOR_ATTACHMENT3);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, normalData.data());
        
        glReadBuffer(GL_COLOR_ATTACHMENT4);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, rotationData.data());
        
        glReadBuffer(GL_COLOR_ATTACHMENT5);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, pbrData.data());
        
        // Convert non-empty pixels to gaussians
        for (size_t i = 0; i < pixelCount; i++) {
            size_t base = i * 4;
            
            // Check if pixel has valid data (alpha > 0 in color)
            if (colorData[base + 3] < 0.01f) continue;
            
            Gaussian g;
            
            // Position
            g.x = positionData[base + 0];
            g.y = positionData[base + 1];
            g.z = positionData[base + 2];
            
            // Color (convert to SH0)
            glm::vec3 color(colorData[base + 0], colorData[base + 1], colorData[base + 2]);
            if (options.srgbConversion) {
                color = srgbToLinear(color);
            }
            glm::vec3 sh0 = colorToSH0(color);
            g.r = sh0.r;
            g.g = sh0.g;
            g.b = sh0.b;
            g.opacity = colorData[base + 3];
            
            // Scale
            g.scale_x = scaleData[base + 0] * options.scaleMultiplier;
            g.scale_y = scaleData[base + 1] * options.scaleMultiplier;
            g.scale_z = scaleData[base + 2] * options.scaleMultiplier;
            
            // Normal
            g.nx = normalData[base + 0];
            g.ny = normalData[base + 1];
            g.nz = normalData[base + 2];
            
            // Rotation
            g.rot_w = rotationData[base + 0];
            g.rot_x = rotationData[base + 1];
            g.rot_y = rotationData[base + 2];
            g.rot_z = rotationData[base + 3];
            
            // PBR
            g.metallic = pbrData[base + 0];
            g.roughness = pbrData[base + 1];
            g.ao = pbrData[base + 2];
            
            if (g.isValid()) {
                result.gaussians.push_back(g);
            }
        }
    }
    
    ConversionResult convert(const Mesh& mesh, const ConversionOptions& options) {
        Scene scene;
        scene.meshes.push_back(mesh);
        scene.bbox = mesh.bbox;
        return convert(scene, options);
    }
};

//------------------------------------------------------------------------------
// Public interface
//------------------------------------------------------------------------------

GPUConverter::GPUConverter() : impl_(std::make_unique<Impl>()) {}
GPUConverter::~GPUConverter() = default;

bool GPUConverter::initialize() {
    return impl_->initialize();
}

bool GPUConverter::isInitialized() const {
    return impl_->initialized_;
}

ConversionResult GPUConverter::convert(const Scene& scene, const ConversionOptions& options) {
    return impl_->convert(scene, options);
}

ConversionResult GPUConverter::convert(const Mesh& mesh, const ConversionOptions& options) {
    return impl_->convert(mesh, options);
}

bool GPUConverter::isAvailable() {
    return HeadlessContext::isAvailable();
}

std::string GPUConverter::getErrorMessage() const {
    return impl_->errorMessage_;
}

} // namespace mesh2splat
