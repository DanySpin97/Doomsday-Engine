/**\file image.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_IMAGE_H
#define LIBDENG_IMAGE_H

/**
 * @defgroup imageConversionFlags Image Conversion Flags.
 */
/*@{*/
#define ICF_UPSCALE_SAMPLE_WRAPH    (0x1)
#define ICF_UPSCALE_SAMPLE_WRAPV    (0x2)
#define ICF_UPSCALE_SAMPLE_WRAP     (ICF_UPSCALE_SAMPLE_WRAPH|ICF_UPSCALE_SAMPLE_WRAPV)
/*@}*/

/**
 * @defgroup imageFlags Image Flags
 */
/*@{*/
#define IMGF_IS_MASKED              (0x1)
/*@}*/

typedef struct image_s {
    /// @see imageFlags
    int flags;

    /// Indentifier of the color palette used/assumed or @c 0 if none (1-based).
    colorpaletteid_t paletteId;

    /// Dimensions of the image in pixels.
    int width;
    int height;

    /// Bytes per pixel in the data buffer.
    int pixelSize;

    /// Pixel color/palette (+alpha) data buffer.
    uint8_t* pixels;
} image_t;

void GL_InitImage(image_t* image);

void GL_PrintImageMetadata(const image_t* image);

/**
 * Loads PCX, TGA and PNG images. The returned buffer must be freed
 * with M_Free. Color keying is done if "-ck." is found in the filename.
 * The allocated memory buffer always has enough space for 4-component
 * colors.
 */
uint8_t* GL_LoadImage(image_t* image, const char* filePath);
uint8_t* GL_LoadImageStr(image_t* image, const ddstring_t* filePath);

/// Release image pixel data.
void GL_DestroyImagePixels(image_t* image);

/// @return  @c true if the image pixel data contains alpha information.
boolean GL_ImageHasAlpha(const image_t* image);

/**
 * Converts the image by converting it to a luminance map and then moving
 * the resultant luminance data into the alpha channel. The color channel(s)
 * are then filled all-white.
 */
void GL_ConvertToAlpha(image_t* image, boolean makeWhite);

/**
 * Converts the image data to grayscale luminance in-place.
 */
void GL_ConvertToLuminance(image_t* image, boolean retainAlpha);

#endif /* LIBDENG_IMAGE_H */
