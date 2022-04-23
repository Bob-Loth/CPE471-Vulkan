#define _DISABLE_EXTENDED_ALIGNED_STORAGE // Fix MSVC warning about binary compatability

#include "VulkanGraphicsApp.h"
#include "data/VertexGeometry.h"
#include "data/UniformBuffer.h"
#include "data/VertexInput.h"
#include "utils/BufferedTimer.h"
#include "load_obj.h"
#include "load_gltf.h"
#include "load_texture.h"

#include <iostream>
#include <limits>
#include <memory> // Include shared_ptr
#include <map>
#include <string>

#define ENABLE_GLM_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/string_cast.hpp>

using namespace std;
enum ShadingLayer { BLINN_PHONG, NORMAL_MAP, TEXTURE_MAP, TEXTURED_FLAT, TEXTURED_SHADED, NO_FORCED_LAYER };
ShadingLayer currentShadingLayer = NO_FORCED_LAYER; /*static initialization problem generates linker error, using global for now*/


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
using UniformWorldInfo     = UniformStructData<WorldInfo>;
using UniformTransformData = UniformStructData<Transforms>;
using UniformAnimShadeData = UniformStructData<AnimShadeData>;

using UniformWorldInfoPtr     = std::shared_ptr<UniformWorldInfo>;
using UniformTransformDataPtr = std::shared_ptr<UniformTransformData>;
using UniformAnimShadeDataPtr = std::shared_ptr<UniformAnimShadeData>;

/// Our application class. Pay attention to the member variables defined at the end. 
class Application : public VulkanGraphicsApp
{
 public:
    void init();
    void run();
    void updateView();
    void updatePerspective();
    void cleanup();

    //updates shading based on key holds
    void observeCurrentShadingLayer();
    //records the original shading.
    void recordShadingLayers();

    static void resizeCallback(GLFWwindow* aWindow, int aWidth, int aHeight);
    static void scrollCallback(GLFWwindow* aWindow, double aXOffset, double aYOffset);
    static void keyCallback(GLFWwindow* aWindow, int key, int scancode, int action, int mods);

 protected:
    void initGeometry();
    void initShaders();
    void initUniforms();
    void render(double dt);

    /// Collection describing the overall layout of all uniform data being used. 
    UniformDataLayoutSet mUniformLayoutSet;

    /// Collection of objects in our scene. Each is geometry loaded from an .obj file.
    std::unordered_map<std::string, ObjMultiShapeGeometry> mObjects;
    /// Collection of model transform data. Contains an entry for each object in mObjects.
    std::unordered_map<std::string, UniformTransformDataPtr> mObjectTransforms;
    /// Collection of extra per-object data. Contains an entry for each object in mObjects.
    std::unordered_map<std::string, UniformAnimShadeDataPtr> mObjectAnimShade;
    //holds the original state of each of the object's shading layer
    std::unordered_map<std::string, ShadingLayer> mKeyCallbackHolds;
    
    
    
    /// An wrapped instance of struct WorldInfo made available automatically as uniform data in our shaders.
    UniformWorldInfoPtr mWorldInfo = nullptr;

    /// Static variables to be updated by glfw callbacks. 
    static float smViewZoom;
    static bool smResizeFlag;
};

float Application::smViewZoom = 7.0f;
bool Application::smResizeFlag = false;

void Application::resizeCallback(GLFWwindow* aWindow, int aWidth, int aHeight){
    smResizeFlag = true;
}

/// Callback for mousewheel scrolling. Used to zoom the view. 
void Application::scrollCallback(GLFWwindow* aWindow, double aXOffset, double aYOffset){
    const float scrollSensitivity = 1.0f;
    smViewZoom = glm::clamp(smViewZoom + float(-aYOffset)*scrollSensitivity, 2.0f, 30.0f);
}

void Application::recordShadingLayers() {
    for (auto& obj : mObjectAnimShade) {
        mKeyCallbackHolds[obj.first] = static_cast<ShadingLayer>(obj.second->getStruct().shadingLayer);
    }
}

void Application::observeCurrentShadingLayer() {
    for (auto& obj : mObjectAnimShade) {
        if (currentShadingLayer == NO_FORCED_LAYER) { //revert to each object's original layer
            obj.second->getStruct().shadingLayer = mKeyCallbackHolds[obj.first];
        }
        else { //observe and apply the current shading layer to all objects
            obj.second->getStruct().shadingLayer = currentShadingLayer;
        }
    }
}

