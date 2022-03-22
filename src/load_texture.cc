/*
* contents adapted from https://vulkan-tutorial.com/
* Creative Commons 0 Licensing information: https://creativecommons.org/publicdomain/zero/1.0/
*/

#include "load_texture.h"
#include "VulkanGraphicsApp.h"
#include "vkutils/VmaHost.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


using namespace std;

TextureLoader::TextureLoader(VulkanDeviceBundle deviceBundle) :
    deviceBundle(deviceBundle), commandPool(VK_NULL_HANDLE){}

TextureLoader::TextureLoader() : 
    deviceBundle(),
    commandPool(VK_NULL_HANDLE) {}

TextureLoader::~TextureLoader() {}



std::vector<VkDescriptorSetLayoutBinding> TextureLoader::getDescriptorSetLayoutBindings(int bindingNum) const {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = bindingNum;
    samplerBinding.descriptorCount = TEXTURE_ARRAY_SIZE;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.pImmutableSamplers = nullptr;
    samplerBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    
    bindings.push_back(samplerBinding);

    return(bindings);
}



//returns the first available memory type index for a given physicalDevice
uint32_t findMemoryType(VulkanDeviceBundle deviceBundle, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(deviceBundle.physicalDevice.handle(), &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (typeFilter & (1 << i) && memProperties.memoryTypes[i].propertyFlags & properties) {
            return i;
        }
    }
    cerr << "failed to find suitable memory type. Consider expanding typeFilter, or reexamining properties." << endl;
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


const Texture* TextureLoader::getTexture(uint32_t index) const
{
    if (index > mInstanceCount - 1 || index < 0) {
        cerr << "invalid index, textureLoader contains only" << mInstanceCount << " valid indices" << endl;
        exit(1);
    }
    return &textures[index];
}

std::array<VkDescriptorImageInfo, TextureLoader::TEXTURE_ARRAY_SIZE> TextureLoader::getDescriptorImageInfos(){
    std::array<VkDescriptorImageInfo, TEXTURE_ARRAY_SIZE> infos;
    for (uint32_t i = 0; i < textures.size(); i++) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textures[i].imageView;
        imageInfo.sampler = textures[i].sampler;

        infos[i] = imageInfo;
    }
    for (uint32_t i = textures.size(); i < TEXTURE_ARRAY_SIZE; i++) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textures[0].imageView;
        imageInfo.sampler = textures[0].sampler;

        infos[i] = imageInfo;
    }
    return(infos);
}

