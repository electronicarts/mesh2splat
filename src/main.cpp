#include <string>
#include <vector>
#include <tuple>
#include <math.h>
#include <glm.hpp>
#include <set>
#include <stdint.h>
#include <chrono>

#include "glewGlfwHandlers/glewGlfwHandler.hpp"
#include "parsers.hpp"
#include "utils/gaussianShapesUtilities.hpp"
#include "gaussianComputations.hpp"
#include "utils/shaderUtils.hpp"
#include "utils/normalizedUvUnwrapping.hpp"
#include "utils/argparser.hpp"
#include "renderer/renderer.hpp"
#include "renderer/mesh2SplatHandler.hpp"
#include "renderer/IoHandler.hpp"


int main(int argc, char** argv) {
    GlewGlfwHandler glewGlfwHandler(glm::ivec2(1080, 720), "Mesh2Splat");
    if(glewGlfwHandler.init() == -1) return -1;

    Renderer simpleRenderer;
    ImGuiUI ImGuiUI(3, 0, 0.1f, 0.5f); //TODO: give a meaning to these params
    ImGuiUI.initialize(glewGlfwHandler.getWindow());
    simpleRenderer.initializeOpenGLState();
    simpleRenderer.renderLoop(glewGlfwHandler.getWindow(), ImGuiUI);

    return 0;
}

