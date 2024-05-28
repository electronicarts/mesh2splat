#include "parsers.hpp"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../thirdParty/tiny_gltf.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../thirdParty/stb_image_resize.h"


//TODO: Careful to remember that the image is saved with its original name, if you change filename after 
std::pair<unsigned char*, int> loadImageAndBpp(std::string texturePath, int& textureWidth, int& textureHeight) //TODO: structs structs!
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

    if (textureWidth > MAX_TEXTURE_SIZE)
    {
        // Specify new width and height

        int new_width = MAX_TEXTURE_SIZE;
        int new_height = static_cast<int>(new_width * aspect_ratio);

        // Allocate memory for the resized image
        unsigned char* resized_data = (unsigned char*)malloc(new_width * new_height * bpp);

        // Resize the image
        stbir_resize_uint8(image, textureWidth, textureHeight, 0, resized_data, new_width, new_height, 0, bpp);
        textureWidth = new_width;
        textureHeight = new_height;

        // Save the resized image
        //stbi_write_png(resized_texture_name_location.c_str(), new_width, new_height, bpp, resized_data, new_width * bpp);
        stbi_image_free(image);
        std::cout << "\nImage: " << resized_texture_name_location << "  width: " << textureWidth << "  height: " << textureHeight << " " << bpp << std::endl;

        return std::make_pair(resized_data, bpp);
       
    }

    return std::make_pair(image, bpp);
}

