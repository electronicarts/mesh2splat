#pragma once

#include <string>
#include <vector>
#include <map>
#include <utility>

#include <glm/glm.hpp>
#include <tiny_gltf.h>

// NOTE: fix include paths for this repo layout:
#include "../renderer/renderPasses/RenderContext.hpp"
#include "utils.hpp"

class SceneManager
{
public:
    SceneManager(RenderContext& context);
    ~SceneManager();

    bool loadModel(const std::string& filePath, const std::string& parentFolder);
    bool loadPly(const std::string& filePath);

    void exportSplats(const std::string outputFile, unsigned int exportFormat);

    void updateMeshes();
    void cleanup();

private:
    RenderContext& renderContext;

    bool parseGltfFile(const std::string& filePath,
                       const std::string& parentFolder,
                       std::vector<utils::Mesh>& meshes);

    void setupMeshBuffers(std::vector<utils::Mesh>& meshes);
    void loadTextures(const std::vector<utils::Mesh>& meshes);
    void generateNormalizedUvCoordinates(std::vector<utils::Mesh>& meshes);

    void parseGltfTextureInfo(const tinygltf::Model& model,
                              const tinygltf::Parameter& textureParameter,
                              std::string base_folder,
                              std::string name,
                              utils::TextureInfo& info);

    void parseGltfMaterial(const tinygltf::Model& model,
                           int materialIndex,
                           std::string base_folder,
                           utils::MaterialGltf& materialGltf);

private:
    // ---------------------------------------------------------------------
    // SAFE glTF attribute readers (respect accessor stride/offset/componentType)
    // ---------------------------------------------------------------------
    static bool readVec3Attribute(const tinygltf::Model& model, int accessorIndex, std::vector<glm::vec3>& out);
    static bool readVec2Attribute(const tinygltf::Model& model, int accessorIndex, std::vector<glm::vec2>& out);
    static bool readVec4Attribute(const tinygltf::Model& model, int accessorIndex, std::vector<glm::vec4>& out);
    static bool readIndexAttribute(const tinygltf::Model& model, int accessorIndex, std::vector<int>& out);

    static float readComponentAsFloat(const unsigned char* p, int componentType, bool normalized);
    static int   componentByteSize(int componentType);

    static bool checkAccessorIsVecN(const tinygltf::Accessor& acc, int expectedType, const char* debugName);
};
