#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "load_gltf.h"

using namespace tinygltf;
using namespace glm;

ObjMultiShapeGeometry load_gltf_to_vulkan(const VulkanDeviceBundle& aDeviceBundle, std::string filename, bool isBinary) {
    Model model;
    TinyGLTF loader;
    std::string err;
    std::string warn;
    
    bool ret;
    if (isBinary) {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    }
    else {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
    }
    
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

void process_vertices(const Model& model, const Accessor& accessor, std::vector<ObjVertex>& objVertices, glm::mat4 CTM) {
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
        objVertices.emplace_back(ObjVertex{CTM * glm::vec4(ptr_to_vec3(memoryLocation), 1.0f) });
    }
}

void process_normals(const Model& model, const Accessor& accessor, std::vector<ObjVertex>& objVertices, size_t cumulativeIndexCount){
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
        objVertices.at(i + cumulativeIndexCount).normal = std::move(ptr_to_vec3(memoryLocation));
    }
}

void process_texcoords(const Model& model, const Accessor& accessor, std::vector<ObjVertex>& objVertices, size_t cumulativeIndexCount){
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
        vec2 v = ptr_to_vec2(memoryLocation);
        v = vec2(v.s, -v.t); //deal with the fact that the obj format's texcoord.t is inverted, and the base code assumes the obj format.
        objVertices.at(i + cumulativeIndexCount).texCoord = std::move(v);
    }

}

void process_indices(const Model& model, const Accessor& accessor, std::vector<ObjMultiShapeGeometry::index_t>& outputIndices, size_t cumulativeIndexCount) {
    assert(
        accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT 
        || accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE
        || accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT);
    assert(accessor.type == TINYGLTF_TYPE_SCALAR);
    
    BufferView bufferView = model.bufferViews[accessor.bufferView];
    int stride = accessor.ByteStride(bufferView);
    size_t offset = accessor.byteOffset + bufferView.byteOffset;

    //the buffer itself.
    Buffer buffer = model.buffers[bufferView.buffer];
    auto data = buffer.data; //the vector of bytes. bufferView and accessor will be used to read it.

    for (int i = 0; i < accessor.count; i++) {
        //convince the compiler that data points to a particular size of data. Move data ptr forward by stride bytes.
        unsigned int val;
        if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            unsigned short* memoryLocation = reinterpret_cast<unsigned short*>(data.data() + offset + (i * static_cast<size_t>(stride)));
            val = *memoryLocation;
        }
        else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            unsigned char* memoryLocation = reinterpret_cast<unsigned char*>(data.data() + offset + (i * static_cast<size_t>(stride)));
            val = *memoryLocation;
        }
        else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            unsigned int* memoryLocation = reinterpret_cast<unsigned int*>(data.data() + offset + (i * static_cast<size_t>(stride)));
            val = *memoryLocation;
        }
        outputIndices.push_back(val + cumulativeIndexCount);
    }

}

//apply parent's transforms to the children
mat4 TreeNode::computeCTM(){
    if (!useEmbeddedTransforms) return mat4(1.0f);
    mat4 builtCTM = CTM; //the node's CTM will still contain only the node-local transform.
    TreeNode* currentParent = parent;
    while (currentParent != nullptr) {
        builtCTM = currentParent->CTM * builtCTM;
        currentParent = currentParent->parent;
    }
    return builtCTM; 
}

void SceneGraph::constructCTMTree(const tinygltf::Model& model, Node input, TreeNode* parent) {
    TreeNode node = TreeNode(input, mat4(1.0f));//initialize a TreeNode with no matrix transforms.

    //apply any individual transforms in order.
    if (node.getNode().translation.size() == 3) {
        node.CTM = glm::translate(node.CTM, vec3(node.getNode().translation[0], node.getNode().translation[1], node.getNode().translation[2]));
    }
    if (node.getNode().rotation.size() == 4) {
        quat q = make_quat(node.getNode().rotation.data());
        
        node.CTM *= mat4(q);
    }
    if (node.getNode().scale.size() == 3) {
        node.CTM = glm::scale(node.CTM, vec3(node.getNode().scale[0], node.getNode().scale[1], node.getNode().scale[2]));
    }
    //There's also a completed matrix transform option, so if the file has that, override the CTM with this new matrix.
    if (node.getNode().matrix.size() == 16) {
        node.CTM = glm::make_mat4x4(node.getNode().matrix.data());
        
    }
    //use recursion to apply the matrix stack concept to this node.getNode()'s children. Not tested currently, as lantern test gltf doesn't have children
    //if the node has children
    if (!node.getNode().children.empty()) {
        for (size_t i = 0; i < node.getNode().children.size(); i++) {
            //construct a new TreeNode that has the child node, and the CTM of the parent
            Node childNode = model.nodes[node.getNode().children[i]];
            //call this function again.
            constructCTMTree(model, childNode, &node);
        }
    }
    if (parent != nullptr) { //this is a child of some other node.
        parent->children.push_back(&node);
    }
    else { //no parent, this is a root node.
        sceneGraph.push_back(node);
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
    
    SceneGraph graph;
    for (const auto& node : model.nodes) {
        graph.constructCTMTree(model, node, nullptr);
    }
    
    
    // Loop over shapes in the gltf file

    //the gltf format's individual primitive indices all start from 0. 
    //This is incompatible with the way the .obj format stores its shape data, where the first index of the next shape starts where the previous shape left off.
    //To make the format consistent, this adds the previous vertex index number to the shape. 
    //E.g. if shape 0 has 30, shape 1 has 60 indices, then shape 2's indices will start from 90 instead of 0.
    std::vector<ObjVertex> objVertices;
    size_t cumulativeIndexCount = 0;
    for (TreeNode& treeNode : graph.data()) {
        //construct the CTM from the node's data.
        glm::mat4 currentTransformMatrix = treeNode.computeCTM();//constructCTM(model, TreeNode(node, mat4(1.0f)));
        if (treeNode.getNode().mesh == -1) {
            continue; //skip this node if it contains no meshes. This should be pretty rare.
        }
        for (const auto& primitive : model.meshes[treeNode.getNode().mesh].primitives) {//shapes in mesh
            assert(primitive.mode == TINYGLTF_MODE_TRIANGLES); //only work with triangle data for now.
            std::vector<ObjMultiShapeGeometry::index_t> outputIndices;

            //determine where our data is
            auto attrMap = primitive.attributes;
            //assume the gltf files have vertex positions and indices.
            vertexIndex = attrMap["POSITION"];
            Accessor vertAcc = model.accessors[vertexIndex];
            cumulativeIndexCount = objVertices.size(); //add in the amount of vertices we added, so the next shape's index starts where we left off.
            process_vertices(model, vertAcc, objVertices, currentTransformMatrix);

            Accessor indexAcc = model.accessors[primitive.indices];


            process_indices(model, indexAcc, outputIndices, cumulativeIndexCount);
            

            //optionally find normal data and include it
            if (attrMap.find("NORMAL") != attrMap.end()) {
                normalIndex = attrMap["NORMAL"];
                Accessor normAcc = model.accessors[normalIndex];
                process_normals(model, normAcc, objVertices, cumulativeIndexCount);
            }

            //optionally find texture data and include it
            if (attrMap.find("TEXCOORD_0") != attrMap.end()) {
                textureIndex = attrMap["TEXCOORD_0"];
                Accessor texAcc = model.accessors[textureIndex];
                process_texcoords(model, texAcc, objVertices, cumulativeIndexCount);
            }

            ivGeoOut.addShape(outputIndices);
        }
    }
    ivGeoOut.setVertices(objVertices);
}
