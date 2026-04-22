///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - CPU Converter Implementation        //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "CPUConverter.hpp"
#include "Rasterizer.hpp"
#include "../core/TextureSampler.hpp"
#include <chrono>
#include <sstream>
#include <algorithm>

namespace mesh2splat {

//------------------------------------------------------------------------------
// Implementation class
//------------------------------------------------------------------------------

class CPUConverter::Impl {
public:
    Impl() = default;
    
    ConversionResult convert(const Scene& scene, const ConversionOptions& options) {
        ConversionResult result;
        result.usedBackend = Backend::CPU;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        try {
            // Create rasterizer with specified resolution and mode
            Rasterizer rasterizer(options.resolution, options.rasterizationMode);
            
            // Count total triangles
            for (const auto& mesh : scene.meshes) {
                result.totalTriangles += mesh.faces.size();
            }
            
            if (options.verbose) {
                std::ostringstream oss;
                oss << "Processing " << scene.meshes.size() << " meshes with "
                    << result.totalTriangles << " triangles at resolution "
                    << options.resolution << " using "
                    << (options.rasterizationMode == RasterizationMode::UV ? "UV" : "Projection")
                    << " mode";
                result.messages.push_back(oss.str());
            }
            
            // Reserve estimated space for gaussians
            // Rough estimate: resolution^2 gaussians per mesh (upper bound)
            size_t estimatedGaussians = scene.meshes.size() * options.resolution * options.resolution / 4;
            result.gaussians.reserve(std::min(estimatedGaussians, size_t(10000000)));
            
            // Process each mesh
            for (size_t meshIdx = 0; meshIdx < scene.meshes.size(); ++meshIdx) {
                const auto& mesh = scene.meshes[meshIdx];
                
                if (options.verbose) {
                    std::ostringstream oss;
                    oss << "Processing mesh " << meshIdx << ": " << mesh.name
                        << " (" << mesh.faces.size() << " triangles)";
                    result.messages.push_back(oss.str());
                }
                
                // Rasterize mesh and collect gaussians
                rasterizer.rasterize(mesh, [&](const RasterFragment& frag, const Material& material) {
                    if (!frag.valid) return;
                    
                    // Sample material textures at fragment UV
                    MaterialSample matSample = MaterialSampler::sample(
                        material, frag.uv, frag.normal, frag.tangent);
                    
                    // Skip fully transparent pixels
                    if (matSample.baseColor.a < 0.01f) return;
                    
                    // Create gaussian from fragment
                    Gaussian g;
                    
                    // Position
                    g.x = frag.position.x;
                    g.y = frag.position.y;
                    g.z = frag.position.z;
                    
                    // Color: convert to SH0 coefficients
                    glm::vec3 color(matSample.baseColor.r, matSample.baseColor.g, matSample.baseColor.b);
                    if (options.srgbConversion) {
                        color = srgbToLinear(color);
                    }
                    glm::vec3 sh0 = colorToSH0(color);
                    g.r = sh0.r;
                    g.g = sh0.g;
                    g.b = sh0.b;
                    g.opacity = matSample.baseColor.a;
                    
                    // Scale (from rasterizer Jacobian computation)
                    g.scale_x = frag.scale.x * options.scaleMultiplier;
                    g.scale_y = frag.scale.y * options.scaleMultiplier;
                    g.scale_z = frag.scale.z * options.scaleMultiplier;
                    
                    // Rotation (from rasterizer)
                    // frag.rotation is glm::vec4(w, x, y, z)
                    g.rot_w = frag.rotation.x;  // .x is actually w
                    g.rot_x = frag.rotation.y;  // .y is actually x
                    g.rot_y = frag.rotation.z;  // .z is actually y
                    g.rot_z = frag.rotation.w;  // .w is actually z
                    
                    // Normal (from material normal map applied to geometry normal)
                    g.nx = matSample.normal.x;
                    g.ny = matSample.normal.y;
                    g.nz = matSample.normal.z;
                    
                    // PBR properties
                    g.metallic = matSample.metallic;
                    g.roughness = matSample.roughness;
                    g.ao = matSample.ao;
                    
                    // Validate and add
                    if (g.isValid()) {
                        result.gaussians.push_back(g);
                    }
                });
            }
            
            result.totalGaussians = result.gaussians.size();
            result.success = true;
            
            if (options.verbose) {
                std::ostringstream oss;
                oss << "Generated " << result.totalGaussians << " gaussians";
                result.messages.push_back(oss.str());
            }
            
        } catch (const std::exception& e) {
            result.success = false;
            result.errorMessage = std::string("Conversion failed: ") + e.what();
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        result.conversionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        
        return result;
    }
    
    ConversionResult convert(const Mesh& mesh, const ConversionOptions& options) {
        // Wrap single mesh in a scene
        Scene scene;
        scene.meshes.push_back(mesh);
        scene.bbox = mesh.bbox;
        return convert(scene, options);
    }
};

//------------------------------------------------------------------------------
// CPUConverter public interface
//------------------------------------------------------------------------------

CPUConverter::CPUConverter() : impl_(std::make_unique<Impl>()) {}

CPUConverter::~CPUConverter() = default;

ConversionResult CPUConverter::convert(const Scene& scene, const ConversionOptions& options) {
    return impl_->convert(scene, options);
}

ConversionResult CPUConverter::convert(const Mesh& mesh, const ConversionOptions& options) {
    return impl_->convert(mesh, options);
}

} // namespace mesh2splat
