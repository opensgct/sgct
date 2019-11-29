/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2019                                                               *
 * For conditions of distribution and use, see copyright notice in sgct.h                *
 ****************************************************************************************/

#ifdef SGCT_HAS_TEXT

#include <sgct/font.h>

#include <sgct/logger.h>
#include <sgct/ogl_headers.h>
#include <freetype/ftstroke.h>
#include <array>
#include <optional>

namespace {
    struct GlyphData {
        FT_Glyph glyph;
        FT_Glyph strokeGlyph;
        FT_Stroker stroker;
        FT_BitmapGlyph bitmapGlyph;
        FT_BitmapGlyph bitmapStrokeGlyph;
        FT_Bitmap* bitmap;
        FT_Bitmap* strokeBitmap;
    };

    bool getPixelData(FT_Library library, FT_Face face, FT_Fixed strokeSize, int& width,
                      int& height, std::vector<unsigned char>& pixels, GlyphData& gd)
    {
        // Move the face's glyph into a Glyph object
        FT_Error glyphErr = FT_Get_Glyph(face->glyph, &gd.glyph);
        FT_Error strokeErr = FT_Get_Glyph(face->glyph, &gd.strokeGlyph);
        if (glyphErr || strokeErr) {
            return false;
        }

        gd.stroker = nullptr;
        FT_Error error = FT_Stroker_New(library, &gd.stroker);
        if (!error) {
            FT_Stroker_Set(
                gd.stroker,
                64 * strokeSize,
                FT_STROKER_LINECAP_ROUND,
                FT_STROKER_LINEJOIN_ROUND,
                0
            );

            FT_Glyph_Stroke(&gd.strokeGlyph, gd.stroker, 1);
        }

        // Convert the glyph to a bitmap
        FT_Glyph_To_Bitmap(&gd.glyph, ft_render_mode_normal, nullptr, 1);
        gd.bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(gd.glyph);

        FT_Glyph_To_Bitmap(&gd.strokeGlyph, ft_render_mode_normal, nullptr, 1);
        gd.bitmapStrokeGlyph = reinterpret_cast<FT_BitmapGlyph>(gd.strokeGlyph);

        // This pointer will make accessing the bitmap easier
        gd.bitmap = &gd.bitmapGlyph->bitmap;
        gd.strokeBitmap = &gd.bitmapStrokeGlyph->bitmap;

        // Use our helper function to get the widths of the bitmap data that we will need
        // in order to create our texture
        width = gd.strokeBitmap->width;
        height = gd.strokeBitmap->rows;

        // Allocate memory for the texture data
        pixels.resize(2 * width * height);
        std::fill(pixels.begin(), pixels.end(), static_cast<unsigned char>(0));

        // read alpha to one channel and stroke - alpha in the second channel. We use the
        // ?: operator so that value which we use will be 0 if we are in the padding zone,
        // and whatever is the the Freetype bitmap otherwise
        const int offsetWidth = (gd.strokeBitmap->width - gd.bitmap->width) / 2;
        const int offsetRows = (gd.strokeBitmap->rows - gd.bitmap->rows) / 2;
        for (int j = 0; j < height; ++j) {
            for (int i = 0; i < width; ++i) {
                const int k = i - offsetWidth;
                const int l = j - offsetRows;

                const int idx = 2 * (i + j * width);
                if (k >= static_cast<int>(gd.bitmap->width) ||
                    l >= static_cast<int>(gd.bitmap->rows) || k < 0 || l < 0)
                {
                    pixels[idx] = 0;
                }
                else {
                    pixels[idx] = gd.bitmap->buffer[k + gd.bitmap->width * l];
                }

                const bool strokeInRange =
                    i >= static_cast<int>(gd.strokeBitmap->width) ||
                    j >= static_cast<int>(gd.strokeBitmap->rows);
                unsigned char strokeVal = strokeInRange ?
                    0 : gd.strokeBitmap->buffer[i + gd.strokeBitmap->width * j];

                pixels[idx + 1] = strokeVal < pixels[idx] ? pixels[idx] : strokeVal;
            }
        }

        return true;
    }

