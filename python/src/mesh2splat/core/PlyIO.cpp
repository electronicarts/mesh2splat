///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - PLY I/O Implementation              //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "PlyIO.hpp"
#include <happly.h>
#include <fstream>
#include <cmath>
#include <stdexcept>

namespace mesh2splat {

namespace {

// Convert float to uint8 with clamping
inline uint8_t floatToByte(float v) {
    float clamped = glm::clamp(v, 0.0f, 1.0f);
    return static_cast<uint8_t>(std::round(clamped * 255.0f));
}

// Sigmoid function for opacity
inline float sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

// Inverse sigmoid for encoding opacity
inline float inverseSigmoid(float x) {
    x = glm::clamp(x, 0.0001f, 0.9999f);
    return std::log(x / (1.0f - x));
}

// Octahedral wrap helper
inline glm::vec2 octWrap(const glm::vec2& v) {
    glm::vec2 result;
    result.x = (1.0f - std::abs(v.y)) * (v.x >= 0.0f ? 1.0f : -1.0f);
    result.y = (1.0f - std::abs(v.x)) * (v.y >= 0.0f ? 1.0f : -1.0f);
    return result;
}

} // anonymous namespace

glm::vec2 PlyIO::encodeOctahedral(const glm::vec3& normal) {
    // Normalize to unit octahedron
    glm::vec3 n = normal / (std::abs(normal.x) + std::abs(normal.y) + std::abs(normal.z) + epsilon::kNormalize);
    
    glm::vec2 result;
    if (n.z >= 0.0f) {
        result = glm::vec2(n.x, n.y);
    } else {
        result = octWrap(glm::vec2(n.x, n.y));
    }
    
    // Map from [-1,1] to [0,1]
    result.x = result.x * 0.5f + 0.5f;
    result.y = result.y * 0.5f + 0.5f;
    
    return result;
}

void PlyIO::save(const std::string& filename,
                 const std::vector<Gaussian>& gaussians,
                 PlyFormat format,
                 float scaleMultiplier,
                 bool flipY) {
    switch (format) {
        case PlyFormat::Standard:
            writeStandard(filename, gaussians, scaleMultiplier, flipY);
            break;
        case PlyFormat::PBR:
            writePBR(filename, gaussians, scaleMultiplier, flipY);
            break;
        case PlyFormat::Compressed:
            writeCompressed(filename, gaussians, scaleMultiplier, flipY);
            break;
        default:
            writeStandard(filename, gaussians, scaleMultiplier, flipY);
            break;
    }
}

void PlyIO::writeStandard(const std::string& filename,
                          const std::vector<Gaussian>& gaussians,
                          float scaleMultiplier,
                          bool flipY) {
    std::ofstream file(filename, std::ios::binary | std::ios::out);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }
    
    // Write header
    file << "ply\n";
    file << "format binary_little_endian 1.0\n";
    file << "element vertex " << gaussians.size() << "\n";
    
    file << "property float x\n";
    file << "property float y\n";
    file << "property float z\n";
    
    file << "property float nx\n";
    file << "property float ny\n";
    file << "property float nz\n";
    
    file << "property float f_dc_0\n";
    file << "property float f_dc_1\n";
    file << "property float f_dc_2\n";
    
    // f_rest_0 to f_rest_44 (higher order SH coefficients, set to 0)
    for (int i = 0; i <= 44; ++i) {
        file << "property float f_rest_" << i << "\n";
    }
    
    file << "property float opacity\n";
    
    file << "property float scale_0\n";
    file << "property float scale_1\n";
    file << "property float scale_2\n";
    
    file << "property float rot_0\n";
    file << "property float rot_1\n";
    file << "property float rot_2\n";
    file << "property float rot_3\n";
    
    file << "end_header\n";
    
