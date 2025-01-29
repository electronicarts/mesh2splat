#include "SceneManager.hpp"
#include <iostream>
#include <cstring>

SceneManager::SceneManager(RenderContext& context) : renderContext(context)
{
}

SceneManager::~SceneManager() {
    cleanup();
}


bool SceneManager::loadModel(const std::string& filePath, const std::string& parentFolder) {
    std::vector<Mesh> meshes;
    if (!parseGltfFile(filePath, parentFolder, meshes)) {
        std::cerr << "Failed to parse GLTF file: " << filePath << std::endl;
        return false;
    }

    generateNormalizedUvCoordinates(meshes);
    setupMeshBuffers(meshes);
    //TODO!!!: this for now actually works with only 1 mesh
    loadTextures(meshes);
    glUtils::generateTextures(renderContext.material, renderContext.textureTypeMap);

    return true;
}

bool SceneManager::loadPly(const std::string& filePath) {
    std::vector<GaussianDataSSBO> gaussians;
    parsers::loadPlyFile(filePath, gaussians);

    //TODO: bad, very bad
    glGenBuffers(1, &(renderContext.drawIndirectBuffer));
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderContext.drawIndirectBuffer);
    glBufferData(GL_DRAW_INDIRECT_BUFFER,
                    sizeof(IRenderPass::DrawElementsIndirectCommand),
                    nullptr,
                    GL_DYNAMIC_DRAW);

    IRenderPass::DrawElementsIndirectCommand cmd_init;
    cmd_init.count         = 4;  
    cmd_init.instanceCount = 0;  
    cmd_init.first         = 0;
    cmd_init.baseVertex    = 0;
    cmd_init.baseInstance  = 0;

    glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(IRenderPass::DrawElementsIndirectCommand), &cmd_init);

    glUtils::fillGaussianBufferSsbo(&(renderContext.gaussianBuffer), gaussians);
    renderContext.countFromPly = gaussians.size();
    return true;
}

template <typename T>
const T* SceneManager::getBufferData(const tinygltf::Model& model, int accessorIndex) {

    const auto& accessor    = model.accessors[accessorIndex];
    const auto& bufferView  = model.bufferViews[accessor.bufferView];
    const auto& buffer      = model.buffers[bufferView.buffer];
    
    //TODO: This may not be too safe
    const T* dataPtr = reinterpret_cast<const T*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
    
    return dataPtr;
}

TextureInfo SceneManager::parseGltfTextureInfo(const tinygltf::Model& model, const tinygltf::Parameter& textureParameter, std::string base_folder) {
    TextureInfo info;

    auto it = textureParameter.json_double_value.find("index");
    if (it != textureParameter.json_double_value.end()) {
        int textureIndex = static_cast<int>(it->second);
        if (textureIndex >= 0 && textureIndex < model.textures.size()) {
            const tinygltf::Texture& texture = model.textures[textureIndex];
            if (texture.source >= 0 && texture.source < model.images.size()) {
                const tinygltf::Image& image = model.images[texture.source];

                std::string fileExtension = image.mimeType.substr(image.mimeType.find_last_of('/') + 1);
                info.path = base_folder + image.name + "." + fileExtension;

                // Dimensions
                info.width = image.width;
                info.height = image.height;
            }

            // Handling texCoord index if present
            auto texCoordIt = textureParameter.json_double_value.find("texCoord");
            if (texCoordIt != textureParameter.json_double_value.end()) {
                info.texCoordIndex = static_cast<int>(texCoordIt->second);
            }
            else {
                info.texCoordIndex = 0; // Default texture coordinate set
            }
        }
    }

    return info;
}

