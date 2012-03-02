/**\file p_surface.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_play.h"

#include "materialvariant.h"

boolean Surface_IsDrawable(surface_t* surface)
{
    return surface->material && Material_IsDrawable(surface->material);
}

boolean Surface_AttachedToMap(surface_t* suf)
{
    assert(suf);
    if(!suf->owner) return false;
    if(DMU_GetType(suf->owner) == DMU_PLANE)
    {
        sector_t* sec = ((plane_t*)suf->owner)->sector;
        if(0 == sec->subsectorCount)
            return false;
    }
    return true;
}

boolean Surface_SetMaterial(surface_t* suf, material_t* mat)
{
    assert(suf);
    if(suf->material != mat)
    {
        if(Surface_AttachedToMap(suf))
        {
            // No longer a missing texture fix?
            if(mat && (suf->oldFlags & SUIF_FIX_MISSING_MATERIAL))
                suf->inFlags &= ~SUIF_FIX_MISSING_MATERIAL;

            if(!ddMapSetup)
            {
                // If this plane's surface is in the decorated list, remove it.
                R_SurfaceListRemove(decoratedSurfaceList, suf);
                // If this plane's surface is in the glowing list, remove it.
                R_SurfaceListRemove(glowingSurfaceList, suf);

                if(mat)
                {
                    if(Material_HasGlow(mat))
                        R_SurfaceListAdd(glowingSurfaceList, suf);
                    if(Materials_HasDecorations(mat))
                        R_SurfaceListAdd(decoratedSurfaceList, suf);

                    if(DMU_GetType(suf->owner) == DMU_PLANE)
                    {
                        const ded_ptcgen_t* def = Materials_PtcGenDef(mat);
                        P_SpawnPlaneParticleGen(def, (plane_t*)suf->owner);
                    }
                }
            }
        }

        suf->material = mat;
        suf->oldFlags = suf->inFlags;
        if(Surface_AttachedToMap(suf))
            suf->inFlags |= SUIF_UPDATE_DECORATIONS;
    }
    return true;
}

boolean Surface_SetMaterialOriginX(surface_t* suf, float x)
{
    assert(suf);
    if(suf->offset[VX] != x)
    {
        suf->offset[VX] = x;
        if(Surface_AttachedToMap(suf))
        {
            suf->inFlags |= SUIF_UPDATE_DECORATIONS;
            if(!ddMapSetup)
                R_SurfaceListAdd(movingSurfaceList, suf);
        }
    }
    return true;
}

boolean Surface_SetMaterialOriginY(surface_t* suf, float y)
{
    assert(suf);
    if(suf->offset[VY] != y)
    {
        suf->offset[VY] = y;
        if(Surface_AttachedToMap(suf))
        {
            suf->inFlags |= SUIF_UPDATE_DECORATIONS;
            if(!ddMapSetup)
                R_SurfaceListAdd(movingSurfaceList, suf);
        }
    }
    return true;
}

boolean Surface_SetMaterialOrigin(surface_t* suf, float x, float y)
{
    assert(suf);
    if(suf->offset[VX] != x || suf->offset[VY] != y)
    {
        suf->offset[VX] = x;
        suf->offset[VY] = y;
        if(Surface_AttachedToMap(suf))
        {
            suf->inFlags |= SUIF_UPDATE_DECORATIONS;
            if(!ddMapSetup)
                R_SurfaceListAdd(movingSurfaceList, suf);
        }
    }
    return true;
}

boolean Surface_SetColorRed(surface_t* suf, float r)
{
    assert(suf);
    r = MINMAX_OF(0, r, 1);
    if(suf->rgba[CR] != r)
    {
        // \todo when surface colours are intergrated with the
        // bias lighting model we will need to recalculate the
        // vertex colours when they are changed.
        suf->rgba[CR] = r;
    }
    return true;
}

boolean Surface_SetColorGreen(surface_t* suf, float g)
{
    assert(suf);
    g = MINMAX_OF(0, g, 1);
    if(suf->rgba[CG] != g)
    {
        // \todo when surface colours are intergrated with the
        // bias lighting model we will need to recalculate the
        // vertex colours when they are changed.
        suf->rgba[CG] = g;
    }
    return true;
}

boolean Surface_SetColorBlue(surface_t* suf, float b)
{
    assert(suf);
    b = MINMAX_OF(0, b, 1);
    if(suf->rgba[CB] != b)
    {
        // \todo when surface colours are intergrated with the
        // bias lighting model we will need to recalculate the
        // vertex colours when they are changed.
        suf->rgba[CB] = b;
    }
    return true;
}

boolean Surface_SetAlpha(surface_t* suf, float a)
{
    assert(suf);
    a = MINMAX_OF(0, a, 1);
    if(suf->rgba[CA] != a)
    {
        // \todo when surface colours are intergrated with the
        // bias lighting model we will need to recalculate the
        // vertex colours when they are changed.
        suf->rgba[CA] = a;
    }
    return true;
}

boolean Surface_SetColorAndAlpha(surface_t* suf, float r, float g, float b, float a)
{
    assert(suf);

    r = MINMAX_OF(0, r, 1);
    g = MINMAX_OF(0, g, 1);
    b = MINMAX_OF(0, b, 1);
    a = MINMAX_OF(0, a, 1);

    if(suf->rgba[CR] == r && suf->rgba[CG] == g && suf->rgba[CB] == b &&
       suf->rgba[CA] == a)
        return true;

    // \todo when surface colours are intergrated with the
    // bias lighting model we will need to recalculate the
    // vertex colours when they are changed.
    suf->rgba[CR] = r;
    suf->rgba[CG] = g;
    suf->rgba[CB] = b;
    suf->rgba[CA] = a;

    return true;
}

boolean Surface_SetBlendMode(surface_t* suf, blendmode_t blendMode)
{
    assert(suf);
    if(suf->blendMode != blendMode)
    {
        suf->blendMode = blendMode;
    }
    return true;
}

void Surface_Update(surface_t* suf)
{
    assert(suf);
    if(!Surface_AttachedToMap(suf)) return;
    suf->inFlags |= SUIF_UPDATE_DECORATIONS;
}

int Surface_SetProperty(surface_t* suf, const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_BLENDMODE: {
        blendmode_t blendMode;
        DMU_SetValue(DMT_SURFACE_BLENDMODE, &blendMode, args, 0);
        Surface_SetBlendMode(suf, blendMode);
        break;
     }
    case DMU_FLAGS:
        DMU_SetValue(DMT_SURFACE_FLAGS, &suf->flags, args, 0);
        break;
    case DMU_COLOR: {
        float rgb[3];
        DMU_SetValue(DMT_SURFACE_RGBA, &rgb[CR], args, 0);
        DMU_SetValue(DMT_SURFACE_RGBA, &rgb[CG], args, 1);
        DMU_SetValue(DMT_SURFACE_RGBA, &rgb[CB], args, 2);
        Surface_SetColorRed(suf, rgb[CR]);
        Surface_SetColorGreen(suf, rgb[CG]);
        Surface_SetColorBlue(suf, rgb[CB]);
        break;
      }
    case DMU_COLOR_RED: {
        float r;
        DMU_SetValue(DMT_SURFACE_RGBA, &r, args, 0);
        Surface_SetColorRed(suf, r);
        break;
      }
    case DMU_COLOR_GREEN: {
        float g;
        DMU_SetValue(DMT_SURFACE_RGBA, &g, args, 0);
        Surface_SetColorGreen(suf, g);
        break;
      }
    case DMU_COLOR_BLUE: {
        float b;
        DMU_SetValue(DMT_SURFACE_RGBA, &b, args, 0);
        Surface_SetColorBlue(suf, b);
        break;
      }
    case DMU_ALPHA: {
        float a;
        DMU_SetValue(DMT_SURFACE_RGBA, &a, args, 0);
        Surface_SetAlpha(suf, a);
        break;
      }
    case DMU_MATERIAL: {
        material_t* mat;
        DMU_SetValue(DMT_SURFACE_MATERIAL, &mat, args, 0);
        Surface_SetMaterial(suf, mat);
        break;
      }
    case DMU_OFFSET_X: {
        float offX;
        DMU_SetValue(DMT_SURFACE_OFFSET, &offX, args, 0);
        Surface_SetMaterialOriginX(suf, offX);
        break;
      }
    case DMU_OFFSET_Y: {
        float offY;
        DMU_SetValue(DMT_SURFACE_OFFSET, &offY, args, 0);
        Surface_SetMaterialOriginY(suf, offY);
        break;
      }
    case DMU_OFFSET_XY: {
        float offset[2];
        DMU_SetValue(DMT_SURFACE_OFFSET, &offset[VX], args, 0);
        DMU_SetValue(DMT_SURFACE_OFFSET, &offset[VY], args, 1);
        Surface_SetMaterialOrigin(suf, offset[VX], offset[VY]);
        break;
      }
    default:
        Con_Error("Surface_SetProperty: Property %s is not writable.\n", DMU_Str(args->prop));
    }

    return false; // Continue iteration.
}

int Surface_GetProperty(const surface_t* suf, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_MATERIAL: {
        material_t* mat = suf->material;
        if(suf->inFlags & SUIF_FIX_MISSING_MATERIAL)
            mat = NULL;
        DMU_GetValue(DMT_SURFACE_MATERIAL, &mat, args, 0);
        break;
      }
    case DMU_OFFSET_X:
        DMU_GetValue(DMT_SURFACE_OFFSET, &suf->offset[VX], args, 0);
        break;
    case DMU_OFFSET_Y:
        DMU_GetValue(DMT_SURFACE_OFFSET, &suf->offset[VY], args, 0);
        break;
    case DMU_OFFSET_XY:
        DMU_GetValue(DMT_SURFACE_OFFSET, &suf->offset[VX], args, 0);
        DMU_GetValue(DMT_SURFACE_OFFSET, &suf->offset[VY], args, 1);
        break;
    case DMU_TANGENT_X:
        DMU_GetValue(DMT_SURFACE_TANGENT, &suf->tangent[VX], args, 0);
        break;
    case DMU_TANGENT_Y:
        DMU_GetValue(DMT_SURFACE_TANGENT, &suf->tangent[VY], args, 0);
        break;
    case DMU_TANGENT_Z:
        DMU_GetValue(DMT_SURFACE_TANGENT, &suf->tangent[VZ], args, 0);
        break;
    case DMU_TANGENT_XYZ:
        DMU_GetValue(DMT_SURFACE_TANGENT, &suf->tangent[VX], args, 0);
        DMU_GetValue(DMT_SURFACE_TANGENT, &suf->tangent[VY], args, 1);
        DMU_GetValue(DMT_SURFACE_TANGENT, &suf->tangent[VZ], args, 2);
        break;
    case DMU_BITANGENT_X:
        DMU_GetValue(DMT_SURFACE_BITANGENT, &suf->bitangent[VX], args, 0);
        break;
    case DMU_BITANGENT_Y:
        DMU_GetValue(DMT_SURFACE_BITANGENT, &suf->bitangent[VY], args, 0);
        break;
    case DMU_BITANGENT_Z:
        DMU_GetValue(DMT_SURFACE_BITANGENT, &suf->bitangent[VZ], args, 0);
        break;
    case DMU_BITANGENT_XYZ:
        DMU_GetValue(DMT_SURFACE_BITANGENT, &suf->bitangent[VX], args, 0);
        DMU_GetValue(DMT_SURFACE_BITANGENT, &suf->bitangent[VY], args, 1);
        DMU_GetValue(DMT_SURFACE_BITANGENT, &suf->bitangent[VZ], args, 2);
        break;
    case DMU_NORMAL_X:
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VX], args, 0);
        break;
    case DMU_NORMAL_Y:
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VY], args, 0);
        break;
    case DMU_NORMAL_Z:
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VZ], args, 0);
        break;
    case DMU_NORMAL_XYZ:
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VX], args, 0);
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VY], args, 1);
        DMU_GetValue(DMT_SURFACE_NORMAL, &suf->normal[VZ], args, 2);
        break;
    case DMU_COLOR:
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CR], args, 0);
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CG], args, 1);
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CB], args, 2);
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CA], args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CR], args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CG], args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CB], args, 0);
        break;
    case DMU_ALPHA:
        DMU_GetValue(DMT_SURFACE_RGBA, &suf->rgba[CA], args, 0);
        break;
    case DMU_BLENDMODE:
        DMU_GetValue(DMT_SURFACE_BLENDMODE, &suf->blendMode, args, 0);
        break;
    case DMU_FLAGS:
        DMU_GetValue(DMT_SURFACE_FLAGS, &suf->flags, args, 0);
        break;
    default:
        Con_Error("Surface_GetProperty: No property %s.\n", DMU_Str(args->prop));
    }

    return false; // Continue iteration.
}