    // Write data
    float zero = 0.0f;
    for (const auto& g : gaussians) {
        // Position (flip: negate Y and Z for 180° rotation around X axis)
        float posX = g.x;
        float posY = flipY ? -g.y : g.y;
        float posZ = flipY ? -g.z : g.z;
        file.write(reinterpret_cast<const char*>(&posX), sizeof(float));
        file.write(reinterpret_cast<const char*>(&posY), sizeof(float));
        file.write(reinterpret_cast<const char*>(&posZ), sizeof(float));
        
        // Normal (flip: negate Y and Z)
        float normX = g.nx;
        float normY = flipY ? -g.ny : g.ny;
        float normZ = flipY ? -g.nz : g.nz;
        file.write(reinterpret_cast<const char*>(&normX), sizeof(float));
        file.write(reinterpret_cast<const char*>(&normY), sizeof(float));
        file.write(reinterpret_cast<const char*>(&normZ), sizeof(float));
        
        // Color (already in SH0 format from converter)
        file.write(reinterpret_cast<const char*>(&g.r), sizeof(float));
        file.write(reinterpret_cast<const char*>(&g.g), sizeof(float));
        file.write(reinterpret_cast<const char*>(&g.b), sizeof(float));
        
        // f_rest (zeros)
        for (int i = 0; i <= 44; ++i) {
            file.write(reinterpret_cast<const char*>(&zero), sizeof(float));
        }
        
        // Opacity (convert to logit space for 3DGS compatibility)
        float logitOpacity = inverseSigmoid(g.opacity);
        file.write(reinterpret_cast<const char*>(&logitOpacity), sizeof(float));
        
        // Scale (log space with safe log to avoid -inf)
        float sx = safeLog(g.scale_x * scaleMultiplier);
        float sy = safeLog(g.scale_y * scaleMultiplier);
        float sz = safeLog(g.scale_z * scaleMultiplier);
        file.write(reinterpret_cast<const char*>(&sx), sizeof(float));
        file.write(reinterpret_cast<const char*>(&sy), sizeof(float));
        file.write(reinterpret_cast<const char*>(&sz), sizeof(float));
        
        // Rotation quaternion - PLY format is rot_0=w, rot_1=x, rot_2=y, rot_3=z
        // flipY: 180° rotation around X axis: q_flip(0,1,0,0) * q = (-x, w, -z, y)
        float rot0, rot1, rot2, rot3;
        if (flipY) {
            rot0 = -g.rot_x;  // new w = -old x
            rot1 = g.rot_w;   // new x = old w
            rot2 = -g.rot_z;  // new y = -old z
            rot3 = g.rot_y;   // new z = old y
        } else {
            rot0 = g.rot_w;   // w
            rot1 = g.rot_x;   // x
            rot2 = g.rot_y;   // y
            rot3 = g.rot_z;   // z
        }
        file.write(reinterpret_cast<const char*>(&rot0), sizeof(float));
        file.write(reinterpret_cast<const char*>(&rot1), sizeof(float));
        file.write(reinterpret_cast<const char*>(&rot2), sizeof(float));
        file.write(reinterpret_cast<const char*>(&rot3), sizeof(float));
    }
    
    file.close();
    if (!file) {
        throw std::runtime_error("Failed to write PLY file (I/O error): " + filename);
    }
}

