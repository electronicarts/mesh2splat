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
    std::vector<utils::Mesh> meshes;
    if (!parseGltfFile(filePath, parentFolder, meshes)) {
        std::cerr << "Failed to parse GLTF file: " << filePath << std::endl;
        return false;
    }

    //generateNormalizedUvCoordinates(meshes);
    setupMeshBuffers(meshes);
    loadTextures(meshes);
    glUtils::generateTextures(renderContext.meshToTextureData);

    return true;
}

bool SceneManager::loadPly(const std::string& filePath) {
    try {
        parsers::loadPlyFile(filePath, renderContext.readGaussians);
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error loading PLY file: " << filePath << std::endl;
        return false;
    }
    return false;
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

void SceneManager::parseGltfTextureInfo(const tinygltf::Model& model, const tinygltf::Parameter& textureParameter, std::string base_folder, std::string name, utils::TextureInfo& info) {

    auto it = textureParameter.json_double_value.find("index");

    if (it != textureParameter.json_double_value.end()) {
        int textureIndex = static_cast<int>(it->second);
        if (textureIndex >= 0 && textureIndex < model.textures.size()) {
            const tinygltf::Texture& texture = model.textures[textureIndex];
            if (texture.source >= 0 && texture.source < model.images.size()) {
                const tinygltf::Image& image = model.images[texture.source];

                std::string fileExtension = image.mimeType.substr(image.mimeType.find_last_of('/') + 1);
                info.path = base_folder + image.name + "." + fileExtension;

                info.width      = image.width;
                info.height     = image.height;

                info.texture.resize(image.image.size());
                std::memcpy(info.texture.data(), image.image.data(), image.image.size());
                
                info.path       = name;
                info.channels   = image.component;
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
}

void SceneManager::parseGltfMaterial(const tinygltf::Model& model, int materialIndex, std::string base_folder, utils::MaterialGltf& materialGltf) {

    if (materialIndex < 0 || materialIndex >= model.materials.size()) {
        return;
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

    //R=ambient occlusion G=roughness B=metallic for AO_Roughness_Metallic
    // Base Color Texture
    auto baseColorTexIt = material.values.find("baseColorTexture");
    if (baseColorTexIt != material.values.end()) {
         parseGltfTextureInfo(model, baseColorTexIt->second, base_folder, "baseColorTexture", materialGltf.baseColorTexture);
    }
    else {
        materialGltf.baseColorTexture.path = EMPTY_TEXTURE;
    }

    // Normal Texture
    auto normalTexIt = material.additionalValues.find("normalTexture");
    if (normalTexIt != material.additionalValues.end()) {
         parseGltfTextureInfo(model, normalTexIt->second, base_folder, "normalTexture", materialGltf.normalTexture);

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
         parseGltfTextureInfo(model, metalRoughTexIt->second, base_folder, "metallicRoughnessTexture", materialGltf.metallicRoughnessTexture);
    }

    // Occlusion Texture
    auto occlusionTexIt = material.additionalValues.find("occlusionTexture");
    if (occlusionTexIt != material.additionalValues.end()) {
        parseGltfTextureInfo(model, occlusionTexIt->second, base_folder, "occlusionTexture", materialGltf.occlusionTexture);

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
        parseGltfTextureInfo(model, emissiveTexIt->second, base_folder, "emissiveTexture", materialGltf.emissiveTexture);
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
}

bool SceneManager::parseGltfFile(const std::string& filePath, const std::string& parentFolder, std::vector<utils::Mesh>& meshes) {
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
    int index = 0;

    for (const auto& mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            std::string baseName = "mesh";
            utils::Mesh myMesh(baseName.append(std::to_string(index)));
            ++index;
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

            //TODO: 01/2025 --> I wrote this part of the code more than a year ago: what was I thinking?
            const glm::vec4* tangents = NULL;
            bool hasTangents = false;
            if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
            {
                tangents = getBufferData<glm::vec4>(model, primitive.attributes.at("TANGENT"));
                hasTangents = true;
            }
            
            parseGltfMaterial(model, primitive.material, parentFolder, myMesh.material);

            //TODO: indices is wrong to be used like this because it is not a global index amongst all primitives
            myMesh.faces.resize(indices.size() / 3);
            utils::Face* dst = myMesh.faces.data();
            
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
void SceneManager::generateNormalizedUvCoordinates(std::vector<utils::Mesh>& meshes)
{
    uvUnwrapping::generateNormalizedUvCoordinatesPerMesh(renderContext.normalizedUvSpaceWidth, renderContext.normalizedUvSpaceHeight, meshes);
}

// Setup Mesh Buffers
void SceneManager::setupMeshBuffers(std::vector<utils::Mesh>& meshes)
{

    renderContext.dataMeshAndGlMesh.clear();
    renderContext.dataMeshAndGlMesh.reserve(meshes.size());

    float totalSurface = 0;
    
    glm::vec3 minBB(FLT_MAX);
    glm::vec3 maxBB(-FLT_MAX);

    for (auto& mesh : meshes) {
        utils::GLMesh glMesh;
        std::vector<float> vertices;  
        float meshSurface = 0;
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

                minBB.x = std::min(minBB.x, face.pos[i].x);
                minBB.y = std::min(minBB.y, face.pos[i].y);
                minBB.z = std::min(minBB.z, face.pos[i].z);

                maxBB.x = std::max(maxBB.x, face.pos[i].x);
                maxBB.y = std::max(maxBB.y, face.pos[i].y);
                maxBB.z = std::max(maxBB.z, face.pos[i].z);

            }
            
            mesh.surfaceArea += utils::triangleArea(face.pos[0], face.pos[1], face.pos[2]);
            
        }
        mesh.bbox = utils::BBox(minBB, maxBB);

        renderContext.totalSurfaceArea += mesh.surfaceArea;

        
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

void SceneManager::loadTextures(const std::vector<utils::Mesh>& meshes)
{
    for (auto& mesh : meshes)
    {
        std::map<std::string, utils::TextureDataGl> textureMapForThisMesh;
        //BASECOLOR ALBEDO TEXTURE LOAD
        if (mesh.material.baseColorTexture.path != EMPTY_TEXTURE)
        {
            utils::TextureDataGl tdgl(mesh.material.baseColorTexture);
            textureMapForThisMesh.insert_or_assign(BASE_COLOR_TEXTURE, tdgl);
        }
        else {
            renderContext.material.baseColorTexture.width = MAX_RESOLUTION_TARGET;
            renderContext.material.baseColorTexture.height = MAX_RESOLUTION_TARGET;
            textureMapForThisMesh.erase(BASE_COLOR_TEXTURE);
        }

        //METALLIC-ROUGHNESS TEXTURE LOAD
        if (mesh.material.metallicRoughnessTexture.path != EMPTY_TEXTURE)
        {
            utils::TextureDataGl tdgl(mesh.material.metallicRoughnessTexture);
            textureMapForThisMesh.insert_or_assign(METALLIC_ROUGHNESS_TEXTURE, tdgl);
        }
        else {
            renderContext.material.metallicRoughnessTexture.width = MAX_RESOLUTION_TARGET;
            renderContext.material.metallicRoughnessTexture.height = MAX_RESOLUTION_TARGET;
            textureMapForThisMesh.erase(METALLIC_ROUGHNESS_TEXTURE);
        }

        //NORMAL TEXTURE LOAD
        if (mesh.material.normalTexture.path != EMPTY_TEXTURE)
        {
            
            utils::TextureDataGl tdgl(mesh.material.normalTexture);
            textureMapForThisMesh.insert_or_assign(NORMAL_TEXTURE, tdgl);
        }
        else {
            renderContext.material.normalTexture.width = MAX_RESOLUTION_TARGET;
            renderContext.material.normalTexture.height = MAX_RESOLUTION_TARGET;
            textureMapForThisMesh.erase(NORMAL_TEXTURE);

        }

        //OCCLUSION TEXTURE LOAD
        if (mesh.material.occlusionTexture.path != EMPTY_TEXTURE)
        {
            utils::TextureDataGl tdgl(mesh.material.occlusionTexture);
            textureMapForThisMesh.insert_or_assign(AO_TEXTURE, tdgl);
        }
        else {
            renderContext.material.occlusionTexture.width = MAX_RESOLUTION_TARGET;
            renderContext.material.occlusionTexture.height = MAX_RESOLUTION_TARGET;
            textureMapForThisMesh.erase(AO_TEXTURE);
        }

        //EMISSIVE TEXTURE LOAD
        if (mesh.material.emissiveTexture.path != EMPTY_TEXTURE)
        {
            utils::TextureDataGl tdgl(mesh.material.emissiveTexture);
            textureMapForThisMesh.insert_or_assign(EMISSIVE_TEXTURE, tdgl);
        }
        else {
            renderContext.material.emissiveTexture.width = MAX_RESOLUTION_TARGET;
            renderContext.material.emissiveTexture.height = MAX_RESOLUTION_TARGET;
            textureMapForThisMesh.erase(EMISSIVE_TEXTURE);
        }

        renderContext.meshToTextureData.insert_or_assign(mesh.name, textureMapForThisMesh);

    }
    
}

void SceneManager::exportPly(const std::string outputFile, unsigned int exportFormat)
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.gaussianBuffer);

    std::vector<utils::GaussianDataSSBO> cpuData(renderContext.numberOfGaussians);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    glGetBufferSubData(
        GL_SHADER_STORAGE_BUFFER,
        0,
        renderContext.numberOfGaussians * sizeof(utils::GaussianDataSSBO),
        cpuData.data()
    );
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    float scaleMultiplier = renderContext.gaussianStd / static_cast<float>(renderContext.resolutionTarget);
    auto format           = exportFormat;  

    std::thread(
        [=, data = std::move(cpuData)]() mutable 
        {
            parsers::savePlyVector(outputFile, data, format, scaleMultiplier);
        }
    ).detach();
    
}


void SceneManager::updateMeshes()
{
}

void SceneManager::cleanup()
{
}