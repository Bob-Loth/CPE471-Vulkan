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

void process_vertices(const Model& model, const Accessor& accessor, std::vector<ObjVertex>& objVertices) {
    assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
    assert(accessor.type == TINYGLTF_TYPE_VEC3);
    
    BufferView bufferView = model.bufferViews[accessor.bufferView];
    int stride = accessor.ByteStride(bufferView);
    size_t offset = accessor.byteOffset + bufferView.byteOffset;

    //the buffer itself.
    Buffer buffer = model.buffers[bufferView.buffer];
    auto data = buffer.data; //the vector of bytes. bufferView and accessor will be used to read it.

    for (int i = 0; i < accessor.count; i++) {
        //convince the compiler that data points to floating point data.
        float* memoryLocation = reinterpret_cast<float*>(data.data() + offset + (i * static_cast<size_t>(stride)));
        //read in the vec3.
        objVertices.emplace_back(ObjVertex{ glm::vec3(ptr_to_vec3(memoryLocation)) });
    }
}

void process_normals(const Model& model, const Accessor& accessor, std::vector<ObjVertex>& objVertices){
    assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
    assert(accessor.type == TINYGLTF_TYPE_VEC3);
    
    BufferView bufferView = model.bufferViews[accessor.bufferView];
    int stride = accessor.ByteStride(bufferView);
    size_t offset = accessor.byteOffset + bufferView.byteOffset;

    //the buffer itself.
    Buffer buffer = model.buffers[bufferView.buffer];
    auto data = buffer.data; //the vector of bytes. bufferView and accessor will be used to read it.

    for (int i = 0; i < accessor.count; i++) {
        //convince the compiler that data points to floating point data.
        float* memoryLocation = reinterpret_cast<float*>(data.data() + offset + (i * static_cast<size_t>(stride)));
        //read in the vec3.
        //objVertices.emplace_back(ObjVertex{ glm::vec3(ptr_to_vec3(memoryLocation)) });
        objVertices.at(i).normal = std::move(ptr_to_vec3(memoryLocation));
    }
}

void process_texcoords(const Model& model, const Accessor& accessor, std::vector<ObjVertex>& objVertices){
    assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
    assert(accessor.type == TINYGLTF_TYPE_VEC2);
    
    BufferView bufferView = model.bufferViews[accessor.bufferView];
    int stride = accessor.ByteStride(bufferView);
    size_t offset = accessor.byteOffset + bufferView.byteOffset;

    //the buffer itself.
    Buffer buffer = model.buffers[bufferView.buffer];
    auto data = buffer.data; //the vector of bytes. bufferView and accessor will be used to read it.

    for (int i = 0; i < accessor.count; i++) {
        //convince the compiler that data points to floating point data. Move data ptr forward by stride bytes.
        float* memoryLocation = reinterpret_cast<float*>(data.data() + offset + (i * static_cast<size_t>(stride)));
        //read in the vec2.
        objVertices.at(i).texCoord = std::move(ptr_to_vec2(memoryLocation));
    }

}

void process_indices(const Model& model, const Accessor& accessor, std::vector<ObjMultiShapeGeometry::index_t>& outputIndices) {
    assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
    assert(accessor.type == TINYGLTF_TYPE_SCALAR);
    
    BufferView bufferView = model.bufferViews[accessor.bufferView];
    int stride = accessor.ByteStride(bufferView);
    size_t offset = accessor.byteOffset + bufferView.byteOffset;

    //the buffer itself.
    Buffer buffer = model.buffers[bufferView.buffer];
    auto data = buffer.data; //the vector of bytes. bufferView and accessor will be used to read it.

    for (int i = 0; i < accessor.count; i++) {
        //convince the compiler that data points to unsigned short data. Move data ptr forward by stride bytes.
        unsigned short* memoryLocation = reinterpret_cast<unsigned short*>(data.data() + offset + (i * static_cast<size_t>(stride)));
        unsigned short val = *memoryLocation;
        outputIndices.push_back(val);
    }

}
//gltf buffers may have many interleaved buffers, and the main objects that
            //define what belongs to what are:
            //Accessors, and Bufferviews

            //the accessor defines access to which bufferview is being used. 
            //ByteOffset and count are most important, as well as which bufferview to use

            //the bufferview refers to a buffer. It also contains:
            //byteOffset: which byte of the buffer to start reading from
            //byteLength: how many bytes to read from the buffer, starting from byteOffset
            //byteStride: Useful for interleaved values. How many bytes are inbetween the start
            //            of each of the objects referred to by the accessor-bufferview combo.
void process_gltf_contents(Model& model, ObjMultiShapeGeometry& ivGeoOut) {
    //verify assumption about gltf data
    

    int vertexIndex = -1;
    int normalIndex = -1;
    int textureIndex = -1;

    size_t totalIndices = 0;
    //count the maximum number of vertices needed for this model
    for (const auto& mesh : model.meshes) {//meshes in scene
        for (const auto& primitive : mesh.primitives) {//shapes in mesh
            size_t numIndices = model.accessors[primitive.indices].count; //indices in shape
            totalIndices += numIndices;      
        }
    }
    
    std::vector<ObjVertex> objVertices;
    
    // Loop over shapes in the gltf file

    for (const auto& mesh : model.meshes) {//meshes in scene
        for (const auto& primitive : mesh.primitives) {//shapes in mesh
            assert(primitive.mode == TINYGLTF_MODE_TRIANGLES); //only work with triangle data for now.
            std::vector<ObjMultiShapeGeometry::index_t> outputIndices;

            //determine where our data is
            auto attrMap = primitive.attributes;
            //assume the gltf files have vertex positions and indices.
            vertexIndex = attrMap["POSITION"];
            Accessor vertAcc = model.accessors[vertexIndex];
            process_vertices(model, vertAcc, objVertices);

            Accessor indexAcc = model.accessors[primitive.indices];
            process_indices(model, indexAcc, outputIndices);

            //optionally find normal data and include it
            if (attrMap.find("NORMAL") != attrMap.end()) {
                normalIndex = attrMap["NORMAL"];
                Accessor normAcc = model.accessors[normalIndex];
                process_normals(model, normAcc, objVertices);
            }

            //optionally find texture data and include it
            if (attrMap.find("TEXCOORD_0") != attrMap.end()) {
                textureIndex = attrMap["TEXCOORD_0"];
                Accessor texAcc = model.accessors[textureIndex];
                process_texcoords(model, texAcc, objVertices);
            }

            ivGeoOut.addShape(outputIndices);
        }
    }
    ivGeoOut.setVertices(objVertices);
}