void PlyIO::writePBR(const std::string& filename,
                     const std::vector<Gaussian>& gaussians,
                     float scaleMultiplier,
                     bool flipY) {
    std::ofstream file(filename, std::ios::binary | std::ios::out);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }
    
    // Write header
    file << "ply\n";
    file << "format binary_little_endian 1.0\n";
    file << "element vertex " << gaussians.size() << "\n";
    
    file << "property float x\n";
    file << "property float y\n";
    file << "property float z\n";
    
    file << "property float nx\n";
    file << "property float ny\n";
    file << "property float nz\n";
    
    file << "property float f_dc_0\n";
    file << "property float f_dc_1\n";
    file << "property float f_dc_2\n";
    
    file << "property float metallicFactor\n";
    file << "property float roughnessFactor\n";
    
    file << "property float opacity\n";
    
    file << "property float scale_0\n";
    file << "property float scale_1\n";
    file << "property float scale_2\n";
    
    file << "property float rot_0\n";
    file << "property float rot_1\n";
    file << "property float rot_2\n";
    file << "property float rot_3\n";
    
    file << "end_header\n";
    
    // Write data
    for (const auto& g : gaussians) {
        // Position (flip: negate Y and Z for 180° rotation around X axis)
        float posX = g.x;
        float posY = flipY ? -g.y : g.y;
        float posZ = flipY ? -g.z : g.z;
        file.write(reinterpret_cast<const char*>(&posX), sizeof(float));
        file.write(reinterpret_cast<const char*>(&posY), sizeof(float));
        file.write(reinterpret_cast<const char*>(&posZ), sizeof(float));
        
        // Normal (flip: negate Y and Z)
        float normX = g.nx;
        float normY = flipY ? -g.ny : g.ny;
        float normZ = flipY ? -g.nz : g.nz;
        file.write(reinterpret_cast<const char*>(&normX), sizeof(float));
        file.write(reinterpret_cast<const char*>(&normY), sizeof(float));
        file.write(reinterpret_cast<const char*>(&normZ), sizeof(float));
        
        // Color (already in SH0 format from converter)
        file.write(reinterpret_cast<const char*>(&g.r), sizeof(float));
        file.write(reinterpret_cast<const char*>(&g.g), sizeof(float));
        file.write(reinterpret_cast<const char*>(&g.b), sizeof(float));
        
        // PBR properties
        file.write(reinterpret_cast<const char*>(&g.metallic), sizeof(float));
        file.write(reinterpret_cast<const char*>(&g.roughness), sizeof(float));
        
        // Opacity (convert to logit space for 3DGS compatibility)
        float logitOpacity = inverseSigmoid(g.opacity);
        file.write(reinterpret_cast<const char*>(&logitOpacity), sizeof(float));
        
        // Scale (log space with safe log to avoid -inf)
        float sx = safeLog(g.scale_x * scaleMultiplier);
        float sy = safeLog(g.scale_y * scaleMultiplier);
        float sz = safeLog(g.scale_z * scaleMultiplier);
        file.write(reinterpret_cast<const char*>(&sx), sizeof(float));
        file.write(reinterpret_cast<const char*>(&sy), sizeof(float));
        file.write(reinterpret_cast<const char*>(&sz), sizeof(float));
        
        // Rotation quaternion - PLY format is rot_0=w, rot_1=x, rot_2=y, rot_3=z
        // flipY: 180° rotation around X axis: q_flip(0,1,0,0) * q = (-x, w, -z, y)
        float rot0, rot1, rot2, rot3;
        if (flipY) {
            rot0 = -g.rot_x;  // new w = -old x
            rot1 = g.rot_w;   // new x = old w
            rot2 = -g.rot_z;  // new y = -old z
            rot3 = g.rot_y;   // new z = old y
        } else {
            rot0 = g.rot_w;   // w
            rot1 = g.rot_x;   // x
            rot2 = g.rot_y;   // y
            rot3 = g.rot_z;   // z
        }
        file.write(reinterpret_cast<const char*>(&rot0), sizeof(float));
        file.write(reinterpret_cast<const char*>(&rot1), sizeof(float));
        file.write(reinterpret_cast<const char*>(&rot2), sizeof(float));
        file.write(reinterpret_cast<const char*>(&rot3), sizeof(float));
    }
    
    file.close();
    if (!file) {
        throw std::runtime_error("Failed to write PLY file (I/O error): " + filename);
    }
}

