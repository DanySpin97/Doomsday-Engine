/**\file image.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_GL_TEXTUREVARIANTSPECIFICATION_H
#define LIBDENG_GL_TEXTUREVARIANTSPECIFICATION_H

typedef enum {
    TC_UNKNOWN = -1,
    TEXTUREVARIANTUSAGECONTEXT_FIRST = 0,
    TC_UI = TEXTUREVARIANTUSAGECONTEXT_FIRST,
    TC_MAPSURFACE_DIFFUSE,
    TC_MAPSURFACE_REFLECTION,
    TC_MAPSURFACE_REFLECTIONMASK,
    TC_MAPSURFACE_LIGHTMAP,
    TC_SPRITE_DIFFUSE,
    TC_MODELSKIN_DIFFUSE,
    TC_MODELSKIN_REFLECTION,
    TC_HALO_LUMINANCE,
    TC_PSPRITE_DIFFUSE,
    TC_SKYSPHERE_DIFFUSE,
    TEXTUREVARIANTUSAGECONTEXT_LAST = TC_SKYSPHERE_DIFFUSE
} texturevariantusagecontext_t;

#define TEXTUREVARIANTUSAGECONTEXT_COUNT (\
    TEXTUREVARIANTUSAGECONTEXT_LAST + 1 - TEXTUREVARIANTUSAGECONTEXT_FIRST )

#define VALID_TEXTUREVARIANTUSAGECONTEXT(tc) (\
    (tc) >= TEXTUREVARIANTUSAGECONTEXT_FIRST && (tc) <= TEXTUREVARIANTUSAGECONTEXT_LAST)

/**
 * @defGroup textureVariantSpecificationFlags  Texture Variant Specification Flags
 */
/*@{*/
#define TSF_ZEROMASK                0x1 // Set pixel alpha to fully opaque.
#define TSF_NO_COMPRESSION          0x2
#define TSF_UPSCALE_AND_SHARPEN     0x4
#define TSF_MONOCHROME              0x8

#define TSF_INTERNAL_MASK           0xff000000
#define TSF_HAS_COLORPALETTE_XLAT   0x80000000
/*@}*/

typedef enum {
    TEXTUREVARIANTSPECIFICATIONTYPE_FIRST = 0,
    TST_GENERAL = TEXTUREVARIANTSPECIFICATIONTYPE_FIRST,
    TST_DETAIL,
    TEXTUREVARIANTSPECIFICATIONTYPE_LAST = TST_DETAIL
} texturevariantspecificationtype_t;

#define TEXTUREVARIANTSPECIFICATIONTYPE_COUNT (\
    TEXTUREVARIANTSPECIFICATIONTYPE_LAST + 1 - TEXTUREVARIANTSPECIFICATIONTYPE_FIRST )

#define VALID_TEXTUREVARIANTSPECIFICATIONTYPE(t) (\
    (t) >= TEXTUREVARIANTSPECIFICATIONTYPE_FIRST && (t) <= TEXTUREVARIANTSPECIFICATIONTYPE_LAST)

typedef struct {
    int tClass, tMap;
} colorpalettetranslationspecification_t;

typedef struct {
    texturevariantusagecontext_t context;
    int flags; /// @see textureVariantSpecificationFlags
    byte border; /// In pixels, added to all four edges of the texture.
    int wrapS, wrapT;
    boolean mipmapped, gammaCorrection, noStretch, toAlpha;
    int anisoFilter;
    colorpalettetranslationspecification_t* translated;
} variantspecification_t;

/**
 * Detail textures are faded to gray depending on the contrast
 * factor. The texture is also progressively faded towards gray
 * when each mipmap level is loaded.
 *
 * Contrast is quantized in order to reduce the number of variants.
 * to a more sensible/manageable number per texture.
 */
#define DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR  (10)

typedef struct {
    uint8_t contrast;
} detailvariantspecification_t;

#define TS_GENERAL(ts)      (&(ts)->data.variant)
#define TS_DETAIL(ts)       (&(ts)->data.detailvariant)

typedef struct texturevariantspecification_s {
    texturevariantspecificationtype_t type;
    union {
        variantspecification_t variant;
        detailvariantspecification_t detailvariant;
    } data; // type-specific data.
} texturevariantspecification_t;

#endif /* LIBDENG_GL_TEXTUREVARIANTSPECIFICATION_H */
