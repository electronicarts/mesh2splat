///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "utils/normalizedUvUnwrapping.hpp"
#include "renderer/renderer.hpp"
#include "glewGlfwHandlers/glewGlfwHandler.hpp"
#include "renderer/guiRendererConcreteMediator.hpp"
#include "utils/argparser.hpp"
#include <cstdlib>

#define IDI_SMALL       101

unsigned int RGBSwizzle(unsigned int c) {
    return (c >> 16) | (c & 0xff00) | ((c & 0xff) << 16);
}

int main(int argc, char** argv) {
//    GlewGlfwHandler glewGlfwHandler(glm::ivec2(1080, 720), "Mesh2Splat");
    GlewGlfwHandler glewGlfwHandler(glm::ivec2(2800, 1280), "Mesh2Splat");

    Camera camera(
        glm::vec3(0.0f, 0.0f, 5.0f), 
        glm::vec3(0.0f, 1.0f, 0.0f), 
        -90.0f, 
        0.0f
    );  

    IoHandler ioHandler(glewGlfwHandler.getWindow(), camera);
    if(glewGlfwHandler.init() == -1) return -1;

    ioHandler.setupCallbacks();

    ImGuiUI ImGuiUI(0.65f, 0.5f); //TODO: give a meaning to these params
    ImGuiUI.initialize(glewGlfwHandler.getWindow());

    // https://stackoverflow.com/questions/7375003/how-to-convert-hicon-to-hbitmap-in-vc
#if _WIN32
    {
        //      HICON hIcon = (HICON)LoadImage(GetModuleHandle(0), L"icon.ico", IMAGE_ICON, 0, 0, 0);
        HICON hIcon = (HICON)LoadImage(GetModuleHandle(0), MAKEINTRESOURCE(IDI_SMALL), IMAGE_ICON, 0, 0, 0);
        //        assert(hIcon);

        if (hIcon)
        {
            HBITMAP hBITMAPcopy;
            ICONINFOEX IconInfo;
            BITMAP BM_32_bit_color;

            memset((void*)&IconInfo, 0, sizeof(ICONINFOEX));
            IconInfo.cbSize = sizeof(ICONINFOEX);
            GetIconInfoEx(hIcon, &IconInfo);

            hBITMAPcopy = (HBITMAP)CopyImage(IconInfo.hbmColor, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
            GetObject(hBITMAPcopy, sizeof(BITMAP), &BM_32_bit_color);
            //Now: BM_32_bit_color.bmBits pointing to BGRA data.(.bmWidth * .bmHeight * (.bmBitsPixel/8))

            //    BITMAP BM_1_bit_mask;
            //HBITMAP IconInfo.hbmMask is 1bit per pxl
            // From HBITMAP to BITMAP for mask
        //    hBITMAPcopy = (HBITMAP)CopyImage(IconInfo.hbmMask, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
        //    GetObject(hBITMAPcopy, sizeof(BITMAP), &BM_1_bit_mask);
            //Now: BM_1_bit_mask.bmBits pointing to mask data (.bmWidth * .bmHeight Bits!)

            assert(BM_32_bit_color.bmBitsPixel == 32);

            GLFWimage images[1];
            images[0].width = BM_32_bit_color.bmWidth;
            images[0].height = BM_32_bit_color.bmHeight;

            std::vector<int> mem;
            mem.resize(images[0].width * images[0].height);
            int* src = (int*)BM_32_bit_color.bmBits;
            for (int y = images[0].height - 1; y >= 0; --y) {
                for (int x = 0; x < images[0].width; ++x) {
                    // seems glfwSetWindowIcon() doesn't support alpha
                    mem[y * images[0].width + x] = RGBSwizzle(*src++);
                }
            }
            images[0].pixels = (unsigned char*)mem.data();
            glfwSetWindowIcon(glewGlfwHandler.getWindow(), 1, images);
        }
    }
#endif

    Renderer renderer(glewGlfwHandler.getWindow(), camera);
    renderer.initialize();

    InputParser input(argc, argv);
    RenderContext* ctx = renderer.getRenderContext();

    ctx->debugUv = input.cmdOptionExists("--debug-uv");
    ctx->debugColor = input.cmdOptionExists("--debug-color");
    ctx->debugTextureStats = input.cmdOptionExists("--debug-texture-stats");
    ctx->debugColorStats = input.cmdOptionExists("--debug-color-stats");
    ctx->debugUvCompare = input.cmdOptionExists("--debug-uv-compare");
    ctx->autoUvWrap = input.cmdOptionExists("--auto-uv-wrap");

    const char* debugUvEnv = std::getenv("MESH2SPLAT_DEBUG_UV");
    if (debugUvEnv && debugUvEnv[0] != '\0' && debugUvEnv[0] != '0') {
        ctx->debugUv = true;
    }
    const char* debugColorEnv = std::getenv("MESH2SPLAT_DEBUG_COLOR");
    if (debugColorEnv && debugColorEnv[0] != '\0' && debugColorEnv[0] != '0') {
        ctx->debugColor = true;
    }
    const char* debugTexEnv = std::getenv("MESH2SPLAT_DEBUG_TEXTURE_STATS");
    if (debugTexEnv && debugTexEnv[0] != '\0' && debugTexEnv[0] != '0') {
        ctx->debugTextureStats = true;
    }
    const char* debugColorStatsEnv = std::getenv("MESH2SPLAT_DEBUG_COLOR_STATS");
    if (debugColorStatsEnv && debugColorStatsEnv[0] != '\0' && debugColorStatsEnv[0] != '0') {
        ctx->debugColorStats = true;
    }

    if (ctx->debugUv || ctx->debugColor || ctx->debugTextureStats || ctx->debugColorStats) {
        const std::string primsOpt = input.getCmdOption("--debug-max-prims");
        const std::string vertsOpt = input.getCmdOption("--debug-max-verts");
        const std::string matsOpt = input.getCmdOption("--debug-max-mats");
        const std::string splatsOpt = input.getCmdOption("--debug-max-splats");
        const std::string summaryOpt = input.getCmdOption("--debug-print-summary");
        const std::string texDownsampleOpt = input.getCmdOption("--texture-stats-downsample");

        auto parsePositive = [](const std::string& value, int fallback) -> int {
            if (value.empty()) return fallback;
            try {
                int parsed = std::stoi(value);
                return parsed > 0 ? parsed : fallback;
            } catch (...) {
                return fallback;
            }
        };

        auto parseBoolDefault = [](const std::string& value, bool fallback) -> bool {
            if (value.empty()) return fallback;
            if (value == "0" || value == "false") return false;
            return true;
        };

        ctx->debugMaxPrimitives = parsePositive(primsOpt, ctx->debugMaxPrimitives);
        ctx->debugMaxVertices = parsePositive(vertsOpt, ctx->debugMaxVertices);
        ctx->debugMaxMaterials = parsePositive(matsOpt, ctx->debugMaxMaterials);
        ctx->debugMaxSplats = parsePositive(splatsOpt, ctx->debugMaxSplats);
        ctx->debugPrintSummary = parseBoolDefault(summaryOpt, ctx->debugPrintSummary);
        ctx->textureStatsDownsample = parsePositive(texDownsampleOpt, ctx->textureStatsDownsample);

        const char* primsEnv = std::getenv("MESH2SPLAT_DEBUG_MAX_PRIMS");
        const char* vertsEnv = std::getenv("MESH2SPLAT_DEBUG_MAX_VERTS");
        const char* matsEnv = std::getenv("MESH2SPLAT_DEBUG_MAX_MATS");
        const char* splatsEnv = std::getenv("MESH2SPLAT_DEBUG_MAX_SPLATS");
        if (primsEnv && primsEnv[0] != '\0') {
            int v = std::atoi(primsEnv);
            if (v > 0) ctx->debugMaxPrimitives = v;
        }
        if (vertsEnv && vertsEnv[0] != '\0') {
            int v = std::atoi(vertsEnv);
            if (v > 0) ctx->debugMaxVertices = v;
        }
        if (matsEnv && matsEnv[0] != '\0') {
            int v = std::atoi(matsEnv);
            if (v > 0) ctx->debugMaxMaterials = v;
        }
        if (splatsEnv && splatsEnv[0] != '\0') {
            int v = std::atoi(splatsEnv);
            if (v > 0) ctx->debugMaxSplats = v;
        }

        const char* texDownsampleEnv = std::getenv("MESH2SPLAT_TEXTURE_STATS_DOWNSAMPLE");
        if (texDownsampleEnv && texDownsampleEnv[0] != '\0') {
            int v = std::atoi(texDownsampleEnv);
            if (v > 0) ctx->textureStatsDownsample = v;
        }
    }

    const std::string wrapOpt = input.getCmdOption("--force-uv-wrap");
    if (!wrapOpt.empty()) {
        if (wrapOpt == "repeat") ctx->forceUvWrapMode = 1;
        else if (wrapOpt == "clamp") ctx->forceUvWrapMode = 2;
        else if (wrapOpt == "mirror") ctx->forceUvWrapMode = 3;
        else ctx->forceUvWrapMode = 0;
    }

    const std::string srgbOpt = input.getCmdOption("--force-srgb");
    if (!srgbOpt.empty()) {
        if (srgbOpt == "on") ctx->forceSrgbMode = 1;
        else if (srgbOpt == "off") ctx->forceSrgbMode = 2;
        else ctx->forceSrgbMode = 0;
    }

    const std::string dcModeOpt = input.getCmdOption("--dc-mode");
    if (!dcModeOpt.empty()) {
        ctx->dcModeSpecified = true;
        if (dcModeOpt == "direct_linear") ctx->dcMode = 1;
        else if (dcModeOpt == "direct_srgb") ctx->dcMode = 2;
        else ctx->dcMode = 0;
    }

    const std::string opacityModeOpt = input.getCmdOption("--opacity-mode");
    if (!opacityModeOpt.empty()) {
        ctx->opacityModeSpecified = true;
        if (opacityModeOpt == "raw") ctx->opacityMode = 1;
        else if (opacityModeOpt == "logit") ctx->opacityMode = 2;
        else ctx->opacityMode = 0;
    }

    GuiRendererConcreteMediator guiRendererMediator(renderer, ImGuiUI);

    float deltaTime = 0.0f; 
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(glewGlfwHandler.getWindow())) {
        
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();
        
        ioHandler.processInput(deltaTime);

        renderer.clearingPrePass(ImGuiUI.getSceneBackgroundColor());

        ImGuiUI.preframe();
        ImGuiUI.renderUI();

        guiRendererMediator.update();

        renderer.renderFrame();

		ImGuiUI.displayGaussianCounts(renderer.getTotalGaussianCount(), renderer.getVisibleGaussianCount());

        ImGuiUI.postframe();

        glfwSwapBuffers(glewGlfwHandler.getWindow());
    }

    glfwTerminate();

    return 0;
}

