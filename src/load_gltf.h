#ifndef LOAD_GLTF_H
#define LOAD_GLTF_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <memory>
#include <future>

#include "geometry.h"
#include "tiny_gltf.h"

enum e_ACCESSOR_TYPE
{
    VERTEX, NORMAL, TEXTURE
};

class TreeNode
{
public:
    TreeNode(tinygltf::Node node, glm::mat4 CTM) : node(node), CTM(CTM), parent(nullptr), useEmbeddedTransforms(true) {};
    tinygltf::Node& getNode() { return node; }
    glm::mat4 computeCTM();

    glm::mat4 CTM;
    std::shared_ptr<TreeNode> parent;
    std::vector<std::shared_ptr<TreeNode>> children;
private:
    tinygltf::Node node;
    bool useEmbeddedTransforms;
};

class SceneGraph
{
public:
    void constructCTMTree(const tinygltf::Model& model, std::shared_ptr<tinygltf::Node> input, std::shared_ptr<TreeNode> parent);
    
    std::vector<std::shared_ptr<TreeNode>>& data() { return sceneGraph; }
private:
    std::vector<std::shared_ptr<TreeNode>> sceneGraph;
};

//a node in the scene graph tree. Wraps tinygltf::Node with some tree traversal and extracts CTM data from tinygltf::model.


ObjMultiShapeGeometry load_gltf_to_vulkan(const VulkanDeviceBundle& aDeviceBundle, std::string filename, bool isBinary);
void process_gltf_contents(tinygltf::Model& model, ObjMultiShapeGeometry& ivGeoOut);
#endif 