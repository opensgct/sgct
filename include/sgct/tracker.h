/*************************************************************************
Copyright (c) 2012-2015 Miroslav Andel
All rights reserved.

For conditions of distribution and use, see copyright notice in sgct.h 
*************************************************************************/

#ifndef __SGCT__TRACKER__H__
#define __SGCT__TRACKER__H__

#include <sgct/trackingdevice.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace sgct {

/**
 * Class that manages a tracking system's properties and devices/sensors
 */
class Tracker {
public:
    explicit Tracker(std::string name);

    void setEnabled(bool state);
    void addDevice(std::string name, size_t index);

    TrackingDevice* getLastDevice() const;
    TrackingDevice* getDevice(size_t index) const;
    TrackingDevice* getDevice(const std::string& name) const;
    TrackingDevice* getDeviceBySensorId(int id) const;

    /// Set the orientation as quaternion
    void setOrientation(glm::quat q);
    /// Set the orientation as euler angles (degrees)
    void setOrientation(float xRot, float yRot, float zRot);
    void setOffset(glm::vec3 offset);
    void setScale(double scaleVal);

    /**
     * Set the tracker system transform matrix
     * worldTransform = (trackerTransform * sensorMat) * deviceTransformMat
     */
    void setTransform(glm::mat4 mat);

    glm::mat4 getTransform() const;
    double getScale() const;

    int getNumberOfDevices() const;
    const std::string& getName() const;

private:
    void calculateTransform();

    std::vector<std::unique_ptr<TrackingDevice>> mTrackingDevices;
    std::string mName;

    double mScale = 1.0;
    glm::mat4 mXform = glm::mat4(1.f);
    glm::mat4 mOrientation;
    glm::vec3 mOffset = glm::vec3(0.f);
};

} // namespace sgct

#endif // __SGCT__TRACKER__H__
