#ifndef VULKAN_GRAPHICS_APP_H_
#define VULKAN_GRAPHICS_APP_H_
#include "application/VulkanSetupCore.h"
#include "application/SwapchainProvider.h"
#include "application/RenderProvider.h"
#include "vkutils/vkutils.h"
#include "data/VertexGeometry.h"
#include "data/UniformBuffer.h"
#include "data/MultiInstanceUniformBuffer.h"
#include "data/MultiInstanceCombinedImageSampler.h"
#include "load_obj.h"
#include "load_texture.h"
#include "utils/common.h"
#include <map>
#include <memory>

///////////////////////////////////////////////////////////////////////////////////////////////////
// The code below defines the types and formatting for uniform data used in our shaders. 
///////////////////////////////////////////////////////////////////////////////////////////////////
//TODO move the View matrix into a struct, possibly Transforms, that will change between draw calls.
// Uniform data that applies to the entire scene, and will not change between draw calls.
struct WorldInfo {
    alignas(16) glm::mat4 View;
    alignas(16) glm::mat4 Perspective;
    glm::vec4 lightPosition[8] = {
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
        glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f),
        glm::vec4(1.0f, 1.0f, -1.0f, 1.0f),
        glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f),
        glm::vec4(1.0f, -1.0f, 1.0f, 1.0f),
        glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f),
        glm::vec4(1.0f, -1.0f, -1.0f, 1.0f),
        glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f)
    };
};

// Model transform matrix which will be different for each object / draw call.
struct Transforms {
    alignas(16) glm::mat4 Model;
};

// Additional uniform data that varies per-object / per-draw.
struct AnimShadeData {
    glm::vec4 diffuseData = glm::vec4(1.0f);
    glm::vec4 ambientData = glm::vec4(0.1f, 0.1f, 0.1f, 0.0f);
    glm::vec4 specularData = glm::vec4(1.0f);
    float shininess = 300.0f;
    uint32_t shadingLayer = 0;
    uint32_t textureIndex = -1; //debug texture
    //default steely material
    AnimShadeData() {};
    //textures only
    AnimShadeData(uint32_t mode, uint32_t texIdx) : shadingLayer(mode), textureIndex(texIdx) {}
    //Use to edit diffuse and shininess only
    AnimShadeData(glm::vec4 dif, float shn, uint32_t mode) :
        diffuseData(dif), shininess(shn), shadingLayer(mode) {}
    //Use to fully control material properties
    AnimShadeData(glm::vec4 dif, glm::vec4 amb, glm::vec4 spc, float shn, uint32_t mode) :
        diffuseData(dif), ambientData(amb), specularData(spc), shininess(shn), shadingLayer(mode) {}
};


// Type definitions wrapping around our structures to make them usable as uniform data
using UniformWorldInfo = UniformStructData<WorldInfo>;
using UniformTransformData = UniformStructData<Transforms>;
using UniformAnimShadeData = UniformStructData<AnimShadeData>;

using UniformWorldInfoPtr = std::shared_ptr<UniformWorldInfo>;
using UniformTransformDataPtr = std::shared_ptr<UniformTransformData>;
using UniformAnimShadeDataPtr = std::shared_ptr<UniformAnimShadeData>;



class VulkanGraphicsApp : virtual public VulkanAppInterface, public CoreLink{
 public:
    /// Default constructor runs full initCore() immediately. Use protected no-init constructor
    /// it this is undesirable. 
    VulkanGraphicsApp() {initCore();}

    void init();
    void render();
    void cleanup();
    
    GLFWwindow* getWindowPtr() const {return(mSwapchainProvider->getWindowPtr());}
    const VkExtent2D& getFramebufferSize() const;
    size_t getFrameNumber() const {return(mFrameNumber);}

    /// Setup the uniform buffer that will be used by all MultiShape objects in the scene
    /// 'aUniformLayout' specifies the layout of uniform data available to all instances.
    void initMultis(const UniformDataLayoutSet& aUniformLayout);
    /// Add loaded obj and a set of interfaces for its uniform data
    void addMultiShapeObject(const ObjMultiShapeGeometry& mObject, const std::vector<UniformDataInterfaceSet>& aUniformData);

    void addSingleInstanceUniform(uint32_t aBindPoint, const UniformDataInterfacePtr& aUniformInterface);

    void setVertexShader(const std::string& aShaderName, const VkShaderModule& aShaderModule);
    void setFragmentShader(const std::string& aShaderName, const VkShaderModule& aShaderModule);



    const VkCommandPool getCommandPool() const { return mCommandPool; }
    TextureLoader textureLoader;
 protected:
    friend class VulkanProviderInterface;
    
    virtual const VkApplicationInfo& getAppInfo() const override;
    virtual const std::vector<std::string>& getRequestedValidationLayers() const override;

    /// Collection describing the overall layout of all uniform data being used. 
    UniformDataLayoutSet mUniformLayoutSet;

    /// Collection of objects in our scene. Each is geometry loaded from an .obj file.
    std::unordered_map<std::string, ObjMultiShapeGeometry> mObjects;
    /// Collection of model transform data. Contains an entry for each object in mObjects.
    std::unordered_map<std::string, UniformTransformDataPtr> mObjectTransforms;
    /// Collection of extra per-object data. Contains an entry for each object in mObjects.
    std::unordered_map<std::string, UniformAnimShadeDataPtr> mObjectAnimShade;

#ifdef CPE471_VULKAN_SAFETY_RAILS
 private:
#else
 protected:
#endif

    VulkanGraphicsApp(bool noInitCore){if(!noInitCore) initCore();}

    void initCore(); 

    void initCommandPool(); 
    void initTextures();
    void initRenderPipeline();
    void initFramebuffers();
    void initCommands();
    void initSync();

    void resetRenderSetup();
    void cleanupSwapchainDependents();

    void initTransferCmdBuffer();
    void transferGeometry();

    void initUniformResources();
    void initUniformDescriptorPool();
    void allocateDescriptorSets();
    void writeDescriptorSets();
    void reinitUniformResources();

    size_t mFrameNumber = 0;

    std::shared_ptr<VulkanSetupCore> mCoreProvider = nullptr; // Shadows CoreLink::mCoreProvider 
    std::shared_ptr<SwapchainProvider> mSwapchainProvider = nullptr;

    const static int IN_FLIGHT_FRAME_LIMIT = 2;
    std::vector<VkFramebuffer> mSwapchainFramebuffers;
    std::vector<VkSemaphore> mImageAvailableSemaphores;
    std::vector<VkSemaphore> mRenderFinishSemaphores;
    std::vector<VkFence> mInFlightFences;

    vkutils::VulkanBasicRasterPipelineBuilder mRenderPipeline;
    vkutils::VulkanDepthBundle mDepthBundle;

    VkCommandPool mCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> mCommandBuffers;
    VkCommandBuffer mTransferCmdBuffer = VK_NULL_HANDLE;

    std::unordered_map<std::string, VkShaderModule> mShaderModules;
    std::string mVertexKey;
    std::string mFragmentKey;

    std::vector<ObjMultiShapeGeometry> mMultiShapeObjects;

    
    std::shared_ptr<MultiInstanceUniformBuffer> mMultiUniformBuffer = nullptr;
    
    UniformBuffer mSingleUniformBuffer; 

    VkDeviceSize mTotalUniformDescriptorSetCount = 0;
    VkDescriptorPool mResourceDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout mUniformDescriptorSetLayout;
    std::vector<VkDescriptorSet> mUniformDescriptorSets;
    
};

#endif
