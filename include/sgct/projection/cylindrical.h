/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2020                                                               *
 * For conditions of distribution and use, see copyright notice in LICENSE.md            *
 ****************************************************************************************/

#ifndef __SGCT__CYLINDRICAL__H__
#define __SGCT__CYLINDRICAL__H__

#include <sgct/projection/nonlinearprojection.h>

#include <sgct/callbackdata.h>
#include <sgct/frustum.h>

namespace sgct {

class CylindricalProjection : public NonLinearProjection {
public:
    CylindricalProjection(const Window* parent);
    ~CylindricalProjection();

    void render(const Window& window, const BaseViewport& viewport,
        Frustum::Mode) override;

    void renderCubemap(Window& window, Frustum::Mode frustumMode) override;

    void update(glm::vec2 size) override;

    void setRotation(float rotation);
    void setHeightOffset(float heightOffset);
    void setRadius(float radius);

private:
    void initVBO() override;
    void initViewports() override;
    void initShaders() override;

    void blitCubeFace(int face);
    void attachTextures(int face);

    float _rotation = 0.f;
    float _heightOffset = 0.f;
    float _radius = 5.f;

    struct {
        int cubemap = -1;
        int rotation = -1;
        int heightOffset = -1;
    } _shaderLoc;
    unsigned int _vao = 0;
    unsigned int _vbo = 0;
    ShaderProgram _shader;
    ShaderProgram _depthCorrectionShader;
};

} // namespace sgct

#endif // __SGCT__CYLINDRICAL__H__
