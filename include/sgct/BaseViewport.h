/*************************************************************************
Copyright (c) 2012-2015 Miroslav Andel
All rights reserved.

For conditions of distribution and use, see copyright notice in sgct.h 
*************************************************************************/

#ifndef __SGCT__BASE_VIEWPORT__H__
#define __SGCT__BASE_VIEWPORT__H__

#include <sgct/Frustum.h>
#include <sgct/SGCTProjection.h>
#include <sgct/SGCTProjectionPlane.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace sgct_core {

class SGCTUser;

/**
 * This class holds and manages viewportdata and calculates frustums
 */
class BaseViewport {
public:
    BaseViewport();
    virtual ~BaseViewport() = default;

    /// Name this viewport
    void setName(std::string name);
    void setPos(glm::vec2 position);
    void setSize(glm::vec2 size);
    void setEnabled(bool state);
    void setUser(SGCTUser& user);
    void setUserName(std::string userName);
    void setEye(Frustum::FrustumMode eye);
    
    const std::string& getName() const;
    const glm::vec2& getPosition() const;
    const glm::vec2& getSize() const;
    float getHorizontalFieldOfViewDegrees() const;
    
    SGCTUser& getUser() const;
    Frustum::FrustumMode getEye() const;
    SGCTProjection& getProjection(Frustum::FrustumMode frustumMode);
    const SGCTProjection& getProjection(Frustum::FrustumMode frustumMode) const;
    SGCTProjection& getProjection();
    SGCTProjectionPlane& getProjectionPlane();
    glm::quat getRotation() const;
    glm::vec4 getFOV() const;
    float getDistance() const;
    
    bool isEnabled() const;
    void linkUserName();

    void calculateFrustum(Frustum::FrustumMode frustumMode,
        float nearClippingPlane, float farClippingPlane);

    /// Make projection symmetric relative to user
    void calculateNonLinearFrustum(Frustum::FrustumMode frustumMode,
        float nearClippingPlane, float farClippingPlane);
    void setViewPlaneCoordsUsingFOVs(float up, float down, float left, float right,
        glm::quat rot, float dist = 10.f);
    void setViewPlaneCoordsFromUnTransformedCoords(glm::vec3 lowerLeft,
        glm::vec3 upperLeft, glm::vec3 upperRight, const glm::quat& rot);
    void updateFovToMatchAspectRatio(float oldRatio, float newRatio);
    void setHorizontalFieldOfView(float horizFovDeg, float aspectRatio);

protected:
    struct {
        SGCTProjection mono;
        SGCTProjection stereoLeft;
        SGCTProjection stereoRight;
    } mProjections;
    
    SGCTProjectionPlane mProjectionPlane;
    Frustum::FrustumMode mEye = Frustum::MonoEye;

    SGCTUser& mUser;
    std::string mName = "NoName";
    std::string mUserName;
    bool mEnabled = true;
    glm::vec2 mPosition = glm::vec2(0.f, 0.f);
    glm::vec2 mSize = glm::vec2(1.f, 1.f);

    struct {
        glm::vec3 lowerLeft;
        glm::vec3 upperLeft;
        glm::vec3 upperRight;
    } mUnTransformedViewPlaneCoords;
    glm::quat mRot;
    float mDistance;
    glm::vec4 mFOV;
};

} // namespace sgct_core

#endif // __SGCT__BASE_VIEWPORT__H__
