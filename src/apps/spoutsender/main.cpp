#include <sgct/commandline.h>
#include <sgct/engine.h>
#include <sgct/shadermanager.h>
#include <sgct/shareddata.h>
#include <sgct/texturemanager.h>
#include <sgct/window.h>
#include <sgct/utils/box.h>
#include <SpoutLibrary.h>
#include <sstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
    std::unique_ptr<sgct::utils::Box> box;
    GLint matrixLoc = -1;
    GLuint texture = 0;

    struct SpoutData {
        SPOUTHANDLE spoutSender;
        char senderName[256];
        bool initialized;
    };
    std::vector<SpoutData> spoutSendersData;

    size_t spoutSendersCount = 0;
    std::vector<std::pair<int, bool>> windowData; // index and if lefteye
    std::vector<std::string> senderNames;

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

void drawFun() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    constexpr const double Speed = 0.44;

    // create scene transform (animation)
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
    const ShaderProgram& prog = ShaderManager::instance().getShaderProgram("xform");
    prog.bind();
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniformMatrix4fv(matrixLoc, 1, GL_FALSE, glm::value_ptr(mvp));

    box->draw();

    prog.unbind();
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}

void postDrawFun() {
    glActiveTexture(GL_TEXTURE0);
    
    for (size_t i = 0; i < spoutSendersCount; i++) {
        if (!spoutSendersData[i].initialized) {
            continue;
        }
        int winIndex = windowData[i].first;

        GLuint texId;
        if (windowData[i].second) {
            texId = Engine::instance().getWindow(winIndex).getFrameBufferTexture(
                Engine::TextureIndex::LeftEye
            );
        }
        else {
            texId = Engine::instance().getWindow(winIndex).getFrameBufferTexture(
                Engine::TextureIndex::RightEye
            );
        }
            
        glBindTexture(GL_TEXTURE_2D, texId);
            
        spoutSendersData[i].spoutSender->SendTexture(
            texId,
            static_cast<GLuint>(GL_TEXTURE_2D),
            Engine::instance().getWindow(winIndex).getFramebufferResolution().x,
            Engine::instance().getWindow(winIndex).getFramebufferResolution().y
        );
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void preSyncFun() {
    if (Engine::instance().isMaster()) {
        currentTime.setVal(Engine::getTime());
    }
}

void preWindowInitFun() {
    std::string baseName = "SGCT_Window";

    //get number of framebuffer textures
    for (int i = 0; i < Engine::instance().getNumberOfWindows(); i++) {
        // do not resize buffers while minimized
        Engine::instance().getWindow(i).setFixResolution(true);

        if (Engine::instance().getWindow(i).isStereo()) {
            senderNames.push_back(baseName + std::to_string(i) + "_Left");
            windowData.push_back(std::pair(i, true));

            senderNames.push_back(baseName + std::to_string(i) + "_Right");
            windowData.push_back(std::pair(i, false));
        }
        else {
            senderNames.push_back(baseName + std::to_string(i));
            windowData.push_back(std::pair(i, true));
        }
    }

    spoutSendersCount = senderNames.size();
}

void initOGLFun() {
    // setup spout
    // Create a new SpoutData for every SGCT window
    spoutSendersData.resize(spoutSendersCount);
    for (size_t i = 0; i < spoutSendersCount; i++) {
        spoutSendersData[i].spoutSender = GetSpout();

        strcpy_s(spoutSendersData[i].senderName, senderNames[i].c_str());
        int winIndex = windowData[i].first;
        
        const bool success = spoutSendersData[i].spoutSender->CreateSender(
            spoutSendersData[i].senderName,
            Engine::instance().getWindow(winIndex).getFramebufferResolution().x,
            Engine::instance().getWindow(winIndex).getFramebufferResolution().y
        );
        spoutSendersData[i].initialized = success;
    }
    
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

    for (size_t i = 0; i < spoutSendersCount; i++) {
        spoutSendersData[i].spoutSender->ReleaseSender();
        spoutSendersData[i].spoutSender->Release();
    }
}

int main(int argc, char* argv[]) {
    std::vector<std::string> arg(argv + 1, argv + argc);
    Configuration config = parseArguments(arg);
    config::Cluster cluster = loadCluster(config.configFilename);

    Engine::instance().setInitOGLFunction(initOGLFun);
    Engine::instance().setDrawFunction(drawFun);
    Engine::instance().setPostDrawFunction(postDrawFun);
    Engine::instance().setPreSyncFunction(preSyncFun);
    Engine::instance().setCleanUpFunction(cleanUpFun);
    Engine::instance().setPreWindowFunction(preWindowInitFun);

    Engine::instance().setEncodeFunction(encodeFun);
    Engine::instance().setDecodeFunction(decodeFun);

    try {
        Engine::instance().init(Engine::RunMode::OpenGL_3_3_Core_Profile, cluster);
        Engine::instance().render();
    }
    catch (const std::runtime_error& e) {
        MessageHandler::printError("%s", e.what());
        Engine::destroy();
        return EXIT_FAILURE;
    }
    Engine::destroy();
    exit(EXIT_SUCCESS);
}
