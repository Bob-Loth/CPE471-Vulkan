#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "load_gltf.h"
using namespace tinygltf;
ObjMultiShapeGeometry load_gltf_to_vulkan(const VulkanDeviceBundle& aDeviceBundle, std::string filename) {
    Model model;
    TinyGLTF loader;
    std::string err;
    std::string warn;
    
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
    if (!warn.empty()) {
        std::cout << "gltf loader: " << warn << std::endl;
    }
    if (!err.empty()) {
        std::cout << "gltf loader: " << err << std::endl;
    }
    if (!ret) {
        std::cout << "gltf loader: " << "failed to parse glTF" << std::endl;
    }
    ObjMultiShapeGeometry ivGeo(aDeviceBundle);
    process_gltf_contents(model, ivGeo);

    return ivGeo;
}

void process_gltf_contents(Model& model, ObjMultiShapeGeometry& ivGeoOut) {
    //verify assumption about gltf data
    int vertexIndex = -1;
    int normalIndex = -1;
    int textureIndex = -1;

    size_t totalIndices = 0;
    //count the maximum number of vertices needed for this model
    for (const auto& mesh : model.meshes) {//meshes in scene
        for (const auto& primitive : mesh.primitives) {//shapes in mesh
            auto attribMap = primitive.attributes;
            //determine where our data is
            vertexIndex = attribMap["POSITION"];
            normalIndex = attribMap["NORMAL"];
            textureIndex = attribMap["TEXCOORD_0"];
            size_t numIndices = model.accessors[primitive.indices].count;
            totalIndices += numIndices;
            auto vertices = model.accessors[vertexIndex];
            std::cout << "here\n";
        }
    }
    
    //allocate space for all vertices
    std::vector<ObjVertex> objVertices;
    objVertices.reserve(totalIndices / 3);
    // Map allows us to avoid duplicating vertices by ignoring combinations of attributes we've already seen.
    std::unordered_map<size_t, size_t> seenIndices;
    // Loop over shapes in the gltf file

    //triangles only, for the moment.


}
