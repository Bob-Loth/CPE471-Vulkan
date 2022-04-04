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
#include <glm/gtc/type_ptr.hpp>

#include "matrixMath.h"
using namespace std;

///////////////////////////////////////////////////////////////////////////////////////////////////
// The code below defines the types and formatting for uniform data used in our shaders. 
///////////////////////////////////////////////////////////////////////////////////////////////////
//TODO move the View matrix into a struct, possibly Transforms, that will change between draw calls.
// Uniform data that applies to the entire scene, and will not change between draw calls.
struct WorldInfo {
    alignas(16) glm::mat4 View;
    alignas(16) glm::mat4 Perspective;
};

// Model transform matrix which will be different for each object / draw call.
struct Transforms {
    alignas(16) glm::mat4 Model;
};

// Additional uniform data that varies per-object / per-draw.
struct AnimShadeData {
    int unused;
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
    
    assert(mWorldInfo != nullptr);

    //TODO remove or simplify for assignment
    Matrix Translate = createTranslateMat(-2, 0, 0);
    Matrix Rotate = createRotationMatY(-glm::pi<float>() / 6.0);
    Matrix View = multMat(Translate, Rotate);
    mWorldInfo->getStruct().View = glm::make_mat4(View.data());
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

    //TODO remove or simplify for assignment
    // Get pointers to the individual transforms for each object in the scene
    static vector<UniformTransformDataPtr> cubeTfs = { mObjectTransforms["cube0"], mObjectTransforms["cube1"], mObjectTransforms["cube2"], mObjectTransforms["cube3"] };
    
    Matrix verticalScale1 = createScaleMat(0.2, 1.6, 0.2);
    Matrix verticalScale2 = createScaleMat(0.2, 0.8, 0.2);
    Matrix rotation = createRotationMatZ(glm::pi<float>()/2.0);

    Matrix cube0 = multMat(createTranslateMat(2, 0, -5), verticalScale1);
    Matrix cube1 = multMat(createTranslateMat(0, 0, -5), verticalScale1);
    Matrix cube2 = multMat(createTranslateMat(-1, 0, -5), verticalScale1);
    Matrix cube3 = multMat(createTranslateMat(-0.5, 0, -5), multMat(rotation, verticalScale2));

    cubeTfs[0]->getStruct().Model = glm::make_mat4(cube0.data());
    cubeTfs[1]->getStruct().Model = glm::make_mat4(cube1.data());
    cubeTfs[2]->getStruct().Model = glm::make_mat4(cube2.data());
    cubeTfs[3]->getStruct().Model = glm::make_mat4(cube3.data());

    // Tell the GPU to render a frame. 
    VulkanGraphicsApp::render();
} 


void Application::initGeometry(){
    //non-instanced for now, as the geometry and number of instances are both small
    for (int i = 0; i < 4; ++i) {
        mObjects["cube" + to_string(i)] = load_gltf_to_vulkan(getPrimaryDeviceBundle(), STRIFY(ASSET_DIR) "Cube/Cube.gltf");
        mObjectTransforms["cube" + to_string(i)] = UniformTransformData::create();
        mObjectAnimShade["cube" + to_string(i)] = UniformAnimShadeData::create();
        mObjectAnimShade["cube" + to_string(i)]->setStruct(AnimShadeData());
    }

    for (int i = 0; i < 4; ++i) {
        VulkanGraphicsApp::addMultiShapeObject(mObjects["cube" + to_string(i)], { {1, mObjectTransforms["cube" + to_string(i)]}, {2, mObjectAnimShade["cube" + to_string(i)]} });
    }
}

/// Initialize our shaders
void Application::initShaders(){
    // Get the handle representing the GPU. 
    VkDevice logicalDevice = VulkanGraphicsApp::getPrimaryDeviceBundle().logicalDevice;

    // Load the compiled shader code from disk. 
    VkShaderModule vertShader = vkutils::load_shader_module(logicalDevice, STRIFY(SHADER_DIR) "/standard.vert.spv");
    VkShaderModule fragShader = vkutils::load_shader_module(logicalDevice, STRIFY(SHADER_DIR) "/standard.frag.spv");
    
    assert(vertShader != VK_NULL_HANDLE);
    assert(fragShader != VK_NULL_HANDLE);

    VulkanGraphicsApp::setVertexShader("standard.vert", vertShader);
    VulkanGraphicsApp::setFragmentShader("vertexColor.frag", fragShader);
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
