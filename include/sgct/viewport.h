/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2019                                                               *
 * For conditions of distribution and use, see copyright notice in sgct.h                *
 ****************************************************************************************/

#ifndef __SGCT__VIEWPORT__H__
#define __SGCT__VIEWPORT__H__

#include <sgct/baseviewport.h>

#include <sgct/correctionmesh.h>
#include <memory>
#include <string>
#include <vector>

namespace sgct::config {
    struct FisheyeProjection;
    struct MpcdiProjection;
    struct PlanarProjection;
    struct SphericalMirrorProjection;
    struct SpoutOutputProjection;
    struct Viewport;
} // namespace sgct::config

namespace sgct::core {

class NonLinearProjection;

/// This class holds and manages viewportdata and calculates frustums
class Viewport : public BaseViewport {
public:
    void applyViewport(const sgct::config::Viewport& viewport);
    void applySettings(const sgct::config::MpcdiProjection& mpcdi);
    void setMpcdiWarpMesh(std::vector<char> data);
    // void setTracked(bool state);
    void loadData();

    /// Render the viewport mesh which the framebuffer texture is attached to
    void renderQuadMesh() const;

    /// Render the viewport mesh which the framebuffer texture is attached to
    void renderWarpMesh() const;

    /// Render the viewport mesh which the framebuffer texture is attached to
    void renderMaskMesh() const;

    bool hasOverlayTexture() const;
    bool hasBlendMaskTexture() const;
    bool hasBlackLevelMaskTexture() const;
    bool hasSubViewports() const;
    bool isTracked() const;
    unsigned int getOverlayTextureIndex() const;
    unsigned int getBlendMaskTextureIndex() const;
    unsigned int getBlackLevelMaskTextureIndex() const;
    NonLinearProjection* getNonLinearProjection() const;
    const std::vector<char>& mpcdiWarpMesh() const;

private:
    void applyPlanarProjection(const config::PlanarProjection& proj);
    void applyFisheyeProjection(const config::FisheyeProjection& proj);
    void applySpoutOutputProjection(const config::SpoutOutputProjection& proj);
    void applySphericalMirrorProjection(const config::SphericalMirrorProjection& proj);

    CorrectionMesh _mesh;
    std::string _overlayFilename;
    std::string _blendMaskFilename;
    std::string _blackLevelMaskFilename;
    std::string _meshFilename;
    std::string _meshHint;
    bool _isTracked = false;
    unsigned int _overlayTextureIndex = 0;
    unsigned int _blendMaskTextureIndex = 0;
    unsigned int _blackLevelMaskTextureIndex = 0;

    std::unique_ptr<NonLinearProjection> _nonLinearProjection;
    std::vector<char> _mpcdiWarpMesh;
};

} // namespace sgct::core

#endif // __SGCT__VIEWPORT__H__