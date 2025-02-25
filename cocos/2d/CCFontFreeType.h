/****************************************************************************
 Copyright (c) 2013      Zynga Inc.
 Copyright (c) 2013-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
 Copyright (c) 2021 Bytedance Inc.

 https://adxe.org

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#ifndef _FontFreetype_h_
#define _FontFreetype_h_

/// @cond DO_NOT_SHOW

#include "2d/CCFont.h"

#include "ft2build.h"
#include <string>

#include FT_FREETYPE_H
#include FT_STROKER_H

NS_CC_BEGIN

class CC_DLL FontFreeType : public Font
{
public:
    static const int DistanceMapSpread;

    static FontFreeType* create(const std::string& fontName,
                                float fontSize,
                                GlyphCollection glyphs,
                                const char* customGlyphs,
                                bool distanceFieldEnabled = false,
                                float outline             = 0);

    static void shutdownFreeType();

    /*
     * @remark: if you want enable stream parsing, you need do one of follow steps
     *          a. disable .ttf compress on .apk, see:
     *             https://simdsoft.com/notes/#build-apk-config-nocompress-file-type-at-appbuildgradle
     *          b. uncomporess .ttf to disk by yourself.
     */
    static void setStreamParsingEnabled(bool bEnabled) { _streamParsingEnabled = bEnabled; }
    static bool isStreamParsingEnabled() { return _streamParsingEnabled; }

    /*
    **TrueType fonts with native bytecode hinting**
    *
    *   All applications that handle TrueType fonts with native hinting must
    *   be aware that TTFs expect different rounding of vertical font
    *   dimensions.  The application has to cater for this, especially if it
    *   wants to rely on a TTF's vertical data (for example, to properly align
    *   box characters vertically).
    *   - Since freetype-2.8.1 TureType matrics isn't sync to size_matrics
    *   - By default it's enabled for compatible with cocos2d-x-4.0 or older with freetype-2.5.5
    *   - Please see freetype.h
    * */
    static void setNativeBytecodeHintingEnabled(bool bEnabled) { _doNativeBytecodeHinting = bEnabled; }
    static bool isNativeBytecodeHintingEnabled() { return _doNativeBytecodeHinting; }

    bool isDistanceFieldEnabled() const { return _distanceFieldEnabled; }

    float getOutlineSize() const { return _outlineSize; }

    void renderCharAt(unsigned char* dest,
                      int posX,
                      int posY,
                      unsigned char* bitmap,
                      int32_t bitmapWidth,
                      int32_t bitmapHeight);

    FT_Encoding getEncoding() const { return _encoding; }

    int* getHorizontalKerningForTextUTF32(const std::u32string& text, int& outNumLetters) const override;

    unsigned char* getGlyphBitmap(uint64_t theChar, int32_t& outWidth, int32_t& outHeight, Rect& outRect, int& xAdvance);

    int getFontAscender() const;
    const char* getFontFamily() const;
    std::string getFontName() const { return _fontName; }

    virtual FontAtlas* createFontAtlas() override;
    virtual int getFontMaxHeight() const override { return _lineHeight; }

    static void releaseFont(const std::string& fontName);

private:
    static const char* _glyphASCII;
    static const char* _glyphNEHE;
    static FT_Library _FTlibrary;
    static bool _FTInitialized;
    static bool _streamParsingEnabled;
    static bool _doNativeBytecodeHinting;

    FontFreeType(bool distanceFieldEnabled = false, float outline = 0);
    virtual ~FontFreeType();

    bool createFontObject(const std::string& fontName, float fontSize);

    bool initFreeType();
    FT_Library getFTLibrary();

    int getHorizontalKerningForChars(uint64_t firstChar, uint64_t secondChar) const;
    unsigned char* getGlyphBitmapWithOutline(uint64_t code, FT_BBox& bbox);

    void setGlyphCollection(GlyphCollection glyphs, const char* customGlyphs = nullptr);
    const char* getGlyphCollection() const;

    FT_Face _fontRef;
    std::unique_ptr<FT_StreamRec> _fontStream;
    FT_Stroker _stroker;
    FT_Encoding _encoding;

    std::string _fontName;
    bool _distanceFieldEnabled;
    float _outlineSize;
    int _ascender;
    int _descender;
    int _lineHeight;
    FontAtlas* _fontAtlas;

    GlyphCollection _usedGlyphs;
    std::string _customGlyphs;
};

/// @endcond

NS_CC_END

#endif
