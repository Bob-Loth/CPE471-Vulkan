#ifndef LOAD_GLTF_H
#define LOAD_GLTF_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "geometry.h"
#include "tiny_gltf.h"

enum e_ACCESSOR_TYPE
{
    VERTEX, NORMAL, TEXTURE
};

ObjMultiShapeGeometry load_gltf_to_vulkan(const VulkanDeviceBundle& aDeviceBundle, std::string filename);
void process_gltf_contents(tinygltf::Model& model, ObjMultiShapeGeometry& ivGeoOut);
#endif 