///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - pybind11 Bindings                   //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include "Converter.hpp"
#include "core/Types.hpp"
#include "core/GltfLoader.hpp"
#include "core/PlyIO.hpp"

namespace py = pybind11;

namespace mesh2splat {

/// Convert gaussians to numpy arrays for efficient Python access
py::dict gaussiansToNumpy(const std::vector<Gaussian>& gaussians) {
    size_t n = gaussians.size();
    
    // Create numpy arrays for each attribute (requires GIL)
    auto positions = py::array_t<float>({n, size_t(3)});
    auto colors = py::array_t<float>({n, size_t(3)});
    auto opacities = py::array_t<float>(n);
    auto scales = py::array_t<float>({n, size_t(3)});
    auto rotations = py::array_t<float>({n, size_t(4)});
    auto normals = py::array_t<float>({n, size_t(3)});
    auto metallic = py::array_t<float>(n);
    auto roughness = py::array_t<float>(n);
    auto ao = py::array_t<float>(n);
    
    // Get raw pointers for GIL-free copy
    float* pos_data = positions.mutable_data();
    float* col_data = colors.mutable_data();
    float* opa_data = opacities.mutable_data();
    float* sca_data = scales.mutable_data();
    float* rot_data = rotations.mutable_data();
    float* nor_data = normals.mutable_data();
    float* met_data = metallic.mutable_data();
    float* rou_data = roughness.mutable_data();
    float* ao_data = ao.mutable_data();
    
    // Release GIL during data copy for large datasets
    {
        py::gil_scoped_release release;
        
        for (size_t i = 0; i < n; i++) {
            const auto& g = gaussians[i];
            
            pos_data[i * 3 + 0] = g.x;
            pos_data[i * 3 + 1] = g.y;
            pos_data[i * 3 + 2] = g.z;
            
            col_data[i * 3 + 0] = g.r;
            col_data[i * 3 + 1] = g.g;
            col_data[i * 3 + 2] = g.b;
            
            opa_data[i] = g.opacity;
            
            sca_data[i * 3 + 0] = g.scale_x;
            sca_data[i * 3 + 1] = g.scale_y;
            sca_data[i * 3 + 2] = g.scale_z;
            
            rot_data[i * 4 + 0] = g.rot_w;
            rot_data[i * 4 + 1] = g.rot_x;
            rot_data[i * 4 + 2] = g.rot_y;
            rot_data[i * 4 + 3] = g.rot_z;
            
            nor_data[i * 3 + 0] = g.nx;
            nor_data[i * 3 + 1] = g.ny;
            nor_data[i * 3 + 2] = g.nz;
            
            met_data[i] = g.metallic;
            rou_data[i] = g.roughness;
            ao_data[i] = g.ao;
        }
    }
    // GIL re-acquired here
    
    py::dict result;
    result["positions"] = positions;
    result["colors"] = colors;
    result["opacities"] = opacities;
    result["scales"] = scales;
    result["rotations"] = rotations;
    result["normals"] = normals;
    result["metallic"] = metallic;
    result["roughness"] = roughness;
    result["ao"] = ao;
    
    return result;
}

PYBIND11_MODULE(_mesh2splat, m) {
    m.doc() = "Mesh2Splat: Fast mesh to 3D Gaussian splat conversion";
    
    //--------------------------------------------------------------------------
    // Enums
    //--------------------------------------------------------------------------
    
    py::enum_<Backend>(m, "Backend", "Backend selection for conversion")
        .value("Auto", Backend::Auto, "Automatically select best available backend (GPU preferred)")
        .value("CPU", Backend::CPU, "CPU-only backend (portable, always available)")
        .value("GPU", Backend::GPU, "GPU backend using OpenGL (faster, requires headless context)")
        .export_values();
    
    py::enum_<PlyFormat>(m, "PlyFormat", "Output PLY format")
        .value("Standard", PlyFormat::Standard, "Standard 3DGS PLY format")
        .value("PBR", PlyFormat::PBR, "Extended format with metallic/roughness/AO")
        .value("Compressed", PlyFormat::Compressed, "Compressed format")
        .export_values();
    
    py::enum_<RasterizationMode>(m, "RasterizationMode", "Rasterization mode for conversion")
        .value("UV", RasterizationMode::UV, "Rasterize in original mesh UV space (texture-based)")
        .value("Projection", RasterizationMode::Projection, "Rasterize using orthogonal projection (triplanar)")
        .export_values();
    
    py::enum_<DcMode>(m, "DcMode", "DC (color) encoding mode")
        .value("Current", DcMode::Current, "SH0 encoding (default, matches original behavior)")
        .value("DirectLinear", DcMode::DirectLinear, "Linear RGB directly")
        .value("DirectSrgb", DcMode::DirectSrgb, "sRGB values directly")
        .export_values();
    
    py::enum_<OpacityMode>(m, "OpacityMode", "Opacity encoding mode")
        .value("Current", OpacityMode::Current, "Format-specific default")
        .value("Raw", OpacityMode::Raw, "Raw opacity (0-1)")
        .value("Logit", OpacityMode::Logit, "Inverse sigmoid (standard for PLY)")
        .export_values();
    
    //--------------------------------------------------------------------------
    // ConversionOptions
    //--------------------------------------------------------------------------
    
    py::class_<ConversionOptions>(m, "ConversionOptions", "Options for mesh to splat conversion")
        .def(py::init<>())
        .def_readwrite("resolution", &ConversionOptions::resolution,
            "Resolution of UV space rasterization (default: 512)")
        .def_readwrite("ply_format", &ConversionOptions::plyFormat,
            "Output PLY format (default: Standard)")
        .def_readwrite("scale_multiplier", &ConversionOptions::scaleMultiplier,
            "Scale multiplier for gaussian scales (default: 1.0)")
        .def_readwrite("srgb_conversion", &ConversionOptions::srgbConversion,
            "Whether to use sRGB color space conversion (default: True)")
        .def_readwrite("backend", &ConversionOptions::backend,
            "Backend selection (default: Auto)")
        .def_readwrite("rasterization_mode", &ConversionOptions::rasterizationMode,
            "Rasterization mode: UV (texture-based) or Projection (triplanar) (default: UV)")
        .def_readwrite("dc_mode", &ConversionOptions::dcMode,
            "DC (color) encoding mode (default: Current/SH0)")
        .def_readwrite("opacity_mode", &ConversionOptions::opacityMode,
            "Opacity encoding mode (default: Logit)")
        .def_readwrite("verbose", &ConversionOptions::verbose,
            "Enable verbose logging (default: False)")
        .def_readwrite("flip_y", &ConversionOptions::flipY,
            "Apply 180-degree X-axis rotation for SuperSplat/viewer compatibility (default: True)")
        .def("__repr__", [](const ConversionOptions& o) {
            std::string mode_str = (o.rasterizationMode == RasterizationMode::UV) ? "UV" : "Projection";
            return "<ConversionOptions resolution=" + std::to_string(o.resolution) + 
                   " mode=" + mode_str + ">";
        });
    
    //--------------------------------------------------------------------------
    // Gaussian
    //--------------------------------------------------------------------------
    
    py::class_<Gaussian>(m, "Gaussian", "A single 3D Gaussian splat")
        .def(py::init<>())
        .def_readwrite("x", &Gaussian::x, "X position")
        .def_readwrite("y", &Gaussian::y, "Y position")
        .def_readwrite("z", &Gaussian::z, "Z position")
        .def_readwrite("r", &Gaussian::r, "Red SH0 coefficient")
        .def_readwrite("g", &Gaussian::g, "Green SH0 coefficient")
        .def_readwrite("b", &Gaussian::b, "Blue SH0 coefficient")
        .def_readwrite("opacity", &Gaussian::opacity, "Opacity (0-1)")
        .def_readwrite("scale_x", &Gaussian::scale_x, "X scale")
        .def_readwrite("scale_y", &Gaussian::scale_y, "Y scale")
        .def_readwrite("scale_z", &Gaussian::scale_z, "Z scale")
        .def_readwrite("rot_x", &Gaussian::rot_x, "Rotation quaternion X")
        .def_readwrite("rot_y", &Gaussian::rot_y, "Rotation quaternion Y")
        .def_readwrite("rot_z", &Gaussian::rot_z, "Rotation quaternion Z")
        .def_readwrite("rot_w", &Gaussian::rot_w, "Rotation quaternion W")
        .def_readwrite("nx", &Gaussian::nx, "Normal X")
        .def_readwrite("ny", &Gaussian::ny, "Normal Y")
        .def_readwrite("nz", &Gaussian::nz, "Normal Z")
        .def_readwrite("metallic", &Gaussian::metallic, "Metallic (PBR)")
        .def_readwrite("roughness", &Gaussian::roughness, "Roughness (PBR)")
        .def_readwrite("ao", &Gaussian::ao, "Ambient occlusion (PBR)")
        .def("is_valid", &Gaussian::isValid, "Check if gaussian is valid")
        .def("__repr__", [](const Gaussian& g) {
            return "<Gaussian pos=(" + std::to_string(g.x) + "," + 
                   std::to_string(g.y) + "," + std::to_string(g.z) + ")>";
        });
    
    //--------------------------------------------------------------------------
    // ConversionResult
    //--------------------------------------------------------------------------
    
    py::class_<ConversionResult>(m, "ConversionResult", "Result of mesh to splat conversion")
        .def(py::init<>())
        .def_readonly("gaussians", &ConversionResult::gaussians, "List of gaussians")
        .def_readonly("total_triangles", &ConversionResult::totalTriangles, "Total input triangles")
        .def_readonly("total_gaussians", &ConversionResult::totalGaussians, "Total output gaussians")
        .def_readonly("conversion_time_ms", &ConversionResult::conversionTimeMs, "Conversion time in milliseconds")
        .def_readonly("used_backend", &ConversionResult::usedBackend, "Backend that was used")
        .def_readonly("messages", &ConversionResult::messages, "Info/warning messages")
        .def_readonly("success", &ConversionResult::success, "Whether conversion succeeded")
        .def_readonly("error_message", &ConversionResult::errorMessage, "Error message if failed")
        .def("to_numpy", [](const ConversionResult& r) {
            return gaussiansToNumpy(r.gaussians);
        }, "Convert gaussians to numpy arrays")
        .def("__repr__", [](const ConversionResult& r) {
            if (r.success) {
                return "<ConversionResult success=True gaussians=" + 
                       std::to_string(r.totalGaussians) + " time=" + 
                       std::to_string(r.conversionTimeMs) + "ms>";
            } else {
                return "<ConversionResult success=False error=\"" + r.errorMessage + "\">";
            }
        });
    
    //--------------------------------------------------------------------------
    // Converter (main class)
    //--------------------------------------------------------------------------
    
    py::class_<Converter>(m, "Converter", "Main mesh to Gaussian splat converter")
        .def(py::init<Backend>(), py::arg("backend") = Backend::Auto,
            "Create a converter with specified backend")
        .def("convert_file", &Converter::convertFile,
            py::arg("path"), py::arg("options") = ConversionOptions(),
            py::call_guard<py::gil_scoped_release>(),
            "Load and convert a GLTF/GLB file to gaussians")
        .def("convert", [](Converter& c, const Scene& scene, const ConversionOptions& options) {
            return c.convert(scene, options);
        }, py::arg("scene"), py::arg("options") = ConversionOptions(),
            py::call_guard<py::gil_scoped_release>(),
            "Convert a loaded scene to gaussians")
        .def("is_ready", &Converter::isReady, "Check if converter is ready")
        .def("get_active_backend", &Converter::getActiveBackend, "Get the active backend")
        .def("get_error_message", &Converter::getErrorMessage, "Get error message if not ready")
        .def_static("get_available_backends", &Converter::getAvailableBackends,
            "Get list of available backends on this system")
        .def_static("is_backend_available", &Converter::isBackendAvailable,
            py::arg("backend"), "Check if a specific backend is available")
        .def("__repr__", [](const Converter& c) {
            std::string backend_str;
            switch (c.getActiveBackend()) {
                case Backend::CPU: backend_str = "CPU"; break;
                case Backend::GPU: backend_str = "GPU"; break;
                default: backend_str = "Auto"; break;
            }
            return "<Converter backend=" + backend_str + " ready=" + 
                   (c.isReady() ? "True" : "False") + ">";
        });
    
    //--------------------------------------------------------------------------
    // PlyIO for save/load
    //--------------------------------------------------------------------------
    
    py::class_<PlyIO>(m, "PlyIO", "PLY file I/O utilities")
        .def_static("save", &PlyIO::save,
            py::arg("path"), py::arg("gaussians"), 
            py::arg("format") = PlyFormat::Standard,
            py::arg("scale_multiplier") = 1.0f,
            py::arg("flip_y") = true,
            py::call_guard<py::gil_scoped_release>(),
            "Save gaussians to PLY file")
        .def_static("load", &PlyIO::load,
            py::arg("path"),
            py::call_guard<py::gil_scoped_release>(),
            "Load gaussians from PLY file");
    
    //--------------------------------------------------------------------------
    // BBox
    //--------------------------------------------------------------------------
    
    py::class_<BBox>(m, "BBox", "Axis-aligned bounding box")
        .def(py::init<>())
        .def_readonly("min", &BBox::min, "Minimum corner")
        .def_readonly("max", &BBox::max, "Maximum corner")
        .def("center", &BBox::center, "Get center point")
        .def("size", &BBox::size, "Get size");
    
    //--------------------------------------------------------------------------
    // Material
    //--------------------------------------------------------------------------
    
    py::class_<Material>(m, "Material", "PBR material properties")
        .def(py::init<>())
        .def_readonly("name", &Material::name)
        .def_readonly("metallic_factor", &Material::metallicFactor)
        .def_readonly("roughness_factor", &Material::roughnessFactor);
    
    //--------------------------------------------------------------------------
    // Face
    //--------------------------------------------------------------------------
    
    py::class_<Face>(m, "Face", "Triangle face with vertex attributes")
        .def(py::init<>());
    
    //--------------------------------------------------------------------------
    // Mesh
    //--------------------------------------------------------------------------
    
    py::class_<Mesh>(m, "Mesh", "Mesh primitive with material")
        .def(py::init<>())
        .def_readonly("name", &Mesh::name)
        .def_readonly("faces", &Mesh::faces)
        .def_readonly("material", &Mesh::material)
        .def_readonly("surface_area", &Mesh::surfaceArea)
        .def_readonly("bbox", &Mesh::bbox)
        .def("__repr__", [](const Mesh& m) {
            return "<Mesh name=\"" + m.name + "\" faces=" + std::to_string(m.faces.size()) + ">";
        });
    
    //--------------------------------------------------------------------------
    // Scene
    //--------------------------------------------------------------------------
    
    py::class_<Scene>(m, "Scene", "Complete scene with multiple meshes")
        .def(py::init<>())
        .def_readonly("meshes", &Scene::meshes)
        .def_readonly("bbox", &Scene::bbox)
        .def_readonly("source_path", &Scene::sourcePath)
        .def("__repr__", [](const Scene& s) {
            return "<Scene meshes=" + std::to_string(s.meshes.size()) + " source=\"" + s.sourcePath + "\">";
        });
    
    //--------------------------------------------------------------------------
    // GltfLoader
    //--------------------------------------------------------------------------
    
    py::class_<GltfLoader>(m, "GltfLoader", "GLTF/GLB file loader")
        .def(py::init<>())
        .def("load", &GltfLoader::load, py::arg("path"),
            py::call_guard<py::gil_scoped_release>(),
            "Load a GLTF or GLB file and return a Scene");
    
    //--------------------------------------------------------------------------
    // Module-level functions
    //--------------------------------------------------------------------------
    
    m.def("convert", [](const std::string& input_path, 
                        const std::string& output_path,
                        const ConversionOptions& options) {
        return convertMeshToSplat(input_path, output_path, options);
    }, py::arg("input_path"), py::arg("output_path"), 
       py::arg("options") = ConversionOptions(),
       py::call_guard<py::gil_scoped_release>(),
       "Convert a mesh file to PLY splat file (convenience function)");
    
    m.def("get_version", &getVersion, "Get library version");
    m.def("get_build_info", &getBuildInfo, "Get build information");
    
    m.def("gaussians_to_numpy", &gaussiansToNumpy, py::arg("gaussians"),
        "Convert a list of Gaussian objects to numpy arrays");
}

} // namespace mesh2splat
