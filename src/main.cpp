#include "utils/utils.hpp"
#include "utils/normalizedUvUnwrapping.hpp"
#include "renderer/renderer.hpp"
#include "glewGlfwHandlers/glewGlfwHandler.hpp"


int main(int argc, char** argv) {
    GlewGlfwHandler glewGlfwHandler(glm::ivec2(1080, 720), "Mesh2Splat");
    if(glewGlfwHandler.init() == -1) return -1;



    ImGuiUI ImGuiUI(3, 0, 0.1f, 0.5f); //TODO: give a meaning to these params
    ImGuiUI.initialize(glewGlfwHandler.getWindow());
    Renderer renderer(glewGlfwHandler.getWindow());
    renderer.initialize();

    while (!glfwWindowShouldClose(glewGlfwHandler.getWindow())) {
        glfwPollEvents();
        renderer.updateTransformations();
        renderer.clearingPrePass(ImGuiUI.getSceneBackgroundColor());

        ImGuiUI.preframe();
        ImGuiUI.renderUI();

        double currentTime = glfwGetTime();
        if (currentTime - renderer.getLastShaderCheckTime() > 1.0) {
            ImGuiUI.setRunConversion(renderer.updateShadersIfNeeded());
            renderer.setLastShaderCheckTime(currentTime);
        }

        {
            if (ImGuiUI.shouldLoadNewMesh() && !ImGuiUI.getFilePath().empty()) {
                //TODO: abstract all of this mesh loading somewhere else!!!!
                std::vector<Mesh> meshes;
                parseGltfFileToMesh(ImGuiUI.getFilePath(), ImGuiUI.getFilePathParentFolder(), meshes);
                printf("Parsed gltf mesh file\n");

                printf("Generating normalized UV coordinates (XATLAS)\n");
                generateNormalizedUvCoordinatesPerFace(renderer.getRenderContext()->normalizedUvSpaceHeight, renderer.getRenderContext()->normalizedUvSpaceHeight, meshes);
    
                printf("Loading mesh into OPENGL buffers\n");
                generateMeshesVBO(meshes, renderer.getRenderContext()->dataMeshAndGlMesh);
                
                //mesh2SplatConversionHandler.prepareMeshAndUploadToGPU(gui.getFilePath(), gui.getFilePathParentFolder(), dataMeshAndGlMesh, normalizedUvSpaceWidth, normalizedUvSpaceHeight);
                for (auto& mesh : renderer.getRenderContext()->dataMeshAndGlMesh)
                {
                    Mesh meshData = mesh.first;
                    GLMesh meshGl = mesh.second;
                    printf("Loading textures into utility std::map\n");
                    loadAllTextureMapImagesIntoMap(meshData.material, renderer.getRenderContext()->textureTypeMap);
                    generateTextures(meshData.material, renderer.getRenderContext()->textureTypeMap);
                }
                renderer.setViewportResolutionForConversion(ImGuiUI.getResolutionTarget());
                renderer.enableRenderPass("conversion");              
                ImGuiUI.setLoadNewMesh(false);
                ImGuiUI.setMeshLoaded(true);

            }


            if (ImGuiUI.shouldRunConversion()) {
                renderer.enableRenderPass("conversion"); 
                renderer.setViewportResolutionForConversion(ImGuiUI.getResolutionTarget());
                ImGuiUI.setRunConversion(false);
            }

            if (ImGuiUI.shouldSavePly() && !ImGuiUI.getFilePathParentFolder().empty())
            {
                GaussianDataSSBO* gaussianData = nullptr;
                unsigned int gaussianCount;
                read3dgsDataFromSsboBuffer(renderer.getRenderContext()->drawIndirectBuffer, renderer.getRenderContext()->gaussianBuffer, gaussianData, gaussianCount);
                writeBinaryPlyStandardFormatFromSSBO(ImGuiUI.getFullFilePathDestination(), gaussianData, gaussianCount);
                ImGuiUI.setShouldSavePly(false);
            }
            if (ImGuiUI.wasMeshLoaded())
            {
                renderer.resetRendererViewportResolution();
                renderer.setStdDevFromImGui(ImGuiUI.getGaussianStd());
                renderer.enableRenderPass("radixSort");
                renderer.enableRenderPass("gaussianSplatting");
            }

        }
        
        renderer.renderFrame();

        ImGuiUI.displayGaussianCount(renderer.getGaussianCountFromIndirectBuffer());
        ImGuiUI.postframe();

        glfwSwapBuffers(glewGlfwHandler.getWindow());
    }

    glfwTerminate();

    return 0;
}

