#include <sgct/action.h>
#include <sgct/commandline.h>
#include <sgct/engine.h>
#include <sgct/keys.h>
#include <sgct/shadermanager.h>
#include <sgct/shareddata.h>
#include <sgct/texturemanager.h>
#include <sgct/window.h>
#include <sgct/utils/box.h>
#include <GLFW/glfw3.h>
#include <SpoutLibrary.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
    std::unique_ptr<sgct::utils::Box> box;
    GLint matrixLoc = -1;
    GLint flipLoc = -1;
    GLuint texture = 0;

    SPOUTHANDLE receiver = nullptr;
    char senderName[256];
    unsigned int width;
    unsigned int height;
    bool initialized = false;

    // variables to share across cluster
    sgct::SharedDouble currentTime(0.0);

    constexpr const char* vertexShader = R"(
  #version 330 core

  layout(location = 0) in vec2 texCoords;
  layout(location = 1) in vec3 normals;
  layout(location = 2) in vec3 vertPositions;

  uniform mat4 mvp;
  uniform int flip;

  out vec2 uv;

  void main() {
    // Output position of the vertex, in clip space : MVP * position
    gl_Position = mvp * vec4(vertPositions, 1.0);
    uv.x = texCoords.x;
    if (flip == 0) {
      uv.y = texCoords.y;
    }
    else {
      uv.y = 1.0 - texCoords.y;
    }
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
 
bool bindSpout() {
    const bool creationSuccess = receiver->CreateReceiver(senderName, width, height);
    if (!initialized && creationSuccess) {
        MessageHandler::printInfo(
            "Spout: Initing %ux%u texture from '%s'", width, height, senderName
        );
        initialized = true;
    }

    if (initialized) {
        const bool receiveSucess = receiver->ReceiveTexture(senderName, width, height);
        if (receiveSucess) {
            return receiver->BindSharedTexture();
        }
        else {
            MessageHandler::printInfo("Spout disconnected");

            // reset if disconnected
            initialized = false;
            senderName[0] = '\0';
            receiver->ReleaseReceiver();
        }
    }

    return false;
}

void drawFun() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    constexpr const double Speed = 0.44;

    //create scene transform (animation)
    glm::mat4 scene = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -3.f));
    scene = glm::rotate(
        scene,
        static_cast<float>(currentTime.getVal() * Speed),
        glm::vec3(0.f, -1.f, 0.f)
    );
    scene = glm::rotate(
        scene,
        static_cast<float>(currentTime.getVal() * (Speed / 2.0)),
        glm::vec3(1.f, 0.f, 0.f)
    );

    const glm::mat4 mvp = Engine::instance().getCurrentModelViewProjectionMatrix() *
                          scene;

    glActiveTexture(GL_TEXTURE0);

    // spout init
    bool spoutStatus = false;
    // check if spout supported (DX11 interop)
    if (glfwExtensionSupported("WGL_NV_DX_interop2")) {
        spoutStatus = bindSpout();
    }

    const ShaderProgram& prog = ShaderManager::instance().getShaderProgram("xform");
    prog.bind();

    // DirectX textures are flipped around the Y axis compared to OpenGL
    if (!spoutStatus) {
        glUniform1i(flipLoc, 0);
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    else {
        glUniform1i(flipLoc, 1);
    }

    glUniformMatrix4fv(matrixLoc, 1, GL_FALSE, glm::value_ptr(mvp));

    box->draw();

    prog.unbind();

    if (spoutStatus) {
        receiver->UnBindSharedTexture();
    }

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}

void preSyncFun() {
    if (Engine::instance().isMaster()) {
        currentTime.setVal(Engine::getTime());
    }
}

void initOGLFun() {
    // setup spout
    senderName[0] = '\0';
    receiver = GetSpout();
    
    // set background
    Engine::instance().setClearColor(glm::vec4(0.3f, 0.3f, 0.3f, 0.f));
    
    texture = TextureManager::instance().loadTexture("box", "box.png", true);

    box = std::make_unique<utils::Box>(2.f, utils::Box::TextureMappingMode::Regular);

    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    ShaderManager::instance().addShaderProgram("xform", vertexShader, fragmentShader);
    const ShaderProgram& prog = ShaderManager::instance().getShaderProgram("xform");
    prog.bind();

    matrixLoc = glGetUniformLocation(prog.getId(), "mvp");
    glUniform1i(glGetUniformLocation(prog.getId(), "tex"), 0);
    flipLoc = glGetUniformLocation(prog.getId(), "flip");
    glUniform1i(flipLoc, 0);

    prog.unbind();
}

void encodeFun() {
    SharedData::instance().writeDouble(currentTime);
}

void decodeFun() {
    SharedData::instance().readDouble(currentTime);
}

void cleanUpFun() {
    box = nullptr;

    if (receiver) {
        receiver->ReleaseReceiver();
        receiver->Release();
    }
}

void keyCallback(int key, int, int action, int) {
    if (key == key::Esc && action == action::Press) {
        Engine::instance().terminate();
    }
}

int main(int argc, char* argv[]) {
    std::vector<std::string> arg(argv + 1, argv + argc);
    Configuration config = parseArguments(arg);
    config::Cluster cluster = loadCluster(config.configFilename);
    Engine::create(config);

    Engine::instance().setInitOGLFunction(initOGLFun);
    Engine::instance().setDrawFunction(drawFun);
    Engine::instance().setPreSyncFunction(preSyncFun);
    Engine::instance().setCleanUpFunction(cleanUpFun);
    Engine::instance().setKeyboardCallbackFunction(keyCallback);

    SharedData::instance().setEncodeFunction(encodeFun);
    SharedData::instance().setDecodeFunction(decodeFun);

    try {
        Engine::instance().init(Engine::RunMode::OpenGL_3_3_Core_Profile, cluster);
        Engine::instance().render();
    }
    catch (const std::runtime_error & e) {
        MessageHandler::printError("%s", e.what());
        Engine::destroy();
        return EXIT_FAILURE;
    }
    Engine::destroy();
    exit(EXIT_SUCCESS);
}
