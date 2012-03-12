/**\file rend_decor.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * Surface Decorations.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_misc.h"

#include "materialvariant.h"

// MACROS ------------------------------------------------------------------

// Quite a bit of decorations, there!
#define MAX_DECOR_LIGHTS    (16384)

BEGIN_PROF_TIMERS()
  PROF_DECOR_UPDATE,
  PROF_DECOR_PROJECT,
  PROF_DECOR_ADD_LUMINOUS
END_PROF_TIMERS()

// TYPES -------------------------------------------------------------------

typedef struct decorsource_s {
    float           pos[3];
    float           maxDistance;
    const Surface*  surface;
    BspLeaf*        bspLeaf;
    unsigned int    lumIdx; // index+1 of linked lumobj, or 0.
    float           fadeMul;
    const ded_decorlight_t* def;
    DGLuint flareTex;
    struct decorsource_s* next;
} decorsource_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void updateSideSectionDecorations(SideDef* side, sidedefsection_t section);
static void updatePlaneDecorations(Plane* pln);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte useLightDecorations = true;
float decorMaxDist = 2048; // No decorations are visible beyond this.
float decorLightBrightFactor = 1;
float decorLightFadeAngle = .1f;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static uint numDecorations = 0;

static decorsource_t* sourceFirst = 0;
static decorsource_t* sourceCursor = 0;

// CODE --------------------------------------------------------------------

void Rend_DecorRegister(void)
{
    C_VAR_BYTE("rend-light-decor", &useLightDecorations, 0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-angle", &decorLightFadeAngle, 0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-bright", &decorLightBrightFactor, 0, 0, 10);
}

/**
 * Clears the list of decoration dummies.
 */
static void clearDecorations(void)
{
    numDecorations = 0;
    sourceCursor = sourceFirst;
}

extern void getLightingParams(float x, float y, float z, BspLeaf* bspLeaf,
                              float distance, boolean fullBright,
                              float ambientColor[3], uint* lightListIdx);

static void projectDecoration(decorsource_t* src)
{
    float distance, brightness;
    float v1[2], min, max;
    const lumobj_t* lum;
    vissprite_t* vis;

    // Does it pass the sector light limitation?
    min = src->def->lightLevels[0];
    max = src->def->lightLevels[1];

    if(!((brightness = R_CheckSectorLight(src->bspLeaf->sector->lightLevel, min, max)) > 0))
        return;

    if(src->fadeMul <= 0)
        return;

    // Is the point in range?
    distance = Rend_PointDist3D(src->pos);
    if(distance > src->maxDistance)
        return;

    /// @fixme dj: Why is LO_GetLuminous returning NULL given a supposedly valid index?
    if(!LO_GetLuminous(src->lumIdx))
        return;

    // Calculate edges of the shape.
    v1[VX] = src->pos[VX];
    v1[VY] = src->pos[VY];

    /**
     * Model decorations become model-type vissprites.
     * Light decorations become flare-type vissprites.
     */
    vis = R_NewVisSprite();
    vis->type = VSPR_FLARE;

    vis->center[VX] = src->pos[VX];
    vis->center[VY] = src->pos[VY];
    vis->center[VZ] = src->pos[VZ];
    vis->distance = distance;

    lum = LO_GetLuminous(src->lumIdx);

    vis->data.flare.isDecoration = true;
    vis->data.flare.lumIdx = src->lumIdx;

    // Color is taken from the associated lumobj.
    V3_Copy(vis->data.flare.color, LUM_OMNI(lum)->color);

    if(src->def->haloRadius > 0)
        vis->data.flare.size = MAX_OF(1, src->def->haloRadius * 60 * (50 + haloSize) / 100.0f);
    else
        vis->data.flare.size = 0;

    if(src->flareTex != 0)
        vis->data.flare.tex = src->flareTex;
    else
    {   // Primary halo disabled.
        vis->data.flare.flags |= RFF_NO_PRIMARY;
        vis->data.flare.tex = 0;
    }

    // Halo brightness drops as the angle gets too big.
    vis->data.flare.mul = 1;
    if(src->def->elevation < 2 && decorLightFadeAngle > 0) // Close the surface?
    {
        float vector[3], dot;

        vector[VX] = src->pos[VX] - vx;
        vector[VY] = src->pos[VY] - vz;
        vector[VZ] = src->pos[VZ] - vy;
        M_Normalize(vector);
        dot = -(src->surface->normal[VX] * vector[VX] +
                src->surface->normal[VY] * vector[VY] +
                src->surface->normal[VZ] * vector[VZ]);

        if(dot < decorLightFadeAngle / 2)
            vis->data.flare.mul = 0;
        else if(dot < 3 * decorLightFadeAngle)
            vis->data.flare.mul = (dot - decorLightFadeAngle / 2) / (2.5f * decorLightFadeAngle);
    }
}

