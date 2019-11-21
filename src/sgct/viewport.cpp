/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2019                                                               *
 * For conditions of distribution and use, see copyright notice in sgct.h                *
 ****************************************************************************************/

#include <sgct/viewport.h>

#include <sgct/clustermanager.h>
#include <sgct/config.h>
#include <sgct/fisheyeprojection.h>
#include <sgct/messagehandler.h>
#include <sgct/readconfig.h>
#include <sgct/screencapture.h>
#include <sgct/sphericalmirrorprojection.h>
#include <sgct/spoutoutputprojection.h>
#include <sgct/texturemanager.h>
#include <algorithm>
#include <array>
#include <optional>
#include <variant>

namespace {
    // Helper structs for the visitor pattern of the std::variant on projections
    template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
    template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
} // namespace

namespace sgct::core {

void Viewport::applyViewport(const config::Viewport& viewport) {
    if (viewport.user) {
        setUserName(*viewport.user);
    }
    if (viewport.overlayTexture) {
        _overlayFilename = *viewport.overlayTexture;
    }
    if (viewport.blendMaskTexture) {
        _blendMaskFilename = *viewport.blendMaskTexture;
    }
    if (viewport.blendLevelMaskTexture) {
        _blackLevelMaskFilename = *viewport.blendLevelMaskTexture;
    }
    if (viewport.correctionMeshTexture) {
        _meshFilename = *viewport.correctionMeshTexture;
    }
    if (viewport.meshHint) {
        _meshHint = *viewport.meshHint;
    }
    if (viewport.isTracked) {
        _isTracked = *viewport.isTracked;
    }
    if (viewport.eye) {
        Frustum::Mode e = [](config::Viewport::Eye e) {
            switch (e) {
                case config::Viewport::Eye::Mono:
                    return Frustum::Mode::MonoEye;
                case config::Viewport::Eye::StereoLeft:
                    return Frustum::Mode::StereoLeftEye;
                case config::Viewport::Eye::StereoRight:
                    return Frustum::Mode::StereoRightEye;
                default:
                    throw std::logic_error("Unhandled case label");
            }
        }(*viewport.eye);
        setEye(e);
    }

    if (viewport.position) {
        setPos(*viewport.position);
    }
    if (viewport.size) {
        setSize(*viewport.size);
    }

    std::visit(overloaded {
        [](const config::NoProjection&) {},
        [this](const config::PlanarProjection& p) { applyPlanarProjection(p); },
        [this](const config::FisheyeProjection& p) { applyFisheyeProjection(p); },
        [this](const config::SphericalMirrorProjection& p) {
            applySphericalMirrorProjection(p);
        },
        [this](const config::SpoutOutputProjection& p) { applySpoutOutputProjection(p); },
        [this](const config::ProjectionPlane& p) {
            _projectionPlane.setCoordinates(p.lowerLeft, p.upperLeft, p.upperRight);
            _viewPlane.lowerLeft = p.lowerLeft;
            _viewPlane.upperLeft = p.upperLeft;
            _viewPlane.upperRight = p.upperRight;
        },
    }, viewport.projection);
}

void Viewport::applySettings(const sgct::config::MpcdiProjection& mpcdi) {
    if (mpcdi.position) {
        setPos(*mpcdi.position);
    }
    if (mpcdi.size) {
        setSize(*mpcdi.size);
    }
    if (mpcdi.frustum) {
        setViewPlaneCoordsUsingFOVs(
            mpcdi.frustum->up,
            mpcdi.frustum->down,
            mpcdi.frustum->left,
            mpcdi.frustum->right,
            mpcdi.orientation.value_or(glm::quat()),
            mpcdi.distance.value_or(10.f)
        );
        if (mpcdi.offset) {
            _projectionPlane.offset(*mpcdi.offset);
        }
    }
}

void Viewport::applyPlanarProjection(const config::PlanarProjection& proj) {
    setViewPlaneCoordsUsingFOVs(
        proj.fov.up,
        proj.fov.down,
        proj.fov.left,
        proj.fov.right,
        proj.orientation.value_or(glm::quat()),
        proj.fov.distance.value_or(10.f)
    );
    if (proj.offset) {
        _projectionPlane.offset(*proj.offset);
    }
}

void Viewport::applyFisheyeProjection(const config::FisheyeProjection& proj) {
    std::unique_ptr<FisheyeProjection> fishProj = std::make_unique<FisheyeProjection>();
    fishProj->setUser(_user);
    
    if (proj.fov) {
        fishProj->setFOV(*proj.fov);
    }
    if (proj.quality) {
        fishProj->setCubemapResolution(*proj.quality);
    }
    if (proj.method) {
        FisheyeProjection::FisheyeMethod m = [](config::FisheyeProjection::Method m) {
            switch (m) {
                case config::FisheyeProjection::Method::FourFace:
                    return FisheyeProjection::FisheyeMethod::FourFaceCube;
                case config::FisheyeProjection::Method::FiveFace:
                    return FisheyeProjection::FisheyeMethod::FiveFaceCube;
                default:
                    throw std::logic_error("Unhandled case label");
            }
        }(*proj.method);
        fishProj->setRenderingMethod(m);
    }
    if (proj.interpolation) {
        NonLinearProjection::InterpolationMode i =
            [](config::FisheyeProjection::Interpolation i) {
                switch (i) {
                    case config::FisheyeProjection::Interpolation::Linear:
                        return core::NonLinearProjection::InterpolationMode::Linear;
                    case config::FisheyeProjection::Interpolation::Cubic:
                        return core::NonLinearProjection::InterpolationMode::Cubic;
                    default:
                        throw std::logic_error("Unhandled case label");
                }
            }(*proj.interpolation);
        fishProj->setInterpolationMode(i);
    }
    if (proj.tilt) {
        fishProj->setTilt(*proj.tilt);
    }
    if (proj.diameter) {
        fishProj->setDomeDiameter(*proj.diameter);
    }
    if (proj.crop) {
        config::FisheyeProjection::Crop crop = *proj.crop;
        fishProj->setCropFactors(crop.left, crop.right, crop.bottom, crop.top);
    }
    if (proj.offset) {
        fishProj->setBaseOffset(*proj.offset);
    }
    if (proj.background) {
        fishProj->setClearColor(*proj.background);
    }
    fishProj->setUseDepthTransformation(true);
    _nonLinearProjection = std::move(fishProj);
}

void Viewport::applySpoutOutputProjection(const config::SpoutOutputProjection& p) {
#ifdef SGCT_HAS_SPOUT  
    std::unique_ptr<SpoutOutputProjection> proj =
        std::make_unique<SpoutOutputProjection>();
    proj->setUser(_user);
    if (p.quality) {
        proj->setCubemapResolution(*p.quality);
    }
    if (p.mapping) {
        SpoutOutputProjection::Mapping m = [](config::SpoutOutputProjection::Mapping m) {
            switch (m) {
                case config::SpoutOutputProjection::Mapping::Fisheye:
                    return SpoutOutputProjection::Mapping::Fisheye;
                case config::SpoutOutputProjection::Mapping::Equirectangular:
                    return SpoutOutputProjection::Mapping::Equirectangular;
                case config::SpoutOutputProjection::Mapping::Cubemap:
                    return SpoutOutputProjection::Mapping::Cubemap;
                default:
                    throw std::logic_error("Unhandled case label");
            }
        }(*p.mapping);
        proj->setSpoutMapping(m);
    }
    proj->setSpoutMappingName(p.mappingSpoutName);
    if (p.background) {
        proj->setClearColor(*p.background);
    }
    if (p.channels) {
        config::SpoutOutputProjection::Channels c = *p.channels;
        proj->setSpoutChannels(c.right, c.zLeft, c.bottom, c.top, c.left, c.zRight);
    }
    if (p.orientation) {
        proj->setSpoutRigOrientation(*p.orientation);
    }

    _nonLinearProjection = std::move(proj);
#else
    (void)p;
    MessageHandler::printWarning("Spout library not added to SGCT");
#endif
}

void Viewport::applySphericalMirrorProjection(const config::SphericalMirrorProjection& p)
{
    auto proj = std::make_unique<SphericalMirrorProjection>(
        p.mesh.bottom,
        p.mesh.left,
        p.mesh.right,
        p.mesh.top
    );

    proj->setUser(_user);
    if (p.quality) {
        proj->setCubemapResolution(*p.quality);
    }
    if (p.tilt) {
        proj->setTilt(*p.tilt);
    }
    if (p.background) {
        proj->setClearColor(*p.background);
    }

    _nonLinearProjection = std::move(proj);
}

void Viewport::setMpcdiWarpMesh(std::vector<char> data) {
    _mpcdiWarpMesh = std::move(data);
}

void Viewport::loadData() {
    TextureManager& mgr = TextureManager::instance();
    if (!_overlayFilename.empty()) {
        _overlayTextureIndex = mgr.loadTexture(_overlayFilename, true, 1);
    }

    if (!_blendMaskFilename.empty()) {
        _blendMaskTextureIndex = mgr.loadTexture(_blendMaskFilename, true, 1);
    }

    if (!_blackLevelMaskFilename.empty()) {
        _blackLevelMaskTextureIndex = mgr.loadTexture(_blackLevelMaskFilename, true, 1);
    }

    if (!_mpcdiWarpMesh.empty()) {
        _mesh.loadMesh("mesh.mpcdi", *this, parseCorrectionMeshHint("mpcdi"));
    }
    else {
        // load default if _meshFilename is empty
        _mesh.loadMesh(_meshFilename, *this, parseCorrectionMeshHint(_meshHint));
    }
}

void Viewport::renderQuadMesh() const {
    if (_isEnabled) {
        _mesh.renderQuadMesh();
    }
}

void Viewport::renderWarpMesh() const {
    if (_isEnabled) {
        _mesh.renderWarpMesh();
    }
}

void Viewport::renderMaskMesh() const {
    if (_isEnabled) {
        _mesh.renderMaskMesh();
    }
}

bool Viewport::hasOverlayTexture() const {
    return _overlayTextureIndex != 0;
}

bool Viewport::hasBlendMaskTexture() const {
    return _blendMaskTextureIndex != 0;
}

bool Viewport::hasBlackLevelMaskTexture() const {
    return _blackLevelMaskTextureIndex != 0;
}

bool Viewport::hasSubViewports() const {
    return _nonLinearProjection != nullptr;
}

bool Viewport::isTracked() const {
    return _isTracked;
}

unsigned int Viewport::getOverlayTextureIndex() const {
    return _overlayTextureIndex;
}

unsigned int Viewport::getBlendMaskTextureIndex() const {
    return _blendMaskTextureIndex;
}

unsigned int Viewport::getBlackLevelMaskTextureIndex() const {
    return _blackLevelMaskTextureIndex;
}

NonLinearProjection* Viewport::getNonLinearProjection() const {
    return _nonLinearProjection.get();
}

const std::vector<char>& Viewport::mpcdiWarpMesh() const {
    return _mpcdiWarpMesh;
}

} // namespace sgct::core