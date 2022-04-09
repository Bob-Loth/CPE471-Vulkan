# CPE471-VulkanBase-ObjLoading

## Cheatsheet

Take a look at main.cc::AnimShadeData initializations, possibly change default to not be so dark on textured objects.

main.cc::initGeometry
	Load objects using load_obj.cc::load_obj_to_vulkan
	Initialize object transforms using UniformTransformData::create()
	initialize shading options/texture indices using UniformAnimShadeData::create();
	UniformStructData has a setStruct method that synchronizes changes in struct data with the GPU memory.
	Application::recordShadingLayers must be called before pressing keys. Currently, the key callbacks are registered, then the shading layers are recorded, then the window initializes, so it might break the recording aspect if keys are held in-between recording layers and window initialization. 
	VulkanGraphicsApp::addMultiShapeObject adds the object vbo data, and any non-texture uniforms (the base code). Texture data loading is handled by textureLoader, which places the binding point after the last binding point specified in the original descriptor set layouts.

VulkanGraphicsApp::initTextures
	Currently using texture arrays, with 16 as the current maximum. If more are desired, or if this does not run on certain hardware, I’ll need to query what features the device supports, and provide a fallback implementation. From what I’ve read, some mobile devices do not support texture arrays, but their usage is widely implemented in modern desktop and laptop processors, with most devices supporting arrays of at least size 80. 
	The texture created in the zeroth position of textureLoader serves as a fallback texture, where any object not explicitly given a texture index will use this texture.
	Textures created by the programmer will be stored in indexes 1-15, and addressed from 0-14, to prevent unintended access to the debug texture. There is also an exception that is thrown if the programmer tries allotting more than TEXTURE_ARRAY_SIZE - 1 textures. See current implementation of TextureLoader::getDescriptorImageInfos.

### glTF loader features
the current iteration of the glTF loader supports: 
- reading glTF nodes, 
- constructing a tree of nodes,
- Applying the current node's matrix transforms to current and child nodes,
- reading in vertex+index data
- reading in any existing normal data
- reading in the first set of texture coordinate data (glTF supports multiple texture mappings)


Resources for Vulkan middleware
https://github.com/vinjn/awesome-vulkan