/**
 * Re-initialize the decoration source tracking (might be called during a map
 * load or othersuch situation).
 */
void Rend_DecorInit(void)
{
    clearDecorations();
}

/**
 * Project all the non-clipped decorations. They become regular vissprites.
 */
void Rend_ProjectDecorations(void)
{
    if(sourceFirst == sourceCursor)
        return;
    if(!useLightDecorations)
        return;

    { decorsource_t* src = sourceFirst;
    do
    {
        projectDecoration(src);
    } while((src = src->next) != sourceCursor);
    }
}

static void addLuminousDecoration(decorsource_t* src)
{
    const ded_decorlight_t* def = src->def;
    float brightness;
    uint i, lumIdx;
    float min, max;
    lumobj_t* l;

    // Does it pass the sector light limitation?
    min = def->lightLevels[0];
    max = def->lightLevels[1];

    if(!((brightness = R_CheckSectorLight(src->bspLeaf->sector->lightLevel, min, max)) > 0))
        return;

    // Apply the brightness factor (was calculated using sector lightlevel).
    src->fadeMul = brightness * decorLightBrightFactor;
    src->lumIdx = 0;

    if(src->fadeMul <= 0)
        return;

    /**
     * @todo From here on is pretty much the same as LO_AddLuminous,
     *       reconcile the two.
     */

    lumIdx = LO_NewLuminous(LT_OMNI, src->bspLeaf);
    l = LO_GetLuminous(lumIdx);

    l->pos[VX] = src->pos[VX];
    l->pos[VY] = src->pos[VY];
    l->pos[VZ] = src->pos[VZ];
    l->maxDistance = src->maxDistance;
    l->decorSource = src;

    LUM_OMNI(l)->zOff = 0;
    LUM_OMNI(l)->tex = GL_PrepareLightMap(def->sides);
    LUM_OMNI(l)->ceilTex = GL_PrepareLightMap(def->up);
    LUM_OMNI(l)->floorTex = GL_PrepareLightMap(def->down);

    // These are the same rules as in DL_MobjRadius().
    LUM_OMNI(l)->radius = def->radius * 40 * loRadiusFactor;

    // Don't make a too small or too large light.
    if(LUM_OMNI(l)->radius > loMaxRadius)
        LUM_OMNI(l)->radius = loMaxRadius;

    for(i = 0; i < 3; ++i)
        LUM_OMNI(l)->color[i] = def->color[i] * src->fadeMul;

    src->lumIdx = lumIdx;
}

/**
 * Create lumobjs for all decorations who want them.
 */
void Rend_AddLuminousDecorations(void)
{
BEGIN_PROF( PROF_DECOR_ADD_LUMINOUS );

    if(useLightDecorations && sourceFirst != sourceCursor)
    {
        decorsource_t* src = sourceFirst;
        do
        {
            addLuminousDecoration(src);
        } while((src = src->next) != sourceCursor);
    }

END_PROF( PROF_DECOR_ADD_LUMINOUS );
}

/**
 * Create a new decoration source.
 */