MaterialGltf SceneManager::parseGltfMaterial(const tinygltf::Model& model, int materialIndex, std::string base_folder) {
    MaterialGltf materialGltf;

    if (materialIndex < 0 || materialIndex >= model.materials.size()) {
        return materialGltf;
    }

    const tinygltf::Material& material = model.materials[materialIndex];

    materialGltf.name = material.name;

    // Base Color Factor
    auto colorIt = material.values.find("baseColorFactor");
    if (colorIt != material.values.end()) {
        materialGltf.baseColorFactor = glm::vec4(
            static_cast<float>(colorIt->second.ColorFactor()[0]),
            static_cast<float>(colorIt->second.ColorFactor()[1]),
            static_cast<float>(colorIt->second.ColorFactor()[2]),
            static_cast<float>(colorIt->second.ColorFactor()[3])
        );
    }

    //Remember that it should be: R=ambient occlusion G=roughness B=metallic for the AO_Roughness_Metallic texture map

    // Base Color Texture
    auto baseColorTexIt = material.values.find("baseColorTexture");
    if (baseColorTexIt != material.values.end()) {
        materialGltf.baseColorTexture = parseGltfTextureInfo(model, baseColorTexIt->second, base_folder);
    }
    else {
        materialGltf.baseColorTexture.path = EMPTY_TEXTURE;
    }

    // Normal Texture
    auto normalTexIt = material.additionalValues.find("normalTexture");
    if (normalTexIt != material.additionalValues.end()) {
        materialGltf.normalTexture = parseGltfTextureInfo(model, normalTexIt->second, base_folder);

        auto scaleIt = normalTexIt->second.json_double_value.find("scale");
        if (scaleIt != normalTexIt->second.json_double_value.end()) {
            materialGltf.normalScale = static_cast<float>(scaleIt->second);
        }
        else {
            materialGltf.normalScale = 1.0f; // Default scale if not specified
        }
    }
    else {
        materialGltf.normalTexture.path = EMPTY_TEXTURE;
    }

    // Metallic-Roughness Texture
    auto metalRoughTexIt = material.values.find("metallicRoughnessTexture");
    if (metalRoughTexIt != material.values.end()) {
        materialGltf.metallicRoughnessTexture = parseGltfTextureInfo(model, metalRoughTexIt->second, base_folder);
    }

    // Occlusion Texture
    auto occlusionTexIt = material.additionalValues.find("occlusionTexture");
    if (occlusionTexIt != material.additionalValues.end()) {
        materialGltf.occlusionTexture = parseGltfTextureInfo(model, occlusionTexIt->second, base_folder);

        auto scaleIt = occlusionTexIt->second.json_double_value.find("strength");
        if (scaleIt != occlusionTexIt->second.json_double_value.end()) {
            materialGltf.occlusionStrength = static_cast<float>(scaleIt->second);
        }
        else {
            materialGltf.occlusionStrength = 1.0f; // Default scale if not specified
        }
    }
    else {
        materialGltf.occlusionTexture.path = EMPTY_TEXTURE;
    }

    // Emissive Texture
    auto emissiveTexIt = material.additionalValues.find("emissiveTexture");
    if (emissiveTexIt != material.additionalValues.end()) {
        materialGltf.emissiveTexture = parseGltfTextureInfo(model, emissiveTexIt->second, base_folder);
    }
    else {
        materialGltf.emissiveTexture.path = EMPTY_TEXTURE;
    }


    // Emissive Factor
    auto emissiveFactorIt = material.values.find("emissiveFactor");
    if (emissiveFactorIt != material.values.end()) {
        materialGltf.emissiveFactor = glm::vec3(
            static_cast<float>(emissiveFactorIt->second.number_array[0]),
            static_cast<float>(emissiveFactorIt->second.number_array[1]),
            static_cast<float>(emissiveFactorIt->second.number_array[2])
        );
    }

    // Metallic and Roughness Factors
    materialGltf.metallicFactor = material.pbrMetallicRoughness.metallicFactor;
    materialGltf.roughnessFactor = material.pbrMetallicRoughness.roughnessFactor;

    return materialGltf;
}

