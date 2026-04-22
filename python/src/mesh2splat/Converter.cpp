///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - Converter Implementation            //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "Converter.hpp"
#include "cpu/CPUConverter.hpp"
#include "core/GltfLoader.hpp"
#include "core/PlyIO.hpp"

#include <filesystem>
#include <sstream>

#ifdef MESH2SPLAT_ENABLE_GPU
#include "gpu/GPUConverter.hpp"
#endif

namespace mesh2splat {

//------------------------------------------------------------------------------
// Version info
//------------------------------------------------------------------------------

std::string getVersion() {
    return "1.0.0";
}

std::string getBuildInfo() {
    std::ostringstream oss;
    oss << "mesh2splat v" << getVersion();
    oss << " (";
#ifdef MESH2SPLAT_ENABLE_GPU
    oss << "GPU+";
#endif
    oss << "CPU)";
    
#if defined(__APPLE__)
    oss << " [macOS]";
#elif defined(__linux__)
    oss << " [Linux]";
#else
    oss << " [Unknown OS]";
#endif
    
    return oss.str();
}

//------------------------------------------------------------------------------
// Backend availability
//------------------------------------------------------------------------------

std::vector<Backend> Converter::getAvailableBackends() {
    std::vector<Backend> backends;
    backends.push_back(Backend::CPU); // CPU is always available
    
#ifdef MESH2SPLAT_ENABLE_GPU
    if (GPUConverter::isAvailable()) {
        backends.push_back(Backend::GPU);
    }
#endif
    
    return backends;
}

bool Converter::isBackendAvailable(Backend backend) {
    switch (backend) {
        case Backend::CPU:
            return true;
        case Backend::GPU:
#ifdef MESH2SPLAT_ENABLE_GPU
            return GPUConverter::isAvailable();
#else
            return false;
#endif
        case Backend::Auto:
            return true; // Auto is always "available"
    }
    return false;
}

//------------------------------------------------------------------------------
// Converter Implementation
//------------------------------------------------------------------------------

class Converter::Impl {
public:
    std::unique_ptr<CPUConverter> cpuConverter_;
#ifdef MESH2SPLAT_ENABLE_GPU
    std::unique_ptr<GPUConverter> gpuConverter_;
#endif
    Backend requestedBackend_ = Backend::Auto;
    Backend activeBackend_ = Backend::CPU;
    std::string errorMessage_;
    bool ready_ = false;
    
    Impl(Backend backend) : requestedBackend_(backend) {
        initialize();
    }
    
    void initialize() {
        // Try to initialize the requested backend
        switch (requestedBackend_) {
            case Backend::Auto:
                // Try GPU first, fallback to CPU
#ifdef MESH2SPLAT_ENABLE_GPU
                if (tryInitializeGPU()) {
                    activeBackend_ = Backend::GPU;
                    ready_ = true;
                    return;
                }
#endif
                // Fallback to CPU
                if (tryInitializeCPU()) {
                    activeBackend_ = Backend::CPU;
                    ready_ = true;
                    return;
                }
                errorMessage_ = "Failed to initialize any backend";
                break;
                
            case Backend::GPU:
#ifdef MESH2SPLAT_ENABLE_GPU
                if (tryInitializeGPU()) {
                    activeBackend_ = Backend::GPU;
                    ready_ = true;
                } else {
                    if (errorMessage_.empty()) {
                        errorMessage_ = "GPU backend initialization failed";
                    }
                }
#else
                errorMessage_ = "GPU backend not compiled in";
#endif
                break;
                
            case Backend::CPU:
                if (tryInitializeCPU()) {
                    activeBackend_ = Backend::CPU;
                    ready_ = true;
                } else {
                    errorMessage_ = "CPU backend initialization failed";
                }
                break;
        }
    }
    
