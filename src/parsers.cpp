#include "parsers.hpp"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../thirdParty/tiny_gltf.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../thirdParty/stb_image_resize.h"

//TODO should not by default use 3 channels but for now that is ok
//TODO: Careful to remember that the image is saved with its original name, if you change filename after 
std::pair<unsigned char*, int> loadImage(std::string texturePath, int& textureWidth, int& textureHeight)
{
    size_t pos = texturePath.rfind('.');
    std::string image_format = texturePath.substr(pos + 1);
    int bpp;
    unsigned char* image = stbi_load(texturePath.c_str(), &textureWidth, &textureHeight, &bpp, 0);

    if (image == NULL) {
        std::cout << "Error in loading the image, cannot find " << texturePath << "\n" << std::endl;
        exit(1);
    }

    //TODO: use or printf or std::cout, choose
    std::cout << "Image: " << texturePath << "  width: " << textureWidth << "  height: " << textureHeight << " " << bpp << std::endl;

    std::string resized_texture_name_location = BASE_DATASET_FOLDER + std::string("resized_texture") + "." + image_format;
    float aspect_ratio = (float)textureHeight / (float)textureWidth;

    if (textureWidth > MAX_TEXTURE_WIDTH)
    {
        // Specify new width and height

        int new_width = MAX_TEXTURE_WIDTH;
        int new_height = static_cast<int>(new_width * aspect_ratio);

        // Allocate memory for the resized image
        unsigned char* resized_data = (unsigned char*)malloc(new_width * new_height * bpp);

        // Resize the image
        stbir_resize_uint8(image, textureWidth, textureHeight, 0, resized_data, new_width, new_height, 0, bpp);

        // Save the resized image
        stbi_write_png(resized_texture_name_location.c_str(), new_width, new_height, bpp, resized_data, new_width * bpp);
        
        stbi_image_free(resized_data);
        stbi_image_free(image);
        
        image = stbi_load(resized_texture_name_location.c_str(), &textureWidth, &textureHeight, &bpp, 0);

        if (image == NULL) {
            printf("Error in loading the resized image\n");
            exit(1);
        }

        std::cout << "Image: " << resized_texture_name_location << "  width: " << textureWidth << "  height: " << textureHeight << " " << bpp << std::endl;
                
    }

    return std::make_pair(image, bpp);
}


std::pair<bool, int> loadImageIntoVector(std::string filepath, int& width, int& height, std::vector<unsigned char>& vecToFill)
{
    auto imageAndBpp = loadImage(filepath, width, height);
    bool found = false;

    unsigned char* textureImage = std::get<0>(imageAndBpp);
    int bpp                     = std::get<1>(imageAndBpp);

    if (!textureImage) {
        std::cerr << "Failed to load image from " << filepath << std::endl;
    }
    else {
        vecToFill.resize(width * height * bpp);
        std::memcpy(&vecToFill[0], textureImage, vecToFill.size());
        stbi_image_free(textureImage); // Freeing the allocated memory
        found = true;
    }  

    return std::make_pair(found, bpp);
}

