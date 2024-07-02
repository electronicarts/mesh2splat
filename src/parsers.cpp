#include "parsers.hpp"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../thirdParty/tiny_gltf.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../thirdParty/stb_image_resize.h"

//TODO: Careful to remember that the image is saved with its original name, if you change filename after 
TextureDataGl loadImageAndBpp(std::string texturePath, int& textureWidth, int& textureHeight) //TODO: structs structs!
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

    std::string resized_texture_name_location = BASE_DATASET_FOLDER + std::string("resized_texture") + texturePath + "." + image_format;
    float aspect_ratio = (float)textureHeight / (float)textureWidth;

    if (textureWidth > RESOLUTION_TARGET)
    {
        // Specify new width and height

        int new_width = RESOLUTION_TARGET;
        int new_height = static_cast<int>(new_width * aspect_ratio);

        // Allocate memory for the resized image, use new not malloc
        unsigned char* resized_data = (unsigned char*)malloc(new_width * new_height * bpp);

        // Resize the image
        stbir_resize_uint8(image, textureWidth, textureHeight, 0, resized_data, new_width, new_height, 0, bpp);
        textureWidth = new_width;
        textureHeight = new_height;

        // Save the resized image
        //stbi_write_png(resized_texture_name_location.c_str(), new_width, new_height, bpp, resized_data, new_width * bpp);
        stbi_image_free(image);
        std::cout << "\nImage: " << resized_texture_name_location << "  width: " << textureWidth << "  height: " << textureHeight << " BPP:" << bpp << "\n" << std::endl;

        return TextureDataGl(resized_data, bpp);
       
    }

    return TextureDataGl(image, bpp);
}

//Factors taken from: https://gist.github.com/SubhiH/b34e74ffe4fd1aab046bcf62b7f12408
void convertToGrayscale(const unsigned char* src, unsigned char* dst, int width, int height, int channels) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int index = (y * width + x) * channels;
            unsigned char r = src[index + 0];
            unsigned char g = src[index + 1];
            unsigned char b = src[index + 2];
            unsigned char gray = static_cast<unsigned char>(0.11 * r + 0.59 * g + 0.3 * b);
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

