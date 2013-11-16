/** @file colorpalettes.cpp  Color palette resource collection
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_base.h"
#include "de_console.h"
#include "render/r_main.h"
#ifdef __CLIENT__
#  include "gl/gl_texmanager.h"
#endif
#include <de/memory.h>
#include <de/memoryzone.h>

#include "resource/colorpalettes.h"

using namespace de;

byte *translationTables;

#define COLORPALETTENAME_MAXLEN     (32)

/**
 * Color palette name binding. Mainly intended as a public API convenience.
 * Internally we are only interested in the associated palette indices.
 */
struct ColorPaletteBind
{
    char name[COLORPALETTENAME_MAXLEN+1];
    uint idx;
};

colorpaletteid_t defaultColorPalette;

static boolean initedColorPalettes = false;

static int numColorPalettes;
static ColorPalette **colorPalettes;
static int numColorPaletteBinds;
static ColorPaletteBind *colorPaletteBinds;

static colorpaletteid_t colorPaletteNumForName(char const *name)
{
    DENG2_ASSERT(initedColorPalettes);

    // Linear search (sufficiently fast enough given the probably small set
    // and infrequency of searches).
    for(int i = 0; i < numColorPaletteBinds; ++i)
    {
        ColorPaletteBind *pal = &colorPaletteBinds[i];
        if(!strnicmp(pal->name, name, COLORPALETTENAME_MAXLEN))
        {
            return i + 1; // Already registered. 1-based index.
        }
    }

    return 0; // Not found.
}

static int createColorPalette(int const compOrder[3], uint8_t const compSize[3],
    uint8_t const *data, ushort num)
{
    DENG2_ASSERT(initedColorPalettes && compOrder && compSize && data);

    ColorPalette *pal = new ColorPalette(compOrder, compSize, data, num);

    colorPalettes = (ColorPalette **) M_Realloc(colorPalettes, (numColorPalettes + 1) * sizeof(*colorPalettes));
    colorPalettes[numColorPalettes] = pal;

    return ++numColorPalettes; // 1-based index.
}

void R_InitTranslationTables()
{
    // The translation tables consist of a number of translation maps, each
    // containing 256 palette indices.
    translationTables = (byte *) Z_Calloc(NUM_TRANSLATION_TABLES * 256, PU_REFRESHTRANS, 0);
}

void R_UpdateTranslationTables()
{
    Z_FreeTags(PU_REFRESHTRANS, PU_REFRESHTRANS);
    R_InitTranslationTables();
}

byte const *R_TranslationTable(int tclass, int tmap)
{
    // Is translation unnecessary?
    if(!tclass && !tmap) return 0;

    int trans = de::max(0, NUM_TRANSLATION_MAPS_PER_CLASS * tclass + tmap - 1);
    LOG_DEBUG("tclass=%i tmap=%i => TransPal# %i") << tclass << tmap << trans;

    return translationTables + trans * 256;
}

void R_InitColorPalettes()
{
    if(initedColorPalettes)
    {
        // Re-init.
        R_DestroyColorPalettes();
        return;
    }

    colorPalettes = 0;
    numColorPalettes = 0;

    colorPaletteBinds = 0;
    numColorPaletteBinds = 0;
    defaultColorPalette = 0;

    initedColorPalettes = true;
}

void R_DestroyColorPalettes()
{
    if(!initedColorPalettes) return;

    if(0 != numColorPalettes)
    {
        for(int i = 0; i < numColorPalettes; ++i)
        {
            delete(colorPalettes[i]);
        }
        M_Free(colorPalettes); colorPalettes = 0;
        numColorPalettes = 0;
    }

    if(colorPaletteBinds)
    {
        M_Free(colorPaletteBinds); colorPaletteBinds = 0;
        numColorPaletteBinds = 0;
    }

    defaultColorPalette = 0;
}

int R_ColorPaletteCount()
{
    DENG2_ASSERT(initedColorPalettes);
    return numColorPalettes;
}

ColorPalette *R_ToColorPalette(colorpaletteid_t id)
{
    DENG2_ASSERT(initedColorPalettes);
    if(id == 0 || id - 1 >= (unsigned) numColorPaletteBinds)
    {
        id = defaultColorPalette;
    }

    if(id != 0 && numColorPaletteBinds > 0)
    {
        return colorPalettes[colorPaletteBinds[id-1].idx-1];
    }
    return 0;
}

