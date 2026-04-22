///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "parsers.hpp"
#include <cmath>
#include <algorithm>

namespace parsers
{
    // Helper function: compute DC (color) based on DcMode
    static glm::vec3 computeDcFromColor(const glm::vec3& colorLinear, DcMode dcMode)
    {
        switch (dcMode)
        {
            case DcMode::DirectLinear:
                return colorLinear;
            case DcMode::DirectSrgb:
                return utils::linear_to_srgb_float(colorLinear);
            case DcMode::Current:
            default:
                return utils::getShFromColor(colorLinear);
        }
    }

    // Helper function: encode opacity based on OpacityMode
    static float encodeOpacity(float opacityLinear, OpacityMode mode, bool defaultLogit)
    {
        bool useLogit = defaultLogit;
        if (mode == OpacityMode::Raw) useLogit = false;
        else if (mode == OpacityMode::Logit) useLogit = true;

        if (!useLogit)
            return opacityLinear;

        // Inverse sigmoid with clamping for numerical stability
        auto invSigmoid = [](float a) -> float {
            const float eps = 1e-6f;
            if (!std::isfinite(a)) a = 0.5f;
            a = std::clamp(a, eps, 1.0f - eps);
            return std::log(a / (1.0f - a));
        };

        return invSigmoid(opacityLinear);
    }

    // Safe log function to avoid -inf
    static float safeLog(float v)
    {
        const float eps = 1e-12f;
        if (!std::isfinite(v) || v <= eps) v = eps;
        return std::log(v);
    } 
    //TODO: Careful to remember that the image is saved with its original name, if you change filename after 
    utils::TextureDataGl loadImageAndBpp(std::string texturePath, int& textureWidth, int& textureHeight) 
    {
        size_t pos = texturePath.rfind('.');
        std::string image_format = texturePath.substr(pos + 1);
        int bpp;
        unsigned char* image = stbi_load(texturePath.c_str(), &textureWidth, &textureHeight, &bpp, 0);

        if (image == NULL) {
            std::cout << "Error in loading the image, cannot find " << texturePath << "\n" << std::endl;
            exit(1);
        }

        std::cout << "Image: " << texturePath << "  width: " << textureWidth << "  height: " << textureHeight << " " << bpp << std::endl;
        std::string filePath(texturePath);
        std::string parentFolder = "";
        if (!filePath.empty()) {
            size_t lastSlashPos = filePath.find_last_of("/\\");
            if (lastSlashPos != std::string::npos) {
                parentFolder = filePath.substr(0, lastSlashPos + 1);
            }
            else {
                parentFolder = "";
            }
        }
        std::string resized_texture_name_location = parentFolder + std::string("resized_texture") + texturePath + "." + image_format;
        float aspect_ratio = (float)textureHeight / (float)textureWidth;

        if (textureWidth > MAX_RESOLUTION_TARGET)
        {
            // Specify new width and height
            int new_width = MAX_RESOLUTION_TARGET;
            int new_height = static_cast<int>(new_width * aspect_ratio);
    
            // Allocate memory for the resized image, use new not malloc
            unsigned char* resized_data = new unsigned char[new_width * new_height * bpp];
    
            // Resize the image
            stbir_resize_uint8(image, textureWidth, textureHeight, 0, resized_data, new_width, new_height, 0, bpp);
            textureWidth = new_width;
            textureHeight = new_height;
    
            // Free the original image
            stbi_image_free(image);
            std::cout << "\nImage: " << resized_texture_name_location << "  width: " << textureWidth << "  height: " << textureHeight << " BPP:" << bpp << "\n" << std::endl;
    
            // Convert resized_data to vector and return
            std::vector<unsigned char> resizedVec(resized_data, resized_data + new_width * new_height * bpp);
            delete[] resized_data;
            return utils::TextureDataGl(std::move(resizedVec), bpp);
            
        }
        
        // Convert original image to vector and return
        std::vector<unsigned char> imageVec(image, image + textureWidth * textureHeight * bpp);
        stbi_image_free(image);
        return utils::TextureDataGl(std::move(imageVec), bpp);
    }

