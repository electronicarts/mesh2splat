///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "utils.hpp"

namespace utils
{
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

    glm::vec3 getShFromColor(glm::vec3 color)
    {
        glm::vec3 sh = color - glm::vec3(0.5f) / SH_COEFF0;
        return glm::vec3(sh.x, sh.y, sh.z);
    }

    glm::vec3 getColorFromSh(glm::vec3 sh)
    {
        glm::vec3 col = sh * SH_COEFF0 + glm::vec3(0.5f);
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
        //TODO: should pass true width/height not with -1
        //TODO: consider bilinear filtering, why implement again all this when it is already supported in hardware...
        // Convert back to pixel coordinates
        int x = static_cast<int>(uv.x * textureWidth - 0.5f);
        int y = static_cast<int>(uv.y * textureHeight - 0.5f); //ATTENTION: remember to then swap the order of the pixels in the rgba_to_pos

        // Clamp the coordinates to ensure they're within the texture bounds
        // This accounts for potential rounding errors that might place the pixel outside the texture
        x = glm::clamp(x, 0, textureWidth - 1);
        y = glm::clamp(y, 0, textureHeight - 1);

        return glm::ivec2(x, y);        
    }

    std::pair<glm::vec2, glm::vec2> computeUVBoundingBox(const glm::vec2* triangleUVs) {
        if (triangleUVs == NULL) {
            return { glm::vec2(0, 0), glm::vec2(0, 0) };
        }

        float minU = triangleUVs[0].x, maxU = triangleUVs[0].x;
        float minV = triangleUVs[0].y, maxV = triangleUVs[0].y;

        for (int i = 0; i < 3; i++) {
            glm::vec2 uv = triangleUVs[i];
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

    glm::vec3 linear_to_srgb_float(glm::vec3 rgb) {
        return glm::vec3(linear_to_srgb_float(rgb.r), linear_to_srgb_float(rgb.g), linear_to_srgb_float(rgb.b));
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

    glm::vec3 srgb_to_linear_float(glm::vec3 rgb) {
        return glm::vec3(srgb_to_linear_float(rgb.r), srgb_to_linear_float(rgb.g), srgb_to_linear_float(rgb.b));
    }



    glm::vec4 rgbaAtPos(const int width, int X, int Y, unsigned char* rgb_image, const int bpp) {
        size_t index = (Y * width + X) * bpp;

        //TODO: remember to switch back to [ ] instead of .at once finished debugging
        float r = static_cast<float>(rgb_image[index]) / 255.0f;
        float g = static_cast<float>(rgb_image[index + 1]) / 255.0f;
        float b = static_cast<float>(rgb_image[index + 2]) / 255.0f;
        //float a = (bpp >= 4 && index + 3 < rgb_image.size()) ? (static_cast<float>(rgb_image[index + 3]) / 255.0f) : 1.0f;
        //TODO: what to do about opacity?

        return { r, g, b, 1.0f };
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

    //https://en.wikipedia.org/wiki/Cube_mapping
    void convert_xyz_to_cube_uv(float x, float y, float z, int* index, float* u, float* v)
    {
        float absX = fabs(x);
        float absY = fabs(y);
        float absZ = fabs(z);

        int isXPositive = x > 0 ? 1 : 0;
        int isYPositive = y > 0 ? 1 : 0;
        int isZPositive = z > 0 ? 1 : 0;

        float maxAxis, uc, vc;

        // POSITIVE X
        if (isXPositive && absX >= absY && absX >= absZ) {
            // u (0 to 1) goes from +z to -z
            // v (0 to 1) goes from -y to +y
            maxAxis = absX;
            uc = -z;
            vc = y;
            *index = 0;
        }
        // NEGATIVE X
        if (!isXPositive && absX >= absY && absX >= absZ) {
            // u (0 to 1) goes from -z to +z
            // v (0 to 1) goes from -y to +y
            maxAxis = absX;
            uc = z;
            vc = y;
            *index = 1;
        }
        // POSITIVE Y
        if (isYPositive && absY >= absX && absY >= absZ) {
            // u (0 to 1) goes from -x to +x
            // v (0 to 1) goes from +z to -z
            maxAxis = absY;
            uc = x;
            vc = -z;
            *index = 2;
        }
        // NEGATIVE Y
        if (!isYPositive && absY >= absX && absY >= absZ) {
            // u (0 to 1) goes from -x to +x
            // v (0 to 1) goes from -z to +z
            maxAxis = absY;
            uc = x;
            vc = z;
            *index = 3;
        }
        // POSITIVE Z
        if (isZPositive && absZ >= absX && absZ >= absY) {
            // u (0 to 1) goes from -x to +x
            // v (0 to 1) goes from -y to +y
            maxAxis = absZ;
            uc = x;
            vc = y;
            *index = 4;
        }
        // NEGATIVE Z
        if (!isZPositive && absZ >= absX && absZ >= absY) {
            // u (0 to 1) goes from +x to -x
            // v (0 to 1) goes from -y to +y
            maxAxis = absZ;
            uc = -x;
            vc = y;
            *index = 5;
        }

        // Convert range from -1 to 1 to 0 to 1
        *u = 0.5f * (uc / maxAxis + 1.0f);
        *v = 0.5f * (vc / maxAxis + 1.0f);
    }

    void convert_cube_uv_to_xyz(int index, float u, float v, float* x, float* y, float* z)
    {
        // convert range 0 to 1 to -1 to 1
        float uc = 2.0f * u - 1.0f;
        float vc = 2.0f * v - 1.0f;
        switch (index)
        {
            case 0: *x = 1.0f; *y = vc; *z = -uc; break;	// POSITIVE X
            case 1: *x = -1.0f; *y = vc; *z = uc; break;	// NEGATIVE X
            case 2: *x = uc; *y = 1.0f; *z = -vc; break;	// POSITIVE Y
            case 3: *x = uc; *y = -1.0f; *z = vc; break;	// NEGATIVE Y
            case 4: *x = uc; *y = vc; *z = 1.0f; break;	    // POSITIVE Z
            case 5: *x = -uc; *y = vc; *z = -1.0f; break;	// NEGATIVE Z
        }
    }

    glm::vec3 GenerateTangent(glm::vec3& normal) {
        glm::vec3 tangent;
        if (abs(normal.x) < abs(normal.z))
            tangent = glm::cross(normal, glm::vec3(0.0f, 0.0f, 0.0f));
        else
            tangent = glm::cross(normal, glm::vec3(1.0f, 1.0f, 1.0f));
        return glm::normalize(tangent);
    }

    glm::mat3 ConstructTBN(glm::vec3 normal) {
        glm::vec3 tangent = GenerateTangent(normal);
        glm::vec3 bitangent = glm::normalize(cross(normal, tangent));
        return glm::mat3(tangent, bitangent, glm::normalize(normal));
    }

    void computeAndLoadTextureInformation(std::map<std::string, std::pair<unsigned char*, int>>& textureTypeMap, MaterialGltf& material, const int x, const int y, glm::vec4& rgba, float& metallicFactor, float& roughnessFactor, glm::vec3& interpolatedNormal, glm::vec3& outputNormal, glm::vec4& interpolatedTangent)
    {
        float aoFactor = 1.0f;
        //TODO: I do not support yet the indirection to the texture component of the model, meaning I cannot take advantage of the samplers
        //TODO: "The first three components (RGB) MUST be encoded with the sRGB transfer function."
        if (textureTypeMap.find(BASE_COLOR_TEXTURE) != textureTypeMap.end()) //use texture for rgba
        {
            rgba = rgbaAtPos(
                material.baseColorTexture.width,
                x, y,
                textureTypeMap[BASE_COLOR_TEXTURE].first, textureTypeMap[BASE_COLOR_TEXTURE].second
            );
        }
        else { //use material for rgba
            rgba = material.baseColorFactor;
        }

        if (textureTypeMap.find(METALLIC_ROUGHNESS_TEXTURE) != textureTypeMap.end()) //use texture for rgba
        {
            glm::vec4 metallicRoughness = rgbaAtPos(
                material.metallicRoughnessTexture.width,
                x, y,
                textureTypeMap[METALLIC_ROUGHNESS_TEXTURE].first, textureTypeMap[METALLIC_ROUGHNESS_TEXTURE].second
            );
            metallicFactor = material.metallicFactor * metallicRoughness.b;
            roughnessFactor = material.roughnessFactor * metallicRoughness.g;
            if (metallicRoughness.r != 1.0f) {
                aoFactor = metallicRoughness.r;
                aoFactor = std::min(std::max(aoFactor, 0.0f), 1.0f);
            }
        }
        else {
            metallicFactor = material.metallicFactor;
            roughnessFactor = material.roughnessFactor;
        }

        if (textureTypeMap.find(NORMAL_TEXTURE) != textureTypeMap.end())
        {
            glm::vec3 rgba_normal_info(rgbaAtPos(
                material.normalTexture.width,
                x, y,
                textureTypeMap[NORMAL_TEXTURE].first, textureTypeMap[NORMAL_TEXTURE].second
            ));

            //rgba_normal_info = glm::vec3(srgb_to_linear_float(rgba_normal_info.x), srgb_to_linear_float(rgba_normal_info.y), srgb_to_linear_float(rgba_normal_info.z));
        
            if (!isnan(interpolatedTangent.x) && !isnan(interpolatedTangent.y) && !isnan(interpolatedTangent.z) && !isnan(interpolatedTangent.w))
            {
                glm::vec3 tangentXYZ(interpolatedTangent);
                glm::vec3 retrievedNormal = glm::normalize(glm::normalize(glm::vec3(rgba_normal_info) * 2.0f - 1.0f) * glm::vec3(material.normalScale, material.normalScale, 1.0f)); //https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_material_normaltextureinfo_scale
                glm::vec3 bitangent = glm::normalize(glm::cross(interpolatedNormal, tangentXYZ)) * interpolatedTangent.w; //tangent.w is the bitangent sign
                glm::mat3 TBN(tangentXYZ, bitangent, interpolatedNormal);
                outputNormal = TBN * retrievedNormal; //should transform in model space
            } else {
                outputNormal = interpolatedNormal;
            }
            //OK so now I have the interpolated tangent, the interpolated normal, and the TBN matrix.
            //Just a heads up for later debugging in case cant fix things: https://www.reddit.com/r/GraphicsProgramming/comments/z2khzc/comment/ixiw0se/ sRGB might break things
        }
        else { //use material for rgba
            rgba = material.baseColorFactor;
        }

        if (textureTypeMap.find(EMISSIVE_TEXTURE) != textureTypeMap.end())
        {
            glm::vec3 rgba_emissive(rgbaAtPos(
                material.emissiveTexture.width,
                x, y,
                textureTypeMap[EMISSIVE_TEXTURE].first, textureTypeMap[EMISSIVE_TEXTURE].second
            ));

            rgba += glm::vec4(rgba_emissive * material.emissiveFactor, 0.0f);

        }

        if (textureTypeMap.find(AO_TEXTURE) != textureTypeMap.end())
        {
            glm::vec3 aoVecFactor(rgbaAtPos(
                material.occlusionTexture.width,
                x, y,
                textureTypeMap[AO_TEXTURE].first, textureTypeMap[AO_TEXTURE].second
            ));

            if (aoVecFactor.r == 1.0f && aoVecFactor.g == 1.0f && aoVecFactor.b == 1.0f)
            {
                aoFactor = material.occlusionStrength;
            } else {
                if (aoVecFactor.r != 1.0f) {
                    aoFactor = aoVecFactor.r * material.occlusionStrength;
                } else if (aoVecFactor.g != 1.0f) {
                    aoFactor = aoVecFactor.g * material.occlusionStrength;
                } else if (aoVecFactor.b != 1.0f) {
                    aoFactor = aoVecFactor.b * material.occlusionStrength;
                }
            }
        }

        //rgba *= aoFactor;

    }

    bool shouldSkip(const GaussianDataSSBO& g) {
        bool skip = false;
        skip |= glm::any(glm::isnan(g.position)) || glm::any(glm::isinf(g.position));
        skip |= glm::any(glm::isnan(g.color))    || glm::any(glm::isinf(g.color));
        skip |= glm::any(glm::isnan(g.scale))    || glm::any(glm::isinf(g.scale));
        skip |= glm::any(glm::isnan(g.normal))   || glm::any(glm::isinf(g.normal));
        skip |= glm::any(glm::isnan(g.rotation)) || glm::any(glm::isinf(g.rotation));
        skip |= glm::any(glm::isnan(g.pbr))      || glm::any(glm::isinf(g.pbr));
        if (skip) return true;
    
        return (g.position == glm::vec4(0.0f) &&
                g.color    == glm::vec4(0.0f) &&
                g.scale    == glm::vec4(0.0f) &&
                g.normal   == glm::vec4(0.0f) &&
                g.rotation == glm::vec4(0.0f) &&
                g.pbr      == glm::vec4(0.0f));
    }

    std::string formatWithCommas(int value) {
        bool isNegative = false;
        std::string numStr;
    
        // Handle negative numbers
        if (value < 0) {
            isNegative = true;
            value = -value;
        }
    
        // Convert integer to string
        numStr = std::to_string(value);
    
        // Insert commas every three digits from the right
        int insertPosition = static_cast<int>(numStr.length()) - 3;
        while (insertPosition > 0) {
            numStr.insert(insertPosition, ",");
            insertPosition -= 3;
        }
    
        // Add negative sign back if necessary
        if (isNegative) {
            numStr = "-" + numStr;
        }
    
        return numStr;
    }

    ModelFileExtension getFileExtension(const std::string& filename)
    {
        size_t pos = filename.rfind('.');
        if (pos == std::string::npos)
            return ModelFileExtension::NONE;

        std::string ext = filename.substr(pos+1);

        if (ext == "glb") return ModelFileExtension::GLB;
        else if (ext == "ply") return ModelFileExtension::PLY;

        return ModelFileExtension::NONE;
    }

    float triangleArea(const glm::vec3& A, const glm::vec3& B, const glm::vec3& C) {
        return 0.5 * std::abs(
            A.x * (B.y - C.y) +
            B.x * (C.y - A.y) +
            C.x * (A.y - B.y)
        );
    }

    std::string getExecutablePath() {
        char buffer[MAX_PATH];
        GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        return std::string(buffer);
    }

    std::string getExecutableDir() {
        std::experimental::filesystem::path exePath(getExecutablePath());
        return exePath.parent_path().string();
    }

    //https://stackoverflow.com/questions/63899489/c-experimental-filesystem-has-no-relative-function
    fs::path relative(fs::path p, fs::path base)
    {
        p = fs::absolute(p);
        base = fs::absolute(base);

        auto mismatched = std::mismatch(p.begin(), p.end(), base.begin(), base.end());

        if (mismatched.first == p.end() && mismatched.second == base.end())
            return ".";

        auto it_p = mismatched.first;
        auto it_base = mismatched.second;

        fs::path ret;

        for (; it_base != base.end(); ++it_base)  ret /= ".."; 

        for (; it_p != p.end(); ++it_p) ret /= *it_p; 

        return ret;
    }
}