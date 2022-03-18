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
            size_t numIndices = model.accessors[primitive.indices].count; //indices in shape
            totalIndices += numIndices;      
        }
    }
    
    //allocate space for all vertices
    std::vector<ObjVertex> objVertices;
    objVertices.reserve(totalIndices / 3);
    // Map allows us to avoid duplicating vertices by ignoring combinations of attributes we've already seen.
    std::unordered_map<index_t, size_t> seenIndices;
    // Loop over shapes in the gltf file

    

    for (const auto& mesh : model.meshes) {//meshes in scene
        for (const auto& primitive : mesh.primitives) {//shapes in mesh
            auto attribMap = primitive.attributes;
            //determine where our data is
            vertexIndex = attribMap["POSITION"];
            normalIndex = attribMap["NORMAL"];
            textureIndex = attribMap["TEXCOORD_0"];

            //gltf buffers may have many interleaved buffers, and the main objects that
            //define what belongs to what are:
            //Accessors, and Bufferviews

            //the accessor defines access to which bufferview is being used. 
            //ByteOffset and count are most important, as well as which bufferview to use
            Accessor vertAcc = model.accessors[vertexIndex];
            Accessor normalAccessor = model.accessors[normalIndex];
            Accessor textureAccessor = model.accessors[textureIndex];

            //verify assumptions about vertex data
            assert(vertAcc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
            assert(vertAcc.type == TINYGLTF_TYPE_VEC3);
            assert(vertAcc.count % 3 == 0);

            //verify assumptions about normal data
            assert(normalAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
            assert(normalAccessor.type == TINYGLTF_TYPE_VEC3);
            assert(normalAccessor.count % 3 == 0);

            //verify assumptions about texture data
            assert(textureAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
            assert(textureAccessor.type == TINYGLTF_TYPE_VEC2);
            assert(textureAccessor.count % 2 == 0);


            //the bufferview refers to a buffer. It also contains:
            //byteOffset: which byte of the buffer to start reading from
            //byteLength: how many bytes to read from the buffer, starting from byteOffset
            //byteStride: Useful for interleaved values. How many bytes are inbetween the start
            //            of each of the objects referred to by the accessor-bufferview combo.
            BufferView vertBV = model.bufferViews[vertAcc.bufferView];
            int stride = vertAcc.ByteStride(vertBV);
            size_t offset = vertAcc.byteOffset + vertBV.byteOffset;

            //the buffer itself.
            Buffer buffer = model.buffers[vertBV.buffer];
            auto data = buffer.data; //the vector of bytes. What we have been looking for. bufferView and accessor will be used to read it.
            
            for (int i = 0; i < vertAcc.count / 3; i++) {
                //convince the compiler that data points to floating point data.
                float* memoryLocation = reinterpret_cast<float*>(data.data() + offset + (i * static_cast<size_t>(stride)));
                //read in the vec3.
                objVertices.emplace_back(ObjVertex{ glm::vec3(ptr_to_vec3(memoryLocation)) });
                
            }
            std::cout << "here\n";
        }
    }
    //triangles only, for the moment.


}