static decorsource_t* addDecoration(void)
{
    decorsource_t*      src;

    // If the cursor is NULL, new sources must be allocated.
    if(!sourceCursor)
    {
        // Allocate a new entry.
        src = Z_Calloc(sizeof(decorsource_t), PU_APPSTATIC, NULL);

        if(!sourceFirst)
        {
            sourceFirst = src;
        }
        else
        {
            src->next = sourceFirst;
            sourceFirst = src;
        }
    }
    else
    {
        // There are old sources to use.
        src = sourceCursor;

        src->fadeMul = 0;
        src->lumIdx = 0;
        src->maxDistance = 0;
        src->pos[VX] = src->pos[VY] = src->pos[VZ] = 0;
        src->bspLeaf = 0;
        src->surface = 0;
        src->def = NULL;
        src->flareTex = 0;

        // Advance the cursor.
        sourceCursor = sourceCursor->next;
    }

    return src;
}

/**
 * A decorsource is created from the specified surface decoration.
 */
static void createDecorSource(const Surface* suf, const surfacedecor_t* dec, const float maxDistance)
{
    decorsource_t* src;

    if(numDecorations >= MAX_DECOR_LIGHTS)
        return; // Out of sources!
    ++numDecorations;

    // Fill in the data for a new surface decoration.
    src = addDecoration();
    src->pos[VX] = dec->pos[VX];
    src->pos[VY] = dec->pos[VY];
    src->pos[VZ] = dec->pos[VZ];
    src->maxDistance = maxDistance;
    src->bspLeaf = dec->bspLeaf;
    src->surface = suf;
    src->fadeMul = 1;
    src->def = dec->def;
    if(src->def)
    {
        const ded_decorlight_t* def = src->def;
        if(!def->flare || Str_CompareIgnoreCase(Uri_Path(def->flare), "-"))
        {
            src->flareTex = GL_PrepareFlareTexture(def->flare, def->flareTexture);
        }
    }
}

/**
 * @return              As this can also be used with iterators, will always
 *                      return @c true.
 */
boolean R_ProjectSurfaceDecorations(Surface* suf, void* context)
{
    float maxDist = *((float*) context);
    uint i;

    if(suf->inFlags & SUIF_UPDATE_DECORATIONS)
    {
        R_ClearSurfaceDecorations(suf);

        switch(DMU_GetType(suf->owner))
        {
        case DMU_SIDEDEF:
            {
            SideDef* side = (SideDef*)suf->owner;
            updateSideSectionDecorations(side, &side->SW_middlesurface == suf? SS_MIDDLE : &side->SW_bottomsurface == suf? SS_BOTTOM : SS_TOP);
            break;
            }
        case DMU_PLANE:
            updatePlaneDecorations((Plane*)suf->owner);
            break;
        default:
            Con_Error("R_ProjectSurfaceDecorations: Internal Error, unknown type %s.", DMU_Str(DMU_GetType(suf->owner)));
            break;
        }
        suf->inFlags &= ~SUIF_UPDATE_DECORATIONS;
    }

    if(useLightDecorations)
    for(i = 0; i < suf->numDecorations; ++i)
    {
        const surfacedecor_t* d = &suf->decorations[i];
        createDecorSource(suf, d, maxDist);
    }

    return true;
}

/**
 * Determine proper skip values.
 */
static void getDecorationSkipPattern(const int patternSkip[2], int* skip)
{
    uint i;
    for(i = 0; i < 2; ++i)
    {
        // Skip must be at least one.
        skip[i] = patternSkip[i] + 1;
        if(skip[i] < 1) skip[i] = 1;
    }
}