/*
std::tuple<std::vector<Mesh>, std::vector<glm::vec3>, std::vector<glm::vec2>, std::vector<glm::vec3>> parseObjFileToMeshes(const std::string& filename) {
    std::vector<glm::vec3> globalVertices;
    std::vector<glm::vec2> globalUvs;
    std::vector<glm::vec3> globalNormals;
    std::vector<Mesh> meshes;
    std::string currentMaterial;
    Mesh* currentMesh = nullptr;

    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        if (line.find('o') == 0 || line.find('g') == 0) {
            std::string meshName = line.substr(2);
            meshes.emplace_back(meshName);
            currentMesh = &meshes.back();
        }
        else if (line.find("usemtl") == 0) {
            currentMaterial = line.substr(7); // Capture material name after "usemtl"
        }
        else if (line.find('v') == 0 && line[1] == ' ') {
            glm::vec3 vertex;
            std::istringstream(line.substr(2)) >> vertex.x >> vertex.y >> vertex.z;
            globalVertices.push_back(vertex);
        }
        else if (line.find("vt") == 0) {
            glm::vec2 uv;
            std::istringstream(line.substr(3)) >> uv.x >> uv.y;
            globalUvs.push_back(uv);
        }
        else if (line.find("vn") == 0) {
            glm::vec3 normal;
            std::istringstream(line.substr(3)) >> normal.x >> normal.y >> normal.z;
            globalNormals.push_back(normal);
        }
        else if (line.find('f') == 0) {
            std::vector<int> vertexIndices, uvIndices, normalIndices;
            std::istringstream iss(line.substr(2));
            std::string part;
            while (iss >> part) {
                std::size_t firstSlash = part.find('/');
                std::size_t secondSlash = part.rfind('/');
                int vertIdx = std::stoi(part.substr(0, firstSlash)) - 1;
                vertexIndices.push_back(vertIdx);

                if (firstSlash != std::string::npos && firstSlash + 1 < secondSlash) {
                    int uvIdx = std::stoi(part.substr(firstSlash + 1, secondSlash - firstSlash - 1)) - 1;
                    uvIndices.push_back(uvIdx);
                }

                if (secondSlash != std::string::npos && secondSlash + 1 < part.size()) {
                    int normIdx = std::stoi(part.substr(secondSlash + 1)) - 1;
                    normalIndices.push_back(normIdx);
                }
            }
            if (currentMesh) {
                if (currentMaterial.length() != 0)
                {
                    currentMesh->faces.emplace_back(vertexIndices, uvIndices, normalIndices, currentMaterial);
                }
                else {
                    currentMesh->faces.emplace_back(vertexIndices, uvIndices, normalIndices, DEFAULT_MATERIAL_NAME); // find: [1] to understand
                }
                
            }
        }
    }

    return std::make_tuple(meshes, globalVertices, globalUvs, globalNormals);
}
*/

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
std::vector<T> getBufferData(const tinygltf::Model& model, int accessorIndex) {
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
    return std::vector<T>(dataPtr, dataPtr + accessor.count);
}

MaterialGltf parseGltfMaterial(const tinygltf::Model& model, int materialIndex) {
    if (materialIndex < 0 || materialIndex >= model.materials.size()) {
        return MaterialGltf(); 
    }

    const tinygltf::Material& gltfMaterial = model.materials[materialIndex];
    MaterialGltf material;

    material.name = gltfMaterial.name;

    // Parse base color factor
    auto colorIt = gltfMaterial.values.find("baseColorFactor");
    if (colorIt != gltfMaterial.values.end()) {
        material.baseColorFactor = glm::vec4(
            static_cast<float>(colorIt->second.ColorFactor()[0]),
            static_cast<float>(colorIt->second.ColorFactor()[1]),
            static_cast<float>(colorIt->second.ColorFactor()[2]),
            static_cast<float>(colorIt->second.ColorFactor()[3])
        );
    }

    // Parse base color texture
    auto texIt = gltfMaterial.values.find("baseColorTexture");
    if (texIt != gltfMaterial.values.end()) {
        int textureIndex = texIt->second.TextureIndex();
        const tinygltf::Texture& texture = model.textures[textureIndex];
        const tinygltf::Image& image = model.images[texture.source];
        //I assume the texture is in the same folder as the loaded .gltf file
        //blender unfortunately does not correctly export the path, so i have no way of recovering the full path of the resource image
        std::string imagePath = BASE_DATASET_FOLDER + image.name + "." + image.mimeType.substr(image.mimeType.find("/") + 1);

        material.baseColorTexture.path = imagePath;
        material.baseColorTexture.texCoordIndex = texIt->second.TextureTexCoord();
        
        //TODO: here I should actually load and save the images instead of just doing this, update it!
        material.baseColorTexture.width = image.width > MAX_TEXTURE_WIDTH ? MAX_TEXTURE_WIDTH : image.width;
        material.baseColorTexture.height = image.height > MAX_TEXTURE_WIDTH ? MAX_TEXTURE_WIDTH : image.height;

    }
    
    material.metallicFactor = gltfMaterial.pbrMetallicRoughness.metallicFactor;
    material.roughnessFactor = gltfMaterial.pbrMetallicRoughness.roughnessFactor;

    return material;
}

