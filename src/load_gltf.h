#ifndef LOAD_GLTF_H
#define LOAD_GLTF_H

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <glm/glm.hpp>

#include "data/VertexGeometry.h"
#include "data/VertexInput.h"
#include "tiny_gltf.h"

using namespace tinygltf;

struct ObjVertex {
    glm::vec3 position{ 0.0f, 0.0f, 0.0f };
    glm::vec3 normal{ 0.0f, 0.0f, 0.0f };
    glm::vec2 texCoord{ 0.0f, 0.0f };
};

using ObjMultiShapeGeometry = MultiShapeGeometry<ObjVertex, uint32_t>;
using ObjVertexInput = VertexInputTemplate<ObjVertex>;

const static ObjVertexInput sObjVertexInput(
    0, // Binding point 
    { // Vertex input attributes
       VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ObjVertex, position)},
       VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ObjVertex, normal)},
       VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ObjVertex, texCoord)}
    }
);

ObjMultiShapeGeometry load_gltf_to_vulkan(const VulkanDeviceBundle& aDeviceBundle, std::string filename);
void process_gltf_contents(Model& model, ObjMultiShapeGeometry& ivGeoOut);
#endif 