void loadAllTexturesIntoMap(MaterialGltf& material, std::map<std::string, std::pair<unsigned char*, int>>& textureTypeMap)
{
    //TODO: THIS IS NOT NICE CODE, IT CAN BE GENERALIZED TO AVOID REPEATING BY SIMPLY PASSING the TextureInfo
    
    //BASECOLOR ALBEDO TEXTURE LOAD
    if (material.baseColorTexture.path != EMPTY_TEXTURE)
    {
        textureTypeMap.emplace(BASE_COLOR_TEXTURE, loadImageAndBpp(material.baseColorTexture.path, material.baseColorTexture.width, material.baseColorTexture.height));
    }
    else {
        material.baseColorTexture.width = MAX_TEXTURE_SIZE;
        material.baseColorTexture.height = MAX_TEXTURE_SIZE;
    }

    //METALLIC-ROUGHNESS TEXTURE LOAD
    if (material.metallicRoughnessTexture.path != EMPTY_TEXTURE)
    {
        textureTypeMap.emplace(METALLIC_ROUGHNESS_TEXTURE, loadImageAndBpp(material.metallicRoughnessTexture.path, material.metallicRoughnessTexture.width, material.metallicRoughnessTexture.height));
    }
    else {
        material.metallicRoughnessTexture.width = MAX_TEXTURE_SIZE;
        material.metallicRoughnessTexture.height = MAX_TEXTURE_SIZE;
    }

    //NORMAL TEXTURE LOAD
    if (material.normalTexture.path != EMPTY_TEXTURE)
    {
        textureTypeMap.emplace(NORMAL_TEXTURE, loadImageAndBpp(material.normalTexture.path, material.normalTexture.width, material.normalTexture.height));
    }
    else {
        material.normalTexture.width = MAX_TEXTURE_SIZE;
        material.normalTexture.height = MAX_TEXTURE_SIZE;
    }

    //OCCLUSION TEXTURE LOAD
    if (material.occlusionTexture.path != EMPTY_TEXTURE)
    {
        textureTypeMap.emplace(OCCLUSION_TEXTURE, loadImageAndBpp(material.occlusionTexture.path, material.occlusionTexture.width, material.occlusionTexture.height));
    }
    else {
        material.occlusionTexture.width = MAX_TEXTURE_SIZE;
        material.occlusionTexture.height = MAX_TEXTURE_SIZE;
    }

    //EMISSIVE TEXTURE LOAD
    if (material.emissiveTexture.path != EMPTY_TEXTURE)
    {
        textureTypeMap.emplace(EMISSIVE_TEXTURE, loadImageAndBpp(material.emissiveTexture.path, material.emissiveTexture.width, material.emissiveTexture.height));
    }
    else {
        material.emissiveTexture.width = MAX_TEXTURE_SIZE;
        material.emissiveTexture.height = MAX_TEXTURE_SIZE;
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


std::map<std::string, Material> parseMtlFile(const std::string& filename) {
    std::map<std::string, Material> materials;
    std::ifstream file(filename);
    std::string line;
    std::string currentMaterialName;

    //Add default material [1]

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string identifier;
        iss >> identifier;

        if (identifier == "newmtl") {
            iss >> currentMaterialName;
            materials[currentMaterialName] = Material();
        }
        else if (identifier == "Ka") {
            glm::vec3 ka;
            iss >> ka.x >> ka.y >> ka.z;
            materials[currentMaterialName].ambient = ka;
        }
        else if (identifier == "Kd") {
            glm::vec3 kd;
            iss >> kd.x >> kd.y >> kd.z;
            materials[currentMaterialName].diffuse = kd;
        }
        else if (identifier == "Ks") {
            glm::vec3 ks;
            iss >> ks.x >> ks.y >> ks.z;
            materials[currentMaterialName].specular = ks;
        }
        else if (identifier == "Ns") {
            float ns;
            iss >> ns;
            materials[currentMaterialName].specularExponent = ns;
        }
        else if (identifier == "d" || identifier == "Tr") {
            float d;
            iss >> d;
            materials[currentMaterialName].transparency = d;
        }
        else if (identifier == "Ni") {
            float ni;
            iss >> ni;
            materials[currentMaterialName].opticalDensity = ni;
        }
        else if (identifier == "map_Kd") {
            std::string map_kd;
            iss >> map_kd;
            materials[currentMaterialName].diffuseMap = map_kd;
        }
    }

    return materials;
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

static TextureInfo parseTextureInfo(const tinygltf::Model& model, const tinygltf::Parameter& textureParameter) {
    TextureInfo info;

    // Checking and extracting the texture index from the parameter
    auto it = textureParameter.json_double_value.find("index");
    if (it != textureParameter.json_double_value.end()) {
        int textureIndex = static_cast<int>(it->second);
        if (textureIndex >= 0 && textureIndex < model.textures.size()) {
            const tinygltf::Texture& texture = model.textures[textureIndex];
            if (texture.source >= 0 && texture.source < model.images.size()) {
                const tinygltf::Image& image = model.images[texture.source];

                // Base path handling
                std::string basePath = BASE_DATASET_FOLDER; // Update this path as necessary.
                std::string fileExtension = image.mimeType.substr(image.mimeType.find_last_of('/') + 1);
                info.path = basePath + image.name + "." + fileExtension;

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


static MaterialGltf parseGltfMaterial(const tinygltf::Model& model, int materialIndex) {
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
        materialGltf.baseColorTexture = parseTextureInfo(model, baseColorTexIt->second);
    }
    else {
        materialGltf.baseColorTexture.path = EMPTY_TEXTURE;
    }

    // Normal Texture
    auto normalTexIt = material.additionalValues.find("normalTexture");
    if (normalTexIt != material.additionalValues.end()) {
        materialGltf.normalTexture = parseTextureInfo(model, normalTexIt->second);

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
        materialGltf.metallicRoughnessTexture = parseTextureInfo(model, metalRoughTexIt->second);
    }

    // Occlusion Texture
    auto occlusionTexIt = material.additionalValues.find("occlusionTexture");
    if (occlusionTexIt != material.additionalValues.end()) {
        materialGltf.occlusionTexture = parseTextureInfo(model, occlusionTexIt->second);

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
        materialGltf.emissiveTexture = parseTextureInfo(model, emissiveTexIt->second);
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

std::vector<Mesh> parseGltfFileToMesh(const std::string& filename) {
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
            
            myMesh.material = parseGltfMaterial(model, primitive.material);

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