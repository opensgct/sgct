/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2020                                                               *
 * For conditions of distribution and use, see copyright notice in LICENSE.md            *
 ****************************************************************************************/

#include <sgct/sgct.h>

#include <sgct/utils/box.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <algorithm>
#include <fstream>
#include <memory>

namespace {
    std::unique_ptr<std::thread> loadThread;
    std::mutex imageMutex;
    GLFWwindow* hiddenWindow;
    GLFWwindow* sharedWindow;
    std::unique_ptr<sgct::Image> transImg;
    unsigned int textureId = 0;

    bool stats = false;
    int32_t texIndex = -1;

    int32_t currentPackage = -1;
    bool transfer = false;
    bool serverUploadDone = false;
    bool clientsUploadDone = false;
    std::vector<std::string> imagePaths;
    std::vector<GLuint> texIds;
    double sendTimer = 0.0;

    bool isRunning = true;

    std::unique_ptr<sgct::utils::Box> box;
    GLint matrixLoc = -1;

    // variables to share across cluster
    double currentTime(0.0);

    constexpr const char* vertexShader = R"(
  #version 330 core

  layout(location = 0) in vec2 texCoords;
  layout(location = 1) in vec3 normals;
  layout(location = 2) in vec3 vertPositions;

  uniform mat4 mvp;
  out vec2 uv;

  void main() {
    gl_Position =  mvp * vec4(vertPositions, 1.0);
    uv = texCoords;
  })";
    
    constexpr const char* fragmentShader = R"(
  #version 330 core

  uniform sampler2D tex;

  in vec2 uv;
  out vec4 color;

  void main() { color = texture(tex, uv); }
)";
} // namespace

using namespace sgct;

void readImage(unsigned char* data, int len) {
    std::unique_lock lk(imageMutex);

    transImg = std::make_unique<Image>();

    try {
        transImg->load(reinterpret_cast<unsigned char*>(data), len);
    }
    catch (const std::runtime_error & e) {
        Log::Error("%s", e.what());
        transImg = nullptr;
    }
}

void startDataTransfer() {
    // iterate
    int id = currentPackage;
    id++;
    currentPackage = id;

    // make sure to keep within bounds
    if (static_cast<int>(imagePaths.size()) <= id) {
        return;
    }

    sendTimer = Engine::getTime();

    // load from file
    const std::string& tmpPair = imagePaths[static_cast<size_t>(id)];

    std::ifstream file(tmpPair.c_str(), std::ios::binary);
    file.seekg(0, std::ios::end);
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (file.read(buffer.data(), size)) {
        const int s = static_cast<int>(size);
        NetworkManager::instance().transferData(buffer.data(), s, id);

        // read the image on master
        readImage(reinterpret_cast<unsigned char*>(buffer.data()), s);
    }
}

void uploadTexture() {
    std::unique_lock lk(imageMutex);

    if (!transImg) {
        // if invalid load
        texIds.push_back(0);
        return;
    }

    glfwMakeContextCurrent(hiddenWindow);

    // create texture
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLenum internalformat;
    GLenum type;

    size_t bpc = transImg->bytesPerChannel();

    switch (transImg->channels()) {
    case 1:
        internalformat = (bpc == 1 ? GL_R8 : GL_R16);
        type = GL_RED;
        break;
    case 2:
        internalformat = (bpc == 1 ? GL_RG8 : GL_RG16);
        type = GL_RG;
        break;
    case 3:
    default:
        internalformat = (bpc == 1 ? GL_RGB8 : GL_RGB16);
        type = GL_BGR;
        break;
    case 4:
        internalformat = (bpc == 1 ? GL_RGBA8 : GL_RGBA16);
        type = GL_BGRA;
        break;
    }

    int mipMapLevels = 8;
    GLenum format = (bpc == 1 ? GL_UNSIGNED_BYTE : GL_UNSIGNED_SHORT);

    glTexStorage2D(
        GL_TEXTURE_2D,
        mipMapLevels,
        internalformat,
        static_cast<GLsizei>(transImg->size().x),
        static_cast<GLsizei>(transImg->size().y)
    );
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        static_cast<GLsizei>(transImg->size().x),
        static_cast<GLsizei>(transImg->size().y),
        type,
        format,
        transImg->data()
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipMapLevels - 1);

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    Log::Info(
        "Texture id %d loaded (%dx%dx%d).",
        tex, transImg->size().x, transImg->size().y, transImg->channels()
    );

    texIds.push_back(tex);
    transImg = nullptr;

    glFinish();
    glfwMakeContextCurrent(nullptr);
}

