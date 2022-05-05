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
    void addMultiShapeObjects();
    void initShaders();
    void initUniforms();
    void render(double dt);

    
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
    
    //~hierarchy~static UniformTransformDataPtr logoTfs = mObjectTransforms["vulkan"];
    //~hierarchy~static UniformTransformDataPtr monkeyTfs = mObjectTransforms["monkey"];
    //~hierarchy~static UniformTransformDataPtr bunnyTfs = mObjectTransforms["bunny"];
    //~hierarchy~static UniformTransformDataPtr teapotTfs = mObjectTransforms["teapot"];
    //~hierarchy~static UniformTransformDataPtr ballTfs = mObjectTransforms["ballTex"];
    static UniformTransformDataPtr cubeTfs = mObjectTransforms["cube"];
    
    //~hierarchy~static UniformTransformDataPtr lanternTfs = mObjectTransforms["lantern"];
    //~hierarchy~static UniformTransformDataPtr orientationTestTfs = mObjectTransforms["OrientationTest"];
    //~hierarchy~static UniformTransformDataPtr cesiumMilkTruckTfs = mObjectTransforms["CesiumMilkTruck"];
    //~hierarchy~static UniformTransformDataPtr buggyTfs = mObjectTransforms["Buggy"];
    static UniformTransformDataPtr dummyTfs = mObjectTransforms["dummy"];
    // Global time
    float gt = static_cast<float>(glfwGetTime());
    
    // Spin the logo in place. 
    //~hierarchy~logoTfs->getStruct().Model = glm::scale(vec3(2.5f)) * glm::rotate(float(gt), vec3(0.0, 1.0, 0.0));
    
    // Spin the ball opposite direction of logo, above it.
    //~hierarchy~ballTfs->getStruct().Model = glm::translate(vec3(0.0, 3.0, 0.0)) * glm::rotate(float(gt), vec3(0.0, -1.0, 0.0));

    // Spin the cube around above both the logo and the ball.
    cubeTfs->getStruct().Model = glm::translate(vec3(0.0, -3.0, 0.0)) * glm::rotate(float(gt), vec3(0.0, -1.0, 0.0)) * glm::scale(vec3(sin(gt), cos(gt), 1));
    
    // move the lantern into the background
    //~hierarchy~lanternTfs->getStruct().Model = glm::translate(vec3(0.0, 0.0, -2.0)) * glm::scale(vec3(0.2));

    //~hierarchy~orientationTestTfs->getStruct().Model = glm::translate(vec3(0.0, 4.0, -4.0)) * glm::rotate(glm::radians(45.0f), vec3(0,1,1)) * glm::scale(vec3(0.1));
    //~hierarchy~cesiumMilkTruckTfs->getStruct().Model = glm::translate(vec3(0.0, -4.0, -4.0)) * glm::scale(vec3(0.5));
    //~hierarchy~buggyTfs->getStruct().Model = glm::translate(vec3(16.0, 4.0, 0.0)) * glm::scale(vec3(0.05));
    //position dummy
    dummyTfs->getStruct().Model = glm::translate(vec3(0.0, 0.0, 2.0)) * glm::rotate(glm::pi<float>()/2, vec3(-1.0, 0.0, 0.0)) * glm::scale(vec3(1.0/25.0));

    // Rotate all other objects around the Vulkan logo in the center
    constexpr float angle = 2.0f*glm::pi<float>()/3.0f; // 120 degrees
    float radius = 4.5f;
    
    //~hierarchy~monkeyTfs->getStruct().Model = glm::rotate(-float(gt), vec3(0.0, 1.0, 0.0)) * glm::translate(radius*vec3(cos(angle*0), .2f*sin(gt*4.0f+angle*0), sin(angle*0))) * glm::rotate(2.0f*float(gt), vec3(0.0, 1.0, 0.0));
    //~hierarchy~bunnyTfs->getStruct().Model  = glm::rotate(-float(gt), vec3(0.0, 1.0, 0.0)) * glm::translate(radius*vec3(cos(angle*1), .2f*sin(gt*4.0f+angle*1), sin(angle*1))) * glm::rotate(2.0f*float(gt), vec3(0.0, 1.0, 0.0));
    //~hierarchy~teapotTfs->getStruct().Model = glm::rotate(-float(gt), vec3(0.0, 1.0, 0.0)) * glm::translate(radius*vec3(cos(angle*2), .2f*sin(gt*4.0f+angle*2), sin(angle*2))) * glm::rotate(2.0f*float(gt), vec3(0.0, 1.0, 0.0));
    
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
    
    //~hierarchy~mObjects["vulkan"] = load_obj_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "/vulkan.obj");
    //~hierarchy~mObjects["monkey"] = load_obj_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "/suzanne.obj");
    //~hierarchy~mObjects["bunny"] = load_obj_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "/bunny.obj");
    //~hierarchy~mObjects["teapot"] = load_obj_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "/teapot.obj");
    //~hierarchy~mObjects["ballTex"] = load_obj_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "/ballTex.obj");
    mObjects["cube"] = load_gltf_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "Cube/Cube.gltf", false);
    
    //~hierarchy~mObjects["lantern"] = load_gltf_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "Lantern/Lantern.gltf", false);
    //~hierarchy~mObjects["OrientationTest"] = load_gltf_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "OrientationTest/OrientationTest.glb", true);
    //~hierarchy~mObjects["CesiumMilkTruck"] = load_gltf_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "CesiumMilkTruck/CesiumMilkTruck.glb", true);
    //~hierarchy~mObjects["Buggy"] = load_gltf_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "Buggy/Buggy.glb", true);
    mObjects["dummy"] = load_obj_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "/dummy.obj");
    

    // Create new uniform data for each object
    
    //~hierarchy~mObjectTransforms["vulkan"] = UniformTransformData::create();
    //~hierarchy~mObjectTransforms["monkey"] = UniformTransformData::create();
    //~hierarchy~mObjectTransforms["bunny"] = UniformTransformData::create();
    //~hierarchy~mObjectTransforms["teapot"] = UniformTransformData::create();
    //~hierarchy~mObjectTransforms["ballTex"] = UniformTransformData::create();
    mObjectTransforms["cube"] = UniformTransformData::create();
    
    //~hierarchy~mObjectTransforms["lantern"] = UniformTransformData::create();
    //~hierarchy~mObjectTransforms["OrientationTest"] = UniformTransformData::create();
    //~hierarchy~mObjectTransforms["CesiumMilkTruck"] = UniformTransformData::create();
    //~hierarchy~mObjectTransforms["Buggy"] = UniformTransformData::create();
    mObjectTransforms["dummy"] = UniformTransformData::create();
    
    //~hierarchy~mObjectAnimShade["vulkan"] = UniformAnimShadeData::create();
    //~hierarchy~mObjectAnimShade["monkey"] = UniformAnimShadeData::create();
    //~hierarchy~mObjectAnimShade["bunny"] = UniformAnimShadeData::create();
    //~hierarchy~mObjectAnimShade["teapot"] = UniformAnimShadeData::create();
    //~hierarchy~mObjectAnimShade["ballTex"] = UniformAnimShadeData::create();
    mObjectAnimShade["cube"] = UniformAnimShadeData::create();
    
    //~hierarchy~mObjectAnimShade["lantern"] = UniformAnimShadeData::create();
    //~hierarchy~mObjectAnimShade["OrientationTest"] = UniformAnimShadeData::create();
    //~hierarchy~mObjectAnimShade["CesiumMilkTruck"] = UniformAnimShadeData::create();
    //~hierarchy~mObjectAnimShade["Buggy"] = UniformAnimShadeData::create();
    mObjectAnimShade["dummy"] = UniformAnimShadeData::create();

    //Make a color map
    auto BlPhColors = unordered_map<string, AnimShadeData>();
    initBlinnPhongColorMap(&BlPhColors);

    //set uniform shading data to colors defined in color map
    
    //~hierarchy~mObjectAnimShade["bunny"]->setStruct(BlPhColors["cyan"]);
    //~hierarchy~mObjectAnimShade["vulkan"]->setStruct(BlPhColors["red"]);
    //~hierarchy~mObjectAnimShade["monkey"]->setStruct(AnimShadeData(TEXTURED_SHADED, 1));
    //~hierarchy~mObjectAnimShade["ballTex"]->setStruct(AnimShadeData(TEXTURED_SHADED, 0));
    mObjectAnimShade["cube"]->setStruct(AnimShadeData(TEXTURED_FLAT, 2));
    
    //~hierarchy~mObjectAnimShade["lantern"]->setStruct(AnimShadeData(TEXTURED_FLAT, 4));
    //~hierarchy~mObjectAnimShade["CesiumMilkTruck"]->setStruct(AnimShadeData(TEXTURED_SHADED, 5));
    //~hierarchy~mObjectAnimShade["Buggy"]->setStruct(BlPhColors["white"]);
    //~hierarchy~mObjectAnimShade["OrientationTest"]->setStruct(BlPhColors["purple"]);
    mObjectAnimShade["dummy"]->setStruct(BlPhColors["purple"]);

    //this is called after all mObjectAnimShades are initialized, which records their initial shading layer for reverting after pressing keybinds 1-5.
    recordShadingLayers();
    // Add object to the scene along with its uniform data
    
    

    //~hierarchy~VulkanGraphicsApp::addMultiShapeObject(
    //~hierarchy~    mObjects["vulkan"], // The object
    //~hierarchy~
    //~hierarchy~    // Collection of uniform data for the object
    //~hierarchy~    {
    //~hierarchy~        {1, mObjectTransforms["vulkan"]}, // Bind transform matrix to binding point #1
    //~hierarchy~        {2, mObjectAnimShade["vulkan"]}, // Bind other uniform data to binding point #2
    //~hierarchy~    }
    //~hierarchy~    
    //~hierarchy~);
    
    // Add the other objects the same way as above. 
    
    //~hierarchy~VulkanGraphicsApp::addMultiShapeObject(mObjects["monkey"], {{1, mObjectTransforms["monkey"]}, {2, mObjectAnimShade["monkey"]}});
    //~hierarchy~VulkanGraphicsApp::addMultiShapeObject(mObjects["bunny"], {{1, mObjectTransforms["bunny"]}, {2, mObjectAnimShade["bunny"]}});
    //~hierarchy~VulkanGraphicsApp::addMultiShapeObject(mObjects["teapot"], {{1, mObjectTransforms["teapot"]}, {2, mObjectAnimShade["teapot"]}});
    //~hierarchy~VulkanGraphicsApp::addMultiShapeObject(mObjects["ballTex"], { {1, mObjectTransforms["ballTex"]}, {2, mObjectAnimShade["ballTex"]}});
    
    //~hierarchy~VulkanGraphicsApp::addMultiShapeObject(mObjects["lantern"], { {1, mObjectTransforms["lantern"]}, {2, mObjectAnimShade["lantern"]} });
    //~hierarchy~VulkanGraphicsApp::addMultiShapeObject(mObjects["OrientationTest"], { {1, mObjectTransforms["OrientationTest"]}, {2, mObjectAnimShade["OrientationTest"]} });
    //~hierarchy~VulkanGraphicsApp::addMultiShapeObject(mObjects["CesiumMilkTruck"], { {1, mObjectTransforms["CesiumMilkTruck"]}, {2, mObjectAnimShade["CesiumMilkTruck"]} });
    //~hierarchy~VulkanGraphicsApp::addMultiShapeObject(mObjects["Buggy"], { {1, mObjectTransforms["Buggy"]}, {2, mObjectAnimShade["Buggy"]} });
    addMultiShapeObjects();
    

}

void Application::addMultiShapeObjects() {
    
    size_t totalShapesAdded = 0;
    size_t previousDescriptorSetPosition = 0;
    //for each multiShapeObject
    for (std::pair<string, ObjMultiShapeGeometry> object : mObjects) {
        string name = object.first;
        vector<UniformDataInterfaceSet> sets;
        //for each shape in that object
        for (int i = 0; i < mObjects[name].shapeCount(); i++) {
            sets.push_back({ {1, mObjectTransforms[name]}, {2, mObjectAnimShade[name]} });
            //this keeps track of where in the vector of descriptor sets the particular shape's descriptor set data was inserted into.
            mObjects[name].setDescriptorSetPosition(i + previousDescriptorSetPosition);
        }

        previousDescriptorSetPosition += sets.size(); //the position we will be inserting into in the next iteration of the outer loop.
        //our insertion helper vector "sets" gets destroyed every loop, hence the +=.
        VulkanGraphicsApp::addMultiShapeObject(mObjects[name], sets);
    }
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
