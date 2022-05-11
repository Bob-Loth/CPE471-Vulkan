#define _DISABLE_EXTENDED_ALIGNED_STORAGE // Fix MSVC warning about binary compatability

#include "VulkanGraphicsApp.h"
#include "data/VertexGeometry.h"
#include "data/UniformBuffer.h"
#include "data/VertexInput.h"
#include "utils/BufferedTimer.h"
#include "load_obj.h"
#include "load_gltf.h"
#include "load_texture.h"
#include "MatrixStack.h"

#include <filesystem>
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
namespace fs = std::filesystem; //fs is now an alias for the very long "std::filesystem" namespace.
enum ShadingLayer { BLINN_PHONG, NORMAL_MAP, TEXTURE_MAP, TEXTURED_FLAT, TEXTURED_SHADED, NO_FORCED_LAYER };
ShadingLayer currentShadingLayer = NO_FORCED_LAYER; /*static initialization problem generates linker error, using global for now*/

class MatrixNode
{
public:
    MatrixNode(int index, glm::mat4 localModelMatrix) : index(index), localModelMatrix(localModelMatrix), parent(-1) {};
    
    //the local model matrix, without any applied matrices from the parent.
    glm::mat4 localModelMatrix;
    // the parent in the shape hierarchy. Null if this is the root node.
    int parent;
    // any children in the shape hierarchy. Empty if this is a leaf node.
    //std::vector<std::shared_ptr<MatrixNode>> children;
private:
    //which shape index does this MatrixNode refer to
    int index;
};

glm::mat4 computeCTM(const vector<MatrixNode>& tree, int indexToCompute) {
    glm::mat4 CTM = tree[indexToCompute].localModelMatrix;
    int currentParent = tree[indexToCompute].parent;
    while (currentParent != -1) {
        CTM = tree[currentParent].localModelMatrix * CTM;
        currentParent = tree[currentParent].parent;
    }
    return CTM;
}

vector<MatrixNode> createVectorOfMatrixNodes(int size) {
    auto ret = vector<MatrixNode>();
    for (int i = 0; i < size; ++i) {
        ret.emplace_back(MatrixNode(i, glm::mat4(1.0f)));
    }
    return ret;
}

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
     void loadShapeFilesFromPath(string path);
     void initGeometry();
    void addMultiShapeObjects();
    void initShaders();
    void initUniforms();
    void initHierarchies();
    void render(double dt);

    //names of the loaded shapefiles.
    std::vector<string> mObjectNames;
    //holds the original state of each of the object's shading layer
    std::unordered_map<std::string, vector<ShadingLayer>> mKeyCallbackHolds;
    
    //used to describe a user-defined hierarchy of a multishape object.
    std::unordered_map<std::string, vector<MatrixNode>> mObjectHierarchies;

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
        for (int i = 0; i < obj.second.size(); i++) {
            mKeyCallbackHolds[obj.first].push_back(static_cast<ShadingLayer>(obj.second[i]->getStruct().shadingLayer));
        }
    }
}

