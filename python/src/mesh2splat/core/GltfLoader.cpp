///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - GLTF Loader Implementation          //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "GltfLoader.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <functional>
#include <cstring>
#include <algorithm>
#include <optional>
#include <filesystem>

namespace mesh2splat {

namespace {

// Helper struct for strided buffer access (supports interleaved vertex data)
template <typename T>
struct StridedAccessor {
    const unsigned char* base;
    size_t stride;
    size_t count;
    
    // H12 fix: use memcpy to avoid UB from unaligned reinterpret_cast
    T operator[](size_t index) const {
        T val;
        std::memcpy(&val, base + index * stride, sizeof(T));
        return val;
    }
};

// Helper to get strided buffer data from tinygltf accessor
template <typename T>
StridedAccessor<T> getBufferData(const tinygltf::Model& model, int accessorIndex) {
    const auto& accessor = model.accessors[accessorIndex];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];
    
    // byteStride = 0 means tightly packed (default to sizeof(T))
    size_t stride = bufferView.byteStride > 0 ? bufferView.byteStride : sizeof(T);
    const unsigned char* base = &buffer.data[bufferView.byteOffset + accessor.byteOffset];
    
    return StridedAccessor<T>{base, stride, accessor.count};
}

// Parse texture info from material
void parseTextureInfo(const tinygltf::Model& model, 
                      const tinygltf::Parameter& textureParameter,
                      const std::string& name,
                      TextureData& info) {
    auto it = textureParameter.json_double_value.find("index");
    if (it == textureParameter.json_double_value.end()) return;
    
    int textureIndex = static_cast<int>(it->second);
    if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures.size())) return;
    
    const tinygltf::Texture& texture = model.textures[textureIndex];
    if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size())) return;
    
    const tinygltf::Image& image = model.images[texture.source];
    
    info.width = image.width;
    info.height = image.height;
    info.channels = image.component;
    info.path = name;
    
    info.data.resize(image.image.size());
    std::memcpy(info.data.data(), image.image.data(), image.image.size());
    
    // Handle texCoord index
    auto texCoordIt = textureParameter.json_double_value.find("texCoord");
    if (texCoordIt != textureParameter.json_double_value.end()) {
        info.texCoordIndex = static_cast<int>(texCoordIt->second);
    } else {
        info.texCoordIndex = 0;
    }
}

// Parse material from tinygltf
void parseMaterial(const tinygltf::Model& model, int materialIndex, Material& mat) {
    if (materialIndex < 0 || materialIndex >= static_cast<int>(model.materials.size())) {
        return;
    }
    
    const tinygltf::Material& material = model.materials[materialIndex];
    mat.name = material.name;
    
    // Base Color Factor
    auto colorIt = material.values.find("baseColorFactor");
    if (colorIt != material.values.end()) {
        mat.baseColorFactor = glm::vec4(
            static_cast<float>(colorIt->second.ColorFactor()[0]),
            static_cast<float>(colorIt->second.ColorFactor()[1]),
            static_cast<float>(colorIt->second.ColorFactor()[2]),
            static_cast<float>(colorIt->second.ColorFactor()[3])
        );
    }
    
    // Base Color Texture
    auto baseColorTexIt = material.values.find("baseColorTexture");
    if (baseColorTexIt != material.values.end()) {
        parseTextureInfo(model, baseColorTexIt->second, "baseColorTexture", mat.baseColorTexture);
    }
    
    // Normal Texture
    auto normalTexIt = material.additionalValues.find("normalTexture");
    if (normalTexIt != material.additionalValues.end()) {
        parseTextureInfo(model, normalTexIt->second, "normalTexture", mat.normalTexture);
        
        auto scaleIt = normalTexIt->second.json_double_value.find("scale");
        if (scaleIt != normalTexIt->second.json_double_value.end()) {
            mat.normalScale = static_cast<float>(scaleIt->second);
        }
    }
    
    // Metallic-Roughness Texture
    auto metalRoughTexIt = material.values.find("metallicRoughnessTexture");
    if (metalRoughTexIt != material.values.end()) {
        parseTextureInfo(model, metalRoughTexIt->second, "metallicRoughnessTexture", mat.metallicRoughnessTexture);
    }
    
    // Occlusion Texture
    auto occlusionTexIt = material.additionalValues.find("occlusionTexture");
    if (occlusionTexIt != material.additionalValues.end()) {
        parseTextureInfo(model, occlusionTexIt->second, "occlusionTexture", mat.occlusionTexture);
        
        auto strengthIt = occlusionTexIt->second.json_double_value.find("strength");
        if (strengthIt != occlusionTexIt->second.json_double_value.end()) {
            mat.occlusionStrength = static_cast<float>(strengthIt->second);
        }
    }
    
    // Emissive Texture
    auto emissiveTexIt = material.additionalValues.find("emissiveTexture");
    if (emissiveTexIt != material.additionalValues.end()) {
        parseTextureInfo(model, emissiveTexIt->second, "emissiveTexture", mat.emissiveTexture);
    }
    
    // Emissive Factor
    // H17 fix: emissiveFactor is in additionalValues, not values, in tinygltf
    auto emissiveFactorIt = material.additionalValues.find("emissiveFactor");
    if (emissiveFactorIt != material.additionalValues.end() && emissiveFactorIt->second.number_array.size() >= 3) {
        mat.emissiveFactor = glm::vec3(
            static_cast<float>(emissiveFactorIt->second.number_array[0]),
            static_cast<float>(emissiveFactorIt->second.number_array[1]),
            static_cast<float>(emissiveFactorIt->second.number_array[2])
        );
    }
    
    // Metallic and Roughness Factors
    mat.metallicFactor = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
    mat.roughnessFactor = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);
}

} // anonymous namespace