void loadAllTexturesIntoMap(MaterialGltf& material, std::map<std::string, TextureDataGl>& textureTypeMap)
{
    
    //BASECOLOR ALBEDO TEXTURE LOAD
    if (material.baseColorTexture.path != EMPTY_TEXTURE)
    {
        textureTypeMap.emplace(BASE_COLOR_TEXTURE, loadImageAndBpp(material.baseColorTexture.path, material.baseColorTexture.width, material.baseColorTexture.height));
    }
    else {
        material.baseColorTexture.width = RESOLUTION_TARGET;
        material.baseColorTexture.height = RESOLUTION_TARGET;
    }

    //METALLIC-ROUGHNESS TEXTURE LOAD
    if (material.metallicRoughnessTexture.path != EMPTY_TEXTURE)
    {
        std::string metallicPath, roughnessPath, folderPath;
        //In case Metallic and Roughness are separate we need to combine them in the appropriate RGB channels
        if (extractImageNames(material.metallicRoughnessTexture.path, folderPath, metallicPath, roughnessPath))
        {
            int channels;
            unsigned char* metallicRoughnessTextureData = combineMetallicRoughness(metallicPath.c_str(), roughnessPath.c_str(), material.metallicRoughnessTexture.width, material.metallicRoughnessTexture.height, channels); 
            textureTypeMap.emplace(METALLIC_ROUGHNESS_TEXTURE, TextureDataGl(metallicRoughnessTextureData, channels));
        }
        else {
            textureTypeMap.emplace(METALLIC_ROUGHNESS_TEXTURE, loadImageAndBpp(material.metallicRoughnessTexture.path, material.metallicRoughnessTexture.width, material.metallicRoughnessTexture.height));
        }
    }
    else {
        material.metallicRoughnessTexture.width = RESOLUTION_TARGET;
        material.metallicRoughnessTexture.height = RESOLUTION_TARGET;
    }

    //NORMAL TEXTURE LOAD
    if (material.normalTexture.path != EMPTY_TEXTURE)
    {
        textureTypeMap.emplace(NORMAL_TEXTURE, loadImageAndBpp(material.normalTexture.path, material.normalTexture.width, material.normalTexture.height));
    }
    else {
        material.normalTexture.width = RESOLUTION_TARGET;
        material.normalTexture.height = RESOLUTION_TARGET;
    }

    //OCCLUSION TEXTURE LOAD
    if (material.occlusionTexture.path != EMPTY_TEXTURE)
    {
        textureTypeMap.emplace(OCCLUSION_TEXTURE, loadImageAndBpp(material.occlusionTexture.path, material.occlusionTexture.width, material.occlusionTexture.height));
    }
    else {
        material.occlusionTexture.width = RESOLUTION_TARGET;
        material.occlusionTexture.height = RESOLUTION_TARGET;
    }

    //EMISSIVE TEXTURE LOAD
    if (material.emissiveTexture.path != EMPTY_TEXTURE)
    {
        textureTypeMap.emplace(EMISSIVE_TEXTURE, loadImageAndBpp(material.emissiveTexture.path, material.emissiveTexture.width, material.emissiveTexture.height));
    }
    else {
        material.emissiveTexture.width = RESOLUTION_TARGET;
        material.emissiveTexture.height = RESOLUTION_TARGET;
    }
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


//https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
template<typename T>
const T* getBufferData(const tinygltf::Model& model, int accessorIndex) {
    //The second step of structuring the data is accomplished with accessor objects. An accessor refers to a bufferview
    //They define how the data of a bufferView has to be interpreted by providing information about the data types and the layout.
    //Each accessor also has a byteOffset property. For the example above, it has been 0 for both accessors, because there was only one accessor for each bufferView.
    //But when multiple accessors refer to the same bufferView, then the byteOffset describes where the data of the accessor starts, relative to the bufferView that it refers to.
    const auto& accessor    = model.accessors[accessorIndex];
    const auto& bufferView  = model.bufferViews[accessor.bufferView]; //"A bufferView describes a "chunk" or a "slice" of the whole, raw buffer data."
    //For a specific buffer, a bufferview might describe the part of the buffer that contains the data of the indices, and the other might describe the vertex positions
    //Bytoffset refers to offset in whole buffer
    const auto& buffer      = model.buffers[bufferView.buffer];
    
    //TODO: This may not be too safe and I should write this a bit better, but it is not production code so for now it works
    const T* dataPtr = reinterpret_cast<const T*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]); //accessor.byteOffset: The offset relative to the start of the buffer view in bytes.
    
    return dataPtr;
}