    //Factors taken from: https://gist.github.com/SubhiH/b34e74ffe4fd1aab046bcf62b7f12408
    static void convertToGrayscale(const unsigned char* src, unsigned char* dst, int width, int height, int channels) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int index = (y * width + x) * channels;
                unsigned char gray;
                if (channels >= 3) {
                    unsigned char r = src[index + 0];
                    unsigned char g = src[index + 1];
                    unsigned char b = src[index + 2];
                    gray = static_cast<unsigned char>(0.11 * r + 0.59 * g + 0.3 * b);
                } else {
                    // For 1 or 2 channel images, just use the first channel
                    gray = src[index];
                }
                dst[y * width + x] = gray;
            }
        }
    }

    unsigned char* combineMetallicRoughness(const char* path1, const char* path2, int& width, int& height, int& channels) {
        int width1, height1, channels1;
        int width2, height2, channels2;

        // Load the first image
        unsigned char* img1 = stbi_load(path1, &width1, &height1, &channels1, 0);
        if (!img1) {
            std::cerr << "Error: Could not load the first image" << std::endl;
            return nullptr;
        }

        // Load the second image
        unsigned char* img2 = stbi_load(path2, &width2, &height2, &channels2, 0);
        if (!img2) {
            std::cerr << "Error: Could not load the second image" << std::endl;
            stbi_image_free(img1);
            return nullptr;
        }

        // Ensure the images are the same size
        if (width1 != width2 || height1 != height2) {
            std::cerr << "Error: Images must be the same size" << std::endl;
            stbi_image_free(img1);
            stbi_image_free(img2);
            return nullptr;
        }

        width = width1;
        height = height1;
        channels = 3;

        unsigned char* gray1 = new unsigned char[width * height]; //should be metallic
        unsigned char* gray2 = new unsigned char[width * height]; //should be roughness

        // Convert the images to grayscale
        convertToGrayscale(img1, gray1, width, height, channels1);
        convertToGrayscale(img2, gray2, width, height, channels2);

        // Create the result image buffer
        unsigned char* result = new unsigned char[width * height * channels]();

        // Populate the result image
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int index = (y * width + x);
                result[index * channels + 0] = 0;
                result[index * channels + 1] = gray2[index]; // Green channel set from roughness
                result[index  * channels + 2] = gray1[index]; // Blue channel set from metallic
            }
        }

        // Free the images
        stbi_image_free(img1);
        stbi_image_free(img2);
        delete[] gray1;
        delete[] gray2;

        return result;
    }

    // Function to split the string based on a delimiter
    std::vector<std::string> splitString(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;
        while (std::getline(ss, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    bool hasValidImageExtension(const std::string& filename) {
        const std::vector<std::string> validExtensions = { ".png", ".jpg", ".jpeg", ".bmp", ".tga" };
        for (const std::string& ext : validExtensions) {
            if (filename.size() >= ext.size() &&
                std::equal(ext.rbegin(), ext.rend(), filename.rbegin())) {
                return true;
            }
        }
        return false;
    }

    // Function to check if the image name is a combined name and extract the individual names
    bool extractImageNames(const std::string& combinedName, std::string& path, std::string& name1, std::string& name2) {
        // Check if the combined name contains a hyphen
        if (combinedName.find('-') == std::string::npos) {
            return false; // Not a combined name
        }

        // Find the position of the last slash to extract the path
        size_t lastSlashPos = combinedName.find_last_of("/\\");
        if (lastSlashPos == std::string::npos) {
            return false; // No valid path found
        }

        path = combinedName.substr(0, lastSlashPos + 1); // Include the slash
        std::string filenames = combinedName.substr(lastSlashPos + 1);

        // Split the combined filename at the hyphen
        std::vector<std::string> names = splitString(filenames, '-');
        if (names.size() != 2) {
            return false; // Unexpected format
        }

        // Remove the double extension from the end of the second part
        size_t lastDotPos = names[1].find_last_of('.');
        if (lastDotPos == std::string::npos) {
            return false; // No extension found
        }

        std::string extensionPart = names[1].substr(lastDotPos);
        if (!hasValidImageExtension(extensionPart)) {
            return false; // The final part does not have a valid image extension
        }

        //names[1] = names[1].substr(0, lastDotPos);

        // Check if both parts have valid image extensions
        if (!hasValidImageExtension(names[0])) {
            names[0] += ".png";
        }
        if (!hasValidImageExtension(names[1])) {
            names[1] += ".png";
        }

        name1 = path + names[0];
        name2 = path + names[1];

        return true;
    }

    glm::vec3 computeNormal(glm::vec3 A, glm::vec3 B, glm::vec3 C) {
        return glm::normalize(glm::cross(B - A, C - A));
    }

    glm::vec3 projectOntoPlane(glm::vec3 point, glm::vec3 normal) {
        glm::mat3 I = glm::mat3(1.0f);  // Identity matrix
        glm::mat3 outerProduct = glm::outerProduct(normal, normal);
        glm::mat3 projectionMatrix = I - outerProduct;
        return projectionMatrix * point;
    }

    std::vector<glm::vec2> projectMeshVertices(const std::vector<glm::vec3>& vertices, glm::vec3 normal) {
        std::vector<glm::vec2> projectedVertices;
        for (const auto& vertex : vertices) {
            glm::vec3 projectedVertex3D = projectOntoPlane(vertex, normal);
            // Assuming projection onto XY plane for 2D use in Triangle
            projectedVertices.push_back(glm::vec2(projectedVertex3D.x, projectedVertex3D.y));
        }
        return projectedVertices;
    }

    void writePbrPLY(const std::string& filename, std::vector<utils::GaussianDataSSBO>& gaussians, float scaleMultiplier, DcMode dcMode, OpacityMode opacityMode, bool flipY) {
        std::ofstream file(filename, std::ios::binary | std::ios::out);

        // Write header in ASCII
        file << "ply\n";
        file << "format binary_little_endian 1.0\n";
        file << "element vertex " << gaussians.size() << "\n";

        file << "property float x\n";       //0
        file << "property float y\n";       //1
        file << "property float z\n";       //2

        file << "property float nx\n";      //3
        file << "property float ny\n";      //4
        file << "property float nz\n";      //5

        file << "property float f_dc_0\n";  //6
        file << "property float f_dc_1\n";  //7
        file << "property float f_dc_2\n";  //8

        file << "property float metallicFactor\n";  //9
        file << "property float roughnessFactor\n"; //10

        file << "property float opacity\n";         //11

        file << "property float scale_0\n";         //12
        file << "property float scale_1\n";         //13
        file << "property float scale_2\n";         //14

        file << "property float rot_0\n";           //15
        file << "property float rot_1\n";           //16
        file << "property float rot_2\n";           //17
        file << "property float rot_3\n";           //18

        file << "end_header\n";

        // Write vertex data in binary
        for (const auto& gaussian : gaussians) {
            // Mean (position) - 180° rotation around X axis negates Y and Z
            float posX = gaussian.position.x;
            float posY = flipY ? -gaussian.position.y : gaussian.position.y;
            float posZ = flipY ? -gaussian.position.z : gaussian.position.z;
            file.write(reinterpret_cast<const char*>(&posX), sizeof(posX));
            file.write(reinterpret_cast<const char*>(&posY), sizeof(posY));
            file.write(reinterpret_cast<const char*>(&posZ), sizeof(posZ));
            
            // Normal - 180° rotation around X axis negates Y and Z
            float normX = gaussian.normal.x;
            float normY = flipY ? -gaussian.normal.y : gaussian.normal.y;
            float normZ = flipY ? -gaussian.normal.z : gaussian.normal.z;
            file.write(reinterpret_cast<const char*>(&normX), sizeof(normX));
            file.write(reinterpret_cast<const char*>(&normY), sizeof(normY));
            file.write(reinterpret_cast<const char*>(&normZ), sizeof(normZ));
            
            //RGB - using configurable DC mode
            glm::vec3 sh0 = computeDcFromColor(glm::vec3(gaussian.color), dcMode);
            
            file.write(reinterpret_cast<const char*>(&sh0.x), sizeof(sh0.x));
            file.write(reinterpret_cast<const char*>(&sh0.y), sizeof(sh0.y));
            file.write(reinterpret_cast<const char*>(&sh0.z), sizeof(sh0.z));

            //---------NEW-----------------------------------------------------
            //Material properties
            file.write(reinterpret_cast<const char*>(&gaussian.pbr.x), sizeof(gaussian.pbr.x));
            file.write(reinterpret_cast<const char*>(&gaussian.pbr.y), sizeof(gaussian.pbr.y));
            //-----------------------------------------------------------------

            //Opacity - using configurable opacity mode (default uses logit)
            float opacityOut = encodeOpacity(gaussian.color.w, opacityMode, true);
            file.write(reinterpret_cast<const char*>(&opacityOut), sizeof(opacityOut));

            // Scale: write log(linearScale * scaleMultiplier), but clamp to avoid -inf
            glm::vec3 packedScale;
            packedScale.x = safeLog(gaussian.linearScale.x * scaleMultiplier);
            packedScale.y = safeLog(gaussian.linearScale.y * scaleMultiplier);
            packedScale.z = safeLog(gaussian.linearScale.z * scaleMultiplier);

            file.write(reinterpret_cast<const char*>(&packedScale.x), sizeof(packedScale.x));
            file.write(reinterpret_cast<const char*>(&packedScale.y), sizeof(packedScale.y));
            file.write(reinterpret_cast<const char*>(&packedScale.z), sizeof(packedScale.z));
            
            // Rotation - 180° around X: q_flip(0,1,0,0) * q = (-x, w, -z, y)
            float rotW, rotX, rotY, rotZ;
            if (flipY) {
                rotW = -gaussian.rotation.x;
                rotX =  gaussian.rotation.w;
                rotY = -gaussian.rotation.z;
                rotZ =  gaussian.rotation.y;
            } else {
                rotW = gaussian.rotation.w;
                rotX = gaussian.rotation.x;
                rotY = gaussian.rotation.y;
                rotZ = gaussian.rotation.z;
            }
            file.write(reinterpret_cast<const char*>(&rotW), sizeof(rotW));
            file.write(reinterpret_cast<const char*>(&rotX), sizeof(rotX));
            file.write(reinterpret_cast<const char*>(&rotY), sizeof(rotY));
            file.write(reinterpret_cast<const char*>(&rotZ), sizeof(rotZ));
        }
        file.close();
    }

    //https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/ adapted from hlsl
    //Should do it in shader...
    inline glm::vec2 OctWrap(const glm::vec2& v)
    {
        glm::vec2 result;
        result.x = (1.0f - std::abs(v.y)) * (v.x >= 0.0f ? 1.0f : -1.0f);
        result.y = (1.0f - std::abs(v.x)) * (v.y >= 0.0f ? 1.0f : -1.0f);
        return result;
    }

    inline glm::vec2 EncodeOcta(const glm::vec3& normal)
    {
        glm::vec3 n = normal / (glm::abs(normal.x) + glm::abs(normal.y) + glm::abs(normal.z) + 1e-8f);
        glm::vec2 resN;

        resN = n.z >= 0.0f ? glm::vec2(n.x, n.y) : OctWrap(glm::vec2(n.x, n.y));

        resN.x = resN.x * 0.5f + 0.5f;
        resN.y = resN.y * 0.5f + 0.5f;

        return glm::vec2(resN.x, resN.y);
    }

    void writeCompressedPbrPLY(const std::string& filename, std::vector<utils::GaussianDataSSBO>& gaussians, float scaleMultiplier, bool flipY) {
        std::ofstream file(filename, std::ios::binary | std::ios::out);

        file << "ply\n";
        file << "format binary_little_endian 1.0\n";
        file << "element vertex " << gaussians.size() << "\n";

        file << "property float x\n";
        file << "property float y\n";
        file << "property float z\n";

        file << "property uint8 red\n";  
        file << "property uint8 green\n";
        file << "property uint8 blue\n"; 
        file << "property uint8 opacity\n";

        file << "property float rot_0\n";           
        file << "property float rot_1\n";           
        file << "property float rot_2\n";           
        file << "property float rot_3\n";           

        file << "property float scale_0\n";         
        file << "property float scale_1\n";         
        file << "property float scale_2\n";   

        file << "property uint8 octa_nx\n";      
        file << "property uint8 octa_ny\n";      

        file << "property uint8 roughness\n";  
        file << "property uint8 metallic\n";        

        file << "end_header\n";

        auto toByte = [](float v)
        {
            float clamped = glm::clamp(v, 0.0f, 1.0f);
            float rounded = std::round(clamped * 255.0f);
            return static_cast<uint8_t>(rounded);
        };
        
        // Write vertex data in binary
        for (const auto& gaussian : gaussians) {
            // POSITION - 180° rotation around X axis negates Y and Z
            float posX = gaussian.position.x;
            float posY = flipY ? -gaussian.position.y : gaussian.position.y;
            float posZ = flipY ? -gaussian.position.z : gaussian.position.z;
            file.write(reinterpret_cast<const char*>(&posX), sizeof(float));
            file.write(reinterpret_cast<const char*>(&posY), sizeof(float));
            file.write(reinterpret_cast<const char*>(&posZ), sizeof(float));
            
            //COLOR
            uint8_t r = toByte(gaussian.color.x);
            uint8_t g = toByte(gaussian.color.y);
            uint8_t b = toByte(gaussian.color.z);
            uint8_t a = toByte(gaussian.color.w);
            file.write(reinterpret_cast<const char*>(&r), sizeof(uint8_t));
            file.write(reinterpret_cast<const char*>(&g), sizeof(uint8_t));
            file.write(reinterpret_cast<const char*>(&b), sizeof(uint8_t));
            file.write(reinterpret_cast<const char*>(&a), sizeof(uint8_t));

            // ROTATION - 180° around X: q_flip(0,1,0,0) * q = (-x, w, -z, y)
            float rotW, rotX, rotY, rotZ;
            if (flipY) {
                rotW = -gaussian.rotation.x;
                rotX =  gaussian.rotation.w;
                rotY = -gaussian.rotation.z;
                rotZ =  gaussian.rotation.y;
            } else {
                rotW = gaussian.rotation.w;
                rotX = gaussian.rotation.x;
                rotY = gaussian.rotation.y;
                rotZ = gaussian.rotation.z;
            }
            file.write(reinterpret_cast<const char*>(&rotW), sizeof(float));
            file.write(reinterpret_cast<const char*>(&rotX), sizeof(float));
            file.write(reinterpret_cast<const char*>(&rotY), sizeof(float));
            file.write(reinterpret_cast<const char*>(&rotZ), sizeof(float));

            //SCALE - using linearScale
            glm::vec3 packedScale;
            float minXY = std::min(gaussian.linearScale.x, gaussian.linearScale.y);
            packedScale.x = safeLog(gaussian.linearScale.x * scaleMultiplier);
            packedScale.y = safeLog(gaussian.linearScale.y * scaleMultiplier);
            packedScale.z = safeLog(minXY * scaleMultiplier);           

            file.write(reinterpret_cast<const char*>(&packedScale.x), sizeof(float));
            file.write(reinterpret_cast<const char*>(&packedScale.y), sizeof(float));
            file.write(reinterpret_cast<const char*>(&packedScale.z), sizeof(float));

            //NORMAL COMPRESSED - apply 180° X rotation to normal before encoding
            glm::vec3 normal = gaussian.normal;
            if (flipY) {
                normal.y = -normal.y;
                normal.z = -normal.z;
            }
            glm::vec2 mapped = EncodeOcta(normal);
            uint8_t nx = static_cast<uint8_t>(glm::clamp(std::roundf(mapped.x * 255.0f), 0.0f, 255.0f));
            uint8_t ny = static_cast<uint8_t>(glm::clamp(std::roundf(mapped.y * 255.0f), 0.0f, 255.0f));
            file.write(reinterpret_cast<const char*>(&nx), sizeof(uint8_t));
            file.write(reinterpret_cast<const char*>(&ny), sizeof(uint8_t));

            //PBR PROPS
            uint8_t roughness = toByte(gaussian.pbr.y); // I assume [0,1] -> [0,255]
            uint8_t metallic  = toByte(gaussian.pbr.x);  

            file.write(reinterpret_cast<const char*>(&roughness), sizeof(uint8_t));
            file.write(reinterpret_cast<const char*>(&metallic),  sizeof(uint8_t));

        }
        file.close();
    }


    void writeBinaryPlyStandardFormat(const std::string& filename, const std::vector<utils::GaussianDataSSBO>& gaussians, float scaleMultiplier, DcMode dcMode, OpacityMode opacityMode, bool flipY) {
        std::ofstream file(filename, std::ios::binary | std::ios::out);
        //TODO: abstract this somehow
        // Write header in ASCII
        file << "ply\n";
        file << "format binary_little_endian 1.0\n";
        file << "element vertex " << gaussians.size() << "\n";

        file << "property float x\n";       // 0
        file << "property float y\n";       // 1
        file << "property float z\n";       // 2

        file << "property float nx\n";      // 3
        file << "property float ny\n";      // 4
        file << "property float nz\n";      // 5

        file << "property float f_dc_0\n";  // 6
        file << "property float f_dc_1\n";  // 7
        file << "property float f_dc_2\n";  // 8

        for (int i = 0; i <= 44; ++i) {
            file << "property float f_rest_" << i << "\n"; // f_rest_0 to f_rest_44
        }

        file << "property float opacity\n"; // 45

        file << "property float scale_0\n"; // 46
        file << "property float scale_1\n"; // 47
        file << "property float scale_2\n"; // 48

        file << "property float rot_0\n";   // 49
        file << "property float rot_1\n";   // 50
        file << "property float rot_2\n";   // 51
        file << "property float rot_3\n";   // 52

        file << "end_header\n";

        // Write vertex data in binary
        for (const auto& gaussian : gaussians) {
            // Mean (position) - 180° rotation around X axis negates Y and Z
            float posX = gaussian.position.x;
            float posY = flipY ? -gaussian.position.y : gaussian.position.y;
            float posZ = flipY ? -gaussian.position.z : gaussian.position.z;
            file.write(reinterpret_cast<const char*>(&posX), sizeof(posX));
            file.write(reinterpret_cast<const char*>(&posY), sizeof(posY));
            file.write(reinterpret_cast<const char*>(&posZ), sizeof(posZ));

            // Normal - 180° rotation around X axis negates Y and Z
            float normX = gaussian.normal.x;
            float normY = flipY ? -gaussian.normal.y : gaussian.normal.y;
            float normZ = flipY ? -gaussian.normal.z : gaussian.normal.z;
            file.write(reinterpret_cast<const char*>(&normX), sizeof(normX));
            file.write(reinterpret_cast<const char*>(&normY), sizeof(normY));
            file.write(reinterpret_cast<const char*>(&normZ), sizeof(normZ));
            
            // RGB - using configurable DC mode
            glm::vec3 sh0 = computeDcFromColor(glm::vec3(gaussian.color), dcMode);
            
            file.write(reinterpret_cast<const char*>(&sh0.x), sizeof(sh0.x));
            file.write(reinterpret_cast<const char*>(&sh0.y), sizeof(sh0.y));
            file.write(reinterpret_cast<const char*>(&sh0.z), sizeof(sh0.z));

            // Fill f_rest_0 to f_rest_44 with zeros
            float zero = 0.0f;
            for (int i = 0; i <= 44; ++i) {
                file.write(reinterpret_cast<const char*>(&zero), sizeof(zero));
            }

            // Opacity - using configurable opacity mode (default uses logit)
            float opacityOut = encodeOpacity(gaussian.color.w, opacityMode, true);
            file.write(reinterpret_cast<const char*>(&opacityOut), sizeof(opacityOut));

            // Scale: write log(linearScale * scaleMultiplier), but clamp to avoid -inf
            glm::vec3 packedScale;
            packedScale.x = safeLog(gaussian.linearScale.x * scaleMultiplier);
            packedScale.y = safeLog(gaussian.linearScale.y * scaleMultiplier);
            packedScale.z = safeLog(gaussian.linearScale.z * scaleMultiplier);

            // Scale
            file.write(reinterpret_cast<const char*>(&packedScale.x), sizeof(packedScale.x));
            file.write(reinterpret_cast<const char*>(&packedScale.y), sizeof(packedScale.y));
            file.write(reinterpret_cast<const char*>(&packedScale.z), sizeof(packedScale.z));

            // Rotation - 180° around X: q_flip(0,1,0,0) * q = (-x, w, -z, y)
            float rotW, rotX, rotY, rotZ;
            if (flipY) {
                rotW = -gaussian.rotation.x;
                rotX =  gaussian.rotation.w;
                rotY = -gaussian.rotation.z;
                rotZ =  gaussian.rotation.y;
            } else {
                rotW = gaussian.rotation.w;
                rotX = gaussian.rotation.x;
                rotY = gaussian.rotation.y;
                rotZ = gaussian.rotation.z;
            }
            file.write(reinterpret_cast<const char*>(&rotW), sizeof(rotW));
            file.write(reinterpret_cast<const char*>(&rotX), sizeof(rotX));
            file.write(reinterpret_cast<const char*>(&rotY), sizeof(rotY));
            file.write(reinterpret_cast<const char*>(&rotZ), sizeof(rotZ));
        }

        file.close();
    }

    void loadPlyFile(std::string plyFileLocation, std::vector<utils::GaussianDataSSBO>& gaussians)
    {
        try {
            happly::PLYData plyIn(plyFileLocation);
            std::vector<float> vertex_x = plyIn.getElement("vertex").getProperty<float>("x");
            std::vector<float> vertex_y = plyIn.getElement("vertex").getProperty<float>("y");
            std::vector<float> vertex_z = plyIn.getElement("vertex").getProperty<float>("z");
        
            std::vector<float> vertex_nx = plyIn.getElement("vertex").getProperty<float>("nx");
            std::vector<float> vertex_ny = plyIn.getElement("vertex").getProperty<float>("ny");
            std::vector<float> vertex_nz = plyIn.getElement("vertex").getProperty<float>("nz");

            std::vector<float> vertex_f_dc_0 = plyIn.getElement("vertex").getProperty<float>("f_dc_0");
            std::vector<float> vertex_f_dc_1 = plyIn.getElement("vertex").getProperty<float>("f_dc_1");
            std::vector<float> vertex_f_dc_2 = plyIn.getElement("vertex").getProperty<float>("f_dc_2");

            //TODO: skipping SHs for now
        
            std::vector<float> vertex_opacity = plyIn.getElement("vertex").getProperty<float>("opacity");
        
            std::vector<float> vertex_scale_0 = plyIn.getElement("vertex").getProperty<float>("scale_0");
            std::vector<float> vertex_scale_1 = plyIn.getElement("vertex").getProperty<float>("scale_1");
            std::vector<float> vertex_scale_2 = plyIn.getElement("vertex").getProperty<float>("scale_2");

            std::vector<float> vertex_rot_0 = plyIn.getElement("vertex").getProperty<float>("rot_0");
            std::vector<float> vertex_rot_1 = plyIn.getElement("vertex").getProperty<float>("rot_1");
            std::vector<float> vertex_rot_2 = plyIn.getElement("vertex").getProperty<float>("rot_2");
            std::vector<float> vertex_rot_3 = plyIn.getElement("vertex").getProperty<float>("rot_3");

            size_t numVertices = vertex_x.size();
            if (vertex_y.size() != numVertices || vertex_z.size() != numVertices ||
                vertex_nx.size() != numVertices || vertex_ny.size() != numVertices ||
                vertex_nz.size() != numVertices || vertex_f_dc_0.size() != numVertices ||
                vertex_f_dc_1.size() != numVertices || vertex_f_dc_2.size() != numVertices ||
                vertex_opacity.size() != numVertices || vertex_scale_0.size() != numVertices ||
                vertex_scale_1.size() != numVertices || vertex_scale_2.size() != numVertices ||
                vertex_rot_0.size() != numVertices || vertex_rot_1.size() != numVertices ||
                vertex_rot_2.size() != numVertices || vertex_rot_3.size() != numVertices) {
                throw std::runtime_error("Inconsistent vertex property sizes in PLY file.");
            }

            gaussians.clear();
            gaussians.reserve(numVertices);

            utils::GaussianDataSSBO gaussian;

            for (size_t i = 0; i < numVertices; ++i) {
                // Set position (w = 1.0 for homogeneous coordinates)
                gaussian.position.x = vertex_x[i];
                gaussian.position.y = vertex_y[i];
                gaussian.position.z = vertex_z[i];
                gaussian.position.w = 1.0f;

                glm::vec3 rgb_color = utils::getColorFromSh(glm::vec3(vertex_f_dc_0[i], vertex_f_dc_1[i], vertex_f_dc_2[i]));
                gaussian.color.x = rgb_color.x;
                gaussian.color.y = rgb_color.y;
                gaussian.color.z = rgb_color.z;
                gaussian.color.w = utils::sigmoid(vertex_opacity[i]);

                // linearScale stores linear-space scale values
                gaussian.linearScale.x = glm::exp(vertex_scale_0[i]);
                gaussian.linearScale.y = glm::exp(vertex_scale_1[i]);
                gaussian.linearScale.z = glm::exp(vertex_scale_2[i]);
                gaussian.linearScale.w = 1.0f;

                gaussian.normal.x = vertex_nx[i];
                gaussian.normal.y = vertex_ny[i];
                gaussian.normal.z = vertex_nz[i];
                gaussian.normal.w = 0.0f;

                glm::quat rot = glm::quat(vertex_rot_0[i], vertex_rot_1[i], vertex_rot_2[i], vertex_rot_3[i]);
                rot = glm::normalize(rot);
                gaussian.rotation.x = rot.w;
                gaussian.rotation.y = rot.x;
                gaussian.rotation.z = rot.y;
                gaussian.rotation.w = rot.z;

                gaussian.pbr = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

                gaussians.push_back(gaussian);
            }
        } catch (const std::exception& exc) {
            std::cerr << exc.what();
        }
        
    }

    void savePlyVector(std::string outputFileLocation, std::vector<utils::GaussianDataSSBO>&& gaussians_3D_list, unsigned int FORMAT, float scaleMultiplier, DcMode dcMode, OpacityMode opacityMode, bool flipY)
    {
        switch (FORMAT)
        {
            case 0:
                writeBinaryPlyStandardFormat(outputFileLocation, gaussians_3D_list, scaleMultiplier, dcMode, opacityMode, flipY);
                break;
    
            case 1:
                writePbrPLY(outputFileLocation, gaussians_3D_list, scaleMultiplier, dcMode, opacityMode, flipY);
                break;

            case 2:
                writeCompressedPbrPLY(outputFileLocation, gaussians_3D_list, scaleMultiplier, flipY);
                break;
    
            default:
                writeBinaryPlyStandardFormat(outputFileLocation, gaussians_3D_list, scaleMultiplier, dcMode, opacityMode, flipY);
                break;
        }
    }
}