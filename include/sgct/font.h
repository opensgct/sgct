/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2019                                                               *
 * For conditions of distribution and use, see copyright notice in sgct.h                *
 ****************************************************************************************/

#ifndef __SGCT__FREETYPE_FONT__H__
#define __SGCT__FREETYPE_FONT__H__

#ifdef SGCT_HAS_TEXT

#include <freetype/ftglyph.h>
#include <freetype/ftstroke.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

namespace sgct::text {

/**
 * Will handle font textures and rendering. Implementation is based on
 * <a href="http://nehe.gamedev.net/tutorial/freetype_fonts_in_opengl/24001/">Nehe's font
 * tutorial for freetype</a>.
 */
class Font {
public:
    struct FontFaceData {
        unsigned int texId = 0;
        float distToNextChar = 0.f;
        glm::vec2 pos;
        glm::vec2 size;
        FT_Glyph glyph;
    };

    /**
     * Initializes all variables needed for the font. Needs to be called before creating
     * any textures for the font.
     *
     * \param face The truetype face pointer
     * \param height Font height in pixels
     */
    Font(FT_Library lib, FT_Face face, unsigned int h);

    /// Cleans up memory used by the Font and destroys the OpenGL objects
    ~Font();

    /// Get the font face data
    const Font::FontFaceData& getFontFaceData(wchar_t c);

    /// Get the vertex array id
    unsigned int getVAO() const;

    /// Get height of the font
    float getHeight() const;

    /**
     * Set the stroke (border) size.
     *
     * \param size The stroke size in pixels
     */
    void setStrokeSize(int size);

private:
    void createCharacter(wchar_t c);

    FT_Library _library;
    FT_Face	_face;
    FT_Fixed _strokeSize = 1;
    float _height;
    std::unordered_map<wchar_t, FontFaceData> _fontFaceData;
    unsigned int _vao = 0;
    unsigned int _vbo = 0;
};

} // namespace sgct

#endif // SGCT_HAS_TEXT
#endif // __SGCT__FREETYPE_FONT__H__