    unsigned int generateTexture(int w, int h, const std::vector<unsigned char>& buffer) {
        unsigned int tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_COMPRESSED_RG,
            w,
            h,
            0,
            GL_RG,
            GL_UNSIGNED_BYTE,
            buffer.data()
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        return tex;
    }

    std::optional<sgct::text::Font::FontFaceData>
    createGlyph(FT_Library library, FT_Face face, FT_Fixed strokeSize, wchar_t c)
    {
        // Load the Glyph for our character.
        // Hints:
        // www.freetype.org/freetype2/docs/reference/ft2-base_interface.html#FT_LOAD_XXX

        FT_UInt charIndex = FT_Get_Char_Index(face, static_cast<FT_ULong>(c));
        if (charIndex == 0) {
            return std::nullopt;
        }

        FT_Error loadError = FT_Load_Glyph(face, charIndex, FT_LOAD_FORCE_AUTOHINT);
        if (loadError) {
            return std::nullopt;
        }

        // load pixel data
        int width;
        int height;
        std::vector<unsigned char> pixels;
        GlyphData gd;
        bool success = getPixelData(library, face, strokeSize, width, height, pixels, gd);
        if (!success) {
            return std::nullopt;
        }

        sgct::text::Font::FontFaceData ffd;
        // create texture
        if (charIndex > 0) {
            ffd.texId = generateTexture(width, height, pixels);
        }
        else {
            ffd.texId = 0;
        }

        // setup geometry data
        ffd.pos.x = static_cast<float>(gd.bitmapGlyph->left);
        // abock (2010-10-19) Don't remove this variable;  if the expression is directly
        // inserted in the static_cast, something goes wrong when rows > top and things
        // get wrongly converted into unsigned integer type before the cast
        const int y = gd.bitmapGlyph->top - gd.bitmap->rows;
        ffd.pos.y = static_cast<float>(y);
        ffd.size.x = static_cast<float>(width);
        ffd.size.y = static_cast<float>(height);

        // delete the stroke glyph
        FT_Stroker_Done(gd.stroker);
        FT_Done_Glyph(gd.strokeGlyph);
        
        // Can't delete them while they are used, delete when font is cleaned
        ffd.glyph = gd.glyph;
        ffd.distToNextChar = static_cast<float>(face->glyph->advance.x / 64);

        return ffd;
    }
} // namespace

namespace sgct::text {

Font::Font(FT_Library lib, FT_Face face, unsigned int height)
    : _library(lib)
    , _face(face)
    , _height(static_cast<float>(height))
{
    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);

    std::array<float, 16> c = {
        // x y s t
        0.f, 1.f, 0.f, 0.f,
        1.f, 1.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f,
        1.f, 0.f, 1.f, 1.f
    };

    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, c.size() * sizeof(float), c.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        4 * sizeof(float),
        reinterpret_cast<void*>(8)
    );

    glBindVertexArray(0);
}

Font::~Font() {
    glDeleteVertexArrays(1, &_vao);
    glDeleteBuffers(1, &_vbo);
    for (const std::pair<const wchar_t, FontFaceData>& n : _fontFaceData) {
        glDeleteTextures(1, &(n.second.texId));
        FT_Done_Glyph(n.second.glyph);
    }

    // This needs to be cleared before the face is destroyed
    _fontFaceData.clear();
    FT_Done_Face(_face);
}

void Font::setStrokeSize(int size) {
    _strokeSize = size;
}

const Font::FontFaceData& Font::getFontFaceData(wchar_t c) {
    if (_fontFaceData.count(c) == 0) {
        // check if c does not exist in map
        createCharacter(c);
    }
    return _fontFaceData[c];
}

unsigned int Font::getVAO() const {
    return _vao;
}

float Font::getHeight() const {
    return _height;
}

void Font::createCharacter(wchar_t c) {
    std::optional<FontFaceData> ffd = createGlyph(_library, _face, _strokeSize, c);
    if (ffd) {
        _fontFaceData[c] = std::move(*ffd);
    }
    else {
        Logger::Error("Error creating character %s", c);
    }
}

} // namespace sgct::text

#endif // SGCT_HAS_TEXT