static uint generateDecorLights(const ded_decorlight_t* def, Surface* suf,
    material_t* mat, const pvec3_t v1, const pvec3_t v2, float width, float height,
    const pvec3_t delta, int axis, float offsetS, float offsetT, Sector* sec)
{
    vec3_t posBase, pos;
    float patternW, patternH;
    float s, t; // Horizontal and vertical offset.
    int skip[2];
    uint num;

    if(!mat || !R_IsValidLightDecoration(def)) return 0;

    // Skip must be at least one.
    getDecorationSkipPattern(def->patternSkip, skip);

    patternW = Material_Width(mat)  * skip[0];
    patternH = Material_Height(mat) * skip[1];

    if(0 == patternW && 0 == patternH) return 0;

    V3_Set(posBase, def->elevation * suf->normal[VX],
                    def->elevation * suf->normal[VY],
                    def->elevation * suf->normal[VZ]);
    V3_Sum(posBase, posBase, v1);

    // Let's see where the top left light is.
    s = M_CycleIntoRange(def->pos[0] - suf->visOffset[0] -
                         Material_Width(mat) * def->patternOffset[0] +
                         offsetS, patternW);
    num = 0;
    for(; s < width; s += patternW)
    {
        t = M_CycleIntoRange(def->pos[1] - suf->visOffset[1] -
                             Material_Height(mat) * def->patternOffset[1] +
                             offsetT, patternH);

        for(; t < height; t += patternH)
        {
            surfacedecor_t* d;
            float offS = s / width, offT = t / height;

            V3_Set(pos, delta[VX] * offS,
                        delta[VY] * (axis == VZ? offT : offS),
                        delta[VZ] * (axis == VZ? offS : offT));
            V3_Sum(pos, posBase, pos);

            if(sec)
            {
                // The point must be inside the correct sector.
                if(!P_IsPointXYInSector(pos[VX], pos[VY], sec))
                    continue;
            }

            d = R_CreateSurfaceDecoration(suf);
            if(d)
            {
                V3_Copy(d->pos, pos);
                d->bspLeaf = P_BspLeafAtPointXY(d->pos[VX], d->pos[VY]);
                d->def = def;
                num++;
            }
        }
    }

    return num;
}

/**
 * Generate decorations for the specified surface.
 */
static void updateSurfaceDecorations2(Surface* suf, float offsetS, float offsetT,
    vec3_t v1, vec3_t v2, Sector* sec, boolean visible)
{
    vec3_t delta;

    V3_Subtract(delta, v2, v1);

    if(visible &&
       (delta[VX] * delta[VY] != 0 ||
        delta[VX] * delta[VZ] != 0 ||
        delta[VY] * delta[VZ] != 0))
    {
        const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
            MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);
        material_t* mat = MaterialVariant_GeneralCase(Materials_ChooseVariant(suf->material, spec, true, true));
        const ded_decor_t* def = Materials_DecorationDef(mat);
        if(def)
        {
            int axis = V3_MajorAxis(suf->normal);
            float width, height;
            uint i;

            if(axis == VX || axis == VY)
            {
                width = sqrt(delta[VX] * delta[VX] + delta[VY] * delta[VY]);
                height = delta[VZ];
            }
            else
            {
                width = sqrt(delta[VX] * delta[VX]);
                height = delta[VY];
            }

            if(width < 0)  width  = -width;
            if(height < 0) height = -height;

            // Generate a number of lights.
            for(i = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
            {
                generateDecorLights(&def->lights[i], suf, mat, v1, v2, width, height, delta, axis, offsetS, offsetT, sec);
            }
        }
    }
}

/**
 * Generate decorations for a plane.
 */
static void updatePlaneDecorations(Plane* pln)
{
    Sector*  sec = pln->sector;
    Surface* suf = &pln->surface;
    vec3_t v1, v2;
    float offsetS, offsetT;

    if(pln->type == PLN_FLOOR)
    {
        V3_Set(v1, sec->aaBox.minX, sec->aaBox.maxY, pln->visHeight);
        V3_Set(v2, sec->aaBox.maxX, sec->aaBox.minY, pln->visHeight);
    }
    else
    {
        V3_Set(v1, sec->aaBox.minX, sec->aaBox.minY, pln->visHeight);
        V3_Set(v2, sec->aaBox.maxX, sec->aaBox.maxY, pln->visHeight);
    }

    offsetS = -fmod(sec->aaBox.minX, 64);
    offsetT = -fmod(sec->aaBox.minY, 64);

    updateSurfaceDecorations2(suf, offsetS, offsetT, v1, v2, sec, suf->material? true : false);
}

