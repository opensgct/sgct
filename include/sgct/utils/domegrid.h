/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2019                                                               *
 * For conditions of distribution and use, see copyright notice in sgct.h                *
 ****************************************************************************************/

#ifndef __SGCT__DOME_GRID__H__
#define __SGCT__DOME_GRID__H__

namespace sgct::utils {

/**
 * Helper class to render a dome grid
 */
class DomeGrid {
public:
    /// This constructor requires a valid OpenGL contex
    DomeGrid(float radius, float FOV, unsigned int segments, unsigned int rings,
        unsigned int resolution = 128);

    /// The destructor requires a valid OpenGL context
    ~DomeGrid();

    void draw();

private:
    unsigned int _resolution;
    unsigned int _rings;
    unsigned int _segments;
    unsigned int _vao = 0;
    unsigned int _vbo = 0;
};

} // namespace sgct::utils

#endif // __SGCT__DOME_GRID__H__