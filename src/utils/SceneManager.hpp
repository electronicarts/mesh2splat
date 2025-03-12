#pragma once
#include "utils.hpp"
#include "../renderer/renderPasses/RenderContext.hpp"
#include "./normalizedUvUnwrapping.hpp"
#include "../parsers/parsers.hpp"
#include "../renderer/RenderPasses.hpp"

class SceneManager {
public:
    SceneManager(RenderContext& context);
    ~SceneManager();

    bool loadModel(const std::string& filePath, const std::string& parentFolder);
    bool loadPly(const std::string& filePath);
    void exportPly(const std::string outputFile, unsigned int exportFormat);

    void updateMeshes();
    void cleanup();

private:
    RenderContext& renderContext;

    bool parseGltfFile(const std::string& filePath, const std::string& parentFolder, std::vector<utils::Mesh>& meshes);
    void parseGltfMaterial(const tinygltf::Model& model, int materialIndex, std::string base_folder, utils::MaterialGltf& materialGltf);
    void parseGltfTextureInfo(const tinygltf::Model& model, const tinygltf::Parameter& textureParameter, std::string base_folder, std::string name, utils::TextureInfo& info);
    void generateNormalizedUvCoordinates(std::vector<utils::Mesh>& meshes);
    void loadTextures(const std::vector<utils::Mesh>& meshes);
    void setupMeshBuffers(const std::vector<utils::Mesh>& meshes);
    template <typename T>
    const T* getBufferData(const tinygltf::Model& model, int accessorIndex);
};