/** Keyboard callback:
 *    G: Toggle cursor grabbing. A grabbed cursor makes controlling the view easier.
 *    F, F11: Toggle fullscreen view.
 *    ESC: Close the application
*/
void Application::keyCallback(GLFWwindow* aWindow, int key, int scancode, int action, int mods){
    if(key == GLFW_KEY_G && action == GLFW_PRESS){
        int mode = glfwGetInputMode(aWindow, GLFW_CURSOR);
        if(mode != GLFW_CURSOR_DISABLED){
            glfwSetInputMode(aWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }else{
            glfwSetInputMode(aWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    //modes start. They are the corresponding enum values, shifted up 1 for use on a keyboard.
    else if (key == GLFW_KEY_1 && action == GLFW_PRESS){ 
        currentShadingLayer = BLINN_PHONG;
    }
    else if (key == GLFW_KEY_1 && action == GLFW_RELEASE) {
        currentShadingLayer = NO_FORCED_LAYER;
    }
    else if (key == GLFW_KEY_2 && action == GLFW_PRESS){
        currentShadingLayer = NORMAL_MAP;
    }
    else if (key == GLFW_KEY_2 && action == GLFW_RELEASE) {
        currentShadingLayer = NO_FORCED_LAYER;
    }
    else if (key == GLFW_KEY_3 && action == GLFW_PRESS){
        currentShadingLayer = TEXTURE_MAP;
    }
    else if (key == GLFW_KEY_3 && action == GLFW_RELEASE) {
        currentShadingLayer = NO_FORCED_LAYER;
    }
    else if (key == GLFW_KEY_4 && action == GLFW_PRESS){
        currentShadingLayer = TEXTURED_FLAT;
    }
    else if (key == GLFW_KEY_4 && action == GLFW_RELEASE) {
        currentShadingLayer = NO_FORCED_LAYER;
    }
    else if (key == GLFW_KEY_5 && action == GLFW_PRESS){
        currentShadingLayer = TEXTURED_SHADED;
    }
    else if (key == GLFW_KEY_5 && action == GLFW_RELEASE){
        currentShadingLayer = NO_FORCED_LAYER;
    }
    //modes end
    else if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS){
        glfwSetWindowShouldClose(aWindow, GLFW_TRUE);
    }
    else if((key == GLFW_KEY_F11 || key == GLFW_KEY_F) && action == GLFW_PRESS){
        GLFWmonitor* monitor = glfwGetWindowMonitor(aWindow);
        static int winLastWidth = 854, winLastHeight = 480;
        
        if(monitor == nullptr){
            // Not fullscreen. Go fullscreen...
            monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if(mode == nullptr){
                std::cerr << "Warning! Unable to go fullscreen because of missing monitor information!" << std::endl;
                return;
            }
            glfwGetWindowSize(aWindow, &winLastWidth, &winLastHeight);
            glfwSetWindowMonitor(aWindow, monitor, 0, 0, mode->width, mode->height, GLFW_DONT_CARE);
        }
        else{
            // Fullscreen, go back to windowed.
            glfwSetWindowMonitor(aWindow, nullptr, 0, 0, winLastWidth, winLastHeight, GLFW_DONT_CARE);
        }
    }
}


int main(){
    Application app;
    app.init();
    app.run();
    app.cleanup();

    return(0);
}

void Application::init(){

    // Set glfw callbacks
    glfwSetWindowSizeCallback(getWindowPtr(), resizeCallback);
    glfwSetScrollCallback(getWindowPtr(), scrollCallback);
    glfwSetKeyCallback(getWindowPtr(), keyCallback);

    // Initialize uniform variables
    initUniforms();
    // Initialize geometry 
    initGeometry();
    // Initialize shaders
    initShaders();
    

    
    // Initialize graphics pipeline and render setup 
    VulkanGraphicsApp::init();
    

}

void Application::run(){
    FpsTimer globalRenderTimer(0);

    GLFWwindow* window = getWindowPtr();

    // Run until the application is closed
    while(!glfwWindowShouldClose(window)){
        // Poll for window events, keyboard and mouse button presses, ect...
        glfwPollEvents();
        //set shading layers based on polled events
        observeCurrentShadingLayer();
        // Update view matrix
        updateView();

        // Render the frame 
        globalRenderTimer.frameStart();
        render(globalRenderTimer.lastStepTime()*1e-6);
        globalRenderTimer.frameFinish();

        // Adjust the viewport if window is resized
        if(smResizeFlag){
            updatePerspective();
        }
    }

    std::cout << "Average Performance: " << globalRenderTimer.getReportString() << std::endl;
    
    // Make sure the GPU is done rendering before exiting. 
    vkDeviceWaitIdle(VulkanGraphicsApp::getPrimaryDeviceBundle().logicalDevice.handle());
}

/// Update view matrix from orbit camera controls 
void Application::updateView(){
    const float xSensitivity = 1.0f/glm::pi<float>();
    const float ySensitivity = xSensitivity;
    const float thetaLimit = glm::radians(89.99f);
    static glm::dvec2 lastPos = glm::dvec2(std::numeric_limits<double>::quiet_NaN());

    glm::dvec2 pos;
    glfwGetCursorPos(getWindowPtr(), &pos.x, &pos.y);
    glm::vec2 delta = pos - lastPos;

    // If this is the first frame, set delta to zero. 
    if(glm::isnan(lastPos.x)){
        delta = glm::vec2(0.0);
    }

    lastPos = pos;

    static float theta, phi;

    phi += glm::radians(delta.x * xSensitivity);
    theta = glm::clamp(theta + glm::radians(delta.y*ySensitivity), -thetaLimit, thetaLimit);

    assert(mWorldInfo != nullptr);

    glm::vec3 eye = smViewZoom*glm::normalize(glm::vec3(cos(phi)*cos(theta), sin(theta), sin(phi)*cos(theta)));
    glm::vec3 look = glm::vec3(0.0);
    mWorldInfo->getStruct().View = glm::lookAt(eye, look, glm::vec3(0.0, 1.0, 0.0));
}

/// Update perspective matrix
void Application::updatePerspective(){
    double width = static_cast<double>(getFramebufferSize().width);
    double height = static_cast<double>(getFramebufferSize().height);
    glm::mat4 P = glm::perspectiveFov(glm::radians(75.0), width, height, .01, 100.0);
    P[1][1] *= -1; // Vulkan uses a flipped y-axis
    mWorldInfo->getStruct().Perspective = P;
}

void Application::cleanup(){
    
    // Let base class handle  cleanup.
    VulkanGraphicsApp::cleanup();
}

/// Animate the objects within our scene and then render it. 
void Application::render(double dt){
    using glm::sin;
    using glm::cos;
    using glm::vec3;

    // Get pointers to the individual transforms for each object in the scene
    
    static UniformTransformDataPtr logoTfs = mObjectTransforms["vulkan"];
    static UniformTransformDataPtr monkeyTfs = mObjectTransforms["monkey"];
    static UniformTransformDataPtr bunnyTfs = mObjectTransforms["bunny"];
    static UniformTransformDataPtr teapotTfs = mObjectTransforms["teapot"];
    static UniformTransformDataPtr ballTfs = mObjectTransforms["ballTex"];
    static UniformTransformDataPtr cubeTfs = mObjectTransforms["cube"];
    
    static UniformTransformDataPtr lanternTfs = mObjectTransforms["lantern"];
    static UniformTransformDataPtr orientationTestTfs = mObjectTransforms["OrientationTest"];
    static UniformTransformDataPtr dummyTfs = mObjectTransforms["dummy"];
    // Global time
    float gt = static_cast<float>(glfwGetTime());
    
    // Spin the logo in place. 
    logoTfs->getStruct().Model = glm::scale(vec3(2.5f)) * glm::rotate(float(gt), vec3(0.0, 1.0, 0.0));
    
    // Spin the ball opposite direction of logo, above it.
    ballTfs->getStruct().Model = glm::translate(vec3(0.0, 3.0, 0.0)) * glm::rotate(float(gt), vec3(0.0, -1.0, 0.0));

    // Spin the cube around above both the logo and the ball.
    cubeTfs->getStruct().Model = glm::translate(vec3(0.0, -3.0, 0.0)) * glm::rotate(float(gt), vec3(0.0, -1.0, 0.0)) * glm::scale(vec3(sin(gt), cos(gt), 1));
    
    // move the lantern into the background
    lanternTfs->getStruct().Model = glm::translate(vec3(0.0, 0.0, -2.0)) * glm::scale(vec3(0.2));

    orientationTestTfs->getStruct().Model = glm::translate(vec3(0.0, 0.0, -2.0)) * glm::scale(vec3(0.2));

    //position dummy
    dummyTfs->getStruct().Model = glm::translate(vec3(0.0, 0.0, 2.0)) * glm::rotate(glm::pi<float>()/2, vec3(-1.0, 0.0, 0.0)) * glm::scale(vec3(1.0/25.0));

    // Rotate all other objects around the Vulkan logo in the center
    constexpr float angle = 2.0f*glm::pi<float>()/3.0f; // 120 degrees
    float radius = 4.5f;
    
    monkeyTfs->getStruct().Model = glm::rotate(-float(gt), vec3(0.0, 1.0, 0.0)) * glm::translate(radius*vec3(cos(angle*0), .2f*sin(gt*4.0f+angle*0), sin(angle*0))) * glm::rotate(2.0f*float(gt), vec3(0.0, 1.0, 0.0));
    bunnyTfs->getStruct().Model  = glm::rotate(-float(gt), vec3(0.0, 1.0, 0.0)) * glm::translate(radius*vec3(cos(angle*1), .2f*sin(gt*4.0f+angle*1), sin(angle*1))) * glm::rotate(2.0f*float(gt), vec3(0.0, 1.0, 0.0));
    teapotTfs->getStruct().Model = glm::rotate(-float(gt), vec3(0.0, 1.0, 0.0)) * glm::translate(radius*vec3(cos(angle*2), .2f*sin(gt*4.0f+angle*2), sin(angle*2))) * glm::rotate(2.0f*float(gt), vec3(0.0, 1.0, 0.0));
    
    // Tell the GPU to render a frame. 
    VulkanGraphicsApp::render();
} 


void initBlinnPhongColorMap(unordered_map<string, AnimShadeData> *map) {
    (*map)["cyan"] = AnimShadeData(
        glm::vec4(0.0, 1.0, 1.0, 1.0),
        glm::vec4(0.05, 0.05, 0.05, 1.0),
        glm::vec4(0.5, 0.5, 0.5, 1.0),
        100.0f,
        false
    );
    (*map)["red"] = AnimShadeData(
        glm::vec4(1.0, 0.0, 0.0, 1.0),
        glm::vec4(0.05, 0.05, 0.05, 1.0),
        glm::vec4(0.5, 0.5, 0.5, 1.0),
        100.0f,
        false
    );
    //example of setting diffuse and shininess only
    (*map)["purple"] = AnimShadeData(
        glm::vec4(0.5, 0.1, 0.7, 1.0),
        50.0f,
        false
    );
    //example of default initialization
    (*map)["white"] = AnimShadeData();
}

void Application::initGeometry(){
    // Load obj files 
    
    mObjects["vulkan"] = load_obj_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "/vulkan.obj");
    mObjects["monkey"] = load_obj_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "/suzanne.obj");
    mObjects["bunny"] = load_obj_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "/bunny.obj");
    mObjects["teapot"] = load_obj_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "/teapot.obj");
    mObjects["ballTex"] = load_obj_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "/ballTex.obj");
    mObjects["cube"] = load_gltf_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "Cube/Cube.gltf", false);
    
    mObjects["lantern"] = load_gltf_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "Lantern/Lantern.gltf", false);
    mObjects["OrientationTest"] = load_gltf_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "OrientationTest/OrientationTest.glb", true);
    mObjects["dummy"] = load_obj_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "/dummy.obj");
    

    // Create new uniform data for each object
    
    mObjectTransforms["vulkan"] = UniformTransformData::create();
    mObjectTransforms["monkey"] = UniformTransformData::create();
    mObjectTransforms["bunny"] = UniformTransformData::create();
    mObjectTransforms["teapot"] = UniformTransformData::create();
    mObjectTransforms["ballTex"] = UniformTransformData::create();
    mObjectTransforms["cube"] = UniformTransformData::create();
    
    mObjectTransforms["lantern"] = UniformTransformData::create();
    mObjectTransforms["OrientationTest"] = UniformTransformData::create();
    mObjectTransforms["dummy"] = UniformTransformData::create();
    
    mObjectAnimShade["vulkan"] = UniformAnimShadeData::create();
    mObjectAnimShade["monkey"] = UniformAnimShadeData::create();
    mObjectAnimShade["bunny"] = UniformAnimShadeData::create();
    mObjectAnimShade["teapot"] = UniformAnimShadeData::create();
    mObjectAnimShade["ballTex"] = UniformAnimShadeData::create();
    mObjectAnimShade["cube"] = UniformAnimShadeData::create();
    
    mObjectAnimShade["lantern"] = UniformAnimShadeData::create();
    mObjectAnimShade["OrientationTest"] = UniformAnimShadeData::create();
    mObjectAnimShade["dummy"] = UniformAnimShadeData::create();

    //Make a color map
    auto BlPhColors = unordered_map<string, AnimShadeData>();
    initBlinnPhongColorMap(&BlPhColors);

    //set uniform shading data to colors defined in color map
    
    mObjectAnimShade["bunny"]->setStruct(BlPhColors["cyan"]);
    mObjectAnimShade["vulkan"]->setStruct(BlPhColors["red"]);
    mObjectAnimShade["monkey"]->setStruct(AnimShadeData(TEXTURED_SHADED, 1));
    mObjectAnimShade["ballTex"]->setStruct(AnimShadeData(TEXTURED_SHADED, 0));
    mObjectAnimShade["cube"]->setStruct(AnimShadeData(TEXTURED_FLAT, 2));
    
    mObjectAnimShade["lantern"]->setStruct(AnimShadeData(TEXTURED_FLAT, 4));
    mObjectAnimShade["OrientationTest"]->setStruct(BlPhColors["purple"]);
    mObjectAnimShade["dummy"]->setStruct(BlPhColors["purple"]);

    //this is called after all mObjectAnimShades are initialized, which records their initial shading layer for reverting after pressing keybinds 1-5.
    recordShadingLayers();
    // Add object to the scene along with its uniform data
    
    VulkanGraphicsApp::addMultiShapeObject(
        mObjects["vulkan"], // The object

        // Collection of uniform data for the object
        {
            {1, mObjectTransforms["vulkan"]}, // Bind transform matrix to binding point #1
            {2, mObjectAnimShade["vulkan"]}, // Bind other uniform data to binding point #2
        }
        
    );
    
    // Add the other objects the same way as above. 
    
    VulkanGraphicsApp::addMultiShapeObject(mObjects["monkey"], {{1, mObjectTransforms["monkey"]}, {2, mObjectAnimShade["monkey"]}});
    VulkanGraphicsApp::addMultiShapeObject(mObjects["bunny"], {{1, mObjectTransforms["bunny"]}, {2, mObjectAnimShade["bunny"]}});
    VulkanGraphicsApp::addMultiShapeObject(mObjects["teapot"], {{1, mObjectTransforms["teapot"]}, {2, mObjectAnimShade["teapot"]}});
    VulkanGraphicsApp::addMultiShapeObject(mObjects["ballTex"], { {1, mObjectTransforms["ballTex"]}, {2, mObjectAnimShade["ballTex"]}});
    VulkanGraphicsApp::addMultiShapeObject(mObjects["cube"], { {1, mObjectTransforms["cube"]}, {2, mObjectAnimShade["cube"]} });
    
    VulkanGraphicsApp::addMultiShapeObject(mObjects["lantern"], { {1, mObjectTransforms["lantern"]}, {2, mObjectAnimShade["lantern"]} });
    VulkanGraphicsApp::addMultiShapeObject(mObjects["OrientationTest"], { {1, mObjectTransforms["OrientationTest"]}, {2, mObjectAnimShade["OrientationTest"]} });
    VulkanGraphicsApp::addMultiShapeObject(mObjects["dummy"], { {1, mObjectTransforms["dummy"]}, {2, mObjectAnimShade["dummy"]} });
}

