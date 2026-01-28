///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

// handles loading GLB
// traversing scene nodes
// applying transforms
// building mesh data/bounds
// preparing GPU buffers/textures

#include "SceneManager.hpp"
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <cfloat>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "normalizedUvUnwrapping.hpp"
#include "parsers/parsers.hpp"


#ifdef _WIN32
  #include <windows.h>
  static void debugPrintWin(const char* s) { OutputDebugStringA(s); }
#else
  static void debugPrintWin(const char* s) { (void)s; }
#endif

namespace
{
    struct NodeWorld
    {
        int nodeIndex = -1;
        glm::mat4 world = glm::mat4(1.0f);
    };

    glm::mat4 getNodeLocalMatrix(const tinygltf::Node& node)
    {
        // glTF node.matrix is stored as 16 doubles (column-major)
        // GLM is column-major too, but we MUST cast double -> float elementwise.
        if (node.matrix.size() == 16)
        {
            glm::mat4 m(1.0f);

            // node.matrix is in column-major order:
            // [ m00 m10 m20 m30  m01 m11 m21 m31  m02 m12 m22 m32  m03 m13 m23 m33 ]
            // GLM indexing is m[col][row]
            for (int i = 0; i < 16; ++i)
            {
                const int col = i / 4;
                const int row = i % 4;
                m[col][row] = static_cast<float>(node.matrix[i]);
            }
            return m;
        }

        glm::vec3 translation(0.0f);
        if (node.translation.size() == 3)
        {
            translation = glm::vec3(
                static_cast<float>(node.translation[0]),
                static_cast<float>(node.translation[1]),
                static_cast<float>(node.translation[2]));
        }

        glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
        if (node.rotation.size() == 4)
        {
            // tinygltf stores [x,y,z,w]
            rotation = glm::quat(
                static_cast<float>(node.rotation[3]),
                static_cast<float>(node.rotation[0]),
                static_cast<float>(node.rotation[1]),
                static_cast<float>(node.rotation[2]));
        }

        glm::vec3 scale(1.0f);
        if (node.scale.size() == 3)
        {
            scale = glm::vec3(
                static_cast<float>(node.scale[0]),
                static_cast<float>(node.scale[1]),
                static_cast<float>(node.scale[2]));
        }

        return glm::translate(glm::mat4(1.0f), translation)
            * glm::mat4_cast(rotation)
            * glm::scale(glm::mat4(1.0f), scale);
    }

    void collectNodes(const tinygltf::Model& model, int nodeIndex, const glm::mat4& parentWorld, std::vector<NodeWorld>& out)
    {
        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size()))
        {
            return;
        }

        const tinygltf::Node& node = model.nodes[nodeIndex];
        glm::mat4 world = parentWorld * getNodeLocalMatrix(node);
        out.push_back({ nodeIndex, world });

        for (int child : node.children)
        {
            collectNodes(model, child, world, out);
        }
    }

    float invSigmoidSafe(float a)
    {
        const float eps = 1e-6f;
        if (!std::isfinite(a)) a = 0.5f;
        a = std::clamp(a, eps, 1.0f - eps);
        return std::log(a / (1.0f - a));
    }

    uint8_t toByte(float v)
    {
        float clamped = glm::clamp(v, 0.0f, 1.0f);
        float rounded = std::round(clamped * 255.0f);
        return static_cast<uint8_t>(rounded);
    }

    float wrapCoord(float v, int mode)
    {
        switch (mode)
        {
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                return glm::clamp(v, 0.0f, 1.0f);
            case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
            {
                float t = std::fmod(v, 2.0f);
                if (t < 0.0f) t += 2.0f;
                return (t <= 1.0f) ? t : (2.0f - t);
            }
            case TINYGLTF_TEXTURE_WRAP_REPEAT:
            default:
                return v - std::floor(v);
        }
    }

    glm::vec2 applyWrap(const glm::vec2& uv, int wrapS, int wrapT)
    {
        return glm::vec2(wrapCoord(uv.x, wrapS), wrapCoord(uv.y, wrapT));
    }

    const char* wrapToString(int mode)
    {
        switch (mode)
        {
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE: return "CLAMP_TO_EDGE";
            case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT: return "MIRRORED_REPEAT";
            case TINYGLTF_TEXTURE_WRAP_REPEAT:
            default:
                return "REPEAT";
        }
    }

    const char* filterToString(int mode)
    {
        switch (mode)
        {
            case TINYGLTF_TEXTURE_FILTER_NEAREST: return "NEAREST";
            case TINYGLTF_TEXTURE_FILTER_LINEAR: return "LINEAR";
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST: return "NEAREST_MIPMAP_NEAREST";
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST: return "LINEAR_MIPMAP_NEAREST";
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR: return "NEAREST_MIPMAP_LINEAR";
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR: return "LINEAR_MIPMAP_LINEAR";
            default: return "DEFAULT";
        }
    }

    const char* accessorTypeToString(int type)
    {
        switch (type)
        {
            case TINYGLTF_TYPE_VEC2: return "VEC2";
            case TINYGLTF_TYPE_VEC3: return "VEC3";
            case TINYGLTF_TYPE_VEC4: return "VEC4";
            case TINYGLTF_TYPE_SCALAR: return "SCALAR";
            case TINYGLTF_TYPE_MAT2: return "MAT2";
            case TINYGLTF_TYPE_MAT3: return "MAT3";
            case TINYGLTF_TYPE_MAT4: return "MAT4";
            default: return "UNKNOWN";
        }
    }

    glm::vec3 srgbToLinear(const glm::vec3& c)
    {
        return utils::srgb_to_linear_float(c);
    }

    struct TextureStatsResult
    {
        glm::vec3 meanSrgb = glm::vec3(0.0f);
        glm::vec3 stdSrgb = glm::vec3(0.0f);
        float stdYSrgb = 0.0f;
        glm::vec3 meanLin = glm::vec3(0.0f);
        glm::vec3 stdLin = glm::vec3(0.0f);
        float stdYLin = 0.0f;
        float gradYLin = 0.0f;
        bool ok = false;
        std::string error;
    };

    TextureStatsResult computeTextureStats(
        const std::vector<unsigned char>& data,
        int width,
        int height,
        int channels,
        int downsample)
    {
        TextureStatsResult out;
        if (width <= 0 || height <= 0 || channels <= 0) {
            out.error = "invalid_dimensions";
            return out;
        }
        if (data.empty()) {
            out.error = "no_pixels";
            return out;
        }
        const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
        const size_t needed = pixelCount * static_cast<size_t>(channels);
        if (data.size() < needed) {
            out.error = "insufficient_pixel_data";
            return out;
        }

        double sumSrgb[3] = {0.0, 0.0, 0.0};
        double sumSrgbSq[3] = {0.0, 0.0, 0.0};
        double sumLin[3] = {0.0, 0.0, 0.0};
        double sumLinSq[3] = {0.0, 0.0, 0.0};
        double sumYSrgb = 0.0;
        double sumYSrgbSq = 0.0;
        double sumYLin = 0.0;
        double sumYLinSq = 0.0;

        for (size_t i = 0; i < pixelCount; ++i)
        {
            size_t idx = i * static_cast<size_t>(channels);
            float r = data[idx + 0] / 255.0f;
            float g = (channels > 1) ? data[idx + 1] / 255.0f : r;
            float b = (channels > 2) ? data[idx + 2] / 255.0f : r;

            glm::vec3 srgb(r, g, b);
            glm::vec3 lin = srgbToLinear(srgb);
            float ySrgb = 0.2126f * srgb.x + 0.7152f * srgb.y + 0.0722f * srgb.z;
            float yLin = 0.2126f * lin.x + 0.7152f * lin.y + 0.0722f * lin.z;

            sumSrgb[0] += srgb.x; sumSrgb[1] += srgb.y; sumSrgb[2] += srgb.z;
            sumSrgbSq[0] += srgb.x * srgb.x; sumSrgbSq[1] += srgb.y * srgb.y; sumSrgbSq[2] += srgb.z * srgb.z;
            sumLin[0] += lin.x; sumLin[1] += lin.y; sumLin[2] += lin.z;
            sumLinSq[0] += lin.x * lin.x; sumLinSq[1] += lin.y * lin.y; sumLinSq[2] += lin.z * lin.z;
            sumYSrgb += ySrgb;
            sumYSrgbSq += ySrgb * ySrgb;
            sumYLin += yLin;
            sumYLinSq += yLin * yLin;
        }

        double count = static_cast<double>(pixelCount);
        glm::vec3 meanSrgb(
            static_cast<float>(sumSrgb[0] / count),
            static_cast<float>(sumSrgb[1] / count),
            static_cast<float>(sumSrgb[2] / count));
        glm::vec3 meanLin(
            static_cast<float>(sumLin[0] / count),
            static_cast<float>(sumLin[1] / count),
            static_cast<float>(sumLin[2] / count));

        glm::vec3 varSrgb(
            static_cast<float>(sumSrgbSq[0] / count - meanSrgb.x * meanSrgb.x),
            static_cast<float>(sumSrgbSq[1] / count - meanSrgb.y * meanSrgb.y),
            static_cast<float>(sumSrgbSq[2] / count - meanSrgb.z * meanSrgb.z));
        glm::vec3 varLin(
            static_cast<float>(sumLinSq[0] / count - meanLin.x * meanLin.x),
            static_cast<float>(sumLinSq[1] / count - meanLin.y * meanLin.y),
            static_cast<float>(sumLinSq[2] / count - meanLin.z * meanLin.z));

        float meanYSrgb = static_cast<float>(sumYSrgb / count);
        float meanYLin = static_cast<float>(sumYLin / count);
        float varYSrgb = static_cast<float>(sumYSrgbSq / count - meanYSrgb * meanYSrgb);
        float varYLin = static_cast<float>(sumYLinSq / count - meanYLin * meanYLin);

        out.meanSrgb = meanSrgb;
        out.stdSrgb = glm::sqrt(glm::max(varSrgb, glm::vec3(0.0f)));
        out.stdYSrgb = std::sqrt(std::max(varYSrgb, 0.0f));
        out.meanLin = meanLin;
        out.stdLin = glm::sqrt(glm::max(varLin, glm::vec3(0.0f)));
        out.stdYLin = std::sqrt(std::max(varYLin, 0.0f));

        int ds = std::max(1, downsample);
        int dsW = std::min(width, ds);
        int dsH = std::min(height, ds);
        if (dsW > 1 && dsH > 1)
        {
            double gradSum = 0.0;
            size_t gradCount = 0;
            for (int y = 0; y < dsH - 1; ++y)
            {
                int srcY = static_cast<int>((static_cast<float>(y) / static_cast<float>(dsH)) * height);
                int srcYNext = static_cast<int>((static_cast<float>(y + 1) / static_cast<float>(dsH)) * height);
                srcY = std::min(srcY, height - 1);
                srcYNext = std::min(srcYNext, height - 1);
                for (int x = 0; x < dsW - 1; ++x)
                {
                    int srcX = static_cast<int>((static_cast<float>(x) / static_cast<float>(dsW)) * width);
                    int srcXNext = static_cast<int>((static_cast<float>(x + 1) / static_cast<float>(dsW)) * width);
                    srcX = std::min(srcX, width - 1);
                    srcXNext = std::min(srcXNext, width - 1);

                    auto sampleY = [&](int px, int py) -> float {
                        size_t idx = (static_cast<size_t>(py) * static_cast<size_t>(width) + static_cast<size_t>(px)) * static_cast<size_t>(channels);
                        float r = data[idx + 0] / 255.0f;
                        float g = (channels > 1) ? data[idx + 1] / 255.0f : r;
                        float b = (channels > 2) ? data[idx + 2] / 255.0f : r;
                        glm::vec3 lin = srgbToLinear(glm::vec3(r, g, b));
                        return 0.2126f * lin.x + 0.7152f * lin.y + 0.0722f * lin.z;
                    };

                    float y00 = sampleY(srcX, srcY);
                    float y10 = sampleY(srcXNext, srcY);
                    float y01 = sampleY(srcX, srcYNext);
                    gradSum += std::abs(y10 - y00) + std::abs(y01 - y00);
                    ++gradCount;
                }
            }
            out.gradYLin = static_cast<float>(gradSum / static_cast<double>(std::max<size_t>(1, gradCount)));
        }

        out.ok = true;
        return out;
    }

    glm::vec4 sampleTextureBilinear(
        const utils::TextureDataGl& tex,
        const glm::vec2& uv,
        int wrapS,
        int wrapT,
        glm::vec4* outRaw,
        bool srgbDecode,
        bool* uvOutOfRange)
    {
        if (uvOutOfRange) {
            *uvOutOfRange = (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f);
        }

        if (tex.width <= 0 || tex.height <= 0 || tex.textureData.empty()) {
            if (outRaw) *outRaw = glm::vec4(1.0f);
            return glm::vec4(1.0f);
        }

        glm::vec2 wrapped = applyWrap(uv, wrapS, wrapT);
        float u = wrapped.x;
        float v = wrapped.y;

        float x = u * static_cast<float>(tex.width) - 0.5f;
        float y = v * static_cast<float>(tex.height) - 0.5f;
        int x0 = static_cast<int>(std::floor(x));
        int y0 = static_cast<int>(std::floor(y));
        int x1 = x0 + 1;
        int y1 = y0 + 1;

        float tx = x - static_cast<float>(x0);
        float ty = y - static_cast<float>(y0);

        auto wrapIndex = [](int value, int max) -> int {
            int m = value % max;
            return m < 0 ? m + max : m;
        };

        auto fetch = [&](int px, int py) -> glm::vec4 {
            int ix = wrapIndex(px, tex.width);
            int iy = wrapIndex(py, tex.height);
            size_t idx = (static_cast<size_t>(iy) * static_cast<size_t>(tex.width) + static_cast<size_t>(ix)) * tex.channels;
            if (idx >= tex.textureData.size()) {
                return glm::vec4(1.0f);
            }
            float r = tex.textureData[idx + 0] / 255.0f;
            float g = (tex.channels > 1 && idx + 1 < tex.textureData.size()) ? tex.textureData[idx + 1] / 255.0f : r;
            float b = (tex.channels > 2 && idx + 2 < tex.textureData.size()) ? tex.textureData[idx + 2] / 255.0f : r;
            float a = 1.0f;
            if (tex.channels >= 4 && idx + 3 < tex.textureData.size()) {
                a = tex.textureData[idx + 3] / 255.0f;
            }
            return glm::vec4(r, g, b, a);
        };

        glm::vec4 c00 = fetch(x0, y0);
        glm::vec4 c10 = fetch(x1, y0);
        glm::vec4 c01 = fetch(x0, y1);
        glm::vec4 c11 = fetch(x1, y1);

        glm::vec4 c0 = c00 * (1.0f - tx) + c10 * tx;
        glm::vec4 c1 = c01 * (1.0f - tx) + c11 * tx;
        glm::vec4 c = c0 * (1.0f - ty) + c1 * ty;
        if (outRaw) {
            *outRaw = c;
        }

        if (srgbDecode) {
            glm::vec3 decoded = utils::srgb_to_linear_float(glm::vec3(c));
            c = glm::vec4(decoded, c.w);
        }

        return c;
    }

    std::string formatVec4(const glm::vec4& v)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3)
            << "(" << v.x << "," << v.y << "," << v.z << "," << v.w << ")";
        return oss.str();
    }

    std::string formatVec3(const glm::vec3& v)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3)
            << "(" << v.x << "," << v.y << "," << v.z << ")";
        return oss.str();
    }

    std::string formatVec2(const glm::vec2& v)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3)
            << "(" << v.x << "," << v.y << ")";
        return oss.str();
    }
}

