#pragma once
/*
 *	This class will hold texture data for a vkInstance.
 */
#include <string>
#include <memory>
#include <utility>
#include <vector>
#include <iostream>

//vulkan types
#include <vulkan/vulkan.h>
//stb_image.h
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"



class TextureLoader
{
public:
	TextureLoader();
	~TextureLoader();
	void createTextureBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags propertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void createTextureImage(std::string imagePath);
	void createTextureImage(std::vector<std::string> imagePaths);
private:
	std::unique_ptr<std::vector<std::pair<int, int>>>  texSizes;
	std::unique_ptr <std::vector<int>> texChannels;
	std::vector<VkBuffer> stagingBuffers;
	std::vector<VkDeviceMemory> stagingBufferMemory;
	
};