std::tuple<std::vector<Mesh>, std::vector<glm::vec3>, std::vector<glm::vec2>, std::vector<glm::vec3>> parseGltfFileToMesh(const std::string& filename) {
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

    std::vector<glm::vec3>  globalVertices;
    std::vector<glm::vec2>  globalUvs;
    std::vector<glm::vec3>  globalNormals;
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

            //TODO: in future hold a meshVertices list specific to each mesh/primitive as indexing is relative to the primitive
            globalVertices.insert(globalVertices.end(), vertices.begin(), vertices.end());
            globalUvs.insert(globalUvs.end(), uvs.begin(), uvs.end());
            globalNormals.insert(globalNormals.end(), normals.begin(), normals.end());

            myMesh.material = parseGltfMaterial(model, primitive.material);

            //TODO: indices is wrong to be used like this because it is not a global index amongst all primitives
            for (size_t i = 0; i < indices.size(); i += 3) {
                std::vector<glm::vec3> faceVertexIndices  = { vertices[indices[i]], vertices[indices[i + 1]], vertices[indices[i + 2]] };
                std::vector<glm::vec2> faceUvIndices      = { uvs[indices[i]], uvs[indices[i + 1]], uvs[indices[i + 2]] };
                std::vector<glm::vec3> faceNormalIndices  = { normals[indices[i]], normals[indices[i + 1]], normals[indices[i + 2]] };
                myMesh.faces.emplace_back(faceVertexIndices, faceUvIndices, faceNormalIndices);
                
            }
            
            meshes.push_back(myMesh);
        }
    }
    return { meshes, globalVertices, globalUvs, globalNormals };
}

void writeBinaryPLY(const std::string& filename, const std::vector<Gaussian3D>& gaussians) {
    std::ofstream file(filename, std::ios::binary | std::ios::out);

    // Write header in ASCII
    file << "ply\n";
    file << "format binary_little_endian 1.0\n";
    file << "element vertex " << gaussians.size() << "\n";

    file << "property float x\n";
    file << "property float y\n";
    file << "property float z\n";

    file << "property float nx\n";
    file << "property float ny\n";
    file << "property float nz\n";

    file << "property float f_dc_0\n";
    file << "property float f_dc_1\n";
    file << "property float f_dc_2\n";
    for (int i = 0; i < 45; i++)
    {
        file << "property float f_rest_" << i << "\n";
    }

    file << "property float opacity\n";

    file << "property float scale_0\n";
    file << "property float scale_1\n";
    file << "property float scale_2\n";

    file << "property float rot_0\n";
    file << "property float rot_1\n";
    file << "property float rot_2\n";
    file << "property float rot_3\n";

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

        // TODO: this takes up basically 65% of the space and I do not even need to use it
        float zeroValue = 0.0f;
        for (int i = 0; i < 45; i++) {
            file.write(reinterpret_cast<const char*>(&zeroValue), sizeof(zeroValue));
        }

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


//vertexIndices, uvIndices, normalIndices
// Function to find a 3D position from UV coordinates, this is probably the main bottleneck, as complexity grows with more complex 3D models.
//Need to think of possible smarter ways to do this
/*
std::tuple<glm::vec3, std::string, glm::vec3, std::array<glm::vec3, 3>, std::array<glm::vec2, 3>> find3DPositionFromUV(const std::vector<Mesh>& meshes, const glm::vec2& targetUv, const std::vector<glm::vec3>& globalVertices, const std::vector<glm::vec2>& globalUvs) {
    for (const auto& mesh : meshes) {
        for (const auto& face : mesh.faces) {
            std::vector<int> vertexIndices = std::get<0>(face);
            std::vector<int> uvIndices = std::get<1>(face);
            std::vector<int> normalIndices = std::get<2>(face);

            if (face.first.size() != 3 || face.second.size() != 3) {
                continue; // Skip non-triangular faces
            }

            std::array<glm::vec2, 3> triangleUvs = { globalUvs[face.second[0]], globalUvs[face.second[1]], globalUvs[face.second[2]] };
            std::array<glm::vec3, 3> triangleVertices = { globalVertices[face.first[0]], globalVertices[face.first[1]], globalVertices[face.first[2]] };

            //std::cout << "rad angle: " << rad_angle << std::endl;
            if (pointInTriangle(targetUv, triangleUvs[0], triangleUvs[1], triangleUvs[2])) {
                float u;
                float v;
                float w;
                computeBarycentricCoords(targetUv, triangleUvs[0], triangleUvs[1], triangleUvs[2], u, v, w);

                glm::vec3 interpolatedPos =
                    globalVertices[face.first[0]] * u +
                    globalVertices[face.first[1]] * v +
                    globalVertices[face.first[2]] * w;

                glm::vec3 normal = glm::cross(globalVertices[face.first[1]] - globalVertices[face.first[0]], globalVertices[face.first[2]] - globalVertices[face.first[0]]);

                return { interpolatedPos, mesh.name,  glm::normalize(normal), triangleVertices, triangleUvs };
            }
        }
    }
    //std::cout << "3D point not found for given UV" << std::endl;
    return { glm::vec3(0,0,0), "NotFound", glm::vec3(0,0,0), {}, {} }; // Return an empty result if no matching face is found
}
*/