// --------------------------- SAFE ACCESSOR HELPERS ---------------------------

int SceneManager::componentByteSize(int componentType)
{
    switch (componentType)
    {
    case TINYGLTF_COMPONENT_TYPE_BYTE:           return 1;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  return 1;
    case TINYGLTF_COMPONENT_TYPE_SHORT:          return 2;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return 2;
    case TINYGLTF_COMPONENT_TYPE_INT:            return 4;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   return 4;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:          return 4;
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:         return 8;
    default: return 0;
    }
}

float SceneManager::readComponentAsFloat(const unsigned char* p, int componentType, bool normalized)
{
    // Note: glTF normalization rules: normalized integer maps to [0,1] or [-1,1]
    switch (componentType)
    {
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
    {
        float v;
        std::memcpy(&v, p, sizeof(float));
        return v;
    }
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
    {
        double v;
        std::memcpy(&v, p, sizeof(double));
        return static_cast<float>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
    {
        uint8_t v = *reinterpret_cast<const uint8_t*>(p);
        if (!normalized) return static_cast<float>(v);
        return static_cast<float>(v) / 255.0f;
    }
    case TINYGLTF_COMPONENT_TYPE_BYTE:
    {
        int8_t v = *reinterpret_cast<const int8_t*>(p);
        if (!normalized) return static_cast<float>(v);
        return std::max(-1.0f, static_cast<float>(v) / 127.0f);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
    {
        uint16_t v;
        std::memcpy(&v, p, sizeof(uint16_t));
        if (!normalized) return static_cast<float>(v);
        return static_cast<float>(v) / 65535.0f;
    }
    case TINYGLTF_COMPONENT_TYPE_SHORT:
    {
        int16_t v;
        std::memcpy(&v, p, sizeof(int16_t));
        if (!normalized) return static_cast<float>(v);
        return std::max(-1.0f, static_cast<float>(v) / 32767.0f);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
    {
        uint32_t v;
        std::memcpy(&v, p, sizeof(uint32_t));
        if (!normalized) return static_cast<float>(v);
        // normalized uint32 is rare; map to [0,1]
        return static_cast<float>(v) / 4294967295.0f;
    }
    case TINYGLTF_COMPONENT_TYPE_INT:
    {
        int32_t v;
        std::memcpy(&v, p, sizeof(int32_t));
        if (!normalized) return static_cast<float>(v);
        // normalized int32 is rare; map to [-1,1]
        return std::max(-1.0f, static_cast<float>(v) / 2147483647.0f);
    }
    default:
        return 0.0f;
    }
}

bool SceneManager::checkAccessorIsVecN(const tinygltf::Accessor& acc, int expectedType, const char* debugName)
{
    if (acc.type != expectedType)
    {
        std::cerr << "ERROR: Accessor for " << debugName << " has unexpected type (acc.type=" << acc.type
                  << ", expected=" << expectedType << ")." << std::endl;
        return false;
    }
    const int cbs = componentByteSize(acc.componentType);
    if (cbs == 0)
    {
        std::cerr << "ERROR: Accessor for " << debugName << " has unsupported componentType=" << acc.componentType << std::endl;
        return false;
    }
    return true;
}

bool SceneManager::readVec3Attribute(const tinygltf::Model& model, int accessorIndex, std::vector<glm::vec3>& out)
{
    if (accessorIndex < 0 || accessorIndex >= (int)model.accessors.size()) return false;
    const auto& acc = model.accessors[accessorIndex];
    if (!checkAccessorIsVecN(acc, TINYGLTF_TYPE_VEC3, "VEC3")) return false;

    const auto& bv = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[bv.buffer];

    const int compSize = componentByteSize(acc.componentType);
    const size_t elemComp = 3;
    const size_t packedStride = elemComp * (size_t)compSize;
    const size_t stride = (bv.byteStride && bv.byteStride >= packedStride) ? (size_t)bv.byteStride : packedStride;

    const size_t baseOffset = (size_t)bv.byteOffset + (size_t)acc.byteOffset;
    const size_t needed = baseOffset + (acc.count - 1) * stride + packedStride;
    if (needed > buf.data.size())
    {
        std::cerr << "ERROR: VEC3 accessor out of range (needed=" << needed << ", buf=" << buf.data.size() << ")." << std::endl;
        return false;
    }

    out.resize(acc.count);
    const unsigned char* base = buf.data.data() + baseOffset;

    for (size_t i = 0; i < acc.count; ++i)
    {
        const unsigned char* p = base + i * stride;
        float x = readComponentAsFloat(p + 0 * compSize, acc.componentType, acc.normalized);
        float y = readComponentAsFloat(p + 1 * compSize, acc.componentType, acc.normalized);
        float z = readComponentAsFloat(p + 2 * compSize, acc.componentType, acc.normalized);
        out[i] = glm::vec3(x, y, z);
    }
    return true;
}

bool SceneManager::readVec2Attribute(const tinygltf::Model& model, int accessorIndex, std::vector<glm::vec2>& out)
{
    if (accessorIndex < 0 || accessorIndex >= (int)model.accessors.size()) return false;
    const auto& acc = model.accessors[accessorIndex];
    if (!checkAccessorIsVecN(acc, TINYGLTF_TYPE_VEC2, "VEC2")) return false;

    const auto& bv = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[bv.buffer];

    const int compSize = componentByteSize(acc.componentType);
    const size_t elemComp = 2;
    const size_t packedStride = elemComp * (size_t)compSize;
    const size_t stride = (bv.byteStride && bv.byteStride >= packedStride) ? (size_t)bv.byteStride : packedStride;

    const size_t baseOffset = (size_t)bv.byteOffset + (size_t)acc.byteOffset;
    const size_t needed = baseOffset + (acc.count - 1) * stride + packedStride;
    if (needed > buf.data.size())
    {
        std::cerr << "ERROR: VEC2 accessor out of range (needed=" << needed << ", buf=" << buf.data.size() << ")." << std::endl;
        return false;
    }

    out.resize(acc.count);
    const unsigned char* base = buf.data.data() + baseOffset;

    for (size_t i = 0; i < acc.count; ++i)
    {
        const unsigned char* p = base + i * stride;
        float x = readComponentAsFloat(p + 0 * compSize, acc.componentType, acc.normalized);
        float y = readComponentAsFloat(p + 1 * compSize, acc.componentType, acc.normalized);
        out[i] = glm::vec2(x, y);
    }
    return true;
}

bool SceneManager::readVec4Attribute(const tinygltf::Model& model, int accessorIndex, std::vector<glm::vec4>& out)
{
    if (accessorIndex < 0 || accessorIndex >= (int)model.accessors.size()) return false;
    const auto& acc = model.accessors[accessorIndex];
    if (!checkAccessorIsVecN(acc, TINYGLTF_TYPE_VEC4, "VEC4")) return false;

    const auto& bv = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[bv.buffer];

    const int compSize = componentByteSize(acc.componentType);
    const size_t elemComp = 4;
    const size_t packedStride = elemComp * (size_t)compSize;
    const size_t stride = (bv.byteStride && bv.byteStride >= packedStride) ? (size_t)bv.byteStride : packedStride;

    const size_t baseOffset = (size_t)bv.byteOffset + (size_t)acc.byteOffset;
    const size_t needed = baseOffset + (acc.count - 1) * stride + packedStride;
    if (needed > buf.data.size())
    {
        std::cerr << "ERROR: VEC4 accessor out of range (needed=" << needed << ", buf=" << buf.data.size() << ")." << std::endl;
        return false;
    }

    out.resize(acc.count);
    const unsigned char* base = buf.data.data() + baseOffset;

    for (size_t i = 0; i < acc.count; ++i)
    {
        const unsigned char* p = base + i * stride;
        float x = readComponentAsFloat(p + 0 * compSize, acc.componentType, acc.normalized);
        float y = readComponentAsFloat(p + 1 * compSize, acc.componentType, acc.normalized);
        float z = readComponentAsFloat(p + 2 * compSize, acc.componentType, acc.normalized);
        float w = readComponentAsFloat(p + 3 * compSize, acc.componentType, acc.normalized);
        out[i] = glm::vec4(x, y, z, w);
    }
    return true;
}

bool SceneManager::readIndexAttribute(const tinygltf::Model& model, int accessorIndex, std::vector<int>& out)
{
    if (accessorIndex < 0 || accessorIndex >= (int)model.accessors.size()) return false;
    const auto& acc = model.accessors[accessorIndex];
    if (acc.type != TINYGLTF_TYPE_SCALAR)
    {
        std::cerr << "ERROR: Index accessor has unexpected type (acc.type=" << acc.type << ")." << std::endl;
        return false;
    }
    if (acc.bufferView < 0 || acc.bufferView >= (int)model.bufferViews.size())
    {
        std::cerr << "ERROR: Index accessor missing bufferView." << std::endl;
        return false;
    }

    const auto& bv = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[bv.buffer];

    const int compSize = componentByteSize(acc.componentType);
    if (compSize <= 0)
    {
        std::cerr << "ERROR: Index accessor unsupported componentType=" << acc.componentType << std::endl;
        return false;
    }

    const size_t packedStride = (size_t)compSize;
    const size_t stride = (bv.byteStride && bv.byteStride >= packedStride) ? (size_t)bv.byteStride : packedStride;
    const size_t baseOffset = (size_t)bv.byteOffset + (size_t)acc.byteOffset;
    if (acc.count == 0)
    {
        std::cerr << "ERROR: Index accessor has zero count." << std::endl;
        return false;
    }
    const size_t needed = baseOffset + (acc.count - 1) * stride + packedStride;
    if (needed > buf.data.size())
    {
        std::cerr << "ERROR: Index accessor out of range (needed=" << needed << ", buf=" << buf.data.size() << ")." << std::endl;
        return false;
    }

    out.resize(acc.count);
    const unsigned char* base = buf.data.data() + baseOffset;

    for (size_t i = 0; i < acc.count; ++i)
    {
        const unsigned char* p = base + i * stride;
        switch (acc.componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            out[i] = static_cast<int>(*reinterpret_cast<const uint8_t*>(p));
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        {
            uint16_t v;
            std::memcpy(&v, p, sizeof(uint16_t));
            out[i] = static_cast<int>(v);
            break;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        {
            uint32_t v;
            std::memcpy(&v, p, sizeof(uint32_t));
            out[i] = static_cast<int>(v);
            break;
        }
        default:
            std::cerr << "ERROR: Unsupported index component type in accessor." << std::endl;
            return false;
        }
    }

    return true;
}

// --------------------------- SceneManager impl ---------------------------

SceneManager::SceneManager(RenderContext& context) : renderContext(context)
{
    debugPrintWin("[mesh2splat] SceneManager.cpp patched build active\n");
    // Note: std::cout may not appear on Windows GUI builds; use OutputDebugStringA above.
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
    catch (const std::exception&)
    {
        std::cerr << "Error loading PLY file: " << filePath << std::endl;
        return false;
    }
    return false;
}

void SceneManager::parseGltfTextureInfo(const tinygltf::Model& model, const tinygltf::Parameter& textureParameter, std::string base_folder, std::string name, utils::TextureInfo& info) {

    auto it = textureParameter.json_double_value.find("index");

    if (it != textureParameter.json_double_value.end()) {
        int textureIndex = static_cast<int>(it->second);
        if (textureIndex >= 0 && textureIndex < (int)model.textures.size()) {
            const tinygltf::Texture& texture = model.textures[textureIndex];
            info.textureIndex = textureIndex;
            info.samplerIndex = texture.sampler;
            if (texture.source >= 0 && texture.source < (int)model.images.size()) {
                const tinygltf::Image& image = model.images[texture.source];
                info.imageIndex = texture.source;
                info.mimeType = image.mimeType;

                std::string fileExtension = image.mimeType.substr(image.mimeType.find_last_of('/') + 1);
                info.path = base_folder + image.name + "." + fileExtension;

                info.width      = image.width;
                info.height     = image.height;

                info.texture.resize(image.image.size());
                std::memcpy(info.texture.data(), image.image.data(), image.image.size());

                info.path       = name;
                info.channels   = image.component;
            }

            if (info.samplerIndex >= 0 && info.samplerIndex < (int)model.samplers.size()) {
                const tinygltf::Sampler& sampler = model.samplers[info.samplerIndex];
                info.wrapS = (sampler.wrapS == 0) ? TINYGLTF_TEXTURE_WRAP_REPEAT : sampler.wrapS;
                info.wrapT = (sampler.wrapT == 0) ? TINYGLTF_TEXTURE_WRAP_REPEAT : sampler.wrapT;
                info.minFilter = sampler.minFilter;
                info.magFilter = sampler.magFilter;
            }
            else {
                info.wrapS = TINYGLTF_TEXTURE_WRAP_REPEAT;
                info.wrapT = TINYGLTF_TEXTURE_WRAP_REPEAT;
                info.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
                info.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
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

    if (materialIndex < 0 || materialIndex >= (int)model.materials.size()) {
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
            materialGltf.normalScale = 1.0f;
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
            materialGltf.occlusionStrength = 1.0f;
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

    materialGltf.metallicFactor  = (float)material.pbrMetallicRoughness.metallicFactor;
    materialGltf.roughnessFactor = (float)material.pbrMetallicRoughness.roughnessFactor;
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

    int index = 0;
    int primGlobalIndex = 0;

    std::vector<NodeWorld> nodeWorlds;
    int sceneIndex = model.defaultScene;
    if (sceneIndex < 0 && !model.scenes.empty())
    {
        sceneIndex = 0;
    }

    if (sceneIndex >= 0 && sceneIndex < static_cast<int>(model.scenes.size()))
    {
        for (int nodeIndex : model.scenes[sceneIndex].nodes)
        {
            collectNodes(model, nodeIndex, glm::mat4(1.0f), nodeWorlds);
        }
    }
    else
    {
        for (int nodeIndex = 0; nodeIndex < static_cast<int>(model.nodes.size()); ++nodeIndex)
        {
            collectNodes(model, nodeIndex, glm::mat4(1.0f), nodeWorlds);
        }
    }

    for (const auto& nodeWorld : nodeWorlds) {
        const tinygltf::Node& node = model.nodes[nodeWorld.nodeIndex];
        if (node.mesh < 0 || node.mesh >= static_cast<int>(model.meshes.size()))
        {
            continue;
        }

        const glm::mat4& world = nodeWorld.world;
        glm::mat3 world3 = glm::mat3(world);

        glm::mat3 normalMatrix(1.0f);
        float det = glm::determinant(world3);
        if (std::abs(det) > 1e-12f)
            normalMatrix = glm::transpose(glm::inverse(world3));

        const auto& mesh = model.meshes[node.mesh];
        for (size_t primIndex = 0; primIndex < mesh.primitives.size(); ++primIndex) {
            const auto& primitive = mesh.primitives[primIndex];
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                std::cerr << "Skipping non-triangle primitive in mesh '" << mesh.name << "'." << std::endl;
                continue;
            }

            std::string baseName = "mesh";
            utils::Mesh myMesh(baseName.append(std::to_string(index)));
            ++index;
            myMesh.sourceName = !mesh.name.empty() ? mesh.name : node.name;
            myMesh.primitiveIndex = primGlobalIndex++;
            myMesh.materialIndex = primitive.material;

            // POSITION is required
            auto posIt = primitive.attributes.find("POSITION");
            if (posIt == primitive.attributes.end())
            {
                std::cerr << "Missing POSITION attribute in mesh '" << mesh.name << "'." << std::endl;
                continue;
            }

            const tinygltf::Accessor& positionAccessor = model.accessors[posIt->second];
            const size_t vertexCount = positionAccessor.count;

            // Read indices (or synthesize)
            std::vector<int> indices;
            if (primitive.indices >= 0) {
                if (!readIndexAttribute(model, primitive.indices, indices))
                {
                    std::cerr << "Unsupported or invalid index accessor in mesh '" << mesh.name << "'." << std::endl;
                    continue;
                }
            }
            else
            {
                indices.resize(vertexCount);
                for (size_t i = 0; i < vertexCount; ++i)
                    indices[i] = static_cast<int>(i);
            }

            if (indices.size() % 3 != 0) {
                std::cerr << "Warning: primitive indices not multiple of 3 in mesh '" << mesh.name << "'. Truncating." << std::endl;
                indices.resize(indices.size() - (indices.size() % 3));
            }

            // SAFE vertex attribute reads (fixes interleaved/strided accessors)
            std::vector<glm::vec3> vertices;
            if (!readVec3Attribute(model, posIt->second, vertices) || vertices.size() != vertexCount)
            {
                std::cerr << "ERROR: Failed reading POSITION attribute safely for mesh '" << mesh.name << "'." << std::endl;
                continue;
            }
            else
            {
                const tinygltf::Accessor& acc = model.accessors[posIt->second];
                const tinygltf::BufferView& bv = model.bufferViews[acc.bufferView];
                // Position accessor debug logging removed (no longer needed).
            }

            std::vector<glm::vec3> normalsVec;
            bool hasNormals = false;
            auto normalIt = primitive.attributes.find("NORMAL");
            if (normalIt != primitive.attributes.end())
            {
                if (readVec3Attribute(model, normalIt->second, normalsVec) && normalsVec.size() == vertexCount)
                    hasNormals = true;
                else
                    std::cerr << "Warning: NORMAL present but could not be read safely; will compute face normals." << std::endl;
            }

            std::vector<glm::vec2> uvsVec;
            bool hasUvs = false;
            auto uvIt = primitive.attributes.find("TEXCOORD_0");
            if (uvIt != primitive.attributes.end())
            {
                const int accessorIndex = uvIt->second;
                if (accessorIndex >= 0 && accessorIndex < (int)model.accessors.size())
                {
                    const tinygltf::Accessor& acc = model.accessors[accessorIndex];
                    myMesh.uvAccessor.hasTexcoord = true;
                    myMesh.uvAccessor.accessorIndex = accessorIndex;
                    myMesh.uvAccessor.accessorType = acc.type;
                    myMesh.uvAccessor.componentType = acc.componentType;
                    myMesh.uvAccessor.normalized = acc.normalized;
                    myMesh.uvAccessor.count = acc.count;
                    myMesh.uvAccessor.accessorByteOffset = acc.byteOffset;

                    if (acc.bufferView >= 0 && acc.bufferView < (int)model.bufferViews.size())
                    {
                        const tinygltf::BufferView& bv = model.bufferViews[acc.bufferView];
                        myMesh.uvAccessor.bufferViewIndex = acc.bufferView;
                        myMesh.uvAccessor.bufferViewByteOffset = bv.byteOffset;
                        myMesh.uvAccessor.bufferViewByteStride = bv.byteStride;
                        myMesh.uvAccessor.bufferIndex = bv.buffer;
                        if (bv.buffer >= 0 && bv.buffer < (int)model.buffers.size())
                        {
                            myMesh.uvAccessor.bufferByteLength = model.buffers[bv.buffer].data.size();
                        }
                    }
                }

                if (readVec2Attribute(model, accessorIndex, uvsVec) && uvsVec.size() == vertexCount)
                    hasUvs = true;
                else
                    std::cerr << "Warning: TEXCOORD_0 present but could not be read safely; using (0,0)." << std::endl;
            }
            else
            {
                myMesh.uvAccessor.hasTexcoord = false;
            }

            std::vector<glm::vec4> tangentsVec;
            bool hasTangents = false;
            auto tanIt = primitive.attributes.find("TANGENT");
            if (tanIt != primitive.attributes.end())
            {
                if (readVec4Attribute(model, tanIt->second, tangentsVec) && tangentsVec.size() == vertexCount)
                    hasTangents = true;
                else
                    std::cerr << "Warning: TANGENT present but could not be read safely; will compute tangents." << std::endl;
            }

            parseGltfMaterial(model, primitive.material, parentFolder, myMesh.material);

            myMesh.faces.resize(indices.size() / 3);
            utils::Face* dst = myMesh.faces.data();

            for (size_t i = 0, count = indices.size(); i < count; i += 3, ++dst) {

                int idx[3] = { indices[i], indices[i + 1], indices[i + 2] };

                for (int e = 0; e < 3; e++)
                {
                    glm::vec4 wsPos = world * glm::vec4(vertices[idx[e]], 1.0f);
                    dst->pos[e] = glm::vec3(wsPos);

                    dst->uv[e] = hasUvs ? uvsVec[idx[e]] : glm::vec2(0.0f);

                    if (hasNormals)
                        dst->normal[e] = glm::normalize(normalMatrix * normalsVec[idx[e]]);
                    else
                        dst->normal[e] = glm::vec3(0.0f);

                    if (hasTangents)
                    {
                        glm::vec3 tangentDir = glm::normalize(world3 * glm::vec3(tangentsVec[idx[e]]));
                        dst->tangent[e] = glm::vec4(tangentDir, tangentsVec[idx[e]].w);
                    }
                }

                if (!hasNormals)
                {
                    glm::vec3 faceNormal = glm::normalize(glm::cross(dst->pos[1] - dst->pos[0], dst->pos[2] - dst->pos[0]));
                    dst->normal[0] = faceNormal;
                    dst->normal[1] = faceNormal;
                    dst->normal[2] = faceNormal;
                }

                if (!hasTangents)
                {
                    if (hasUvs)
                    {
                        glm::vec3 dv1 = dst->pos[1] - dst->pos[0];
                        glm::vec3 dv2 = dst->pos[2] - dst->pos[0];

                        glm::vec2 duv1 = dst->uv[1] - dst->uv[0];
                        glm::vec2 duv2 = dst->uv[2] - dst->uv[0];

                        float denom = duv1.x * duv2.y - duv1.y * duv2.x;
                        if (std::abs(denom) > 1e-8f)
                        {
                            float r = 1.0f / denom;
                            glm::vec3 tangentDir = glm::normalize((dv1 * duv2.y - dv2 * duv1.y) * r);
                            glm::vec4 tangent = glm::vec4(tangentDir, 1.0f);
                            dst->tangent[0] = tangent;
                            dst->tangent[1] = tangent;
                            dst->tangent[2] = tangent;
                        }
                        else
                        {
                            glm::vec4 tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                            dst->tangent[0] = tangent;
                            dst->tangent[1] = tangent;
                            dst->tangent[2] = tangent;
                        }
                    }
                    else
                    {
                        glm::vec4 tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                        dst->tangent[0] = tangent;
                        dst->tangent[1] = tangent;
                        dst->tangent[2] = tangent;
                    }
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

    for (auto& mesh : meshes) {
        utils::GLMesh glMesh;
        std::vector<float> vertices;
        glm::vec3 minBB(FLT_MAX);
        glm::vec3 maxBB(-FLT_MAX);
        mesh.surfaceArea = 0.0f;

        for (const auto& face : mesh.faces) {
            for (int i = 0; i < 3; ++i) {
                vertices.push_back(face.pos[i].x);
                vertices.push_back(face.pos[i].y);
                vertices.push_back(face.pos[i].z);

                vertices.push_back(face.normal[i].x);
                vertices.push_back(face.normal[i].y);
                vertices.push_back(face.normal[i].z);

                vertices.push_back(face.tangent[i].x);
                vertices.push_back(face.tangent[i].y);
                vertices.push_back(face.tangent[i].z);
                vertices.push_back(face.tangent[i].w);

                vertices.push_back(face.uv[i].x);
                vertices.push_back(face.uv[i].y);

                vertices.push_back(face.normalizedUvs[i].x);
                vertices.push_back(face.normalizedUvs[i].y);

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

        glm::vec3 extent = maxBB - minBB;
        glm::vec3 clampedExtent = glm::max(extent, glm::vec3(1e-6f));
        maxBB = minBB + clampedExtent;
        mesh.bbox = utils::BBox(minBB, maxBB);

        renderContext.totalSurfaceArea += mesh.surfaceArea;

        // sanity print (if bbox is tiny/zero, conversion will output nothing)
        if (glm::length(mesh.bbox.max - mesh.bbox.min) < 1e-5f)
            std::cerr << "Warning: mesh bbox is near-zero. Likely bad POSITION read or transforms." << std::endl;

        unsigned int floatsPerVertex = 17;
        glMesh.vertexCount = vertices.size() / floatsPerVertex;

        size_t vertexStride = floatsPerVertex * sizeof(float);

        glGenVertexArrays(1, &glMesh.vao);
        glBindVertexArray(glMesh.vao);

        glGenBuffers(1, &glMesh.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, glMesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (GLuint)vertexStride, (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, (GLuint)vertexStride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, (GLuint)vertexStride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, (GLuint)vertexStride, (void*)(10 * sizeof(float)));
        glEnableVertexAttribArray(3);

        glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, (GLuint)vertexStride, (void*)(12 * sizeof(float)));
        glEnableVertexAttribArray(4);

        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, (GLuint)vertexStride, (void*)(14 * sizeof(float)));
        glEnableVertexAttribArray(5);

        glBindVertexArray(0);

        renderContext.dataMeshAndGlMesh.push_back(std::make_pair(mesh, glMesh));
    }
}

void SceneManager::loadTextures(const std::vector<utils::Mesh>& meshes)
{
    if (renderContext.debugTextureStats)
    {
        std::unordered_map<int, bool> seenImages;
        for (const auto& mesh : meshes)
        {
            if (mesh.material.baseColorTexture.path == EMPTY_TEXTURE)
            {
                continue;
            }

            const auto& info = mesh.material.baseColorTexture;
            const int imageIndex = info.imageIndex;
            if (imageIndex < 0)
            {
                std::cout << "[tex-stats] image=-1 tex=" << info.textureIndex
                          << " firstUse mat=" << mesh.materialIndex
                          << " prim=" << mesh.primitiveIndex
                          << " mesh=\"" << (mesh.sourceName.empty() ? mesh.name : mesh.sourceName)
                          << "\" decode_failed reason=missing_image_index"
                          << std::endl;
                continue;
            }

            if (seenImages.find(imageIndex) != seenImages.end())
            {
                continue;
            }
            seenImages[imageIndex] = true;

            TextureStatsResult stats = computeTextureStats(
                info.texture,
                info.width,
                info.height,
                static_cast<int>(info.channels),
                renderContext.textureStatsDownsample);

            if (!stats.ok)
            {
                std::cout << "[tex-stats] image=" << imageIndex
                          << " tex=" << info.textureIndex
                          << " firstUse mat=" << mesh.materialIndex
                          << " prim=" << mesh.primitiveIndex
                          << " mesh=\"" << (mesh.sourceName.empty() ? mesh.name : mesh.sourceName)
                          << "\" decode_failed reason=" << stats.error
                          << std::endl;
                continue;
            }

            std::cout << "[tex-stats] image=" << imageIndex
                      << " tex=" << info.textureIndex
                      << " firstUse mat=" << mesh.materialIndex
                      << " prim=" << mesh.primitiveIndex
                      << " mesh=\"" << (mesh.sourceName.empty() ? mesh.name : mesh.sourceName)
                      << "\" mime=\"" << info.mimeType
                      << "\" size=" << info.width << "x" << info.height
                      << " ch=" << info.channels
                      << std::endl;

            std::cout << std::fixed << std::setprecision(3)
                      << "  mean_srgb=" << formatVec3(stats.meanSrgb)
                      << " std_srgb=" << formatVec3(stats.stdSrgb)
                      << " stdY_srgb=" << stats.stdYSrgb
                      << std::endl;
            std::cout << std::fixed << std::setprecision(3)
                      << "  mean_lin =" << formatVec3(stats.meanLin)
                      << " std_lin =" << formatVec3(stats.stdLin)
                      << " stdY_lin =" << stats.stdYLin
                      << std::endl;
            std::cout << std::fixed << std::setprecision(4)
                      << "  downsample=" << renderContext.textureStatsDownsample
                      << " gradY_lin=" << stats.gradYLin
                      << std::endl;
        }
    }

    for (auto& mesh : meshes)
    {
        std::map<std::string, utils::TextureDataGl> textureMapForThisMesh;

        if (mesh.material.baseColorTexture.path != EMPTY_TEXTURE)
        {
            utils::TextureDataGl tdgl(mesh.material.baseColorTexture);
            if (renderContext.forceSrgbMode == 1) {
                tdgl.srgb = true;
            } else if (renderContext.forceSrgbMode == 2) {
                tdgl.srgb = false;
            } else {
                tdgl.srgb = true;
            }
            if (renderContext.forceUvWrapMode == 1) {
                tdgl.wrapS = TINYGLTF_TEXTURE_WRAP_REPEAT;
                tdgl.wrapT = TINYGLTF_TEXTURE_WRAP_REPEAT;
            } else if (renderContext.forceUvWrapMode == 2) {
                tdgl.wrapS = TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE;
                tdgl.wrapT = TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE;
            } else if (renderContext.forceUvWrapMode == 3) {
                tdgl.wrapS = TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT;
                tdgl.wrapT = TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT;
            }
            textureMapForThisMesh.insert_or_assign(BASE_COLOR_TEXTURE, tdgl);
        }
        else {
            renderContext.material.baseColorTexture.width = MAX_RESOLUTION_TARGET;
            renderContext.material.baseColorTexture.height = MAX_RESOLUTION_TARGET;
            textureMapForThisMesh.erase(BASE_COLOR_TEXTURE);
        }

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

    namespace {
        std::string appendSuffixBeforeExtension(const std::string& path, const std::string& suffix)
        {
            size_t dot = path.find_last_of('.');
            if (dot == std::string::npos)
                return path + suffix;
            return path.substr(0, dot) + suffix + path.substr(dot);
        }

        glm::vec3 computeDcFromColor(const glm::vec3& colorLinear, int dcMode)
        {
            switch (dcMode)
            {
                case 1: return colorLinear;
                case 2: return utils::linear_to_srgb_float(colorLinear);
                case 0:
                default:
                    return utils::getShFromColor(colorLinear);
            }
        }

        glm::vec3 decodeDcSupersplat(const glm::vec3& dc)
        {
            return dc * SH_COEFF0 + glm::vec3(0.5f);
        }

        glm::vec3 decodeDcRoundTrip(const glm::vec3& dc, int dcMode)
        {
            switch (dcMode)
            {
                case 1: return dc;
                case 2: return utils::srgb_to_linear_float(dc);
                case 0:
                default:
                    return decodeDcSupersplat(dc);
            }
        }

        glm::vec2 applyVFlip(const glm::vec2& uv)
        {
            return glm::vec2(uv.x, 1.0f - uv.y);
        }
    }

    void SceneManager::exportSplats(const std::string outputFile, unsigned int exportFormat)
{
    if (!renderContext.numberOfGaussians)
        return;

    // Ensure GPU writes are visible before readback.
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glFinish();

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

    std::vector<uint32_t> primIds;
    if (renderContext.debugColorStats && renderContext.debugPrimIdBuffer != 0)
    {
        primIds.resize(renderContext.numberOfGaussians);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.debugPrimIdBuffer);
        glGetBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            0,
            renderContext.numberOfGaussians * sizeof(uint32_t),
            primIds.data()
        );
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    if (renderContext.debugUv || renderContext.debugColor)
    {
        const int maxPrims = (renderContext.debugMaxPrimitives > 0) ? renderContext.debugMaxPrimitives : 1;
        const int maxVerts = (renderContext.debugMaxVertices > 0) ? renderContext.debugMaxVertices : 1;
        const int maxSplats = (renderContext.debugMaxSplats > 0) ? renderContext.debugMaxSplats : 1;
        const bool opacityIsLogit = (renderContext.opacityMode == 2) ||
                                    (renderContext.opacityMode == 0 && exportFormat == 1);

        int primPrinted = 0;
        int primsProcessed = 0;
        int missingTexcoord = 0;
        int missingMaterial = 0;
        int missingBaseColorTex = 0;
        size_t totalVerticesProcessed = 0;

        for (const auto& meshPair : renderContext.dataMeshAndGlMesh)
        {
            if (primPrinted >= maxPrims)
            {
                break;
            }

            const utils::Mesh& mesh = meshPair.first;
            const utils::MaterialGltf& material = mesh.material;
            const std::string meshLabel = !mesh.sourceName.empty() ? mesh.sourceName : mesh.name;

            const bool hasTexcoord = mesh.uvAccessor.hasTexcoord;
            if (!hasTexcoord)
                ++missingTexcoord;

            const bool hasMaterial = mesh.materialIndex >= 0;
            if (!hasMaterial)
                ++missingMaterial;

            bool hasBaseColorTex = material.baseColorTexture.path != EMPTY_TEXTURE;
            if (!hasBaseColorTex)
                ++missingBaseColorTex;

            const utils::TextureDataGl* baseTex = nullptr;
            auto meshTexIt = renderContext.meshToTextureData.find(mesh.name);
            if (meshTexIt != renderContext.meshToTextureData.end())
            {
                auto texIt = meshTexIt->second.find(BASE_COLOR_TEXTURE);
                if (texIt != meshTexIt->second.end())
                {
                    baseTex = &texIt->second;
                    hasBaseColorTex = hasBaseColorTex && (!baseTex->textureData.empty());
                }
            }

            int wrapS = baseTex ? baseTex->wrapS : material.baseColorTexture.wrapS;
            int wrapT = baseTex ? baseTex->wrapT : material.baseColorTexture.wrapT;
            if (!baseTex) {
                if (renderContext.forceUvWrapMode == 1) {
                    wrapS = TINYGLTF_TEXTURE_WRAP_REPEAT;
                    wrapT = TINYGLTF_TEXTURE_WRAP_REPEAT;
                } else if (renderContext.forceUvWrapMode == 2) {
                    wrapS = TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE;
                    wrapT = TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE;
                } else if (renderContext.forceUvWrapMode == 3) {
                    wrapS = TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT;
                    wrapT = TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT;
                }
            }

            bool srgbDecode = true;
            if (baseTex) {
                srgbDecode = baseTex->srgb;
            } else if (renderContext.forceSrgbMode == 1) {
                srgbDecode = true;
            } else if (renderContext.forceSrgbMode == 2) {
                srgbDecode = false;
            }

            if (renderContext.debugUv)
            {
                const auto& uvAcc = mesh.uvAccessor;
                std::cout << "[debug-uvmeta] prim=" << mesh.primitiveIndex
                          << " mesh=\"" << meshLabel << "\""
                          << " texcoord=" << (uvAcc.hasTexcoord ? 1 : 0)
                          << " acc=" << uvAcc.accessorIndex
                          << " type=" << accessorTypeToString(uvAcc.accessorType)
                          << " compType=" << uvAcc.componentType
                          << " normalized=" << (uvAcc.normalized ? 1 : 0)
                          << " count=" << uvAcc.count
                          << std::endl;
                std::cout << "  acc.byteOffset=" << uvAcc.accessorByteOffset
                          << " view=" << uvAcc.bufferViewIndex
                          << " view.byteOffset=" << uvAcc.bufferViewByteOffset
                          << " view.byteStride=" << uvAcc.bufferViewByteStride
                          << " buffer=" << uvAcc.bufferIndex
                          << " buffer.byteLength=" << uvAcc.bufferByteLength
                          << std::endl;
            }

            if (renderContext.debugUv || renderContext.debugColor)
            {
                std::cout << "[debug-mat] prim=" << mesh.primitiveIndex
                          << " mat=" << mesh.materialIndex
                          << " name=\"" << material.name << "\""
                          << " hasBaseColorTex=" << (hasBaseColorTex ? 1 : 0)
                          << " tex=" << material.baseColorTexture.textureIndex
                          << " image=" << material.baseColorTexture.imageIndex
                          << " size=" << material.baseColorTexture.width << "x" << material.baseColorTexture.height
                          << " channels=" << material.baseColorTexture.channels
                          << " mime=\"" << material.baseColorTexture.mimeType << "\""
                          << std::endl;
                std::cout << "  sampler=" << material.baseColorTexture.samplerIndex
                          << " wrapS=" << wrapToString(wrapS)
                          << " wrapT=" << wrapToString(wrapT)
                          << " minFilter=" << filterToString(material.baseColorTexture.minFilter)
                          << " magFilter=" << filterToString(material.baseColorTexture.magFilter)
                          << " texCoord=" << material.baseColorTexture.texCoordIndex
                          << " srgbDecode=" << (srgbDecode ? 1 : 0)
                          << " baseColorFactor=" << formatVec4(material.baseColorFactor)
                          << std::endl;
            }

            bool computedUvStats = false;
            float minU = 0.0f;
            float maxU = 0.0f;
            float minV = 0.0f;
            float maxV = 0.0f;
            float oorPct = 0.0f;
            float nanPct = 0.0f;

            if (renderContext.debugUv || renderContext.autoUvWrap)
            {
                minU = FLT_MAX;
                maxU = -FLT_MAX;
                minV = FLT_MAX;
                maxV = -FLT_MAX;
                size_t count = 0;
                size_t totalCount = 0;
                size_t nanCount = 0;
                size_t uLow = 0;
                size_t uHigh = 0;
                size_t vLow = 0;
                size_t vHigh = 0;

                std::ostringstream uvSamples;
                int printedUvs = 0;
                for (const auto& face : mesh.faces)
                {
                    for (int corner = 0; corner < 3; ++corner)
                    {
                        glm::vec2 uv = face.uv[corner];
                        ++totalCount;
                        if (!std::isfinite(uv.x) || !std::isfinite(uv.y))
                        {
                            ++nanCount;
                            continue;
                        }
                        minU = std::min(minU, uv.x);
                        maxU = std::max(maxU, uv.x);
                        minV = std::min(minV, uv.y);
                        maxV = std::max(maxV, uv.y);

                        if (uv.x < 0.0f) ++uLow;
                        if (uv.x > 1.0f) ++uHigh;
                        if (uv.y < 0.0f) ++vLow;
                        if (uv.y > 1.0f) ++vHigh;
                        ++count;

                        if (renderContext.debugUv && printedUvs < maxVerts)
                        {
                            glm::vec2 wrapped = applyWrap(uv, wrapS, wrapT);
                            uvSamples << "uv[" << printedUvs << "]=" << formatVec2(uv)
                                      << "->" << formatVec2(wrapped)
                                      << " ";
                            ++printedUvs;
                        }
                    }
                }

                if (count == 0) count = 1;
                if (totalCount == 0) totalCount = 1;
                oorPct = 100.0f * static_cast<float>(uLow + uHigh + vLow + vHigh) / static_cast<float>(count * 2);
                nanPct = 100.0f * static_cast<float>(nanCount) / static_cast<float>(totalCount);
                if (minU == FLT_MAX) { minU = 0.0f; maxU = 0.0f; minV = 0.0f; maxV = 0.0f; }
                computedUvStats = true;

                if (renderContext.debugUv)
                {
                    std::ostringstream header;
                    header << std::fixed << std::setprecision(3);
                    header << "[debug-uvstats] prim=" << mesh.primitiveIndex
                           << " minU=" << minU << " maxU=" << maxU
                           << " minV=" << minV << " maxV=" << maxV
                           << " oorPct=" << oorPct
                           << " u<0=" << (100.0f * uLow / count)
                           << " u>1=" << (100.0f * uHigh / count)
                           << " v<0=" << (100.0f * vLow / count)
                           << " v>1=" << (100.0f * vHigh / count)
                           << " nanPct=" << nanPct;
                    std::cout << header.str() << std::endl;
                    if (printedUvs > 0)
                        std::cout << "  " << uvSamples.str() << std::endl;

                    if (oorPct > 20.0f)
                    {
                        std::cout << "[debug-uv] WARNING: high UV out-of-range percentage (" << std::fixed << std::setprecision(1)
                                  << oorPct << "%) for prim=" << mesh.primitiveIndex << ". Wrapping behavior likely important."
                                  << std::endl;
                    }

                    if ((material.baseColorTexture.wrapS == TINYGLTF_TEXTURE_WRAP_REPEAT ||
                         material.baseColorTexture.wrapS == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT ||
                         material.baseColorTexture.wrapT == TINYGLTF_TEXTURE_WRAP_REPEAT ||
                         material.baseColorTexture.wrapT == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT) &&
                        renderContext.forceUvWrapMode == 2)
                    {
                        std::cout << "[debug-uv] WARNING: sampler expects REPEAT/MIRRORED_REPEAT but force-uv-wrap=clamp is active."
                                  << std::endl;
                    }
                }
            }

            if (renderContext.autoUvWrap && computedUvStats)
            {
                const float kAutoWrapOorThreshold = 20.0f;
                if (oorPct > kAutoWrapOorThreshold)
                {
                    if (wrapS == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE)
                        wrapS = TINYGLTF_TEXTURE_WRAP_REPEAT;
                    if (wrapT == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE)
                        wrapT = TINYGLTF_TEXTURE_WRAP_REPEAT;

                    if (renderContext.debugUv)
                    {
                        std::cout << "[debug-uv] auto-uv-wrap enabled: overriding clamp to repeat for prim="
                                  << mesh.primitiveIndex << " (oorPct=" << std::fixed << std::setprecision(1)
                                  << oorPct << "%)." << std::endl;
                    }
                }
            }

            if (renderContext.debugColor)
            {
                int printedVerts = 0;
                double diffClampSum = 0.0;
                double diffRepeatVFlipSum = 0.0;
                size_t diffCount = 0;
                for (size_t faceIdx = 0; faceIdx < mesh.faces.size() && printedVerts < maxVerts; ++faceIdx)
                {
                    const utils::Face& face = mesh.faces[faceIdx];
                    for (int corner = 0; corner < 3 && printedVerts < maxVerts; ++corner)
                    {
                        const int vertexIndex = static_cast<int>(faceIdx * 3 + corner);
                        const glm::vec2 uv = face.uv[corner];
                        bool uvOutOfRange = (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f);
                        glm::vec2 uvWrapped = applyWrap(uv, wrapS, wrapT);

                        glm::vec4 sampledRaw(1.0f);
                        glm::vec4 sampled = glm::vec4(1.0f);
                        if (hasBaseColorTex && baseTex)
                        {
                            sampled = sampleTextureBilinear(*baseTex, uv, wrapS, wrapT, &sampledRaw, srgbDecode, &uvOutOfRange);
                        }
                        glm::vec4 finalColor = sampled * material.baseColorFactor;
                        glm::vec3 dc = utils::getShFromColor(glm::vec3(finalColor));

                        float opacityRaw = 0.0f;
                        float opacityAlt = 0.0f;
                        const char* opacityAltLabel = "opacitySig";
                        if (opacityIsLogit)
                        {
                            opacityRaw = invSigmoidSafe(finalColor.w);
                            opacityAlt = utils::sigmoid(opacityRaw);
                        }
                        else if (exportFormat == 2)
                        {
                            uint8_t rawByte = toByte(finalColor.w);
                            opacityRaw = static_cast<float>(rawByte);
                            opacityAlt = invSigmoidSafe(finalColor.w);
                            opacityAltLabel = "opacityLogit";
                        }
                        else
                        {
                            opacityRaw = finalColor.w;
                            opacityAlt = invSigmoidSafe(opacityRaw);
                            opacityAltLabel = "opacityLogit";
                        }

                        std::ostringstream line;
                        line << std::fixed << std::setprecision(3);
                        line << "[debug-color] prim=" << mesh.primitiveIndex
                             << " v=" << vertexIndex
                             << " uv=" << formatVec2(uv)
                             << " uvOOR=" << (uvOutOfRange ? 1 : 0)
                             << " uvWrapped=" << formatVec2(uvWrapped)
                             << std::endl;
                        std::cout << line.str();

                        std::ostringstream line2;
                        line2 << std::fixed << std::setprecision(3);
                        line2 << "  texRGB=" << formatVec3(glm::vec3(sampledRaw))
                              << " texRGBLinear=" << formatVec3(glm::vec3(sampled))
                              << " finalRGB=" << formatVec3(glm::vec3(finalColor))
                              << " dc=" << formatVec3(dc)
                              << " opacityRaw=" << opacityRaw
                              << " " << opacityAltLabel << "=" << opacityAlt
                              << " opacityIsLogit=" << (opacityIsLogit ? 1 : 0);
                        std::cout << line2.str() << std::endl;

                        if (renderContext.debugUvCompare && hasBaseColorTex && baseTex)
                        {
                            glm::vec4 clampRaw(1.0f);
                            glm::vec4 clampSample = sampleTextureBilinear(*baseTex, uv, TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE, TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE, &clampRaw, srgbDecode, nullptr);
                            glm::vec2 uvFlip = applyVFlip(uv);
                            glm::vec4 flipRaw(1.0f);
                            glm::vec4 repeatFlipSample = sampleTextureBilinear(*baseTex, uvFlip, TINYGLTF_TEXTURE_WRAP_REPEAT, TINYGLTF_TEXTURE_WRAP_REPEAT, &flipRaw, srgbDecode, nullptr);

                            auto diff = [](const glm::vec3& a, const glm::vec3& b) -> float {
                                glm::vec3 d = glm::abs(a - b);
                                return (d.x + d.y + d.z) / 3.0f;
                            };

                            diffClampSum += diff(glm::vec3(sampled), glm::vec3(clampSample));
                            diffRepeatVFlipSum += diff(glm::vec3(sampled), glm::vec3(repeatFlipSample));
                            ++diffCount;
                        }

                        ++printedVerts;
                    }
                }

                if (renderContext.debugUvCompare && diffCount > 0)
                {
                    std::cout << "[debug-uv-compare] prim=" << mesh.primitiveIndex
                              << " avgDeltaClamp=" << std::fixed << std::setprecision(4)
                              << (diffClampSum / static_cast<double>(diffCount))
                              << " avgDeltaRepeatVFlip=" << (diffRepeatVFlipSum / static_cast<double>(diffCount))
                              << std::endl;
                }
            }

            totalVerticesProcessed += mesh.faces.size() * 3;
            ++primsProcessed;
            ++primPrinted;
        }

        if (renderContext.debugColor)
        {
            const size_t splatCount = std::min(cpuData.size(), static_cast<size_t>(maxSplats));
            if (splatCount > 0)
            {
                std::cout << "[debug-color] splatSamples count=" << splatCount
                          << " opacityIsLogit=" << (opacityIsLogit ? 1 : 0)
                          << std::endl;
                for (size_t i = 0; i < splatCount; ++i)
                {
                    const utils::GaussianDataSSBO& g = cpuData[i];
                    glm::vec3 dc = utils::getShFromColor(glm::vec3(g.color));
                    float opacityRaw = 0.0f;
                    float opacityAlt = 0.0f;
                    const char* opacityAltLabel = "opacitySig";
                    if (exportFormat == 1)
                    {
                        opacityRaw = invSigmoidSafe(g.color.w);
                        opacityAlt = utils::sigmoid(opacityRaw);
                    }
                    else if (exportFormat == 2)
                    {
                        uint8_t rawByte = toByte(g.color.w);
                        opacityRaw = static_cast<float>(rawByte);
                        opacityAlt = invSigmoidSafe(g.color.w);
                        opacityAltLabel = "opacityLogit";
                    }
                    else
                    {
                        opacityRaw = g.color.w;
                        opacityAlt = invSigmoidSafe(opacityRaw);
                        opacityAltLabel = "opacityLogit";
                    }

                    std::ostringstream line;
                    line << std::fixed << std::setprecision(3);
                    line << "  s=" << i
                         << " dc=" << formatVec3(dc)
                         << " opacityRaw=" << opacityRaw
                         << " " << opacityAltLabel << "=" << opacityAlt
                         << " scale=" << formatVec3(glm::vec3(g.linearScale))
                         << " rot=" << formatVec4(g.rotation);
                    std::cout << line.str() << std::endl;
                }
            }
        }

        if (renderContext.debugPrintSummary && !cpuData.empty())
        {
            glm::vec3 dcMin(FLT_MAX);
            glm::vec3 dcMax(-FLT_MAX);
            glm::vec3 dcSum(0.0f);

            float opacityRawMin = FLT_MAX;
            float opacityRawMax = -FLT_MAX;
            double opacityRawSum = 0.0;

            float opacityAltMin = FLT_MAX;
            float opacityAltMax = -FLT_MAX;
            double opacityAltSum = 0.0;

            float opacityRawByteMin = FLT_MAX;
            float opacityRawByteMax = -FLT_MAX;
            double opacityRawByteSum = 0.0;

            for (const auto& g : cpuData)
            {
                glm::vec3 dc = utils::getShFromColor(glm::vec3(g.color));
                dcMin = glm::min(dcMin, dc);
                dcMax = glm::max(dcMax, dc);
                dcSum += dc;

                float opacityRaw = 0.0f;
                float opacityAlt = 0.0f;
                if (exportFormat == 1)
                {
                    opacityRaw = invSigmoidSafe(g.color.w);
                    opacityAlt = utils::sigmoid(opacityRaw);
                }
                else if (exportFormat == 2)
                {
                    uint8_t rawByte = toByte(g.color.w);
                    float rawByteF = static_cast<float>(rawByte);
                    opacityRawByteMin = std::min(opacityRawByteMin, rawByteF);
                    opacityRawByteMax = std::max(opacityRawByteMax, rawByteF);
                    opacityRawByteSum += rawByteF;
                    opacityRaw = g.color.w;
                    opacityAlt = invSigmoidSafe(opacityRaw);
                }
                else
                {
                    opacityRaw = g.color.w;
                    opacityAlt = invSigmoidSafe(opacityRaw);
                }

                opacityRawMin = std::min(opacityRawMin, opacityRaw);
                opacityRawMax = std::max(opacityRawMax, opacityRaw);
                opacityRawSum += opacityRaw;

                opacityAltMin = std::min(opacityAltMin, opacityAlt);
                opacityAltMax = std::max(opacityAltMax, opacityAlt);
                opacityAltSum += opacityAlt;
            }

            const float count = static_cast<float>(std::max<size_t>(1, cpuData.size()));
            glm::vec3 dcMean = dcSum / count;
            float opacityRawMean = static_cast<float>(opacityRawSum / count);
            float opacityAltMean = static_cast<float>(opacityAltSum / count);

            std::cout << "[debug-summary] prims=" << primsProcessed
                      << " vertices=" << totalVerticesProcessed
                      << " missingTexcoord=" << missingTexcoord
                      << " missingMaterial=" << missingMaterial
                      << " missingBaseColorTex=" << missingBaseColorTex
                      << std::endl;

            std::ostringstream dcLine;
            dcLine << std::fixed << std::setprecision(4);
            dcLine << "  dc0 min/max/mean=" << dcMin.x << "/" << dcMax.x << "/" << dcMean.x
                   << " dc1 min/max/mean=" << dcMin.y << "/" << dcMax.y << "/" << dcMean.y
                   << " dc2 min/max/mean=" << dcMin.z << "/" << dcMax.z << "/" << dcMean.z;
            std::cout << dcLine.str() << std::endl;

            const char* opacityAltLabel = (exportFormat == 1) ? "opacitySig" : "opacityLogit";
            std::ostringstream opacityLine;
            opacityLine << std::fixed << std::setprecision(4);
                opacityLine << "  opacityRaw min/max/mean=" << opacityRawMin << "/" << opacityRawMax << "/" << opacityRawMean
                            << " " << opacityAltLabel << " min/max/mean=" << opacityAltMin << "/" << opacityAltMax << "/" << opacityAltMean
                            << " opacityIsLogit=" << (opacityIsLogit ? 1 : 0);
            std::cout << opacityLine.str() << std::endl;

            if (exportFormat == 2)
            {
                float opacityRawByteMean = static_cast<float>(opacityRawByteSum / count);
                std::ostringstream byteLine;
                byteLine << std::fixed << std::setprecision(4);
                byteLine << "  opacityRawByte min/max/mean=" << opacityRawByteMin << "/" << opacityRawByteMax << "/" << opacityRawByteMean;
                std::cout << byteLine.str() << std::endl;
            }
        }
    }

    if (renderContext.debugColorStats && !cpuData.empty())
    {
        const bool opacityIsLogit = (renderContext.opacityMode == 2) ||
                                    (renderContext.opacityMode == 0 && exportFormat == 1);
        struct Accum3 {
            size_t count = 0;
            glm::vec3 mean = glm::vec3(0.0f);
            glm::vec3 m2 = glm::vec3(0.0f);
            glm::vec3 min = glm::vec3(FLT_MAX);
            glm::vec3 max = glm::vec3(-FLT_MAX);
            float meanY = 0.0f;
            float m2Y = 0.0f;
            void add(const glm::vec3& v)
            {
                ++count;
                glm::vec3 delta = v - mean;
                mean += delta / static_cast<float>(count);
                glm::vec3 delta2 = v - mean;
                m2 += delta * delta2;
                min = glm::min(min, v);
                max = glm::max(max, v);
                float y = 0.2126f * v.x + 0.7152f * v.y + 0.0722f * v.z;
                float dy = y - meanY;
                meanY += dy / static_cast<float>(count);
                float dy2 = y - meanY;
                m2Y += dy * dy2;
            }
            glm::vec3 stddev() const
            {
                if (count < 2) return glm::vec3(0.0f);
                return glm::sqrt(glm::max(m2 / static_cast<float>(count - 1), glm::vec3(0.0f)));
            }
            float stdY() const
            {
                if (count < 2) return 0.0f;
                return std::sqrt(std::max(m2Y / static_cast<float>(count - 1), 0.0f));
            }
        };
        struct Accum1 {
            size_t count = 0;
            double mean = 0.0;
            double m2 = 0.0;
            double min = DBL_MAX;
            double max = -DBL_MAX;
            void add(double v)
            {
                ++count;
                double delta = v - mean;
                mean += delta / static_cast<double>(count);
                double delta2 = v - mean;
                m2 += delta * delta2;
                min = std::min(min, v);
                max = std::max(max, v);
            }
            double stddev() const
            {
                if (count < 2) return 0.0;
                return std::sqrt(std::max(m2 / static_cast<double>(count - 1), 0.0));
            }
        };

        Accum3 globalFinal;
        Accum3 globalDc;
        Accum3 globalDecodedSupersplat;
        Accum3 globalDecodedRoundTrip;
        Accum1 globalOpacityDecoded;

        std::unordered_map<uint32_t, Accum3> primFinal;
        std::unordered_map<uint32_t, Accum3> primDc;
        std::unordered_map<uint32_t, Accum3> primDecodedSupersplat;
        std::unordered_map<uint32_t, Accum3> primDecodedRoundTrip;
        std::unordered_map<uint32_t, Accum1> primOpacityDecoded;
        std::unordered_map<uint32_t, const utils::Mesh*> primToMesh;
        for (const auto& meshPair : renderContext.dataMeshAndGlMesh)
        {
            primToMesh[static_cast<uint32_t>(meshPair.first.primitiveIndex)] = &meshPair.first;
        }

        for (size_t i = 0; i < cpuData.size(); ++i)
        {
            const glm::vec3 finalLin = glm::vec3(cpuData[i].color);
            const glm::vec3 dc = computeDcFromColor(finalLin, renderContext.dcMode);
            const glm::vec3 decodedSupersplat = decodeDcSupersplat(dc);
            const glm::vec3 decodedRoundTrip = decodeDcRoundTrip(dc, renderContext.dcMode);
            double opacityDecoded = opacityIsLogit
                ? static_cast<double>(utils::sigmoid(invSigmoidSafe(cpuData[i].color.w)))
                : static_cast<double>(cpuData[i].color.w);

            globalFinal.add(finalLin);
            globalDc.add(dc);
            globalDecodedSupersplat.add(decodedSupersplat);
            globalDecodedRoundTrip.add(decodedRoundTrip);
            globalOpacityDecoded.add(opacityDecoded);

            if (!primIds.empty())
            {
                uint32_t pid = primIds[i];
                primFinal[pid].add(finalLin);
                primDc[pid].add(dc);
                primDecodedSupersplat[pid].add(decodedSupersplat);
                primDecodedRoundTrip[pid].add(decodedRoundTrip);
                primOpacityDecoded[pid].add(opacityDecoded);
            }
        }

        if (primIds.empty())
        {
            std::cout << "[debug-color-stats] per-primitive unavailable (no primId buffer). Enable --debug-color-stats during conversion." << std::endl;
        }
        else
        {
            std::vector<uint32_t> primKeys;
            primKeys.reserve(primFinal.size());
            for (const auto& entry : primFinal)
                primKeys.push_back(entry.first);
            std::sort(primKeys.begin(), primKeys.end());

            int printed = 0;
            for (uint32_t pid : primKeys)
            {
                if (printed >= renderContext.debugMaxPrimitives)
                    break;
                const Accum3& f = primFinal[pid];
                const Accum3& d = primDc[pid];
                const Accum3& ds = primDecodedSupersplat[pid];
                const Accum3& dr = primDecodedRoundTrip[pid];
                const Accum1& op = primOpacityDecoded[pid];
                const utils::Mesh* mesh = nullptr;
                auto it = primToMesh.find(pid);
                if (it != primToMesh.end())
                    mesh = it->second;
                std::string meshLabel = mesh ? (!mesh->sourceName.empty() ? mesh->sourceName : mesh->name) : "";
                int matIndex = mesh ? mesh->materialIndex : -1;

                std::cout << "[debug-color-stats] prim=" << pid
                          << " mesh=\"" << meshLabel << "\""
                          << " mat=" << matIndex
                          << std::endl;
                std::cout << std::fixed << std::setprecision(3)
                          << "  finalLin mean=" << formatVec3(f.mean)
                          << " std=" << formatVec3(f.stddev())
                          << " min=" << formatVec3(f.min)
                          << " max=" << formatVec3(f.max)
                          << " stdY=" << f.stdY()
                          << std::endl;
                std::cout << std::fixed << std::setprecision(3)
                          << "  dc      mean=" << formatVec3(d.mean)
                          << " std=" << formatVec3(d.stddev())
                          << " min=" << formatVec3(d.min)
                          << " max=" << formatVec3(d.max)
                          << " stdY=" << d.stdY()
                          << std::endl;
                std::cout << std::fixed << std::setprecision(3)
                          << "  decSup  mean=" << formatVec3(ds.mean)
                          << " std=" << formatVec3(ds.stddev())
                          << " min=" << formatVec3(ds.min)
                          << " max=" << formatVec3(ds.max)
                          << " stdY=" << ds.stdY()
                          << std::endl;
                std::cout << std::fixed << std::setprecision(3)
                          << "  decRT   mean=" << formatVec3(dr.mean)
                          << " std=" << formatVec3(dr.stddev())
                          << " min=" << formatVec3(dr.min)
                          << " max=" << formatVec3(dr.max)
                          << " stdY=" << dr.stdY()
                          << std::endl;
                std::cout << std::fixed << std::setprecision(3)
                          << "  opacity mean=" << op.mean
                          << " std=" << op.stddev()
                          << " min=" << op.min
                          << " max=" << op.max
                          << " opacityIsLogit=" << (opacityIsLogit ? 1 : 0)
                          << std::endl;
                ++printed;
            }
        }

        std::cout << "[debug-color-stats] GLOBAL" << std::endl;
        std::cout << std::fixed << std::setprecision(3)
                  << "  finalLin mean=" << formatVec3(globalFinal.mean)
                  << " std=" << formatVec3(globalFinal.stddev())
                  << " min=" << formatVec3(globalFinal.min)
                  << " max=" << formatVec3(globalFinal.max)
                  << " stdY=" << globalFinal.stdY()
                  << std::endl;
        std::cout << std::fixed << std::setprecision(3)
                  << "  dc      mean=" << formatVec3(globalDc.mean)
                  << " std=" << formatVec3(globalDc.stddev())
                  << " min=" << formatVec3(globalDc.min)
                  << " max=" << formatVec3(globalDc.max)
                  << " stdY=" << globalDc.stdY()
                  << std::endl;
        std::cout << std::fixed << std::setprecision(3)
                  << "  decSup  mean=" << formatVec3(globalDecodedSupersplat.mean)
                  << " std=" << formatVec3(globalDecodedSupersplat.stddev())
                  << " min=" << formatVec3(globalDecodedSupersplat.min)
                  << " max=" << formatVec3(globalDecodedSupersplat.max)
                  << " stdY=" << globalDecodedSupersplat.stdY()
                  << std::endl;
        std::cout << std::fixed << std::setprecision(3)
                  << "  decRT   mean=" << formatVec3(globalDecodedRoundTrip.mean)
                  << " std=" << formatVec3(globalDecodedRoundTrip.stddev())
                  << " min=" << formatVec3(globalDecodedRoundTrip.min)
                  << " max=" << formatVec3(globalDecodedRoundTrip.max)
                  << " stdY=" << globalDecodedRoundTrip.stdY()
                  << std::endl;
        std::cout << std::fixed << std::setprecision(3)
                  << "  opacity mean=" << globalOpacityDecoded.mean
                  << " std=" << globalOpacityDecoded.stddev()
                  << " min=" << globalOpacityDecoded.min
                  << " max=" << globalOpacityDecoded.max
                  << " opacityIsLogit=" << (opacityIsLogit ? 1 : 0)
                  << std::endl;

        const size_t sampleCount = std::min<size_t>(5, cpuData.size());
        if (sampleCount > 0)
        {
            std::cout << "[debug-color-stats] ROUNDTRIP samples" << std::endl;
            for (size_t i = 0; i < sampleCount; ++i)
            {
                const glm::vec3 finalLin = glm::vec3(cpuData[i].color);
                const glm::vec3 dc = computeDcFromColor(finalLin, renderContext.dcMode);
                const glm::vec3 decSup = decodeDcSupersplat(dc);
                const glm::vec3 decRT = decodeDcRoundTrip(dc, renderContext.dcMode);
                glm::vec3 errSup = glm::abs(finalLin - decSup);
                glm::vec3 errRT = glm::abs(finalLin - decRT);
                std::cout << std::fixed << std::setprecision(3)
                          << "  s=" << i
                          << " final=" << formatVec3(finalLin)
                          << " decSup=" << formatVec3(decSup)
                          << " errSup=" << formatVec3(errSup)
                          << " decRT=" << formatVec3(decRT)
                          << " errRT=" << formatVec3(errRT)
                          << " dc=" << formatVec3(dc)
                          << std::endl;
            }
        }
    }


    float scaleMultiplier = renderContext.gaussianStd / static_cast<float>(renderContext.resolutionTarget);

    unsigned int format = exportFormat;
    parsers::DcMode dcMode = static_cast<parsers::DcMode>(renderContext.dcMode);
    parsers::OpacityMode opacityMode = static_cast<parsers::OpacityMode>(renderContext.opacityMode);

    std::string outputFileFinal = outputFile;
    if (renderContext.dcModeSpecified)
    {
        if (renderContext.dcMode == 1)
            outputFileFinal = appendSuffixBeforeExtension(outputFile, "_dc-directlin");
        else if (renderContext.dcMode == 2)
            outputFileFinal = appendSuffixBeforeExtension(outputFile, "_dc-directsrgb");
        else
            outputFileFinal = appendSuffixBeforeExtension(outputFile, "_dc-current");
    }

    parsers::saveSplatVector(outputFileFinal, cpuData, format, scaleMultiplier, dcMode, opacityMode);
}

void SceneManager::updateMeshes()
{
}

void SceneManager::cleanup()
{
}
