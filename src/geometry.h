#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <glm/glm.hpp>
#include <tiny_obj_loader.h>

#include "data/VertexGeometry.h"
#include "data/VertexInput.h"


struct index_t : public tinyobj::index_t {
    index_t() = default;
    index_t(const tinyobj::index_t& aOther) : tinyobj::index_t(aOther) {}

    friend bool operator==(const index_t& a, const index_t& b) { return(a.vertex_index == b.vertex_index && a.normal_index == b.normal_index && a.texcoord_index == b.texcoord_index); }
    friend bool operator!=(const index_t& a, const index_t& b) { return(!operator==(a, b)); }
};

template<>
struct std::hash<index_t> {
    size_t operator()(const index_t& aIndexBundle) const noexcept {
        return(
            ((std::hash<int>()(aIndexBundle.vertex_index)
                ^
                (std::hash<int>()(aIndexBundle.normal_index) << 1)) >> 1)
            ^
            (std::hash<int>()(aIndexBundle.texcoord_index) << 1)
            );
    }
};

/// glm doesn't support contruction from pointer, so we'll fake it.
static glm::vec3 ptr_to_vec3(const float* aData) {
    return(glm::vec3(
        aData[0],
        aData[1],
        aData[2]
    ));
}

/// glm doesn't support contruction from pointer, so we'll fake it. 
static glm::vec2 ptr_to_vec2(const float* aData) {
    return(glm::vec2(
        aData[0],
        aData[1]
    ));
}

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

#endif
