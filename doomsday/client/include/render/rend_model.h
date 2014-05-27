/** @file rend_model.h  Model renderer (v2.1).
 *
 * @ingroup render
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_RENDER_MODEL_H
#define DENG_CLIENT_RENDER_MODEL_H

#include "ModelDef"
#include "render/ivissprite.h"

class TextureVariantSpec;

/// Absolute maximum number of vertices per submodel supported by this module.
#define RENDER_MAX_MODEL_VERTS  16192

/// @todo Split this large inflexible structure into logical subcomponent pieces.
struct vismodel_t : public IVissprite
{
// Animation, frame interpolation.
    ModelDef *mf, *nextMF;
    float           inter;
    dd_bool         alwaysInterpolate;
    int             id; // For a unique skin offset.
    int             selector;

// Position/Orientation/Scale
    coord_t         gzt; // The real center point and global top z for silhouette clipping.
    coord_t         srvo[3]; // Short-range visual offset.
    float           yaw, extraYawAngle, yawAngleOffset; ///< @todo We do not need three sets of angles...
    float           pitch, extraPitchAngle, pitchAngleOffset;

    float           extraScale;

    dd_bool         viewAlign;
    dd_bool         mirror; // If true the model will be mirrored about its Z axis (in model space).

// Appearance
    int             flags; // Mobj flags.
    int             tmap;

    // Lighting/color:
    float           ambientColor[4];
    uint            vLightListIdx;

    // Shiney texture mapping:
    float           shineYawOffset;
    float           shinePitchOffset;
    dd_bool         shineTranslateWithViewerPos;
    dd_bool         shinepspriteCoordSpace; // Use the psprite coordinate space hack.

    coord_t _distance; // Vissprites are sorted by distance.
    de::Vector3d _origin;

public:
    virtual ~vismodel_t() {}

    void setup(de::Vector3d const &origin, coord_t distToEye, de::Vector3d const &visOffset,
        float gzt, float yaw, float yawAngleOffset, float pitch, float pitchAngleOffset,
        ModelDef *mf, ModelDef *nextMF, float inter,
        de::Vector4f const &ambientColor,
        uint vLightListIdx,
        int id, int selector, BspLeaf *bspLeafAtOrigin, int mobjDDFlags, int tmap,
        bool viewAlign, bool fullBright, bool alwaysInterpolate);

    // Implements IVissprite.
    coord_t distance() const { return _distance; }
    de::Vector3d const &origin() const { return _origin; }
    void draw();
};

DENG_EXTERN_C byte useModels;
DENG_EXTERN_C int modelLight;
DENG_EXTERN_C int frameInter;
DENG_EXTERN_C float modelAspectMod;
DENG_EXTERN_C int mirrorHudModels;
DENG_EXTERN_C int modelShinyMultitex;
DENG_EXTERN_C float modelSpinSpeed;
DENG_EXTERN_C int maxModelDistance;
DENG_EXTERN_C float rendModelLOD;
DENG_EXTERN_C byte precacheSkins;

/**
 * Registers the console commands and variables used by this module.
 */
void Rend_ModelRegister();

/**
 * Initialize this module.
 */
void Rend_ModelInit();

/**
 * Shuts down this module.
 */
void Rend_ModelShutdown();

/**
 * Expand the render buffer to accommodate rendering models containing at most
 * this number of vertices.
 *
 * @note It is not actually necessary to call this. The vertex buffer will be
 *       enlarged automatically at render time to accommodate a given model so
 *       long as it contains less than RENDER_MAX_MODEL_VERTS. If not the model
 *       will simply not be rendered at all.
 *
 * @note Buffer reallocation is deferred until necessary, so repeatedly calling
 *       this routine during initialization is OK.
 *
 * @param numVertices  New maximum number of vertices we'll be required to handle.
 *
 * @return  @c true= successfully expanded. May fail if @a numVertices is larger
 *          than RENDER_MAX_MODEL_VERTS.
 */
bool Rend_ModelExpandVertexBuffers(uint numVertices);

/**
 * Lookup the texture specification for diffuse model skins.
 *
 * @param  noCompression  @c true= disable texture compression.
 * @return  Specification to be used when preparing such textures.
 */
TextureVariantSpec const &Rend_ModelDiffuseTextureSpec(bool noCompression);

/**
 * Lookup the texture specification for shiny model skins.
 *
 * @return  Specification to be used when preparing such textures.
 */
TextureVariantSpec const &Rend_ModelShinyTextureSpec();

#endif // DENG_CLIENT_RENDER_MODEL_H
