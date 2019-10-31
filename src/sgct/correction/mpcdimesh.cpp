/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2019                                                               *
 * For conditions of distribution and use, see copyright notice in sgct.h                *
 ****************************************************************************************/

#include <sgct/correction/mpcdimesh.h>

#include <sgct/messagehandler.h>
#include <sgct/viewport.h>

namespace {
    void readMeshBuffer(float* dest, unsigned int& idx, const char* src, int size) {
        float val;
        memcpy(&val, &src[idx], size);
        *dest = val;
        idx += size;
    }
} // namespace

namespace sgct::core::correction {

Buffer generateMpcdiMesh(const core::Viewport& parent) {
    Buffer buf;

    MessageHandler::printInfo("Reading MPCDI mesh (PFM format) from buffer");
    const char* srcBuff = parent.mpcdiWarpMesh().data();
    size_t srcSizeBytes = parent.mpcdiWarpMesh().size();

    constexpr const int MaxHeaderLineLength = 100;
    char headerBuffer[MaxHeaderLineLength];
    unsigned int srcIdx = 0;
    int index = 0;
    int nNewlines = 0;
    do {
        char headerChar = 0;

        if (srcIdx == srcSizeBytes) {
            throw std::runtime_error("Error reading from file. Could not find lines");
        }

        headerChar = srcBuff[srcIdx++];
        headerBuffer[index++] = headerChar;
        if (headerChar == '\n') {
            nNewlines++;
        }
    } while (nNewlines < 3);

    char fileFormatHeader[2];
    unsigned int nCols = 0;
    unsigned int nRows = 0;
    const int res = sscanf(headerBuffer, "%2c %d %d", fileFormatHeader, &nCols, &nRows);

    if (res != 4) {
        throw std::runtime_error("Invalid header syntax");
    }

    if (fileFormatHeader[0] != 'P' || fileFormatHeader[1] != 'F') {
        //The 'Pf' header is invalid because PFM grayscale type is not supported.
        throw std::runtime_error("Incorrect file type. Unknown header type");
    }
    const int nCorrectionValues = nCols * nRows;
    std::vector<float> corrGridX(nCorrectionValues);
    std::vector<float> corrGridY(nCorrectionValues);
    float errorPos;

    for (int i = 0; i < nCorrectionValues; ++i) {
        readMeshBuffer(&corrGridX[i], srcIdx, srcBuff, 4);
        readMeshBuffer(&corrGridY[i], srcIdx, srcBuff, 4);
        readMeshBuffer(&errorPos, srcIdx, srcBuff, 4);
    }

    std::vector<glm::vec2> smoothPos(nCorrectionValues);
    std::vector<glm::vec2> warpedPos(nCorrectionValues);
    for (int i = 0; i < nCorrectionValues; ++i) {
        const float gridIdxCol = static_cast<float>(i % nCols);
        const float gridIdxRow = static_cast<float>(i / nCols);
        // Compute XY positions for each point based on a normalized 0,0 to 1,1 grid,
        // add the correction offsets to each warp point
        smoothPos[i].x = gridIdxCol / static_cast<float>(nCols - 1);
        // Reverse the y position as the values from the PFM file are given in raster-scan
        // order, which is left to right but starts at upper-left rather than lower-left.
        smoothPos[i].y = 1.f - (gridIdxRow / static_cast<float>(nRows - 1));
        warpedPos[i].x = smoothPos[i].x + corrGridX[i];
        warpedPos[i].y = smoothPos[i].y + corrGridY[i];
    }

    corrGridX.clear();
    corrGridY.clear();

    buf.vertices.reserve(nCorrectionValues);
    for (int i = 0; i < nCorrectionValues; ++i) {
        CorrectionMeshVertex vertex;
        // init to max intensity (opaque white)
        vertex.r = 1.f;
        vertex.g = 1.f;
        vertex.b = 1.f;
        vertex.a = 1.f;

        vertex.s = smoothPos[i].x;
        vertex.t = smoothPos[i].y;

        // scale to viewport coordinates
        vertex.x = 2.f * warpedPos[i].x - 1.f;
        vertex.y = 2.f * warpedPos[i].y - 1.f;
        buf.vertices.push_back(vertex);
    }
    warpedPos.clear();
    smoothPos.clear();

    buf.indices.reserve(6 * nCorrectionValues);
    for (unsigned int c = 0; c < (nCols - 1); ++c) {
        for (unsigned int r = 0; r < (nRows - 1); ++r) {
            const unsigned int i0 = r * nCols + c;
            const unsigned int i1 = r * nCols + (c + 1);
            const unsigned int i2 = (r + 1) * nCols + (c + 1);
            const unsigned int i3 = (r + 1) * nCols + c;

            // 3      2
            //  x____x
            //  |   /|
            //  |  / |
            //  | /  |
            //  |/   |
            //  x----x
            // 0      1

            // triangle 1
            buf.indices.push_back(i0);
            buf.indices.push_back(i1);
            buf.indices.push_back(i2);

            // triangle 2
            buf.indices.push_back(i0);
            buf.indices.push_back(i2);
            buf.indices.push_back(i3);
        }
    }

    buf.geometryType = GL_TRIANGLES;
    return buf;
}

} // namespace sgct::core::correction