/*************************************************************************
Copyright (c) 2012-2015 Miroslav Andel
All rights reserved.

For conditions of distribution and use, see copyright notice in sgct.h 
*************************************************************************/

#include <sgct/BaseViewport.h>
#include <sgct/ClusterManager.h>
#include <sgct/MessageHandler.h>
#include <sgct/SGCTUser.h>

namespace sgct_core {

BaseViewport::BaseViewport()
    : mUser(ClusterManager::instance()->getDefaultUserPtr())
    , mName("NoName")
{}

/*!
Name this viewport
*/
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

float BaseViewport::getX() const {
    return mX;
}

float BaseViewport::getY() const {
    return mY;
}

float BaseViewport::getXSize() const {
    return mXSize;
}

float BaseViewport::getYSize() const {
    return mYSize;
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

SGCTProjection* BaseViewport::getProjection(Frustum::FrustumMode frustumMode) {
    return &mProjections[frustumMode];
}

SGCTProjection* BaseViewport::getProjection() {
    return &mProjections[mEye];
}

SGCTProjectionPlane* BaseViewport::getProjectionPlane() { return &mProjectionPlane; }

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
    SGCTUser* user = ClusterManager::instance()->getUserPtr(mUserName);
    if (user != nullptr) {
        mUser = user;
    }
}

void BaseViewport::calculateFrustum(const Frustum::FrustumMode& frustumMode,
                                    float near_clipping_plane,
                                    float far_clipping_plane)
{
    glm::vec3 eyePos = mUser->getPos(frustumMode);
    mProjections[frustumMode].calculateProjection(
        eyePos,
        &mProjectionPlane,
        near_clipping_plane,
        far_clipping_plane
    );
}

/*!
   Make projection symmetric relative to user
*/
void BaseViewport::calculateNonLinearFrustum(const Frustum::FrustumMode& frustumMode,
                                             float near_clipping_plane,
                                             float far_clipping_plane)
{
    glm::vec3 eyePos = mUser->getPos();
    glm::vec3 offset = mUser->getPos(frustumMode) - eyePos;
    mProjections[frustumMode].calculateProjection(
        eyePos,
        &mProjectionPlane,
        near_clipping_plane,
        far_clipping_plane,
        offset
    );
}

void BaseViewport::setViewPlaneCoordsUsingFOVs(float up, float down, float left,
                                               float right, glm::quat rot, float dist)
{
    mRot = rot;

    mFOV = glm::vec4(up, down, left, right);
    mDistance = dist;

    mUnTransformedViewPlaneCoords[SGCTProjectionPlane::LowerLeft].x =
        dist * tanf(glm::radians<float>(left));
    mUnTransformedViewPlaneCoords[SGCTProjectionPlane::LowerLeft].y =
        dist * tanf(glm::radians<float>(down));
    mUnTransformedViewPlaneCoords[SGCTProjectionPlane::LowerLeft].z = -dist;

    mUnTransformedViewPlaneCoords[SGCTProjectionPlane::UpperLeft].x =
        dist * tanf(glm::radians<float>(left));
    mUnTransformedViewPlaneCoords[SGCTProjectionPlane::UpperLeft].y =
        dist * tanf(glm::radians<float>(up));
    mUnTransformedViewPlaneCoords[SGCTProjectionPlane::UpperLeft].z = -dist;

    mUnTransformedViewPlaneCoords[SGCTProjectionPlane::UpperRight].x =
        dist * tanf(glm::radians<float>(right));
    mUnTransformedViewPlaneCoords[SGCTProjectionPlane::UpperRight].y =
        dist * tanf(glm::radians<float>(up));
    mUnTransformedViewPlaneCoords[SGCTProjectionPlane::UpperRight].z = -dist;

    setViewPlaneCoordsFromUnTransformedCoords(mUnTransformedViewPlaneCoords, rot);
}

void BaseViewport::setViewPlaneCoordsFromUnTransformedCoords(
                                                         glm::vec3 untransformedCoords[3],
                                                                     const glm::quat& rot)
{
    mProjectionPlane.setCoordinate(0, rot * untransformedCoords[0]);
    mProjectionPlane.setCoordinate(1, rot * untransformedCoords[1]);
    mProjectionPlane.setCoordinate(2, rot * untransformedCoords[2]);
}

void BaseViewport::updateFovToMatchAspectRatio(float oldRatio, float newRatio) {
    for (unsigned int c = static_cast<unsigned int>(SGCTProjectionPlane::LowerLeft);
         c <= static_cast<unsigned int>(SGCTProjectionPlane::UpperRight);
         ++c)
    {
        mUnTransformedViewPlaneCoords[c].x *= newRatio / oldRatio;
    }
    setViewPlaneCoordsFromUnTransformedCoords(mUnTransformedViewPlaneCoords, mRot);
}

float BaseViewport::getHorizontalFieldOfViewDegrees() const {
    float xDist = (mProjectionPlane.getCoordinate(SGCTProjectionPlane::UpperRight).x -
        mProjectionPlane.getCoordinate(SGCTProjectionPlane::UpperLeft).x) / 2;
    float zDist = mProjectionPlane.getCoordinate(SGCTProjectionPlane::UpperRight).z;
    return (glm::degrees(atanf(fabs(xDist / zDist)))) * 2;
}

void BaseViewport::setHorizontalFieldOfView(float horizFovDeg, float aspectRatio) {
    glm::vec2 projPlaneDims;
    float zDist = mProjectionPlane.getCoordinate(SGCTProjectionPlane::UpperRight).z;
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