ColorPalette *R_GetColorPaletteByIndex(int paletteIdx)
{
    DENG2_ASSERT(initedColorPalettes);
    if(paletteIdx > 0 && numColorPalettes >= paletteIdx)
    {
        return colorPalettes[paletteIdx-1];
    }
    Con_Error("R_GetColorPaletteByIndex: Failed to locate palette for index #%i", paletteIdx);
    exit(1); // Unreachable.
}

boolean R_SetDefaultColorPalette(colorpaletteid_t id)
{
    DENG2_ASSERT(initedColorPalettes);
    if(id - 1 < (unsigned) numColorPaletteBinds)
    {
        defaultColorPalette = id;
        return true;
    }
    LOG_VERBOSE("R_SetDefaultColorPalette: Invalid id %u.") << id;
    return false;
}

#undef R_CreateColorPalette
DENG_EXTERN_C colorpaletteid_t R_CreateColorPalette(char const *fmt, char const *name,
    uint8_t const *colorData, int colorCount)
{
    static char const *compNames[] = { "red", "green", "blue" };
    colorpaletteid_t id;
    ColorPaletteBind *bind;
    char const *c, *end;
    int i, pos, compOrder[3];
    uint8_t compSize[3];

    if(!initedColorPalettes)
        Con_Error("R_CreateColorPalette: Color palettes not yet initialized.");

    if(!name || !name[0])
        Con_Error("R_CreateColorPalette: Color palettes require a name.");

    if(strlen(name) > COLORPALETTENAME_MAXLEN)
        Con_Error("R_CreateColorPalette: Failed creating \"%s\", color palette name can be at most %i characters long.", name, COLORPALETTENAME_MAXLEN);

    if(!fmt || !fmt[0])
        Con_Error("R_CreateColorPalette: Failed creating \"%s\", missing format string.", name);

    if(!colorData || colorCount <= 0)
        Con_Error("R_CreateColorPalette: Failed creating \"%s\", cannot create a zero-sized palette.", name);

    // All arguments supplied. Parse the format string.
    std::memset(compOrder, -1, sizeof(compOrder));
    pos = 0;
    end = fmt + (strlen(fmt) - 1);
    c = fmt;
    do
    {
        int comp = -1;
        if     (*c == 'R' || *c == 'r') comp = CR;
        else if(*c == 'G' || *c == 'g') comp = CG;
        else if(*c == 'B' || *c == 'b') comp = CB;

        if(comp != -1)
        {
            // Have we encountered this component yet?
            if(compOrder[comp] == -1)
            {
                // No.
                char const *start;
                size_t numDigits;

                compOrder[comp] = pos++;

                // Read the number of bits.
                start = ++c;
                while((*c >= '0' && *c <= '9') && ++c < end)
                {}

                numDigits = c - start;
                if(numDigits != 0 && numDigits <= 2)
                {
                    char buf[3];

                    std::memset(buf, 0, sizeof(buf));
                    std::memcpy(buf, start, numDigits);

                    compSize[comp] = atoi(buf);

                    // Are we done?
                    if(pos == 3) break;

                    // Unread the last character.
                    c--;
                    continue;
                }
            }
        }

        Con_Error("R_CreateColorPalette: Failed creating \"%s\", invalid character '%c' in format string at position %u.\n", name, *c, (unsigned int) (c - fmt));
    } while(++c <= end);

    if(pos != 3)
    {
        Con_Error("R_CreateColorPalette: Failed creating \"%s\", incomplete format specification.\n", name);
    }

    // Check validity of bits per component.
    for(i = 0; i < 3; ++i)
    {
        if(compSize[i] == 0 || compSize[i] > ColorPalette::max_component_bits)
        {
            Con_Error("R_CreateColorPalette: Failed creating \"%s\", unsupported bit depth %i for %s component.\n", name, compSize[i], compNames[compOrder[i]]);
        }
    }

    if(0 != (id = colorPaletteNumForName(name)))
    {
        // Replacing an existing palette.
        bind = &colorPaletteBinds[id - 1];
        R_GetColorPaletteByIndex(bind->idx)->replaceColorTable(compOrder, compSize, colorData, colorCount);

#ifdef __CLIENT__
        GL_ReleaseTexturesByColorPalette(id);
#endif
    }
    else
    {
        // A new palette.
        colorPaletteBinds = (ColorPaletteBind *) M_Realloc(colorPaletteBinds, (numColorPaletteBinds + 1) * sizeof(ColorPaletteBind));

        bind = &colorPaletteBinds[numColorPaletteBinds];
        std::memset(bind, 0, sizeof(*bind));

        strncpy(bind->name, name, COLORPALETTENAME_MAXLEN);

        id = (colorpaletteid_t) ++numColorPaletteBinds; // 1-based index.
        if(numColorPaletteBinds == 1)
        {
            defaultColorPalette = colorpaletteid_t(numColorPaletteBinds);
        }
    }

    // Generate the color palette.
    bind->idx = createColorPalette(compOrder, compSize, colorData, colorCount);

    return id; // 1-based index.
}

