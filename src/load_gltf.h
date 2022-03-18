#ifndef LOAD_GLTF_H
#define LOAD_GLTF_H

#include <glm/glm.hpp>

#include "geometry.h"
#include "tiny_gltf.h"


ObjMultiShapeGeometry load_gltf_to_vulkan(const VulkanDeviceBundle& aDeviceBundle, std::string filename);
void process_gltf_contents(tinygltf::Model& model, ObjMultiShapeGeometry& ivGeoOut);
#endif 