bool GltfLoader::isGltfFile(const std::string& filePath) {
    std::filesystem::path path(filePath);
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".gltf" || ext == ".glb";
}

Scene GltfLoader::load(const std::string& filePath) {
    messages_.clear();
    errorMessage_.clear();
    Scene scene;
    scene.sourcePath = filePath;
    
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    
    // Determine file type and load
    std::filesystem::path path(filePath);
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    bool ret = false;
    if (ext == ".glb") {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);
    } else {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filePath);
    }
    
    if (!warn.empty()) {
        messages_.push_back("Warning: " + warn);
    }
    
    if (!ret) {
        errorMessage_ = "Failed to load GLTF: " + err;
        throw std::runtime_error(errorMessage_);
    }
    
    // Mesh instance with transform
    struct MeshInstance {
        int meshIndex;
        glm::mat4 transform;
    };
    std::vector<MeshInstance> meshInstances;
    
    // Traverse scene graph
    std::function<void(int, const glm::mat4&)> traverseNode = [&](int nodeIndex, const glm::mat4& parentTransform) {
        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size())) return;
        const tinygltf::Node& node = model.nodes[nodeIndex];
        
        // Build local transform
        glm::mat4 localTransform(1.0f);
        
        if (node.matrix.size() == 16) {
            for (int c = 0; c < 4; c++)
                for (int r = 0; r < 4; r++)
                    localTransform[c][r] = static_cast<float>(node.matrix[c * 4 + r]);
        } else {
            glm::mat4 T(1.0f), R(1.0f), S(1.0f);
            if (node.translation.size() == 3) {
                T = glm::translate(glm::mat4(1.0f), glm::vec3(
                    static_cast<float>(node.translation[0]),
                    static_cast<float>(node.translation[1]),
                    static_cast<float>(node.translation[2])));
            }
            if (node.rotation.size() == 4) {
                glm::quat q(
                    static_cast<float>(node.rotation[3]),  // w
                    static_cast<float>(node.rotation[0]),  // x
                    static_cast<float>(node.rotation[1]),  // y
                    static_cast<float>(node.rotation[2])); // z
                R = glm::mat4_cast(q);
            }
            if (node.scale.size() == 3) {
                S = glm::scale(glm::mat4(1.0f), glm::vec3(
                    static_cast<float>(node.scale[0]),
                    static_cast<float>(node.scale[1]),
                    static_cast<float>(node.scale[2])));
            }
            localTransform = T * R * S;
        }
        
        glm::mat4 worldTransform = parentTransform * localTransform;
        
        if (node.mesh >= 0 && node.mesh < static_cast<int>(model.meshes.size())) {
            meshInstances.push_back({node.mesh, worldTransform});
        }
        
        for (int child : node.children) {
            traverseNode(child, worldTransform);
        }
    };
    
    // Traverse from scene roots
    if (!model.scenes.empty()) {
        int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
        const tinygltf::Scene& gltfScene = model.scenes[sceneIndex];
        for (int rootNode : gltfScene.nodes) {
            traverseNode(rootNode, glm::mat4(1.0f));
        }
    }
    
    // Fallback: add all meshes with identity transform
    if (meshInstances.empty()) {
        for (int i = 0; i < static_cast<int>(model.meshes.size()); i++) {
            meshInstances.push_back({i, glm::mat4(1.0f)});
        }
    }
    
    // Process mesh instances
    int meshCounter = 0;
    glm::vec3 sceneMin(std::numeric_limits<float>::max());
    glm::vec3 sceneMax(std::numeric_limits<float>::lowest());
    
    for (const auto& instance : meshInstances) {
        const tinygltf::Mesh& gltfMesh = model.meshes[instance.meshIndex];
        glm::mat4 worldTransform = instance.transform;
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldTransform)));
        
        for (const auto& primitive : gltfMesh.primitives) {
            // Skip non-triangle primitives
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES && primitive.mode != -1) {
                messages_.push_back("Skipping non-triangle primitive in mesh: " + gltfMesh.name);
                continue;
            }
            
            // Must have POSITION
            if (primitive.attributes.find("POSITION") == primitive.attributes.end()) {
                messages_.push_back("Skipping primitive without POSITION in mesh: " + gltfMesh.name);
                continue;
            }
            
            Mesh mesh;
            std::string baseName = gltfMesh.name.empty() ? "mesh" : gltfMesh.name;
            mesh.name = baseName + "_" + std::to_string(meshCounter++);
            
            // Build index list
            std::vector<uint32_t> indices;
            
            if (primitive.indices >= 0) {
                const tinygltf::Accessor& indicesAccessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& bufferView = model.bufferViews[indicesAccessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                const unsigned char* indexData = buffer.data.data() + bufferView.byteOffset + indicesAccessor.byteOffset;
                indices.resize(indicesAccessor.count);
                
                if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    for (size_t i = 0; i < indicesAccessor.count; i++) {
                        uint16_t val;
                        std::memcpy(&val, indexData + i * sizeof(uint16_t), sizeof(uint16_t));
                        indices[i] = val;
                    }
                } else if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    for (size_t i = 0; i < indicesAccessor.count; i++) {
                        uint32_t val;
                        std::memcpy(&val, indexData + i * sizeof(uint32_t), sizeof(uint32_t));
                        indices[i] = val;
                    }
                } else if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    const uint8_t* buf = reinterpret_cast<const uint8_t*>(indexData);
                    for (size_t i = 0; i < indicesAccessor.count; i++) {
                        indices[i] = buf[i];
                    }
                } else {
                    messages_.push_back("Unsupported index type in mesh: " + mesh.name);
                    continue;
                }
            } else {
                // Non-indexed geometry
                const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
                indices.resize(posAccessor.count);
                for (uint32_t i = 0; i < static_cast<uint32_t>(posAccessor.count); i++) {
                    indices[i] = i;
                }
            }
            
            if (indices.size() < 3 || indices.size() % 3 != 0) {
                messages_.push_back("Invalid index count in mesh: " + mesh.name);
                continue;
            }
            
            // Get vertex data
            auto vertices = getBufferData<glm::vec3>(model, primitive.attributes.at("POSITION"));
            
            std::optional<StridedAccessor<glm::vec3>> normals;
            if (primitive.attributes.count("NORMAL")) {
                normals = getBufferData<glm::vec3>(model, primitive.attributes.at("NORMAL"));
            }
            
            std::optional<StridedAccessor<glm::vec2>> uvs;
            if (primitive.attributes.count("TEXCOORD_0")) {
                uvs = getBufferData<glm::vec2>(model, primitive.attributes.at("TEXCOORD_0"));
            }
            
            std::optional<StridedAccessor<glm::vec4>> tangents;
            if (primitive.attributes.count("TANGENT")) {
                tangents = getBufferData<glm::vec4>(model, primitive.attributes.at("TANGENT"));
            }
            
            // Parse material
            parseMaterial(model, primitive.material, mesh.material);
            
            // Build faces - use push_back to avoid uninitialized entries from skipped faces
            mesh.faces.reserve(indices.size() / 3);
            
            for (size_t i = 0; i < indices.size(); i += 3) {
                uint32_t idx[3] = {indices[i], indices[i + 1], indices[i + 2]};
                
                // H15 fix: bounds-check index data against vertex count
                if (idx[0] >= vertices.count || idx[1] >= vertices.count || idx[2] >= vertices.count) {
                    messages_.push_back("Out-of-bounds vertex index in mesh: " + mesh.name + " (skipping face)");
                    continue;
                }
                
                Face face{};
                for (int e = 0; e < 3; e++) {
                    // Transform position
                    glm::vec4 worldPos = worldTransform * glm::vec4(vertices[idx[e]], 1.0f);
                    face.positions[e] = glm::vec3(worldPos);
                    
                    // UV
                    if (uvs) {
                        face.uvs[e] = (*uvs)[idx[e]];
                    } else {
                        face.uvs[e] = glm::vec2(0.0f);
                    }
                    
                    // Normal
                    if (normals) {
                        face.normals[e] = glm::normalize(normalMatrix * (*normals)[idx[e]]);
                    }
                    
                    // Tangent
                    if (tangents) {
                        glm::vec3 tVec = glm::normalize(glm::mat3(worldTransform) * glm::vec3((*tangents)[idx[e]]));
                        face.tangents[e] = glm::vec4(tVec, (*tangents)[idx[e]].w);
                    }
                }
                
                // Compute face normal if not provided
                if (!normals) {
                    glm::vec3 faceNormal = glm::normalize(glm::cross(
                        face.positions[1] - face.positions[0],
                        face.positions[2] - face.positions[0]));
                    face.normals[0] = faceNormal;
                    face.normals[1] = faceNormal;
                    face.normals[2] = faceNormal;
                }
                
                // Compute tangents if not provided
                if (!tangents) {
                    computeTangents(face);
                }
                
                // Accumulate surface area
                mesh.surfaceArea += triangleArea3D(face.positions[0], face.positions[1], face.positions[2]);
                
                mesh.faces.push_back(face);
            }
            
            // Compute bounding box
            computeBBox(mesh);
            
            // Update scene bounding box
            sceneMin = glm::min(sceneMin, mesh.bbox.min);
            sceneMax = glm::max(sceneMax, mesh.bbox.max);
            
            scene.meshes.push_back(std::move(mesh));
        }
    }
    
    scene.bbox.min = sceneMin;
    scene.bbox.max = sceneMax;
    
    return scene;
}