bool SceneManager::parseGltfFile(const std::string& filePath, const std::string& parentFolder, std::vector<Mesh>& meshes) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    
    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);
    if (!ret) {
        std::cerr << "Failed to load glTF: " << err << std::endl;
        return false;
    }
    

    if (!warn.empty()) {
        std::cout << "glTF parse warning: " << warn << std::endl;
    }

    //remember that "when a 3D model is created as GLTF it is already triangulated"
    for (const auto& mesh : model.meshes) {
        Mesh myMesh(mesh.name);
        
        for (const auto& primitive : mesh.primitives) {
            const tinygltf::Accessor& indicesAccessor = model.accessors[primitive.indices];
            const tinygltf::BufferView& bufferView = model.bufferViews[indicesAccessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
            const unsigned char* indexData = buffer.data.data() + bufferView.byteOffset + indicesAccessor.byteOffset;
            std::vector<int> indices(indicesAccessor.count);

            if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                const uint16_t* buf = reinterpret_cast<const uint16_t*>(indexData);
                for (size_t i = 0; i < indicesAccessor.count; i++) {
                    indices[i] = buf[i];
                }
            }
            else if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                const uint32_t* buf = reinterpret_cast<const uint32_t*>(indexData);
                for (size_t i = 0; i < indicesAccessor.count; i++) {
                    indices[i] = buf[i];
                }
            }

            // Extract vertex data
            auto vertices       = getBufferData<glm::vec3>(model, primitive.attributes.at("POSITION"));
            auto uvs            = getBufferData<glm::vec2>(model, primitive.attributes.at("TEXCOORD_0"));         
            auto normals        = getBufferData<glm::vec3>(model, primitive.attributes.at("NORMAL"));

            //TODO: 01/2025 --> I wrote this part of the code 1 year ago: what was I thinking?
            const glm::vec4* tangents = NULL;
            bool hasTangents = false;
            if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
            {
                tangents = getBufferData<glm::vec4>(model, primitive.attributes.at("TANGENT"));
                hasTangents = true;
            }
            
            myMesh.material = parseGltfMaterial(model, primitive.material, parentFolder);

            //TODO: indices is wrong to be used like this because it is not a global index amongst all primitives
            myMesh.faces.resize(indices.size() / 3);
            Face* dst = myMesh.faces.data();
            
            for (size_t i = 0, count = indices.size(); i < count; i += 3, ++dst) {             

                size_t index[3] = { indices[i], indices[i + 1], indices[i + 2] };

                if (hasTangents)
                {
                    for (int e = 0; e < 3; e++) {
                        dst->tangent[e] = tangents[index[e]];
                    }
                } else {
                    //TODO: HMMMMMMM
                    //but tbh just reimport it in Blender and export the damn tangents (there is a checkbox on the right of the exporter window under "data->mesh->Tangents")
                    glm::vec3 dv1 = dst->pos[1] - dst->pos[0];
                    glm::vec3 dv2 = dst->pos[2] - dst->pos[0];

                    glm::vec2 duv1 = dst->uv[1] - dst->uv[0];
                    glm::vec2 duv2 = dst->uv[2] - dst->uv[0];

                    float r = 1.0f / (duv1.x * duv2.y - duv1.y * duv2.x);
                    glm::vec4 tangent = glm::vec4((dv1 * duv2.y - dv2 * duv1.y) * r, 1.0f);
                    dst->tangent[0] = tangent;
                    dst->tangent[1] = tangent;
                    dst->tangent[2] = tangent;
                }

                for (int e = 0; e < 3; e++)
                {
                    dst->pos[e] = vertices[index[e]];
                    dst->uv[e] = uvs[index[e]];
                    dst->normal[e] = normals[index[e]]; 
                }
                    
            }
            
            meshes.push_back(myMesh);
        }
    }
    return true;
}

// Generate Normalized UV Coordinates
void SceneManager::generateNormalizedUvCoordinates(std::vector<Mesh>& meshes)
{
    uvUnwrapping::generateNormalizedUvCoordinatesPerMesh(renderContext.normalizedUvSpaceHeight, renderContext.normalizedUvSpaceHeight, meshes);
}

// Setup Mesh Buffers
void SceneManager::setupMeshBuffers(const std::vector<Mesh>& meshes)
{

    renderContext.dataMeshAndGlMesh.clear();
    renderContext.dataMeshAndGlMesh.reserve(meshes.size());

    for (auto& mesh : meshes) {
        GLMesh glMesh;
        std::vector<float> vertices;  
        
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
        unsigned int floatsPerVertex = 17;
        glMesh.vertexCount = vertices.size() / floatsPerVertex; // Number of vertices

        // 3 position, 3 normal, 4 tangent, 2 UV, 2 NORMALIZED UVs, 3 scale = 17
        size_t vertexStride = floatsPerVertex * sizeof(float);

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
        // Normalized UV attribute
        glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, vertexStride, (void*)(12 * sizeof(float)));
        glEnableVertexAttribArray(4);
        // Scale attribute
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, vertexStride, (void*)(14 * sizeof(float)));
        glEnableVertexAttribArray(5);

        //Should use array indices for per face data such as rotation and scale or directly compute it in the shader, should actually do it in a compute shader and be done

        // Unbind VAO
        glBindVertexArray(0);

        // Add to list of GLMeshes
        renderContext.dataMeshAndGlMesh.push_back(std::make_pair(mesh, glMesh));
    }

}