static void updateSideSectionDecorations(SideDef* side, sidedefsection_t section)
{
    LineDef*            line;
    Surface*            suf;
    vec3_t              v1, v2;
    int                 sid;
    float               offsetS = 0, offsetT = 0;
    boolean             visible = false;
    const Plane*        frontCeil, *frontFloor, *backCeil = NULL, *backFloor = NULL;
    float               bottom, top;

    if(!side->hedges || !side->hedges[0])
        return;

    line = side->hedges[0]->lineDef;
    sid = (line->L_backside && line->L_backside == side)? 1 : 0;
    frontCeil  = line->L_sector(sid)->SP_plane(PLN_CEILING);
    frontFloor = line->L_sector(sid)->SP_plane(PLN_FLOOR);

    if(line->L_backside)
    {
        backCeil  = line->L_sector(sid^1)->SP_plane(PLN_CEILING);
        backFloor = line->L_sector(sid^1)->SP_plane(PLN_FLOOR);
    }

    switch(section)
    {
    case SS_MIDDLE:
        suf = &side->SW_middlesurface;
        if(suf->material)
        {
            if(!line->L_backside)
            {
                top = frontCeil->visHeight;
                bottom = frontFloor->visHeight;
                if(line->flags & DDLF_DONTPEGBOTTOM)
                    offsetT += frontCeil->visHeight - frontFloor->visHeight;
                visible = true;
            }
            else
            {
                float texOffset[2];
                if(R_FindBottomTop(line, sid, SS_MIDDLE, suf->visOffset[VX], suf->visOffset[VY],
                             frontFloor, frontCeil, backFloor, backCeil,
                             (line->flags & DDLF_DONTPEGBOTTOM)? true : false,
                             (line->flags & DDLF_DONTPEGTOP)? true : false,
                             (side->flags & SDF_MIDDLE_STRETCH)? true : false,
                             LINE_SELFREF(line)? true : false,
                             &bottom, &top, texOffset))
                {
                    //offsetS = texOffset[VX];
                    // Counteract surface material offset (interpreted as geometry offset).
                    offsetT = suf->visOffset[VY];
                    visible = true;
                }
            }
        }
        break;

    case SS_TOP:
        suf = &side->SW_topsurface;
        if(suf->material)
            if(line->L_backside && backCeil->visHeight < frontCeil->visHeight &&
               (!Surface_IsSkyMasked(&backCeil->surface) || !Surface_IsSkyMasked(&frontCeil->surface)))
            {
                top = frontCeil->visHeight;
                bottom  = backCeil->visHeight;
                if(!(line->flags & DDLF_DONTPEGTOP))
                    offsetT += frontCeil->visHeight - backCeil->visHeight;
                visible = true;
            }
        break;

    case SS_BOTTOM:
        suf = &side->SW_bottomsurface;
        if(suf->material)
            if(line->L_backside && backFloor->visHeight > frontFloor->visHeight &&
               (!Surface_IsSkyMasked(&backFloor->surface) || !Surface_IsSkyMasked(&frontFloor->surface)))
            {
                top = backFloor->visHeight;
                bottom  = frontFloor->visHeight;
                if(line->flags & DDLF_DONTPEGBOTTOM)
                    offsetT -= frontCeil->visHeight - backFloor->visHeight;
                visible = true;
            }
        break;
    }

    if(visible)
    {
        V3_Set(v1, line->L_vpos(sid  )[VX], line->L_vpos(sid  )[VY], top);
        V3_Set(v2, line->L_vpos(sid^1)[VX], line->L_vpos(sid^1)[VY], bottom);
    }

    updateSurfaceDecorations2(suf, offsetS, offsetT, v1, v2, NULL, visible);
}

/**
 * Decorations are generated for each frame.
 */
void Rend_InitDecorationsForFrame(void)
{
    surfacelist_t* slist;
#ifdef DD_PROFILE
    static int          i;

    if(++i > 40)
    {
        i = 0;
        PRINT_PROF( PROF_DECOR_UPDATE );
        PRINT_PROF( PROF_DECOR_PROJECT );
        PRINT_PROF( PROF_DECOR_ADD_LUMINOUS );
    }
#endif

    // This only needs to be done if decorations have been enabled.
    if(!useLightDecorations) return;

BEGIN_PROF( PROF_DECOR_PROJECT );

    clearDecorations();

    slist = GameMap_DecoratedSurfaces(theMap);
    if(slist)
    {
        R_SurfaceListIterate(slist, R_ProjectSurfaceDecorations, &decorMaxDist);
    }

END_PROF( PROF_DECOR_PROJECT );
}