static TextureInfo parseTextureInfo(const tinygltf::Model& model, const tinygltf::Parameter& textureParameter, std::string base_folder) {
    TextureInfo info;

    // Checking and extracting the texture index from the parameter
    auto it = textureParameter.json_double_value.find("index");
    if (it != textureParameter.json_double_value.end()) {
        int textureIndex = static_cast<int>(it->second);
        if (textureIndex >= 0 && textureIndex < model.textures.size()) {
            const tinygltf::Texture& texture = model.textures[textureIndex];
            if (texture.source >= 0 && texture.source < model.images.size()) {
                const tinygltf::Image& image = model.images[texture.source];

                std::string fileExtension = image.mimeType.substr(image.mimeType.find_last_of('/') + 1);
                info.path = base_folder + image.name + "." + fileExtension;

                // Dimensions
                info.width = image.width;
                info.height = image.height;
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

    return info;
}


static MaterialGltf parseGltfMaterial(const tinygltf::Model& model, int materialIndex, std::string base_folder) {
    if (materialIndex < 0 || materialIndex >= model.materials.size()) {
        return MaterialGltf();
    }

    const tinygltf::Material& material = model.materials[materialIndex];
    MaterialGltf materialGltf;

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

    //Remember that it should be: R=ambient occlusion G=roughness B=metallic for the AO_Roughness_Metallic texture map

    // Base Color Texture
    auto baseColorTexIt = material.values.find("baseColorTexture");
    if (baseColorTexIt != material.values.end()) {
        materialGltf.baseColorTexture = parseTextureInfo(model, baseColorTexIt->second, base_folder);
    }
    else {
        materialGltf.baseColorTexture.path = EMPTY_TEXTURE;
    }

    // Normal Texture
    auto normalTexIt = material.additionalValues.find("normalTexture");
    if (normalTexIt != material.additionalValues.end()) {
        materialGltf.normalTexture = parseTextureInfo(model, normalTexIt->second, base_folder);

        auto scaleIt = normalTexIt->second.json_double_value.find("scale");
        if (scaleIt != normalTexIt->second.json_double_value.end()) {
            materialGltf.normalScale = static_cast<float>(scaleIt->second);
        }
        else {
            materialGltf.normalScale = 1.0f; // Default scale if not specified
        }
    }
    else {
        materialGltf.normalTexture.path = EMPTY_TEXTURE;
    }

    // Metallic-Roughness Texture
    auto metalRoughTexIt = material.values.find("metallicRoughnessTexture");
    if (metalRoughTexIt != material.values.end()) {
        materialGltf.metallicRoughnessTexture = parseTextureInfo(model, metalRoughTexIt->second, base_folder);
    }

    // Occlusion Texture
    auto occlusionTexIt = material.additionalValues.find("occlusionTexture");
    if (occlusionTexIt != material.additionalValues.end()) {
        materialGltf.occlusionTexture = parseTextureInfo(model, occlusionTexIt->second, base_folder);

        auto scaleIt = occlusionTexIt->second.json_double_value.find("strength");
        if (scaleIt != occlusionTexIt->second.json_double_value.end()) {
            materialGltf.occlusionStrength = static_cast<float>(scaleIt->second);
        }
        else {
            materialGltf.occlusionStrength = 1.0f; // Default scale if not specified
        }
    }
    else {
        materialGltf.occlusionTexture.path = EMPTY_TEXTURE;
    }

    // Emissive Texture
    auto emissiveTexIt = material.additionalValues.find("emissiveTexture");
    if (emissiveTexIt != material.additionalValues.end()) {
        materialGltf.emissiveTexture = parseTextureInfo(model, emissiveTexIt->second, base_folder);
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

    // Metallic and Roughness Factors
    materialGltf.metallicFactor = material.pbrMetallicRoughness.metallicFactor;
    materialGltf.roughnessFactor = material.pbrMetallicRoughness.roughnessFactor;

    return materialGltf;
}

std::vector<Mesh> parseGltfFileToMesh(const std::string& filename, std::string base_folder) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    
    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    if (!ret) {
        std::cerr << "Failed to load glTF: " << err << std::endl;
        return {};
    }
    

    if (!warn.empty()) {
        std::cout << "glTF parse warning: " << warn << std::endl;
    }

    std::vector<Mesh>       meshes;

    //remember that "when a 3D model is created as GLTF it is already triangulated"
    for (const auto& mesh : model.meshes) {
        Mesh myMesh(mesh.name);
        
        for (const auto& primitive : mesh.primitives) {
            const tinygltf::Accessor& indicesAccessor = model.accessors[primitive.indices];
            const tinygltf::BufferView& bufferView = model.bufferViews[indicesAccessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
            const unsigned char* indexData = buffer.data.data() + bufferView.byteOffset + indicesAccessor.byteOffset;
            std::vector<int> indices(indicesAccessor.count);

            if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                const uint16_t* buf = reinterpret_cast<const uint16_t*>(indexData);
                for (size_t i = 0; i < indicesAccessor.count; i++) {
                    indices[i] = buf[i];
                }
            }
            else if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                const uint32_t* buf = reinterpret_cast<const uint32_t*>(indexData);
                for (size_t i = 0; i < indicesAccessor.count; i++) {
                    indices[i] = buf[i];
                }
            }

            // Extract vertex data
            auto vertices       = getBufferData<glm::vec3>(model, primitive.attributes.at("POSITION"));
            auto uvs            = getBufferData<glm::vec2>(model, primitive.attributes.at("TEXCOORD_0"));         
            auto normals        = getBufferData<glm::vec3>(model, primitive.attributes.at("NORMAL"));

            const glm::vec4* tangents = NULL;
            bool hasTangents = false;
            if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
            {
                tangents = getBufferData<glm::vec4>(model, primitive.attributes.at("TANGENT"));
                hasTangents = true;
            }
            
            myMesh.material = parseGltfMaterial(model, primitive.material, base_folder);

            //TODO: indices is wrong to be used like this because it is not a global index amongst all primitives
            myMesh.faces.resize(indices.size() / 3);
            Face* dst = myMesh.faces.data();
            
            for (size_t i = 0, count = indices.size(); i < count; i += 3, ++dst) {             

                size_t index[3] = { indices[i], indices[i + 1], indices[i + 2] };

                if (hasTangents)
                {
                    for (int e = 0; e < 3; e++) {
                        dst->tangent[e] = tangents[index[e]];
                    }
                } else {
                    //TODO: YOU SHOULD FK AVERAGE IT BETWEEN THE TANGENTS OF FACES THAT SHARE THIS VERTEX
                    //but tbh just reimport it in Blender and export the damn tangents (there is a checkbox on the right of the exporter window under "data->mesh->Tangents")
                    glm::vec3 dv1 = dst->pos[1] - dst->pos[0];
                    glm::vec3 dv2 = dst->pos[2] - dst->pos[0];

                    glm::vec2 duv1 = dst->uv[1] - dst->uv[0];
                    glm::vec2 duv2 = dst->uv[2] - dst->uv[0];

                    float r = 1.0f / (duv1.x * duv2.y - duv1.y * duv2.x);
                    glm::vec4 tangent = glm::vec4((dv1 * duv2.y - dv2 * duv1.y) * r, 1.0f);
                    dst->tangent[0] = tangent;
                    dst->tangent[1] = tangent;
                    dst->tangent[2] = tangent;
                }

                for (int e = 0; e < 3; e++)
                {
                    dst->pos[e] = vertices[index[e]];
                    dst->uv[e] = uvs[index[e]];
                    dst->normal[e] = normals[index[e]]; 
                }
                    
            }
            
            meshes.push_back(myMesh);
        }
    }
    return meshes; //TODO: struct struct struct!
}

void writeBinaryPLY(const std::string& filename, const std::vector<Gaussian3D>& gaussians) {
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
        //Mean
        file.write(reinterpret_cast<const char*>(&gaussian.position.x), sizeof(gaussian.position.x));
        file.write(reinterpret_cast<const char*>(&gaussian.position.y), sizeof(gaussian.position.y));
        file.write(reinterpret_cast<const char*>(&gaussian.position.z), sizeof(gaussian.position.z));
        //Normal
        file.write(reinterpret_cast<const char*>(&gaussian.normal.x), sizeof(gaussian.normal.x));
        file.write(reinterpret_cast<const char*>(&gaussian.normal.y), sizeof(gaussian.normal.y));
        file.write(reinterpret_cast<const char*>(&gaussian.normal.z), sizeof(gaussian.normal.z));
        //RGB
        file.write(reinterpret_cast<const char*>(&gaussian.sh0.x), sizeof(gaussian.sh0.x));
        file.write(reinterpret_cast<const char*>(&gaussian.sh0.y), sizeof(gaussian.sh0.y));
        file.write(reinterpret_cast<const char*>(&gaussian.sh0.z), sizeof(gaussian.sh0.z));

        //---------NEW-----------------------------------------------------
        //Material properties

        file.write(reinterpret_cast<const char*>(&gaussian.material.metallicFactor), sizeof(gaussian.material.metallicFactor));
        file.write(reinterpret_cast<const char*>(&gaussian.material.roughnessFactor), sizeof(gaussian.material.roughnessFactor));
        //-----------------------------------------------------------------

        //Opacity
        file.write(reinterpret_cast<const char*>(&gaussian.opacity), sizeof(gaussian.opacity));

        file.write(reinterpret_cast<const char*>(&gaussian.scale.x), sizeof(gaussian.scale.x));
        file.write(reinterpret_cast<const char*>(&gaussian.scale.y), sizeof(gaussian.scale.y));
        file.write(reinterpret_cast<const char*>(&gaussian.scale.z), sizeof(gaussian.scale.z));
        //Rotation
        file.write(reinterpret_cast<const char*>(&gaussian.rotation.x), sizeof(gaussian.rotation.x));
        file.write(reinterpret_cast<const char*>(&gaussian.rotation.y), sizeof(gaussian.rotation.y));
        file.write(reinterpret_cast<const char*>(&gaussian.rotation.z), sizeof(gaussian.rotation.z));
        file.write(reinterpret_cast<const char*>(&gaussian.rotation.w), sizeof(gaussian.rotation.w));
    }
    file.close();
}

void writeBinaryPLY_lit(const std::string& filename, const std::vector<Gaussian3D>& gaussians) {
    std::ofstream file(filename, std::ios::binary | std::ios::out);

    // Write header in ASCII
    file << "ply\n";
    file << "format binary_little_endian 1.0\n";
    file << "element vertex " << gaussians.size() << "\n";

    file << "property float x\n";               //0
    file << "property float y\n";               //1
    file << "property float z\n";               //2

    file << "property float nx\n";              //3
    file << "property float ny\n";              //4
    file << "property float nz\n";              //5

    file << "property float f_dc_0\n";    //6
    file << "property float f_dc_1\n";    //7
    file << "property float f_dc_2\n";    //8

    file << "property float opacity\n";         //9

    file << "property float scale_0\n";         //10
    file << "property float scale_1\n";         //11
    file << "property float scale_2\n";         //12

    file << "property float rot_0\n";           //13
    file << "property float rot_1\n";           //14
    file << "property float rot_2\n";           //15
    file << "property float rot_3\n";           //16

    file << "property float roughness\n";       //17
    file << "property float metallic\n";        //18

    file << "end_header\n";

    // Write vertex data in binary
    for (const auto& gaussian : gaussians) {
        //Mean
        file.write(reinterpret_cast<const char*>(&gaussian.position.x), sizeof(gaussian.position.x));
        file.write(reinterpret_cast<const char*>(&gaussian.position.y), sizeof(gaussian.position.y));
        file.write(reinterpret_cast<const char*>(&gaussian.position.z), sizeof(gaussian.position.z));
        //Normal
        file.write(reinterpret_cast<const char*>(&gaussian.normal.x), sizeof(gaussian.normal.x));
        file.write(reinterpret_cast<const char*>(&gaussian.normal.y), sizeof(gaussian.normal.y));
        file.write(reinterpret_cast<const char*>(&gaussian.normal.z), sizeof(gaussian.normal.z));
        //RGB
        file.write(reinterpret_cast<const char*>(&gaussian.sh0.x), sizeof(gaussian.sh0.x));
        file.write(reinterpret_cast<const char*>(&gaussian.sh0.y), sizeof(gaussian.sh0.y));
        file.write(reinterpret_cast<const char*>(&gaussian.sh0.z), sizeof(gaussian.sh0.z));

        //Opacity
        file.write(reinterpret_cast<const char*>(&gaussian.opacity), sizeof(gaussian.opacity));

        //Scale
        file.write(reinterpret_cast<const char*>(&gaussian.scale.x), sizeof(gaussian.scale.x));
        file.write(reinterpret_cast<const char*>(&gaussian.scale.y), sizeof(gaussian.scale.y));
        file.write(reinterpret_cast<const char*>(&gaussian.scale.z), sizeof(gaussian.scale.z));
        //Rotation
        file.write(reinterpret_cast<const char*>(&gaussian.rotation.x), sizeof(gaussian.rotation.x));
        file.write(reinterpret_cast<const char*>(&gaussian.rotation.y), sizeof(gaussian.rotation.y));
        file.write(reinterpret_cast<const char*>(&gaussian.rotation.z), sizeof(gaussian.rotation.z));
        file.write(reinterpret_cast<const char*>(&gaussian.rotation.w), sizeof(gaussian.rotation.w));

        //---------NEW-----------------------------------------------------
        //Material properties
        file.write(reinterpret_cast<const char*>(&gaussian.material.roughnessFactor), sizeof(gaussian.material.roughnessFactor));
        file.write(reinterpret_cast<const char*>(&gaussian.material.metallicFactor), sizeof(gaussian.material.metallicFactor));
        //-----------------------------------------------------------------
    }
    file.close();
}

void writeBinaryPLY_standard_format(const std::string& filename, const std::vector<Gaussian3D>& gaussians) {
    std::ofstream file(filename, std::ios::binary | std::ios::out);

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
        // Mean
        file.write(reinterpret_cast<const char*>(&gaussian.position.x), sizeof(gaussian.position.x));
        file.write(reinterpret_cast<const char*>(&gaussian.position.y), sizeof(gaussian.position.y));
        file.write(reinterpret_cast<const char*>(&gaussian.position.z), sizeof(gaussian.position.z));

        // Normal
        file.write(reinterpret_cast<const char*>(&gaussian.normal.x), sizeof(gaussian.normal.x));
        file.write(reinterpret_cast<const char*>(&gaussian.normal.y), sizeof(gaussian.normal.y));
        file.write(reinterpret_cast<const char*>(&gaussian.normal.z), sizeof(gaussian.normal.z));

        // RGB
        file.write(reinterpret_cast<const char*>(&gaussian.sh0.x), sizeof(gaussian.sh0.x));
        file.write(reinterpret_cast<const char*>(&gaussian.sh0.y), sizeof(gaussian.sh0.y));
        file.write(reinterpret_cast<const char*>(&gaussian.sh0.z), sizeof(gaussian.sh0.z));

        // Fill f_rest_0 to f_rest_44 with zeros
        float zero = 0.0f;
        for (int i = 0; i <= 44; ++i) {
            file.write(reinterpret_cast<const char*>(&zero), sizeof(zero));
        }

        // Opacity
        file.write(reinterpret_cast<const char*>(&gaussian.opacity), sizeof(gaussian.opacity));

        // Scale
        file.write(reinterpret_cast<const char*>(&gaussian.scale.x), sizeof(gaussian.scale.x));
        file.write(reinterpret_cast<const char*>(&gaussian.scale.y), sizeof(gaussian.scale.y));
        file.write(reinterpret_cast<const char*>(&gaussian.scale.z), sizeof(gaussian.scale.z));

        // Rotation
        file.write(reinterpret_cast<const char*>(&gaussian.rotation.x), sizeof(gaussian.rotation.x));
        file.write(reinterpret_cast<const char*>(&gaussian.rotation.y), sizeof(gaussian.rotation.y));
        file.write(reinterpret_cast<const char*>(&gaussian.rotation.z), sizeof(gaussian.rotation.z));
        file.write(reinterpret_cast<const char*>(&gaussian.rotation.w), sizeof(gaussian.rotation.w));
    }

    file.close();
}