void Application::observeCurrentShadingLayer() {
    for (auto& obj : mObjectAnimShade) {
        for (int i = 0; i < obj.second.size(); i++) {
            if (currentShadingLayer == NO_FORCED_LAYER) { //revert to each object's original layer
                obj.second[i]->getStruct().shadingLayer = mKeyCallbackHolds[obj.first][i];
            }
            else { //observe and apply the current shading layer to all objects
                obj.second[i]->getStruct().shadingLayer = currentShadingLayer;
            }
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

void Application::initHierarchies() {
    vector<MatrixNode> dummyTfs = createVectorOfMatrixNodes(mObjects["dummy"].shapeCount());
    vector<glm::vec3> boxes = mObjects["dummy"].BBoxCenters();
    //give hips a base transform.
    glm::mat4 baseMatrix = glm::translate(glm::vec3(0.0, 0.0, 2.0)) * glm::rotate(glm::pi<float>() / 2, glm::vec3(-1.0, 0.0, 0.0)) * glm::scale(glm::vec3(1.0 / 25.0));
    dummyTfs[12].localModelMatrix = baseMatrix; //this is the root node, so it has no parent. Additional nodes should set a parent node that they will inherit transforms from.
    //set belly, L+R pelvis to be children of hips.
    dummyTfs[13].parent = dummyTfs[11].parent = dummyTfs[5].parent = 12;
    //set each of the legs.
    //right leg.
    for (int i = 4; i >= 0; --i) {
        dummyTfs[i].parent = i + 1;
    }
    //left leg.
    for (int i = 10; i >= 6; --i) {
        dummyTfs[i].parent = i + 1;
    }
    //start setting up things in the upper body.
    // belly -> torso--> neck -> head
    dummyTfs[14].parent = 13;
    dummyTfs[27].parent = 14;
    dummyTfs[28].parent = 27;

    //attach the shoulders to the torso.
    dummyTfs[21].parent = dummyTfs[15].parent = 14;
    //               |-> rshoulder -> bicep -> elbow -> forearm -> wrist -> hand
    for (int i = 20; i >= 16; --i) {
        dummyTfs[i].parent = i - 1;
    }
    //               |-> lshoulder -> bicep -> elbow -> forearm -> wrist -> hand
    for (int i = 26; i >= 22; --i) {
        dummyTfs[i].parent = i - 1;
    }
    //now that we've defined the relationship between each of the multishape objects, send it to the rest of the application.
    mObjectHierarchies["dummy"] = dummyTfs;
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

    initHierarchies();

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
    // Global time
    float gt = static_cast<float>(glfwGetTime());
    
    //use this to set all transform data for all shapes in a given multi-shape object.
    auto setAllObjectTransformData = [this](string name, glm::mat4 M) {
        for (size_t i = 0; i < mObjects[name].shapeCount(); ++i) {
            mObjectTransforms[name][i]->getStruct().Model = M;
        }
    };

    auto setHierarchicalTransformData = [this](string name) {
        for (size_t i = 0; i < mObjects[name].shapeCount(); ++i) {
            mObjectTransforms[name][i]->getStruct().Model = computeCTM(mObjectHierarchies[name], i);
        }
    };

    // Spin the logo in place. 
    setAllObjectTransformData("vulkan", glm::scale(vec3(2.5f)) * glm::rotate(float(gt), vec3(0.0, 1.0, 0.0)));
    
    // Spin the ball opposite direction of logo, above it.
    setAllObjectTransformData("ballTex", glm::translate(vec3(0.0, 3.0, 0.0)) * glm::rotate(float(gt), vec3(0.0, -1.0, 0.0)));

    // Spin the cube around above both the logo and the ball.
    setAllObjectTransformData("Cube", glm::translate(vec3(0.0, -3.0, 0.0)) * glm::rotate(float(gt), vec3(0.0, -1.0, 0.0)) * glm::scale(vec3(sin(gt), cos(gt), 1)));
    
    
    // move the lantern into the background
    setAllObjectTransformData("Lantern", glm::translate(vec3(0.0, 0.0, -2.0)) * glm::scale(vec3(0.2)));
    

    setAllObjectTransformData("OrientationTest", glm::translate(vec3(0.0, 4.0, -4.0)) * glm::rotate(glm::radians(45.0f), vec3(0, 1, 1)) * glm::scale(vec3(0.1)));
    setAllObjectTransformData("CesiumMilkTruck", glm::translate(vec3(0.0, -4.0, -4.0)) * glm::scale(vec3(0.5)));
    setAllObjectTransformData("Buggy", glm::translate(vec3(16.0, 4.0, 0.0)) * glm::scale(vec3(0.05)));

    //position dummy. Refer to Dummy Labels.png in the asset directory for correct indices.
    
    setHierarchicalTransformData("dummy");
    

    // Rotate all other objects around the Vulkan logo in the center
    constexpr float angle = 2.0f*glm::pi<float>()/3.0f; // 120 degrees
    float radius = 4.5f;
    setAllObjectTransformData("suzanne", glm::rotate(-float(gt), vec3(0.0, 1.0, 0.0)) * glm::translate(radius * vec3(cos(angle * 0), .2f * sin(gt * 4.0f + angle * 0), sin(angle * 0))) * glm::rotate(2.0f * float(gt), vec3(0.0, 1.0, 0.0)));
    setAllObjectTransformData("bunny", glm::rotate(-float(gt), vec3(0.0, 1.0, 0.0)) * glm::translate(radius * vec3(cos(angle * 1), .2f * sin(gt * 4.0f + angle * 1), sin(angle * 1))) * glm::rotate(2.0f * float(gt), vec3(0.0, 1.0, 0.0)));
    setAllObjectTransformData("teapot", glm::rotate(-float(gt), vec3(0.0, 1.0, 0.0)) * glm::translate(radius * vec3(cos(angle * 2), .2f * sin(gt * 4.0f + angle * 2), sin(angle * 2))) * glm::rotate(2.0f * float(gt), vec3(0.0, 1.0, 0.0)));
    
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

// loads all .gltf, .glb, and .obj files found in "dir" and its subdirectories into mObjects.
// If you don't want to do this, you have a few options:
// 1. don't use this function, and instead manually load the shapefiles of your choosing using the load_x_to_vulkan functions.
// 2. Create specific omissions/permissions by modifying the lambda functions.
// 3. put any shape files you aren't using in your asset directory into a "junk" folder, and check if the entry.path().string() is equivalent to that junk folder's name
void Application::loadShapeFilesFromPath(string dir) {
    auto isGLTF = [](fs::path de) {return de.extension() == ".gltf"; };
    auto isGLB = [](fs::path de) {return de.extension() == ".glb"; };
    auto isOBJ = [](fs::path de) {return de.extension() == ".obj"; };
    for (const auto& entry : fs::directory_iterator(dir)) {

        if (entry.is_regular_file()) {
            string filenameNoExt = entry.path().stem().string();
           
            if (isGLTF(entry.path())) {
                cout << "loading .gltf file: " << entry.path() << endl;
                mObjects[filenameNoExt] = load_gltf_to_vulkan(getPrimaryDeviceBundle(), entry.path().string(), false);
                mObjectNames.push_back(filenameNoExt);
            }
            else if (isGLB(entry.path())) {
                cout << "loading .glb file: " << entry.path() << endl;
                mObjects[filenameNoExt] = load_gltf_to_vulkan(getPrimaryDeviceBundle(), entry.path().string(), true);
                mObjectNames.push_back(filenameNoExt);
            }
            else if (isOBJ(entry.path())) {
                cout << "loading .obj file: " << entry.path() << endl;
                mObjects[filenameNoExt] = load_obj_to_vulkan(getPrimaryDeviceBundle(), entry.path().string());
                mObjectNames.push_back(filenameNoExt);
            }
                
        }
        else if (entry.is_directory()) { //depth first
            loadShapeFilesFromPath(entry.path().string());
        }
    }
}

void Application::initGeometry(){
    // Load all shape files. This is what pushes filenames to mObjectNames.
    loadShapeFilesFromPath(STRIFY(ASSET_DIR));

    // Create new uniform data for each object
    for (string name : mObjectNames) {
        //create new uniform data for each shape in each object
        for (size_t i = 0; i < mObjects[name].shapeCount(); ++i) {
            mObjectTransforms[name].emplace_back(UniformTransformData::create());
            mObjectAnimShade[name].emplace_back(UniformAnimShadeData::create());
        }
        
    }

    //Make a color map. You can either improve on this, or throw it away and use something more sophisticated.
    auto BlPhColors = unordered_map<string, AnimShadeData>();
    initBlinnPhongColorMap(&BlPhColors);

    //if you don't want to set individual AnimShadeData's for every shape in the object, use this.
    auto setAllAnimShadeData = [this](string name, AnimShadeData animShadeData) {
        for (size_t i = 0; i < mObjects[name].shapeCount(); ++i) {
            mObjectAnimShade[name][i]->setStruct(animShadeData);
        }
    };

    //set uniform shading data to colors defined in color map, or to some texture. The key is the name of the file, without the extension.
    setAllAnimShadeData("bunny", BlPhColors["cyan"]);
    setAllAnimShadeData("vulkan", BlPhColors["red"]);
    setAllAnimShadeData("suzanne", AnimShadeData(TEXTURED_SHADED, 1));
    setAllAnimShadeData("ballTex", AnimShadeData(TEXTURED_SHADED, 0));
    setAllAnimShadeData("Cube", AnimShadeData(TEXTURED_FLAT, 2));
    setAllAnimShadeData("Lantern", AnimShadeData(TEXTURED_FLAT, 4));

    //just to reference the "by-shape" way to do this. Set the lantern body's AnimShadeData to be the emissive texture.
    mObjectAnimShade["Lantern"][2]->setStruct(AnimShadeData(TEXTURED_FLAT, 3));

    setAllAnimShadeData("CesiumMilkTruck", AnimShadeData(TEXTURED_SHADED, 5));
    setAllAnimShadeData("Buggy", BlPhColors["white"]);
    setAllAnimShadeData("OrientationTest", BlPhColors["purple"]);
    setAllAnimShadeData("dummy", BlPhColors["purple"]);

    //this is called after all mObjectAnimShades are initialized, which records their initial shading layer for reverting after pressing keybinds 1-5.
    //Can remove all of the shading layer code if you are not using the debug shader.
    recordShadingLayers();

    // Add objects to the scene along with its uniform data
    addMultiShapeObjects();
}

void Application::addMultiShapeObjects() {
    
    size_t totalShapesAdded = 0;
    size_t previousDescriptorSetPosition = 0;
    //for each multiShapeObject
    for (std::pair<string, ObjMultiShapeGeometry> object : mObjects) {
        string name = object.first;
        vector<UniformDataInterfaceSet> sets;
        //for each shape in that object. Change
        for (int i = 0; i < mObjects[name].shapeCount(); i++) {
            sets.push_back({ {1, mObjectTransforms[name][i]}, {2, mObjectAnimShade[name][i]}});
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