void TextureLoader::createTexture(string imagePath){
    if (commandPool == VK_NULL_HANDLE) {
        throw TextureLoaderException( "TextureLoader::setup() must be called with a valid command pool, before creating texture images.");
    }
    if (textures.size() == TEXTURE_ARRAY_SIZE) {
        throw TextureLoaderException("TextureLoader::createTexture has created the maximum amount of textures for the internal texture array,"
            " including the debug texture at location 0. Increase the size of the texture array. (defined in TextureLoader::TEXTURE_ARRAY_SIZE)");
    }
    textures.emplace_back(Texture(deviceBundle.logicalDevice.handle()));
    stbi_uc* pixels = stbi_load(
        imagePath.c_str(), //the path of the image
        &textures.back().width, //width of the image
        &textures.back().height, //height of the image
        &textures.back().numTextureChannels, //the actual number of channels in the raw image
        STBI_rgb_alpha); //adds an alpha channel to the image for formatting consistency
    if (!pixels) {
        cerr << "failed to load texture: " << imagePath << endl; //TODO maybe throw exception here
        exit(1);
    }
    VkDeviceSize imageSize = (VkDeviceSize)textures.back().width * textures.back().height * STBI_rgb_alpha;
    
    createBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        textures.back().stagingBuffer,
        textures.back().stagingBufferMemory);

    void* data;
    vkMapMemory(deviceBundle.logicalDevice.handle(), textures.back().stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(deviceBundle.logicalDevice.handle(), textures.back().stagingBufferMemory);
    stbi_image_free(pixels);

    textures.back().createImage(deviceBundle);
    transitionImageLayout(textures.back().image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(textures.back().stagingBuffer, textures.back().image, static_cast<uint32_t>(textures.back().width), static_cast<uint32_t>(textures.back().height));
    transitionImageLayout(textures.back().image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(deviceBundle.logicalDevice.handle(), textures.back().stagingBuffer, nullptr);
    vkFreeMemory(deviceBundle.logicalDevice.handle(), textures.back().stagingBufferMemory, nullptr);

    textures.back().createImageView();
    textures.back().createSampler();
    mInstanceCount++;
}

void TextureLoader::createDebugTexture() {
    if (commandPool == VK_NULL_HANDLE) {
        throw TextureLoaderException("TextureLoader::setup() must be called with a valid command pool, before creating texture images.");
    }

    textures.emplace_back(Texture(deviceBundle.logicalDevice.handle()));
    int height = 2, width = 2, channels = 4;
    stbi_uc* pixels = new unsigned char[height * width * channels]; //height, width, channel number
    for (int i = 0; i < height * width * channels;) {
        //purple
        pixels[i++] = 255;
        pixels[i++] = 0;
        pixels[i++] = 255;
        pixels[i++] = 255;
    }
    textures.back().width = width;
    textures.back().height = height;
    textures.back().numTextureChannels = STBI_rgb_alpha;
    VkDeviceSize imageSize = (VkDeviceSize)textures.back().width * textures.back().height * STBI_rgb_alpha;

    createBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        textures.back().stagingBuffer,
        textures.back().stagingBufferMemory);

    void* data;
    vkMapMemory(deviceBundle.logicalDevice.handle(), textures.back().stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(deviceBundle.logicalDevice.handle(), textures.back().stagingBufferMemory);
    stbi_image_free(pixels);

    textures.back().createImage(deviceBundle);
    transitionImageLayout(textures.back().image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(textures.back().stagingBuffer, textures.back().image, static_cast<uint32_t>(textures.back().width), static_cast<uint32_t>(textures.back().height));
    transitionImageLayout(textures.back().image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(deviceBundle.logicalDevice.handle(), textures.back().stagingBuffer, nullptr);
    vkFreeMemory(deviceBundle.logicalDevice.handle(), textures.back().stagingBufferMemory, nullptr);

    textures.back().createImageView();
    textures.back().createSampler();
    mInstanceCount++;
}

void TextureLoader::setup(VkCommandPool commandPool){
    this->commandPool = commandPool;
    //Consider using a fallback texture, like this transparent image. Or bright solid white, depending on the background.
    createDebugTexture();
}

void TextureLoader::cleanup(){
    
    
    //free texture data
    for (auto tex : textures) {
        vkDestroySampler(tex.device, tex.sampler, nullptr);
        //free the texture image view, must be done before freeing the image itself
        vkDestroyImageView(tex.device, tex.imageView, nullptr);
        //free the texture image
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

//takes a created image (from createImage) and creates an image view from it
void Texture::createImageView(){
    VkImageViewCreateInfo imageViewInfo{};
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.image = image;
    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewInfo.subresourceRange.baseMipLevel = 0;
    imageViewInfo.subresourceRange.levelCount = 1;
    imageViewInfo.subresourceRange.baseArrayLayer = 0;
    imageViewInfo.subresourceRange.layerCount = 1;

    if (VK_SUCCESS != vkCreateImageView(device, &imageViewInfo, nullptr, &imageView)) {
        cerr << "failed to create texture image view" << endl;
        exit(1);
    }
}

//for now, use a pretty generic sampler for all textures. Can overload to provide more options, and potentially move a set of common
//samplers into the TextureLoader class.
void Texture::createSampler(){
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST; //No filtering. Other option is VK_FILTER_LINEAR for bilinear filtering
    samplerInfo.minFilter = VK_FILTER_NEAREST; //No filtering, Other option is VK_FILTER_LINEAR for anisotropic filtering
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    //you can query the physical device to check the max available anisotropy, but not enabling it for now.
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    //set to true if you wish to address coordinates from [0,texWidth) and [0, texHeight). False indicates [0,1) on both axes, which is more common.
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    //Used for shadow-mapping
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    //mip-mapping
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    
    if (VK_SUCCESS != vkCreateSampler(device, &samplerInfo, nullptr, &sampler)) {
        cerr << "failed to create texture sampler" << endl;
        exit(1);
    }
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

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

void TextureLoader::transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout){
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; //pipeline barrier
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
