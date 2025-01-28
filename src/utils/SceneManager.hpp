#pragma once
#include "utils.hpp"
#include "../renderer/renderPasses/RenderContext.hpp"
#include "./normalizedUvUnwrapping.hpp"
#include "../parsers/parsers.hpp"
#include "Texture.hpp"

class SceneManager {
public:
    SceneManager(RenderContext& context);
    ~SceneManager();

    bool loadModel(const std::string& filePath, const std::string& parentFolder);
    bool loadPly(const std::string& filePath);

    void updateMeshes();
    void cleanup();

private:
    RenderContext& renderContext;

    bool parseGltfFile(const std::string& filePath, const std::string& parentFolder, std::vector<Mesh>& meshes);
    MaterialGltf parseGltfMaterial(const tinygltf::Model& model, int materialIndex, std::string base_folder);
    TextureInfo parseGltfTextureInfo(const tinygltf::Model& model, const tinygltf::Parameter& textureParameter, std::string base_folder);
    void generateNormalizedUvCoordinates(std::vector<Mesh>& meshes);
    void loadTextures(const std::vector<Mesh>& meshes);
    void setupMeshBuffers(const std::vector<Mesh>& meshes);
    template <typename T>
    const T* getBufferData(const tinygltf::Model& model, int accessorIndex);
};
