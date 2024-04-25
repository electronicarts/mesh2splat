#include "utils.hpp"

bool file_exists(std::string fn) { return std::experimental::filesystem::exists(fn); }

// Function to check if a point is inside a triangle
bool pointInTriangle(const glm::vec2& pt, const glm::vec2& v1, const glm::vec2& v2, const glm::vec2& v3) {
    auto sign = [](const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3) {
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
        };

    float d1 = sign(pt, v1, v2);
    float d2 = sign(pt, v2, v3);
    float d3 = sign(pt, v3, v1);

    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(hasNeg && hasPos);
}

// https://ceng2.ktu.edu.tr/~cakir/files/grafikler/Texture_Mapping.pdf

// Function to calculate barycentric coordinates
bool computeBarycentricCoords(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, float& u, float& v, float& w) {
    glm::vec2 v0 = b - a, v1 = c - a, v2 = p - a;
    float d00 = glm::dot(v0, v0);
    float d01 = glm::dot(v0, v1);
    float d11 = glm::dot(v1, v1);
    float d20 = glm::dot(v2, v0);
    float d21 = glm::dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;
    if (denom == 0) return false; // Collinear or singular triangle
    float invDenom = 1.0f / denom;
    v = (d11 * d20 - d01 * d21) * invDenom;
    w = (d00 * d21 - d01 * d20) * invDenom;
    u = 1.0f - v - w;
    return true; // Successful computation
}

glm::vec3 getColor(glm::vec3 color)
{
    glm::vec3 col = color - glm::vec3(0.5f) / SH_COEFF0;
    return glm::vec3(col.x, col.y, col.z);
}

glm::vec3 floatToVec3(const float val)
{
    return glm::vec3(val, val, val);
}

int inline sign(float x) {
    return (x > 0) ? 1 : ((x < 0) ? -1 : 0);
}

glm::vec2 pixelToUV(const glm::ivec2& pixel, int textureWidth, int textureHeight) {
    // Convert pixel coordinates to normalized UV coordinates by dividing
    // by the texture dimensions. Cast to float to ensure floating-point division.
    float u = (static_cast<float>(pixel.x) / static_cast<float>(textureWidth));
    float v = (static_cast<float>(pixel.y) / static_cast<float>(textureHeight));

    return glm::vec2(u, v);
}

glm::ivec2 uvToPixel(const glm::vec2& uv, int textureWidth, int textureHeight) {
    // Convert back to pixel coordinates
    int x = static_cast<int>(uv.x * textureWidth);
    int y = static_cast<int>(uv.y * textureHeight);

    // Clamp the coordinates to ensure they're within the texture bounds
    // This accounts for potential rounding errors that might place the pixel outside the texture
    x = std::max(0, std::min(x, textureWidth));
    y = std::max(0, std::min(y, textureHeight));

    return glm::ivec2(x, y);
}

std::pair<glm::vec2, glm::vec2> computeUVBoundingBox(const std::vector<glm::vec2>& triangleUVs) {
    if (triangleUVs.empty()) {
        return { glm::vec2(0, 0), glm::vec2(0, 0) };
    }

    float minU = triangleUVs[0].x, maxU = triangleUVs[0].x;
    float minV = triangleUVs[0].y, maxV = triangleUVs[0].y;

    for (const auto& uv : triangleUVs) {
        minU = std::min(minU, uv.x);
        maxU = std::max(maxU, uv.x);

        minV = std::min(minV, uv.y);
        maxV = std::max(maxV, uv.y);
    }

    // Clamp values to [0, 1] 
    minU = std::max(minU, 0.0f);
    maxU = std::min(maxU, 1.0f);
    minV = std::max(minV, 0.0f);
    maxV = std::min(maxV, 1.0f);

    return { glm::vec2(minU, minV), glm::vec2(maxU, maxV) };
}

//https://www.nayuki.io/res/srgb-transform-library/srgb-transform.c
//Assumes 0,...,1 range 
float linear_to_srgb_float(float x) {
    if (x <= 0.0f)
        return 0.0f;
    else if (x >= 1.0f)
        return 1.0f;
    else if (x < 0.0031308f)
        return x * 12.92f;
    else
        return powf(x, 1.0f / 2.4f) * 1.055f - 0.055f;
}


//https://www.nayuki.io/res/srgb-transform-library/srgb-transform.c
//Assumes 0,...,1 range 
float srgb_to_linear_float(float x) {
    if (x <= 0.0f)
        return 0.0f;
    else if (x >= 1.0f)
        return 1.0f;
    else if (x < 0.04045f)
        return x / 12.92f;
    else
        return powf((x + 0.055f) / 1.055f, 2.4f);
}

//https://stackoverflow.com/a/48235560
glm::vec4 rgbaAtPos(const int width, int X, int Y, unsigned char* rgb_image)
{
    unsigned bytePerPixel = 3;
    unsigned char* pixelOffset = rgb_image + (X + width * Y) * bytePerPixel;

    float r = ((static_cast<float>(pixelOffset[0]) / 255.0f));
    float g = ((static_cast<float>(pixelOffset[1]) / 255.0f));
    float b = ((static_cast<float>(pixelOffset[2]) / 255.0f));
    //float a = bytePerPixel >= 4 ? (float)pixelOffset[3] : 255.0f;
    float a = 1.0f;

    return { r, g, b, a };
}

float displacementAtPos(const int width, int X, int Y, unsigned char* displacement_image)
{
    unsigned bytePerPixel = 1;
    unsigned char* pixelOffset = displacement_image + (X + width * Y) * bytePerPixel;
    //from my understanding a negative value is 0.0, if at 148 it means no displacement and at 255, full displacement
    return ((static_cast<float>(pixelOffset[0] - 128) / 255.0f));
}

float computeTriangleAreaUV(const glm::vec2& uv1, const glm::vec2& uv2, const glm::vec2& uv3) {
    float area = 0.5f * std::abs(uv1.x * (uv2.y - uv3.y) +
        uv2.x * (uv3.y - uv1.y) +
        uv3.x * (uv1.y - uv2.y));
    return area;
}