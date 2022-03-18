#include "load_gltf.h"

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
    
}