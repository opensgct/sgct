/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2019                                                               *
 * For conditions of distribution and use, see copyright notice in sgct.h                *
 ****************************************************************************************/

#include <sgct/correction/sciss.h>

#include <sgct/engine.h>
#include <sgct/error.h>
#include <sgct/messagehandler.h>
#include <sgct/viewport.h>
#include <sgct/user.h>

#define Error(code, msg) sgct::Error(sgct::Error::Component::SCISS, code, msg)

namespace {
    struct SCISSTexturedVertex {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
        float tx = 0.f;
        float ty = 0.f;
        float tz = 0.f;
    };

    struct SCISSViewData {
        // Rotation quaternion
        float qx = 0.f;
        float qy = 0.f;
        float qz = 0.f;
        float qw = 1.f;

        // Position of view (currently unused in Uniview)
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;

        float fovUp = 20.f;
        float fovDown = 20.f;
        float fovLeft = 20.f;
        float fovRight = 20.f;
    };
} // namespace

namespace sgct::core::correction {

Buffer generateScissMesh(const std::string& path, core::Viewport& parent) {
    Buffer buf;

    MessageHandler::printInfo("Reading SCISS mesh data from '%s'", path.c_str());

    FILE* file = nullptr;
    file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        throw Error(2013, "Failed to open " + path);
    }


    char fileID[3];
    const size_t retHeader = fread(fileID, sizeof(char), 3, file);

    // check fileID
    if (fileID[0] != 'S' || fileID[1] != 'G' || fileID[2] != 'C' || retHeader != 3) {
        fclose(file);
        throw Error(2014, "Incorrect file id");
    }

    // read file version
    uint8_t fileVersion;
    const size_t retVer = fread(&fileVersion, sizeof(uint8_t), 1, file);
    if (retVer != 1) {
        fclose(file);
        throw Error(2015, "Error parsing file version from file");
    }

    MessageHandler::printDebug("CorrectionMesh: file version %u", fileVersion);

    // read mapping type
    unsigned int type;
    const size_t retType = fread(&type, sizeof(unsigned int), 1, file);
    if (retType != 1) {
        fclose(file);
        throw Error(2016, "Error parsing type from file");
    }

    MessageHandler::printDebug(
        "Mapping type = %s (%u)", type == 0 ? "planar" : "cube", type
    );

    // read viewdata
    SCISSViewData viewData;
    const size_t retData = fread(&viewData, sizeof(SCISSViewData), 1, file);
    double yaw, pitch, roll;
    if (retData != 1) {
        fclose(file);
        throw Error(2017, "Error parsing view data from file");
    }

    const double x = static_cast<double>(viewData.qx);
    const double y = static_cast<double>(viewData.qy);
    const double z = static_cast<double>(viewData.qz);
    const double w = static_cast<double>(viewData.qw);
        
    // Switching the Euler angles to switch from a right-handed coordinate system to
    // a left-handed one
    glm::dvec3 angles = glm::degrees(glm::eulerAngles(glm::dquat(w, y, x, z)));
    yaw = -angles.x;
    pitch = angles.y;
    roll = -angles.z;
        
    MessageHandler::printDebug(
        "Rotation quat = [%f %f %f %f]. yaw = %lf, pitch = %lf, roll = %lf",
        viewData.qx, viewData.qy, viewData.qz, viewData.qw, yaw, pitch, roll);

    MessageHandler::printDebug("Position: %f %f %f", viewData.x, viewData.y, viewData.z);

    MessageHandler::printDebug(
        "FOV: (up %f) (down %f) (left %f) (right %f)",
        viewData.fovUp, viewData.fovDown, viewData.fovLeft, viewData.fovRight
    );

    // read number of vertices
    unsigned int size[2];
    const size_t retSize = fread(size, sizeof(unsigned int), 2, file);
    if (retSize != 2) {
        fclose(file);
        throw std::runtime_error("Error parsing file");
    }

    unsigned int nVertices = 0;
    if (fileVersion == 2) {
        nVertices = size[1];
        MessageHandler::printDebug("Number of vertices = %u", nVertices);
    }
    else {
        nVertices = size[0] * size[1];
        MessageHandler::printDebug(
            "Number of vertices = %u (%ux%u)", nVertices, size[0], size[1]
        );
    }
    // read vertices
    std::vector<SCISSTexturedVertex> texturedVertexList(nVertices);
    const size_t retVertices = fread(
        texturedVertexList.data(),
        sizeof(SCISSTexturedVertex),
        nVertices,
        file
    );
    if (retVertices != nVertices) {
        fclose(file);
        throw Error(2018, "Error parsing vertices from file");
    }

    // read number of indices
    unsigned int nIndices = 0;
    const size_t retIndices = fread(&nIndices, sizeof(unsigned int), 1, file);

    if (retIndices != 1) {
        fclose(file);
        throw Error(2019, "Error parsing indices from file");
    }
    MessageHandler::printDebug("Number of indices = %u", nIndices);

    // read faces
    if (nIndices > 0) {
        buf.indices.resize(nIndices);
        const size_t r = fread(buf.indices.data(), sizeof(unsigned int), nIndices, file);
        if (r != nIndices) {
            fclose(file);
            throw Error(2020, "Error parsing faces from file");
        }
    }

    fclose(file);

    parent.getUser().setPos(glm::vec3(viewData.x, viewData.y, viewData.z));

    parent.setViewPlaneCoordsUsingFOVs(
        viewData.fovUp,
        viewData.fovDown,
        viewData.fovLeft,
        viewData.fovRight,
        glm::quat(viewData.qw, viewData.qx, viewData.qy, viewData.qz)
    );

    Engine::instance().updateFrustums();

    buf.vertices.resize(nVertices);
    for (unsigned int i = 0; i < nVertices; i++) {
        SCISSTexturedVertex& scissVertex = texturedVertexList[i];
        scissVertex.x = glm::clamp(scissVertex.x, 0.f, 1.f);
        scissVertex.y = glm::clamp(scissVertex.y, 0.f, 1.f);
        scissVertex.tx = glm::clamp(scissVertex.tx, 0.f, 1.f);
        scissVertex.ty = glm::clamp(scissVertex.ty, 0.f, 1.f);

        const glm::vec2& s = parent.getSize();
        const glm::vec2& p = parent.getPosition();

        // convert to [-1, 1]
        CorrectionMeshVertex& vertex = buf.vertices[i];
        vertex.x = 2.f * (scissVertex.x * s.x + p.x) - 1.f;
        vertex.y = 2.f * ((1.f - scissVertex.y) * s.y + p.y) - 1.f;

        vertex.s = scissVertex.tx * parent.getSize().x + parent.getPosition().x;
        vertex.t = scissVertex.ty * parent.getSize().y + parent.getPosition().y;

        vertex.r = 1.f;
        vertex.g = 1.f;
        vertex.b = 1.f;
        vertex.a = 1.f;
    }

    if (fileVersion == '2' && size[0] == 4) {
        buf.geometryType = GL_TRIANGLES;
    }
    else if (fileVersion == '2' && size[0] == 5) {
        buf.geometryType = GL_TRIANGLE_STRIP;
    }
    else {
        // assuming v1
        buf.geometryType = GL_TRIANGLE_STRIP;
    }

    return buf;
}

} // namespace sgct::core::correction