void PlyIO::writeCompressed(const std::string& filename,
                            const std::vector<Gaussian>& gaussians,
                            float scaleMultiplier,
                            bool flipY) {
    std::ofstream file(filename, std::ios::binary | std::ios::out);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }
    
    // Write header
    file << "ply\n";
    file << "format binary_little_endian 1.0\n";
    file << "element vertex " << gaussians.size() << "\n";
    
    file << "property float x\n";
    file << "property float y\n";
    file << "property float z\n";
    
    file << "property uint8 red\n";
    file << "property uint8 green\n";
    file << "property uint8 blue\n";
    file << "property uint8 opacity\n";
    
    file << "property float rot_0\n";
    file << "property float rot_1\n";
    file << "property float rot_2\n";
    file << "property float rot_3\n";
    
    file << "property float scale_0\n";
    file << "property float scale_1\n";
    file << "property float scale_2\n";
    
    file << "property uint8 octa_nx\n";
    file << "property uint8 octa_ny\n";
    
    file << "property uint8 roughness\n";
    file << "property uint8 metallic\n";
    
    file << "end_header\n";
    
    // Write data
    for (const auto& g : gaussians) {
        // Position (flip: negate Y and Z for 180° rotation around X axis)
        float posX = g.x;
        float posY = flipY ? -g.y : g.y;
        float posZ = flipY ? -g.z : g.z;
        file.write(reinterpret_cast<const char*>(&posX), sizeof(float));
        file.write(reinterpret_cast<const char*>(&posY), sizeof(float));
        file.write(reinterpret_cast<const char*>(&posZ), sizeof(float));
        
        // Color (uint8)
        uint8_t r = floatToByte(g.r);
        uint8_t gr = floatToByte(g.g);
        uint8_t b = floatToByte(g.b);
        uint8_t a = floatToByte(g.opacity);
        file.write(reinterpret_cast<const char*>(&r), sizeof(uint8_t));
        file.write(reinterpret_cast<const char*>(&gr), sizeof(uint8_t));
        file.write(reinterpret_cast<const char*>(&b), sizeof(uint8_t));
        file.write(reinterpret_cast<const char*>(&a), sizeof(uint8_t));
        
        // Rotation quaternion - PLY format is rot_0=w, rot_1=x, rot_2=y, rot_3=z
        // flipY: 180° rotation around X axis: q_flip(0,1,0,0) * q = (-x, w, -z, y)
        float rot0, rot1, rot2, rot3;
        if (flipY) {
            rot0 = -g.rot_x;  // new w = -old x
            rot1 = g.rot_w;   // new x = old w
            rot2 = -g.rot_z;  // new y = -old z
            rot3 = g.rot_y;   // new z = old y
        } else {
            rot0 = g.rot_w;   // w
            rot1 = g.rot_x;   // x
            rot2 = g.rot_y;   // y
            rot3 = g.rot_z;   // z
        }
        file.write(reinterpret_cast<const char*>(&rot0), sizeof(float));
        file.write(reinterpret_cast<const char*>(&rot1), sizeof(float));
        file.write(reinterpret_cast<const char*>(&rot2), sizeof(float));
        file.write(reinterpret_cast<const char*>(&rot3), sizeof(float));
        
        // Scale (log space with safe log, using min of x,y for z)
        float sx = safeLog(g.scale_x * scaleMultiplier);
        float sy = safeLog(g.scale_y * scaleMultiplier);
        float minXY = std::min(g.scale_x, g.scale_y);
        float sz = safeLog(minXY * scaleMultiplier);
        file.write(reinterpret_cast<const char*>(&sx), sizeof(float));
        file.write(reinterpret_cast<const char*>(&sy), sizeof(float));
        file.write(reinterpret_cast<const char*>(&sz), sizeof(float));
        
        // Normal (flip: negate Y and Z, then octahedral encode)
        float normX = g.nx;
        float normY = flipY ? -g.ny : g.ny;
        float normZ = flipY ? -g.nz : g.nz;
        glm::vec2 octNormal = encodeOctahedral(glm::vec3(normX, normY, normZ));
        uint8_t nx = static_cast<uint8_t>(glm::clamp(std::round(octNormal.x * 255.0f), 0.0f, 255.0f));
        uint8_t ny = static_cast<uint8_t>(glm::clamp(std::round(octNormal.y * 255.0f), 0.0f, 255.0f));
        file.write(reinterpret_cast<const char*>(&nx), sizeof(uint8_t));
        file.write(reinterpret_cast<const char*>(&ny), sizeof(uint8_t));
        
        // PBR (uint8)
        uint8_t roughness = floatToByte(g.roughness);
        uint8_t metallic = floatToByte(g.metallic);
        file.write(reinterpret_cast<const char*>(&roughness), sizeof(uint8_t));
        file.write(reinterpret_cast<const char*>(&metallic), sizeof(uint8_t));
    }
    
    file.close();
    if (!file) {
        throw std::runtime_error("Failed to write PLY file (I/O error): " + filename);
    }
}

