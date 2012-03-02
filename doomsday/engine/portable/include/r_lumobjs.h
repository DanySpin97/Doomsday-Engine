/**\file r_lumobjs.h
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

/**
 * Lumobj (luminous object) management.
 */

#ifndef LIBDENG_REFRESH_LUMINOUS_H
#define LIBDENG_REFRESH_LUMINOUS_H

// Lumobject types.
typedef enum {
    LT_OMNI, // Omni (spherical) light.
    LT_PLANE, // Planar light.
} lumtype_t;

// Helper macros for accessing lum data.
#define LUM_OMNI(x)         (&((x)->data.omni))
#define LUM_PLANE(x)        (&((x)->data.plane))

typedef struct lumobj_s {
    lumtype_t type;
    float pos[3]; // Center of the obj.
    subsector_t* subsector;
    float maxDistance;
    void* decorSource; // decorsource_t ptr, else @c NULL.

    union lumobj_data_u {
        struct lumobj_omni_s {
            float color[3];
            float radius; // Radius for this omnilight source.
            float zOff; // Offset to center from pos[VZ].
            DGLuint tex; // Lightmap texture.
            DGLuint floorTex, ceilTex; // Lightmaps for floor/ceil.
        } omni;
        struct lumobj_plane_s {
            float color[3];
            float intensity;
            DGLuint tex;
            float normal[3];
        } plane;
    } data;
} lumobj_t;

/**
 * Dynlight. Lumobj Projection (POD) stores the results of projection.
 */
typedef struct {
    DGLuint texture;
    float s[2], t[2];
    ColorRawf color;
} dynlight_t;

extern boolean loInited;

extern uint loMaxLumobjs;
extern int loMaxRadius;
extern float loRadiusFactor;
extern byte rendInfoLums;
extern int useMobjAutoLights;

/// Register the console commands, variables, etc..., of this module.
void LO_Register(void);

// Setup.
void LO_InitForMap(void);

/// Release all system resources acquired by this module for managing.
void LO_Clear(void);

/**
 * To be called at the beginning of a world frame (prior to rendering views of the
 * world), to perform necessary initialization within this module.
 */
void LO_BeginWorldFrame(void);

/**
 * To be called at the beginning of a render frame to perform necessary initialization
 * within this module.
 */
void LO_BeginFrame(void);

/**
 * Create lumobjs for all sector-linked mobjs who want them.
 */
void LO_AddLuminousMobjs(void);

/**
 * To be called when lumobjs are disabled to perform necessary bookkeeping within this module.
 */
void LO_UnlinkMobjLumobjs(void);

/// @return  The number of active lumobjs for this frame.
uint LO_GetNumLuminous(void);

/**
 * Construct a new lumobj and link it into subsector @a ssec.
 * @return  Logical index (name) for referencing the new lumobj.
 */
uint LO_NewLuminous(lumtype_t type, subsector_t* ssec);

/// @return  Lumobj associated with logical index @a idx else @c NULL.
lumobj_t* LO_GetLuminous(uint idx);

/// @return  Logical index associated with lumobj @a lum.
uint LO_ToIndex(const lumobj_t* lum);

/// @return  @c true if the lumobj is clipped for the viewer.
boolean LO_IsClipped(uint idx, int i);

/// @return  @c true if the lumobj is hidden for the viewer.
boolean LO_IsHidden(uint idx, int i);

/// @return  Approximated distance between the lumobj and the viewer.
float LO_DistanceToViewer(uint idx, int i);

/**
 * Calculate a distance attentuation factor for a lumobj.
 *
 * @param idx  Logical index associated with the lumobj.
 * @param distance  Distance between the lumobj and the viewer.
 *
 * @return  Attentuation factor [0...1]
 */
float LO_AttenuationFactor(uint idx, float distance);

/**
 * Clip lumobj, omni lights in the given subsector.
 *
 * @param ssecidx  Subsector index in which lights will be clipped.
 */
void LO_ClipInSubsector(uint ssecidx);

/**
 * In the situation where a subsector contains both lumobjs and a polyobj,
 * the lumobjs must be clipped more carefully. Here we check if the line of
 * sight intersects any of the polyobj hedges that face the camera.
 *
 * @param ssecidx  Subsector index in which lumobjs will be clipped.
 */
void LO_ClipInSubsectorBySight(uint ssecidx);

/**
 * Iterate over all luminous objects within the specified origin range, making
 * a callback for each visited. Iteration ends when all selected luminous objects
 * have been visited or a callback returns non-zero.
 *
 * @param subsector  The subsector in which the origin resides.
 * @param x  X coordinate of the origin (must be within @a subsector).
 * @param y  Y coordinate of the origin (must be within @a subsector).
 * @param radius  Radius of the range around the origin point.
 * @param callback  Callback to make for each object.
 * @param paramaters  Data to pass to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int LO_LumobjsRadiusIterator2(subsector_t* subsector, float x, float y, float radius,
    int (*callback) (const lumobj_t* lum, float distance, void* paramaters), void* paramaters);
int LO_LumobjsRadiusIterator(subsector_t* subsector, float x, float y, float radius,
    int (*callback) (const lumobj_t* lum, float distance, void* paramaters)); /* paramaters = NULL */

/**
 * @defgroup projectLightFlags  Flags for LO_ProjectToSurface.
 * @{
 */
#define PLF_SORT_LUMINOSITY_DESC    0x1 /// Sort by descending luminosity, brightest to dullest.
#define PLF_NO_PLANE                0x2 /// Surface is not lit by planar lights.
#define PLF_TEX_FLOOR               0x4 /// Prefer the "floor" slot when picking textures.
#define PLF_TEX_CEILING             0x8 /// Prefer the "ceiling" slot when picking textures.
/**@}*/

/**
 * Project all lights affecting the given quad (world space), calculate
 * coordinates (in texture space) then store into a new list of projections.
 *
 * \assume The coordinates of the given quad must be contained wholly within
 * the subsector specified. This is due to an optimization within the lumobj
 * management which separates them according to their position in the BSP.
 *
 * @param flags  @see projectLightFlags
 * @param ssec  Subsector within which the quad wholly resides.
 * @param blendFactor  Multiplied with projection alpha.
 * @param topLeft  Top left coordinates of the surface being projected to.
 * @param bottomRight  Bottom right coordinates of the surface being projected to.
 * @param topLeft  Top left coordinates of the surface being projected to.
 * @param bottomRight  Bottom right coordinates of the surface being projected to.
 * @param tangent  Normalized tangent of the surface being projected to.
 * @param bitangent  Normalized bitangent of the surface being projected to.
 * @param normal  Normalized normal of the surface being projected to.
 *
 * @return  Projection list identifier if surface is lit else @c 0.
 */
uint LO_ProjectToSurface(int flags, subsector_t* ssec, float blendFactor,
    vec3_t topLeft, vec3_t bottomRight, vec3_t tangent, vec3_t bitangent, vec3_t normal);

/**
 * Iterate over projections in the identified surface-projection list, making
 * a callback for each visited. Iteration ends when all selected projections
 * have been visited or a callback returns non-zero.
 *
 * @param listIdx  Unique identifier of the list to process.
 * @param callback  Callback to make for each visited projection.
 * @param paramaters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int LO_IterateProjections2(uint listIdx, int (*callback) (const dynlight_t*, void*), void* paramaters);
int LO_IterateProjections(uint listIdx, int (*callback) (const dynlight_t*, void*)); /* paramaters = NULL */

void LO_DrawLumobjs(void);

#endif /* LIBDENG_REFRESH_LUMINOUS_H */