/// Initialize our shaders
void Application::initShaders(){
    // Get the handle representing the GPU. 
    VkDevice logicalDevice = VulkanGraphicsApp::getPrimaryDeviceBundle().logicalDevice;

    // Load the compiled shader code from disk. 
    VkShaderModule vertShader = vkutils::load_shader_module(logicalDevice, STRIFY(SHADER_DIR) "/debug.vert.spv");
    VkShaderModule fragShader = vkutils::load_shader_module(logicalDevice, STRIFY(SHADER_DIR) "/debug.frag.spv");
    
    assert(vertShader != VK_NULL_HANDLE);
    assert(fragShader != VK_NULL_HANDLE);

    VulkanGraphicsApp::setVertexShader("debug.vert", vertShader);
    VulkanGraphicsApp::setFragmentShader("debug.frag", fragShader);
}


/// Initialize uniform data and bind them.
void Application::initUniforms(){

    // Specify the structure of single instance uniforms
    mWorldInfo = UniformWorldInfo::create();
    VulkanGraphicsApp::addSingleInstanceUniform(0, mWorldInfo); // Bind world info uniform data to binding point #0

    // Specify the layout for per-object uniform data
    mUniformLayoutSet = UniformDataLayoutSet{
        // {<binding point>, <structure layout>}
        {1, UniformTransformData::sGetLayout()}, // Transform data on binding point #1
        {2, UniformAnimShadeData::sGetLayout()} // Blinn-Phong data on binding point #2
    };
    
    VulkanGraphicsApp::initMultis(mUniformLayoutSet);
    
    updateView();
    updatePerspective();
}