std::vector<Gaussian> PlyIO::load(const std::string& filename) {
    std::vector<Gaussian> gaussians;
    
    try {
        happly::PLYData plyIn(filename);
        
        // Get vertex properties
        auto vertex_x = plyIn.getElement("vertex").getProperty<float>("x");
        auto vertex_y = plyIn.getElement("vertex").getProperty<float>("y");
        auto vertex_z = plyIn.getElement("vertex").getProperty<float>("z");
        
        auto vertex_nx = plyIn.getElement("vertex").getProperty<float>("nx");
        auto vertex_ny = plyIn.getElement("vertex").getProperty<float>("ny");
        auto vertex_nz = plyIn.getElement("vertex").getProperty<float>("nz");
        
        auto vertex_f_dc_0 = plyIn.getElement("vertex").getProperty<float>("f_dc_0");
        auto vertex_f_dc_1 = plyIn.getElement("vertex").getProperty<float>("f_dc_1");
        auto vertex_f_dc_2 = plyIn.getElement("vertex").getProperty<float>("f_dc_2");
        
        auto vertex_opacity = plyIn.getElement("vertex").getProperty<float>("opacity");
        
        auto vertex_scale_0 = plyIn.getElement("vertex").getProperty<float>("scale_0");
        auto vertex_scale_1 = plyIn.getElement("vertex").getProperty<float>("scale_1");
        auto vertex_scale_2 = plyIn.getElement("vertex").getProperty<float>("scale_2");
        
        auto vertex_rot_0 = plyIn.getElement("vertex").getProperty<float>("rot_0");
        auto vertex_rot_1 = plyIn.getElement("vertex").getProperty<float>("rot_1");
        auto vertex_rot_2 = plyIn.getElement("vertex").getProperty<float>("rot_2");
        auto vertex_rot_3 = plyIn.getElement("vertex").getProperty<float>("rot_3");
        
        size_t numVertices = vertex_x.size();
        gaussians.reserve(numVertices);
        
        for (size_t i = 0; i < numVertices; ++i) {
            Gaussian g;
            
            // Position
            g.x = vertex_x[i];
            g.y = vertex_y[i];
            g.z = vertex_z[i];
            
            // Color (convert from SH0)
            glm::vec3 color = sh0ToColor(glm::vec3(vertex_f_dc_0[i], vertex_f_dc_1[i], vertex_f_dc_2[i]));
            g.r = color.r;
            g.g = color.g;
            g.b = color.b;
            
            // Opacity (sigmoid of stored value)
            g.opacity = sigmoid(vertex_opacity[i]);
            
            // Scale (exp of stored log values)
            g.scale_x = std::exp(vertex_scale_0[i]);
            g.scale_y = std::exp(vertex_scale_1[i]);
            g.scale_z = std::exp(vertex_scale_2[i]);
            
            // Normal
            g.nx = vertex_nx[i];
            g.ny = vertex_ny[i];
            g.nz = vertex_nz[i];
            
            // Rotation (normalize quaternion)
            glm::quat rot(vertex_rot_0[i], vertex_rot_1[i], vertex_rot_2[i], vertex_rot_3[i]);
            rot = glm::normalize(rot);
            g.rot_w = rot.w;
            g.rot_x = rot.x;
            g.rot_y = rot.y;
            g.rot_z = rot.z;
            
            // Default PBR values
            g.metallic = 0.0f;
            g.roughness = 1.0f;
            g.ao = 1.0f;
            
            gaussians.push_back(g);
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load PLY file: " + std::string(e.what()));
    }
    
    return gaussians;
}

} // namespace mesh2splat
