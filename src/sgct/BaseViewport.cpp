/*************************************************************************
Copyright (c) 2012-2015 Miroslav Andel
All rights reserved.

For conditions of distribution and use, see copyright notice in sgct.h 
*************************************************************************/

#include <sgct/BaseViewport.h>

#include <sgct/ClusterManager.h>
#include <sgct/SGCTUser.h>

namespace sgct_core {

BaseViewport::BaseViewport() : mUser(ClusterManager::instance()->getDefaultUser()) {}

void BaseViewport::setName(std::string name) {
    mName = std::move(name);
}

void BaseViewport::setPos(float x, float y) {
    mX = x;
    mY = y;
}

void BaseViewport::setSize(float x, float y) {
    mXSize = x;
    mYSize = y;
}

void BaseViewport::setEnabled(bool state) {
    mEnabled = state;
}

bool BaseViewport::isEnabled() const {
    return mEnabled;
}

void BaseViewport::setEye(Frustum::FrustumMode eye) {
    mEye = eye;
}

const std::string& BaseViewport::getName() const {
    return mName;
}

glm::vec2 BaseViewport::getPosition() const {
    return { mX, mY };
}

glm::vec2 BaseViewport::getSize() const {
    return { mXSize, mYSize };
}

void BaseViewport::setUser(SGCTUser* user) {
    mUser = user;
}

SGCTUser* BaseViewport::getUser() const {
    return mUser;
}

Frustum::FrustumMode BaseViewport::getEye() const {
    return mEye;
}

SGCTProjection& BaseViewport::getProjection(Frustum::FrustumMode frustumMode) {
    return mProjections[frustumMode];
}

const SGCTProjection& BaseViewport::getProjection(Frustum::FrustumMode frustumMode) const
{
    return mProjections[frustumMode];
}


SGCTProjection& BaseViewport::getProjection() {
    return mProjections[mEye];
}

SGCTProjectionPlane& BaseViewport::getProjectionPlane() {
    return mProjectionPlane;
}

glm::quat BaseViewport::getRotation() const {
    return mRot;
}

glm::vec4 BaseViewport::getFOV() const {
    return mFOV;
}

float BaseViewport::getDistance() const {
    return mDistance;
}

void BaseViewport::setUserName(std::string userName) {
    mUserName = std::move(userName);
    linkUserName();
}

void BaseViewport::linkUserName() {
    SGCTUser* user = ClusterManager::instance()->getUser(mUserName);
    if (user != nullptr) {
        mUser = user;
    }
}

void BaseViewport::calculateFrustum(const Frustum::FrustumMode& frustumMode,
                                    float nearClippingPlane,
                                    float farClippingPlane)
{
    glm::vec3 eyePos = [this, frustumMode]() {
        switch (frustumMode) {
            case Frustum::FrustumMode::MonoEye:
                return mUser->getPosMono();
            case Frustum::FrustumMode::StereoLeftEye:
                return mUser->getPosLeftEye();
            case Frustum::FrustumMode::StereoRightEye:
                return mUser->getPosRightEye();
            default:
                return glm::vec3();
        }
    }();
    mProjections[frustumMode].calculateProjection(
        eyePos,
        mProjectionPlane,
        nearClippingPlane,
        farClippingPlane
    );
}

void BaseViewport::calculateNonLinearFrustum(const Frustum::FrustumMode& frustumMode,
                                             float nearClippingPlane,
                                             float farClippingPlane)
{
    glm::vec3 eyePos = mUser->getPosMono();
    glm::vec3 offset = [this, frustumMode, eyePos]() {
        switch (frustumMode) {
        case Frustum::FrustumMode::MonoEye:
            return mUser->getPosMono() - eyePos;
        case Frustum::FrustumMode::StereoLeftEye:
            return mUser->getPosLeftEye() - eyePos;
        case Frustum::FrustumMode::StereoRightEye:
            return mUser->getPosRightEye() - eyePos;
        default:
            return glm::vec3();
        }
    }();

    mProjections[frustumMode].calculateProjection(
        eyePos,
        mProjectionPlane,
        nearClippingPlane,
        farClippingPlane,
        offset
    );
}

void BaseViewport::setViewPlaneCoordsUsingFOVs(float up, float down, float left,
                                               float right, glm::quat rot, float dist)
{
    mRot = rot;

    mFOV = glm::vec4(up, down, left, right);
    mDistance = dist;

    mUnTransformedViewPlaneCoords.lowerLeft.x =
        dist * tanf(glm::radians<float>(left));
    mUnTransformedViewPlaneCoords.lowerLeft.y =
        dist * tanf(glm::radians<float>(down));
    mUnTransformedViewPlaneCoords.lowerLeft.z = -dist;

    mUnTransformedViewPlaneCoords.upperLeft.x =
        dist * tanf(glm::radians<float>(left));
    mUnTransformedViewPlaneCoords.upperLeft.y =
        dist * tanf(glm::radians<float>(up));
    mUnTransformedViewPlaneCoords.upperLeft.z = -dist;

    mUnTransformedViewPlaneCoords.upperRight.x =
        dist * tanf(glm::radians<float>(right));
    mUnTransformedViewPlaneCoords.upperRight.y =
        dist * tanf(glm::radians<float>(up));
    mUnTransformedViewPlaneCoords.upperRight.z = -dist;

    setViewPlaneCoordsFromUnTransformedCoords(
        mUnTransformedViewPlaneCoords.lowerLeft,
        mUnTransformedViewPlaneCoords.upperLeft,
        mUnTransformedViewPlaneCoords.upperRight,
        rot
    );
}

void BaseViewport::setViewPlaneCoordsFromUnTransformedCoords(glm::vec3 lowerLeft,
                                                             glm::vec3 upperLeft,
                                                             glm::vec3 upperRight,
                                                             const glm::quat& rot)
{
    mProjectionPlane.setCoordinateLowerLeft(rot * std::move(lowerLeft));
    mProjectionPlane.setCoordinateUpperLeft(rot * std::move(upperLeft));
    mProjectionPlane.setCoordinateUpperRight(rot * std::move(upperRight));
}

void BaseViewport::updateFovToMatchAspectRatio(float oldRatio, float newRatio) {
    mUnTransformedViewPlaneCoords.lowerLeft.x *= newRatio / oldRatio;
    mUnTransformedViewPlaneCoords.upperLeft.x *= newRatio / oldRatio;
    mUnTransformedViewPlaneCoords.upperRight.x *= newRatio / oldRatio;
    setViewPlaneCoordsFromUnTransformedCoords(
        mUnTransformedViewPlaneCoords.lowerLeft,
        mUnTransformedViewPlaneCoords.upperLeft,
        mUnTransformedViewPlaneCoords.upperRight,
        mRot
    );
}

float BaseViewport::getHorizontalFieldOfViewDegrees() const {
    float xDist = (mProjectionPlane.getCoordinateUpperRight().x -
        mProjectionPlane.getCoordinateUpperLeft().x) / 2;
    float zDist = mProjectionPlane.getCoordinateUpperRight().z;
    return (glm::degrees(atanf(fabs(xDist / zDist)))) * 2;
}

void BaseViewport::setHorizontalFieldOfView(float horizFovDeg, float aspectRatio) {
    glm::vec2 projPlaneDims;
    float zDist = mProjectionPlane.getCoordinateUpperRight().z;
    projPlaneDims.x = fabs(zDist) * tanf(glm::radians<float>(horizFovDeg) / 2);
    projPlaneDims.y = projPlaneDims.x / aspectRatio;
    float verticalAngle = glm::degrees(atanf(projPlaneDims.y / fabs(zDist)));

    setViewPlaneCoordsUsingFOVs(
         verticalAngle,
        -verticalAngle,
        -horizFovDeg / 2,
         horizFovDeg / 2,
         mRot,
         fabs(zDist)
    );
}

} // namespace sgct_core