    bool tryInitializeCPU() {
        try {
            cpuConverter_ = std::make_unique<CPUConverter>();
            return true;
        } catch (...) {
            return false;
        }
    }
    
#ifdef MESH2SPLAT_ENABLE_GPU
    bool tryInitializeGPU() {
        try {
            gpuConverter_ = std::make_unique<GPUConverter>();
            if (!gpuConverter_->initialize()) {
                errorMessage_ = gpuConverter_->getErrorMessage();
                gpuConverter_.reset();
                return false;
            }
            return true;
        } catch (...) {
            return false;
        }
    }
#endif
    
    ConversionResult convert(const Scene& scene, const ConversionOptions& options) {
        if (!ready_) {
            ConversionResult result;
            result.success = false;
            result.errorMessage = "Converter not ready: " + errorMessage_;
            return result;
        }
        
        // Use override backend from options if specified
        Backend effectiveBackend = (options.backend != Backend::Auto) ? options.backend : activeBackend_;
        
#ifdef MESH2SPLAT_ENABLE_GPU
        if (effectiveBackend == Backend::GPU && gpuConverter_) {
            return gpuConverter_->convert(scene, options);
        }
#endif
        
        if (cpuConverter_) {
            return cpuConverter_->convert(scene, options);
        }
        
        ConversionResult result;
        result.success = false;
        result.errorMessage = "No converter available";
        return result;
    }
    
    ConversionResult convert(const Mesh& mesh, const ConversionOptions& options) {
        Scene scene;
        scene.meshes.push_back(mesh);
        scene.bbox = mesh.bbox;
        return convert(scene, options);
    }
    
    ConversionResult convertFile(const std::string& path, const ConversionOptions& options) {
        // Validate input path exists
        if (!std::filesystem::exists(path)) {
            ConversionResult result;
            result.success = false;
            result.errorMessage = "File not found: " + path;
            return result;
        }
        
        // Validate file extension
        if (!GltfLoader::isGltfFile(path)) {
            ConversionResult result;
            result.success = false;
            result.errorMessage = "Unsupported file format. Expected .gltf or .glb: " + path;
            return result;
        }
        
        // Load the file
        GltfLoader loader;
        Scene scene = loader.load(path);
        
        if (scene.meshes.empty()) {
            ConversionResult result;
            result.success = false;
            result.errorMessage = "Failed to load file or no meshes found: " + loader.getErrorMessage();
            return result;
        }
        
        return convert(scene, options);
    }
};

//------------------------------------------------------------------------------
// Converter public interface
//------------------------------------------------------------------------------

Converter::Converter(Backend backend) : impl_(std::make_unique<Impl>(backend)) {}
Converter::~Converter() = default;

Converter::Converter(Converter&& other) noexcept = default;
Converter& Converter::operator=(Converter&& other) noexcept = default;

ConversionResult Converter::convert(const Scene& scene, const ConversionOptions& options) {
    return impl_->convert(scene, options);
}

ConversionResult Converter::convert(const Mesh& mesh, const ConversionOptions& options) {
    return impl_->convert(mesh, options);
}

ConversionResult Converter::convertFile(const std::string& path, const ConversionOptions& options) {
    return impl_->convertFile(path, options);
}

Backend Converter::getActiveBackend() const {
    return impl_->activeBackend_;
}

bool Converter::isReady() const {
    return impl_->ready_;
}

std::string Converter::getErrorMessage() const {
    return impl_->errorMessage_;
}

//------------------------------------------------------------------------------
// Convenience functions
//------------------------------------------------------------------------------

bool convertMeshToSplat(const std::string& inputPath,
                        const std::string& outputPath,
                        const ConversionOptions& options) {
    Converter converter(options.backend);
    if (!converter.isReady()) {
        return false;
    }
    
    ConversionResult result = converter.convertFile(inputPath, options);
    if (!result.success) {
        return false;
    }
    
    // Save to PLY
    PlyIO::save(outputPath, result.gaussians, options.plyFormat, 
                options.scaleMultiplier, options.flipY);
    return true;
}

} // namespace mesh2splat
