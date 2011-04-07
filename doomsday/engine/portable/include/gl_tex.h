/**\file gl_tex.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Image manipulation and evaluation algorithms.
 */

#ifndef LIBDENG_IMAGE_MANIPULATION_H
#define LIBDENG_IMAGE_MANIPULATION_H

typedef struct pointlight_analysis_s {
    float originX, originY, brightMul;
    float color[3];
} pointlight_analysis_t;

typedef struct ambientlight_analysis_s {
    float color[3]; // Average color.
    float colorAmplified[3]; // Average color amplified.
} ambientlight_analysis_t;

typedef struct averagecolor_analysis_s {
    float color[3];
} averagecolor_analysis_t;

/**
 * @param pixels  Luminance image to be enhanced.
 * @param width  Logical width of the image in pixels.
 * @param height  Logical height of the image in pixels.
 * @param hasAlpha  @c true == @a pixels is luminance plus alpha data.
 */
void AmplifyLuma(uint8_t* pixels, int width, int height, boolean hasAlpha);

/**
 * Take the input buffer and convert to color keyed. A new buffer may be
 * needed if the input buffer has three color components.
 * Color keying is done for both (0,255,255) and (255,0,255).
 *
 * @return  If the in buffer wasn't large enough will return a ptr to the
 *      newly allocated buffer which must be freed with free(), else @a buf.
 */
uint8_t* ApplyColorKeying(uint8_t* pixels, int width, int height,
    int pixelSize);

/**
 * @param pixels  RGBA data. Input read here, and output written here.
 * @param width  Logical width of the image in pixels.
 * @param height  Logical height of the image in pixels.
 */
#if 0 // dj: Doesn't make sense, "darkness" applied to an alpha channel?
void BlackOutlines(uint8_t* pixels, int width, int height);
#endif

/**
 * Spread the color of none masked pixels outwards into the masked area.
 * This addresses the "black outlines" produced by texture filtering due to
 * sampling the default (black) color.
 */
void ColorOutlinesIdx(uint8_t* pixels, int width, int height);

/**
 * @param pixels  RGB(a) image to be enhanced.
 * @param width  Logical width of the image in pixels.
 * @param height  Logical height of the image in pixels.
 * @param pixelsize  Size of each pixel. Handles 3 and 4.
 */
void Desaturate(uint8_t* pixels, int width, int height, int pixelSize);

/**
 * \important Does not conform to any standard technique and adjustments
 * are applied symmetrically for all color components.
 *
 * @param pixels  RGB(a) image to be enhanced.
 * @param width  Logical width of the image in pixels.
 * @param height  Logical height of the image in pixels.
 * @param pixelsize  Size of each pixel. Handles 3 and 4.
 */
void EnhanceContrast(uint8_t* pixels, int width, int height, int pixelSize);

/**
 * Equalize the specified luminance map such that the minimum and maximum
 * brightness covers the whole [0...255] range.
 *
 * \algorithm Calculates shift deltas for bright and dark-side pixels by
 * averaging the luminosity of all pixels in the original image.
 *
 * @param pixels  Luminance image to equalize.
 * @param width  Logical width of the image in pixels.
 * @param height  Logical height of the image in pixels.
 */
void EqualizeLuma(uint8_t* pixels, int width, int height, float* rBaMul,
    float* rHiMul, float* rLoMul);

/**
 * @param pixels  RGB(a) image to evaluate.
 * @param width  Logical width of the image in pixels.
 * @param height  Logical height of the image in pixels.
 * @param color  Determined average color written here.
 */
void FindAverageColor(const uint8_t* pixels, int width, int height,
    int pixelSize, float color[3]);

/**
 * @param pixels  Index-color image to evaluate.
 * @param width  Logical width of the image in pixels.
 * @param height  Logical height of the image in pixels.
 * @param paletteIdx  Index of the color palette to use else @c 0
 * @param hasAlpha  @c true == @a pixels includes alpha data.
 * @param color  Determined average color written here.
 */
void FindAverageColorIdx(const uint8_t* pixels, int width, int height,
    int paletteIdx, boolean hasAlpha, float color[3]);

/**
 * @param pixels  RGB(a) image to evaluate.
 * @param width  Logical width of the image in pixels.
 * @param height  Logical height of the image in pixels.
 * @param color  Determined average color written here.
 */
void FindAverageLineColor(const uint8_t* pixels, int width, int height,
    int pixelSize, int line, float color[3]);

/**
 * @param pixels  Index-color image to evaluate.
 * @param width  Logical width of the image in pixels.
 * @param height  Logical height of the image in pixels.
 * @param paletteIdx  Index of the color palette to use else @c 0
 * @param hasAlpha  @c true == @a pixels includes alpha data.
 * @param color  Determined average color written here.
 */
void FindAverageLineColorIdx(const uint8_t* pixels, int width, int height,
    int line, int paletteIdx, boolean hasAlpha, float color[3]);

/**
 * Calculates a clip region for the image that excludes alpha pixels.
 * \algorithm: Cross spread from bottom > top, right > left (inside out).
 *
 * @param pixels  Image data to be processed.
 * @param width  Logical width of the image in pixels.
 * @param height  Logical height of the image in pixels.
 * @param pixelsize  Size of each pixel. Handles 1 (==2), 3 and 4.
 * @param region  Determined region written here.
 */
void FindClipRegionNonAlpha(const uint8_t* pixels, int width, int height,
    int pixelSize, int region[4]);

/**
 * @param pixels  RGB(a) image to be enhanced.
 * @param width  Logical width of the image in pixels.
 * @param height  Logical height of the image in pixels.
 * @param pixelsize  Size of each pixel. Handles 3 and 4.
 */
void SharpenPixels(uint8_t* pixels, int width, int height, int pixelSize);

uint8_t* GL_ScaleBuffer(const uint8_t* pixels, int width, int height,
    int pixelSize, int outWidth, int outHeight);

void* GL_ScaleBufferEx(const void* datain, int width, int height, int pixelSize,
    /*GLint typein,*/ int rowLength, int alignment, int skiprows, int skipPixels,
    int outWidth, int outHeight, /*GLint typeout,*/ int outRowLength, int outAlignment,
    int outSkipRows, int outSkipPixels);

uint8_t* GL_ScaleBufferNearest(const uint8_t* pixels, int width, int height,
    int pixelSize, int outWidth, int outHeight);

/**
 * Works within the given data, reducing the size of the picture to half
 * its original.
 *
 * @param width  Width of the final texture, must be power of two.
 * @param height  Height of the final texture, must be power of two.
 */
void GL_DownMipmap32(uint8_t* pixels, int width, int height, int pixelSize);

/**
 * Works within the given data, reducing the size of the picture to half
 * its original.
 *
 * @param width  Width of the final texture, must be power of two.
 * @param height  Height of the final texture, must be power of two.
 */
void GL_DownMipmap8(uint8_t* in, uint8_t* fadedOut, int width, int height,
    float fade);

boolean GL_PalettizeImage(uint8_t* out, int outformat, int paletteIdx,
    boolean gammaCorrect, const uint8_t* in, int informat, int width, int height);

boolean GL_QuantizeImageToPalette(uint8_t* out, int outformat, int paletteIdx,
    const uint8_t* in, int informat, int width, int height);

/**
 * Desaturates the texture in the dest buffer by averaging the colour then
 * looking up the nearest match in the palette. Increases the brightness
 * to maximum.
 */
void GL_DeSaturatePalettedImage(uint8_t* buffer, int paletteIdx, int width, int height);

#endif /* LIBDENG_IMAGE_MANIPULATION_H */