void SceneManager::loadTextures(const std::vector<Mesh>& meshes)
{
    
    for (auto& mesh : meshes)
    {
        //TODO!!!: the render context supports only one mesh
        //BASECOLOR ALBEDO TEXTURE LOAD
        if (mesh.material.baseColorTexture.path != EMPTY_TEXTURE)
        {
            renderContext.textureTypeMap.insert_or_assign(BASE_COLOR_TEXTURE, parsers::loadImageAndBpp(mesh.material.baseColorTexture.path, renderContext.material.baseColorTexture.width, renderContext.material.baseColorTexture.height));
        }
        else {
            renderContext.material.baseColorTexture.width = MAX_RESOLUTION_TARGET;
            renderContext.material.baseColorTexture.height = MAX_RESOLUTION_TARGET;
            renderContext.textureTypeMap.erase(BASE_COLOR_TEXTURE);
        }

        //METALLIC-ROUGHNESS TEXTURE LOAD
        if (mesh.material.metallicRoughnessTexture.path != EMPTY_TEXTURE)
        {
            std::string metallicPath, roughnessPath, folderPath;
            //In case Metallic and Roughness are separate we need to combine them in the appropriate RGB channels
            if (parsers::extractImageNames(mesh.material.metallicRoughnessTexture.path, folderPath, metallicPath, roughnessPath))
            {
                int channels;
                unsigned char* metallicRoughnessTextureData = parsers::combineMetallicRoughness(metallicPath.c_str(), roughnessPath.c_str(), renderContext.material.metallicRoughnessTexture.width, renderContext.material.metallicRoughnessTexture.height, channels); 
                renderContext.textureTypeMap.insert_or_assign(METALLIC_ROUGHNESS_TEXTURE, TextureDataGl(metallicRoughnessTextureData, channels));
            }
            else {
                renderContext.textureTypeMap.insert_or_assign(METALLIC_ROUGHNESS_TEXTURE, parsers::loadImageAndBpp(mesh.material.metallicRoughnessTexture.path, renderContext.material.metallicRoughnessTexture.width, renderContext.material.metallicRoughnessTexture.height));
            }
        }
        else {
            renderContext.material.metallicRoughnessTexture.width = MAX_RESOLUTION_TARGET;
            renderContext.material.metallicRoughnessTexture.height = MAX_RESOLUTION_TARGET;
            renderContext.textureTypeMap.erase(METALLIC_ROUGHNESS_TEXTURE);
        }

        //NORMAL TEXTURE LOAD
        if (mesh.material.normalTexture.path != EMPTY_TEXTURE)
        {
            renderContext.textureTypeMap.insert_or_assign(NORMAL_TEXTURE, parsers::loadImageAndBpp(mesh.material.normalTexture.path, renderContext.material.normalTexture.width, renderContext.material.normalTexture.height));
        }
        else {
            renderContext.material.normalTexture.width = MAX_RESOLUTION_TARGET;
            renderContext.material.normalTexture.height = MAX_RESOLUTION_TARGET;
            renderContext.textureTypeMap.erase(NORMAL_TEXTURE);

        }

        //OCCLUSION TEXTURE LOAD
        if (mesh.material.occlusionTexture.path != EMPTY_TEXTURE)
        {
            renderContext.textureTypeMap.insert_or_assign(AO_TEXTURE, parsers::loadImageAndBpp(mesh.material.occlusionTexture.path, renderContext.material.occlusionTexture.width, renderContext.material.occlusionTexture.height));
        }
        else {
            renderContext.material.occlusionTexture.width = MAX_RESOLUTION_TARGET;
            renderContext.material.occlusionTexture.height = MAX_RESOLUTION_TARGET;
            renderContext.textureTypeMap.erase(AO_TEXTURE);
        }

        //EMISSIVE TEXTURE LOAD
        if (mesh.material.emissiveTexture.path != EMPTY_TEXTURE)
        {
            renderContext.textureTypeMap.insert_or_assign(EMISSIVE_TEXTURE, parsers::loadImageAndBpp(mesh.material.emissiveTexture.path, renderContext.material.emissiveTexture.width, renderContext.material.emissiveTexture.height));
        }
        else {
            renderContext.material.emissiveTexture.width = MAX_RESOLUTION_TARGET;
            renderContext.material.emissiveTexture.height = MAX_RESOLUTION_TARGET;
            renderContext.textureTypeMap.erase(EMISSIVE_TEXTURE);
        }
    }
    
}

void SceneManager::updateMeshes()
{
}

void SceneManager::cleanup()
{
}