#undef R_GetColorPaletteNumForName
DENG_EXTERN_C colorpaletteid_t R_GetColorPaletteNumForName(char const *name)
{
    if(!initedColorPalettes)
    {
        Con_Error("R_GetColorPaletteNumForName: Color palettes not yet initialized.");
    }

    if(name && name[0] && qstrlen(name) <= COLORPALETTENAME_MAXLEN)
    {
        return colorPaletteNumForName(name);
    }

    return 0;
}

#undef R_GetColorPaletteNameForNum
DENG_EXTERN_C char const *R_GetColorPaletteNameForNum(colorpaletteid_t id)
{
    if(!initedColorPalettes)
    {
        Con_Error("R_GetColorPaletteNameForNum: Color palettes not yet initialized.");
    }

    if(id != 0 && id - 1 < (unsigned)numColorPaletteBinds)
    {
        return colorPaletteBinds[id-1].name;
    }

    return 0;
}

#undef R_GetColorPaletteRGBubv
DENG_EXTERN_C void R_GetColorPaletteRGBubv(colorpaletteid_t paletteId, int colorIdx, uint8_t rgb[3],
    boolean applyTexGamma)
{
    if(!initedColorPalettes)
    {
        Con_Error("R_GetColorPaletteRGBubv: Color palettes not yet initialized.");
    }
    if(!rgb)
    {
        Con_Error("R_GetColorPaletteRGBubv: Invalid arguments (rgb==NULL).");
    }

    if(colorIdx < 0)
    {
        rgb[CR] = rgb[CG] = rgb[CB] = 0;
        return;
    }

    if(ColorPalette *palette = R_ToColorPalette(paletteId))
    {
        Vector3ub palColor = palette->color(colorIdx);
        rgb[CR] = palColor.x;
        rgb[CG] = palColor.y;
        rgb[CB] = palColor.z;
        if(applyTexGamma)
        {
            rgb[CR] = texGammaLut[rgb[CR]];
            rgb[CG] = texGammaLut[rgb[CG]];
            rgb[CB] = texGammaLut[rgb[CB]];
        }
        return;
    }

    LOG_WARNING("R_GetColorPaletteRGBubv: Failed to locate ColorPalette for id %i.") << paletteId;
}

#undef R_GetColorPaletteRGBf
DENG_EXTERN_C void R_GetColorPaletteRGBf(colorpaletteid_t paletteId, int colorIdx, float rgb[3],
    boolean applyTexGamma)
{
    if(!initedColorPalettes)
    {
        Con_Error("R_GetColorPaletteRGBf: Color palettes not yet initialized.");
    }
    if(!rgb)
    {
        Con_Error("R_GetColorPaletteRGBf: Invalid arguments (rgb==NULL).");
    }

    if(colorIdx < 0)
    {
        rgb[CR] = rgb[CG] = rgb[CB] = 0;
        return;
    }

    if(ColorPalette *palette = R_ToColorPalette(paletteId))
    {
        if(applyTexGamma)
        {
            Vector3ub palColor = palette->color(colorIdx);
            rgb[CR] = texGammaLut[palColor.x] * reciprocal255;
            rgb[CG] = texGammaLut[palColor.y] * reciprocal255;
            rgb[CB] = texGammaLut[palColor.z] * reciprocal255;
        }
        else
        {
            Vector3f palColor = palette->colorf(colorIdx);
            rgb[CR] = palColor.x;
            rgb[CG] = palColor.y;
            rgb[CB] = palColor.z;
        }
        return;
    }

    LOG_WARNING("R_GetColorPaletteRGBf: Failed to locate ColorPalette for id %i.") << paletteId;
}
