/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2019                                                               *
 * For conditions of distribution and use, see copyright notice in sgct.h                *
 ****************************************************************************************/

#ifndef __SGCT__SPHERICAL_MIRROR_PROJECTION__H__
#define __SGCT__SPHERICAL_MIRROR_PROJECTION__H__

#include <sgct/nonlinearprojection.h>

#include <sgct/correctionmesh.h>
#include <glm/glm.hpp>

namespace sgct::core {

/// This class manages and renders non-linear fisheye projections
class SphericalMirrorProjection : public NonLinearProjection {
public:
    SphericalMirrorProjection(std::string bottomMesh, std::string leftMesh,
        std::string rightMesh, std::string topMesh);
    virtual ~SphericalMirrorProjection() = default;

    void update(glm::vec2 size) override;

    /// Render the non linear projection to currently bounded FBO
    void render() override;

    /// Render the enabled faces of the cubemap
    void renderCubemap() override;

    /**
     * Set the dome tilt angle used in the spherical mirror renderer. The tilt angle is
     * from the horizontal.
     *
     * \param angle the tilt angle in degrees
     */
    void setTilt(float angle);

private:
    void initTextures() override;
    void initVBO() override;
    void initViewports() override;
    void initShaders() override;
       
    float _tilt = 0.f;
    float _diameter = 2.4f;

    // mesh data
    struct {
        CorrectionMesh bottom;
        CorrectionMesh left;
        CorrectionMesh right;
        CorrectionMesh top;
    } _meshes;
    struct {
        std::string bottom;
        std::string left;
        std::string right;
        std::string top;
    } _meshPaths;

    // shader locations
    int _texLoc = -1;
    int _matrixLoc = -1;
};

} // namespace sgct::core

#endif // __SGCT__SPHERICAL_MIRROR_PROJECTION__H__