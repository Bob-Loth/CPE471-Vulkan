#include "load_texture.h"
#include "VulkanGraphicsApp.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


using namespace std;

TextureLoader::TextureLoader(VulkanDeviceBundle deviceBundle) :
    deviceBundle(deviceBundle), commandPool(VK_NULL_HANDLE){}

TextureLoader::TextureLoader() : 
    deviceBundle(),
    commandPool(VK_NULL_HANDLE) {}

TextureLoader::~TextureLoader() {}

//returns the first available memory type index for a given physicalDevice
uint32_t findMemoryType(VulkanDeviceBundle deviceBundle, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(deviceBundle.physicalDevice.handle(), &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (typeFilter & (1 << i) && memProperties.memoryTypes[i].propertyFlags & properties) {
            return i;
        }
    }
    cerr << "failed to fine suitable memory type. Consider expanding typeFilter, or reexamining properties." << endl;
    exit(1);
}

//load a single texture
void TextureLoader::createBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags propertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, //the type of this structure
        nullptr, // no structure extending this structure
        VkBufferCreateFlags(),
        size,
        bufferUsage,
        VK_SHARING_MODE_EXCLUSIVE, //no access from other queue families, see https://vulkan.lunarg.com/doc/view/1.2.154.1/windows/tutorial/html/03-init_device.html
        0, //number of entries in the pQueueFamilyIndices array
        nullptr //set to null if in EXCLUSIVE sharing mode
    };
    if (VK_SUCCESS != vkCreateBuffer(deviceBundle.logicalDevice.handle(), &bufferInfo, nullptr, &buffer)) {
        cerr << "failed to create buffer" << endl;
        exit(1);
    }
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(deviceBundle.logicalDevice.handle(), buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        NULL,
        memRequirements.size,
        findMemoryType(deviceBundle, memRequirements.memoryTypeBits, propertyFlags)
    };

    if (VK_SUCCESS != vkAllocateMemory(deviceBundle.logicalDevice.handle(), &allocInfo, nullptr, &bufferMemory)) {
        cerr << "failed to allocate buffer memory" << endl;
        exit(1);
    }
    vkBindBufferMemory(deviceBundle.logicalDevice.handle(), buffer, bufferMemory, 0);
}



void TextureLoader::createTextureImage(string textureName, string imagePath){
    textures[textureName] = Texture(deviceBundle.logicalDevice.handle());
    stbi_uc* pixels = stbi_load(
        imagePath.c_str(), //the path of the image
        &textures[textureName].width, //width of the image
        &textures[textureName].height, //height of the image
        &textures[textureName].numTextureChannels, //the actual number of channels in the raw image
        STBI_rgb_alpha); //adds an alpha channel to the image for formatting consistency
    if (!pixels) {
        cerr << "failed to load texture: " << imagePath << endl; //TODO maybe throw exception here
        exit(1);
    }
    VkDeviceSize imageSize = textures[textureName].width * textures[textureName].height * STBI_rgb_alpha;
    
    createBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        textures[textureName].stagingBuffer,
        textures[textureName].stagingBufferMemory);

    void* data;
    vkMapMemory(deviceBundle.logicalDevice.handle(), textures[textureName].stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(deviceBundle.logicalDevice.handle(), textures[textureName].stagingBufferMemory);
    stbi_image_free(pixels);

    textures[textureName].createImage(deviceBundle);
    transitionImageLayout(textures[textureName].image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(textures[textureName].stagingBuffer, textures[textureName].image, static_cast<uint32_t>(textures[textureName].width), static_cast<uint32_t>(textures[textureName].height));
    transitionImageLayout(textures[textureName].image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(deviceBundle.logicalDevice.handle(), textures[textureName].stagingBuffer, nullptr);
    vkFreeMemory(deviceBundle.logicalDevice.handle(), textures[textureName].stagingBufferMemory, nullptr);
}

void TextureLoader::setup(VkCommandPool commandPool){
    this->commandPool = commandPool;
}

void TextureLoader::cleanup(){
    for (auto texture : textures) {
        auto tex = texture.second;
        vkDestroyImage(tex.device, tex.image, nullptr);
        vkFreeMemory(tex.device, tex.imageMemory, nullptr);
    }
}


Texture::~Texture(){
    
}

void Texture::createImage(VulkanDeviceBundle deviceBundle) {
    VkImageCreateInfo info = initVkImageCreateInfo();
    if (VK_SUCCESS != vkCreateImage(deviceBundle.logicalDevice.handle(), &info, nullptr, &image)) {
        cerr << "failed to create texture image" << endl;
        exit(1);
    }
    
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(deviceBundle.logicalDevice.handle(), image, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(deviceBundle, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (VK_SUCCESS != vkAllocateMemory(deviceBundle.logicalDevice.handle(), &allocInfo, nullptr, &imageMemory)) {
        cerr << "failed to allocate image memory" << endl;
        exit(1);
    }

    vkBindImageMemory(deviceBundle.logicalDevice.handle(), image, imageMemory, 0);
}

VkCommandBuffer TextureLoader::beginSingleTimeCommands(){
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(deviceBundle.logicalDevice.handle(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void TextureLoader::endSingleTimeCommands(VkCommandBuffer commandBuffer){
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(deviceBundle.logicalDevice.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(deviceBundle.logicalDevice.getGraphicsQueue());

    vkFreeCommandBuffers(deviceBundle.logicalDevice.handle(), commandPool, 1, &commandBuffer);
}

void TextureLoader::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height){
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0,0,0 };
    region.imageExtent = { width, height, 1 };
    endSingleTimeCommands(commandBuffer);
}

void TextureLoader::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout){
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    endSingleTimeCommands(commandBuffer);
}

VkImageCreateInfo Texture::initVkImageCreateInfo() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = static_cast<uint32_t>(width);
    imageInfo.extent.height = static_cast<uint32_t>(height);
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    return imageInfo;
}