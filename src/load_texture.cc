#include "load_texture.h"



using namespace std;

TextureLoader::TextureLoader() {
    texSizes = make_unique<vector<pair<int, int>>>();
    texChannels = make_unique<vector<int>>();
}

TextureLoader::~TextureLoader() {
    texSizes = nullptr;
    texChannels = nullptr;
}
//load a single texture

void TextureLoader::createTextureBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags propertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
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
    

}

void TextureLoader::createTextureImage(string imagePath){


    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    



    texSizes->emplace_back(make_pair<int, int>(0,0));
    texChannels->emplace_back(0);
    stbi_uc* pixels = stbi_load(
        imagePath.c_str(), //the path of the image
        &texSizes->back().first, //width of the image
        &texSizes->back().second, //height of the image
        &texChannels->back(), //the actual number of channels in the raw image
        STBI_rgb_alpha); //adds an alpha channel to the image for formatting consistency
    if (!pixels) {
        cerr << "failed to load texture: " << imagePath << endl; //TODO maybe throw exception here
    }
    else {

    }
}




//load multiple textures with a single call to createTextureImage
void TextureLoader::createTextureImage(vector<string> imagePaths) {
    throw std::runtime_error("unfinished function call");
}