void GltfLoader::computeTangents(Face& face) {
    // Compute tangent using UV deltas (simplified MikkTSpace-like approach)
    glm::vec3 dp1 = face.positions[1] - face.positions[0];
    glm::vec3 dp2 = face.positions[2] - face.positions[0];
    glm::vec2 duv1 = face.uvs[1] - face.uvs[0];
    glm::vec2 duv2 = face.uvs[2] - face.uvs[0];
    
    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    if (std::fabs(det) < 1e-8f) {
        det = 1.0f;
    }
    
    float invDet = 1.0f / det;
    
    glm::vec3 tangent = (dp1 * duv2.y - dp2 * duv1.y) * invDet;
    glm::vec3 bitangent = (dp2 * duv1.x - dp1 * duv2.x) * invDet;
    
    tangent = glm::normalize(tangent);
    bitangent = glm::normalize(bitangent);
    
    glm::vec3 normal = glm::normalize(glm::cross(dp1, dp2));
    float handedness = (glm::dot(glm::cross(normal, tangent), bitangent) < 0.0f) ? -1.0f : 1.0f;
    
    glm::vec4 finalTangent = glm::vec4(tangent, handedness);
    
    face.tangents[0] = finalTangent;
    face.tangents[1] = finalTangent;
    face.tangents[2] = finalTangent;
}

void GltfLoader::computeBBox(Mesh& mesh) {
    glm::vec3 minBB(std::numeric_limits<float>::max());
    glm::vec3 maxBB(std::numeric_limits<float>::lowest());
    
    for (const auto& face : mesh.faces) {
        for (int i = 0; i < 3; i++) {
            minBB = glm::min(minBB, face.positions[i]);
            maxBB = glm::max(maxBB, face.positions[i]);
        }
    }
    
    mesh.bbox.min = minBB;
    mesh.bbox.max = maxBB;
}

} // namespace mesh2splat
