#include "utils.hpp"
#include "../thirdParty/xatlas/xatlas.h"
#define DRAW_UV_MAPPING 0

void generateNormalizedUvCoordinatesPerFace(int& uvSpaceWidth, int& uvSpaceHeight, std::vector<Mesh>& meshes);

void generateNormalizedUvCoordinatesPerFace(int& uvSpaceWidth, int& uvSpaceHeight, Mesh& meshes);