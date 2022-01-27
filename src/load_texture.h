#pragma once
/*
 *	This class will hold texture data for a vkInstance.
 */
#include <string>
#include <memory>
#include <utility>
#include <vector>
#include <iostream>

#include "vkutils/vkutils.h"
//vulkan types
#include <vulkan/vulkan.h>
//stb_image.h

#include <unordered_map>

class Texture { //TODO add support for mipmapping
public:
	VkDevice device;
	int width = 0;
	int height = 0;
	int numTextureChannels = 0;
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	VkImage image;
	VkDeviceMemory imageMemory;
	Texture() : device(VK_NULL_HANDLE), stagingBuffer(VK_NULL_HANDLE), stagingBufferMemory(VK_NULL_HANDLE), image(VK_NULL_HANDLE), imageMemory(VK_NULL_HANDLE) {};
	Texture(VkDevice device) : device(device), stagingBuffer(VK_NULL_HANDLE), stagingBufferMemory(VK_NULL_HANDLE), image(VK_NULL_HANDLE), imageMemory(VK_NULL_HANDLE){};
	~Texture();
	void createImage(VulkanDeviceBundle deviceBundle);
	VkImageCreateInfo initVkImageCreateInfo();
};

class TextureLoader
{	
public:
	TextureLoader(VulkanDeviceBundle deviceBundle);
	TextureLoader();
	~TextureLoader();
	

	//given a textureName mnemonic, and a path to an image file, constructs a VkImage, allocates device memory and staging buffer memory.
	void createTextureImage(std::string textureName, std::string imagePath);
	const Texture getTexture(std::string textureName) const { return textures.at(textureName); }
	void setup(VkCommandPool commandPool);
	void cleanup();
private:
	VulkanDeviceBundle deviceBundle; //TODO provide functions to update device bundle, if necessary in the future
	VkCommandPool commandPool; //TODO provide functions to update command pool, if necessary in the future
	//texture data, held in a map and accessed by a user-provided string mnemonic
	std::unordered_map<std::string, Texture> textures;
	//private helper functions
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags propertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	//consider learning about and using vkutils::QueueClosure();
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
};




