///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "utils/normalizedUvUnwrapping.hpp"
#include "renderer/renderer.hpp"
#include "glewGlfwHandlers/glewGlfwHandler.hpp"
#include "renderer/guiRendererConcreteMediator.hpp"

#define IDI_SMALL       101

unsigned int RGBSwizzle(unsigned int c) {
    return (c >> 16) | (c & 0xff00) | ((c & 0xff) << 16);
}

int main(int argc, char** argv) {
    GlewGlfwHandler glewGlfwHandler(glm::ivec2(1080, 720), "Mesh2Splat");
    
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