void threadWorker() {
    while (isRunning) {
        // runs only on master
        if (transfer && !serverUploadDone && !clientsUploadDone) {
            startDataTransfer();
            transfer = false;

            // load texture on master
            uploadTexture();
            serverUploadDone = true;

            if (ClusterManager::instance().numberOfNodes() == 1) {
                // no cluster
                clientsUploadDone = true;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void drawFun(const RenderData& data) {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    constexpr const double Speed = 0.44;

    // create scene transform (animation)
    glm::mat4 scene = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -3.f));
    scene = glm::rotate(
        scene,
        static_cast<float>(currentTime * Speed),
        glm::vec3(0.f, -1.f, 0.f)
    );
    scene = glm::rotate(
        scene,
        static_cast<float>(currentTime * (Speed / 2.0)),
        glm::vec3(1.f, 0.f, 0.f)
    );

    const glm::mat4 mvp = data.modelViewProjectionMatrix * scene;

    glActiveTexture(GL_TEXTURE0);
    
    if (texIndex != -1) {
        glBindTexture(GL_TEXTURE_2D, texIds[texIndex]);
    }
    else {
        glBindTexture(GL_TEXTURE_2D, textureId);
    }

    ShaderManager::instance().shaderProgram("xform").bind();
    glUniformMatrix4fv(matrixLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    box->draw();
    ShaderManager::instance().shaderProgram("xform").unbind();

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}

void preSyncFun() {
    if (Engine::instance().isMaster()) {
        currentTime = Engine::getTime();
        
        // if texture is uploaded then iterate the index
        if (serverUploadDone && clientsUploadDone) {
            texIndex++;
            serverUploadDone = false;
            clientsUploadDone = false;
        }
    }
}

void postSyncPreDrawFun() {
    Engine::instance().setStatsGraphVisibility(stats);
}

void initOGLFun(GLFWwindow* win) {
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    sharedWindow = win;
    hiddenWindow = glfwCreateWindow(1, 1, "Thread Window", nullptr, sharedWindow);

    if (!hiddenWindow) {
        Log::Info("Failed to create loader context");
    }

    glfwMakeContextCurrent(sharedWindow);

    if (Engine::instance().isMaster()) {
        loadThread = std::make_unique<std::thread>(threadWorker);
    }

    textureId = TextureManager::instance().loadTexture("box.png", true, 8.f);
    box = std::make_unique<utils::Box>(2.f, utils::Box::TextureMappingMode::Regular);

    // Set up backface culling
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW); // our polygon winding is counter clockwise

    ShaderManager::instance().addShaderProgram("xform", vertexShader, fragmentShader);
    const ShaderProgram& prog = ShaderManager::instance().shaderProgram("xform");
    prog.bind();
    matrixLoc = glGetUniformLocation(prog.id(), "mvp");
    glUniform1i(glGetUniformLocation(prog.id(), "tex"), 0);
    prog.unbind();
}

std::vector<std::byte> encodeFun() {
    std::vector<std::byte> data;
    serializeObject(data, currentTime);
    serializeObject(data, stats);
    serializeObject(data, texIndex);
    return data;
}

void decodeFun(const std::vector<std::byte>& data, unsigned int pos) {
    deserializeObject(data, pos, currentTime);
    deserializeObject(data, pos, stats);
    deserializeObject(data, pos, texIndex);
}

void cleanUpFun() {
    box = nullptr;
    
    for (size_t i = 0; i < texIds.size(); i++) {
        GLuint tex = texIds[i];
        if (tex) {
            glDeleteTextures(1, &tex);
        }
    }
    texIds.clear();

    if (hiddenWindow) {
        glfwDestroyWindow(hiddenWindow);
    }
}

void keyCallback(Key key, Modifier, Action action, int) {
    if (Engine::instance().isMaster() && (action == Action::Press)) {
        switch (key) {
            case Key::Esc:
                Engine::instance().terminate();
                break;
            case Key::S:
                stats = !stats;
                break;
            default:
                break;
        }
    }
}

void dataTransferDecoder(void* data, int length, int packageId, int clientIndex) {
    Log::Info(
        "Decoding %d bytes in transfer id: %d on node %d", length, packageId, clientIndex
    );

    currentPackage = packageId;

    // read the image on master
    readImage(reinterpret_cast<unsigned char*>(data), length);
    uploadTexture();
}

void dataTransferStatus(bool connected, int clientIndex) {
    Log::Info(
        "Transfer node %d is %s.", clientIndex, connected ? "connected" : "disconnected"
    );
}

void dataTransferAcknowledge(int packageId, int clientIndex) {
    Log::Info(
        "Transfer id: %d is completed on node %d.", packageId, clientIndex
    );
    
    static int counter = 0;
    if (packageId == currentPackage) {
        counter++;
        if (counter == (ClusterManager::instance().numberOfNodes() - 1)) {
            clientsUploadDone = true;
            counter = 0;

            Log::Info(
                "Time to distribute and upload textures on cluster: %f ms", 
                (Engine::getTime() - sendTimer) * 1000.0
            );
        }
    }
}

void dropCallback(int, const char** paths) {
    if (!Engine::instance().isMaster()) {
        return;
    }

    // simply pick the first path to transmit
    std::string tmpStr(paths[0]);

    // transform to lowercase
    std::transform(
        tmpStr.begin(),
        tmpStr.end(),
        tmpStr.begin(),
        [](char c) { return static_cast<char>(c); }
    );

    const bool foundJpg = tmpStr.find(".jpg") != std::string::npos;
    const bool foundJpeg = tmpStr.find(".jpeg") != std::string::npos;
    const bool foundPng = tmpStr.find(".png") != std::string::npos;
    if (foundJpg || foundJpeg || foundPng) {
        imagePaths.push_back(paths[0]);
        transfer = true;
    }
}

int main(int argc, char** argv) {
    std::vector<std::string> arg(argv + 1, argv + argc);
    Configuration config = parseArguments(arg);
    config::Cluster cluster = loadCluster(config.configFilename);

    Engine::Callbacks callbacks;
    callbacks.initOpenGL = initOGLFun;
    callbacks.draw = drawFun;
    callbacks.preSync = preSyncFun;
    callbacks.postSyncPreDraw = postSyncPreDrawFun;
    callbacks.cleanUp = cleanUpFun;
    callbacks.keyboard = keyCallback;
    callbacks.drop = dropCallback;
    callbacks.dataTransferDecode = dataTransferDecoder;
    callbacks.dataTransferStatus = dataTransferStatus;
    callbacks.dataTransferAcknowledge = dataTransferAcknowledge;
    callbacks.encode = encodeFun;
    callbacks.decode = decodeFun;

    try {
        Engine::create(cluster, callbacks, config);
    }
    catch (const std::runtime_error& e) {
        Log::Error("%s", e.what());
        Engine::destroy();
        return EXIT_FAILURE;
    }

    Engine::instance().render();

    isRunning = false;
    if (loadThread) {
        loadThread->join();
        loadThread = nullptr;
    }

    Engine::destroy();
    exit(EXIT_SUCCESS);
}
