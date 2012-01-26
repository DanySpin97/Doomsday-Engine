/**\file rend_sprite.c
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
 * Rendering Map Objects as 2D Sprites.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_ui.h"

#include "texture.h"
#include "texturevariant.h"
#include "materialvariant.h"

// MACROS ------------------------------------------------------------------

#define DOTPROD(a, b)       (a[0]*b[0] + a[1]*b[1] + a[2]*b[2])

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void setupPSpriteParams(rendpspriteparams_t *params,
                               vispsprite_t *spr);
static void setupModelParamsForVisPSprite(rendmodelparams_t* params,
                                          vispsprite_t* spr);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int spriteLight = 4;
float maxSpriteAngle = 60;

// If true - use the "no translucency" blending mode for sprites/masked walls
byte noSpriteTrans = false;
int useSpriteAlpha = 1;
int useSpriteBlend = 1;

byte devNoSprites = false;
byte devThinkerIds = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Rend_SpriteRegister(void)
{
    // Cvars
    C_VAR_INT("rend-sprite-align", &alwaysAlign, 0, 0, 3);
    C_VAR_FLOAT("rend-sprite-align-angle", &maxSpriteAngle, 0, 0, 90);
    C_VAR_INT("rend-sprite-alpha", &useSpriteAlpha, 0, 0, 1);
    C_VAR_INT("rend-sprite-blend", &useSpriteBlend, 0, 0, 1);
    C_VAR_INT("rend-sprite-lights", &spriteLight, 0, 0, 10);
    C_VAR_BYTE("rend-sprite-mode", &noSpriteTrans, 0, 0, 1);
    C_VAR_INT("rend-sprite-noz", &noSpriteZWrite, 0, 0, 1);
    C_VAR_BYTE("rend-sprite-precache", &precacheSprites, 0, 0, 1);
    C_VAR_BYTE("rend-dev-nosprite", &devNoSprites, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-thinker-ids", &devThinkerIds, CVF_NO_ARCHIVE, 0, 1);
}

static __inline void renderQuad(dgl_vertex_t *v, dgl_color_t *c, dgl_texcoord_t *tc)
{
    glBegin(GL_QUADS);
        glColor4ubv(c[0].rgba);
        glTexCoord2fv(tc[0].st);
        glVertex3fv(v[0].xyz);

        glColor4ubv(c[1].rgba);
        glTexCoord2fv(tc[1].st);
        glVertex3fv(v[1].xyz);

        glColor4ubv(c[2].rgba);
        glTexCoord2fv(tc[2].st);
        glVertex3fv(v[2].xyz);

        glColor4ubv(c[3].rgba);
        glTexCoord2fv(tc[3].st);
        glVertex3fv(v[3].xyz);
    glEnd();
}

static int drawThinkerId(thinker_t* thinker, void* context)
{
#define MAX_THINKER_DIST  2048
    const Point2Raw labelOrigin = { 2, 2 };
    float* eye = (float*) context;
    float pos[3], dist, alpha;
    char buf[80];
    mobj_t* mo;

    // Skip non-mobjs.
    if(!P_IsMobjThinker(thinker->function)) return false;

    mo = (mobj_t*)thinker;
    pos[VX] = mo->pos[VX];
    pos[VY] = mo->pos[VY];
    pos[VZ] = mo->pos[VZ] + mo->height/2;

    dist = M_Distance(pos, eye);
    alpha = 1.f - MIN_OF(dist, MAX_THINKER_DIST) / MAX_THINKER_DIST;

    if(alpha > 0)
    {
        float scale = dist / (theWindow->geometry.size.width / 2);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(pos[VX], pos[VZ], pos[VY]);
        glRotatef(-vang + 180, 0, 1, 0);
        glRotatef(vpitch, 1, 0, 0);
        glScalef(-scale, -scale, 1);

        sprintf(buf, "%i", mo->thinker.id);
        UI_TextOutEx(buf, &labelOrigin, UI_Color(UIC_TITLE), alpha);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

    return false; // Continue iteration.
#undef MAX_THINKER_DIST
}

/**
 * Debugging aid for visualizing thinker IDs.
 */
void Rend_DrawThinkerIds(void)
{
    float eye[3];

    if(!devThinkerIds)
        return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    eye[VX] = vx;
    eye[VY] = vz;
    eye[VZ] = vy;

    P_IterateThinkers(NULL, 0x1 | 0x2, drawThinkerId, eye);

    // Restore previous state.
    glEnable(GL_DEPTH_TEST);
}

void Rend_Draw3DPlayerSprites(void)
{
    int                 i;

    // Setup the modelview matrix.
    Rend_ModelViewMatrix(false);

    if(usingFog)
        glEnable(GL_FOG);

    // Clear Z buffer. This will prevent the psprites from being clipped
    // by nearby polygons.
    glClear(GL_DEPTH_BUFFER_BIT);

    for(i = 0; i < DDMAXPSPRITES; ++i)
    {
        vispsprite_t*       spr = &visPSprites[i];

        if(spr->type != VPSPR_MODEL)
            continue; // Not used.

        {
        rendmodelparams_t   params;
        setupModelParamsForVisPSprite(&params, spr);
        Rend_RenderModel(&params);
        }
    }

    if(usingFog)
        glDisable(GL_FOG);
}

/**
 * Set all the colors in the array to that specified.
 */
void Spr_UniformVertexColors(int count, dgl_color_t *colors,
                             const float *rgba)
{
    for(; count-- > 0; colors++)
    {
        colors->rgba[CR] = (byte) (255 * rgba[CR]);
        colors->rgba[CG] = (byte) (255 * rgba[CG]);
        colors->rgba[CB] = (byte) (255 * rgba[CB]);
        colors->rgba[CA] = (byte) (255 * rgba[CA]);
    }
}

typedef struct {
    float               color[3], extra[3];
    dgl_vertex_t*        normal;
    uint                processedLights, maxLights;
} lightspriteparams_t;

static boolean lightSprite(const vlight_t* vlight, void* context)
{
    float*              dest;
    float               dot;
    lightspriteparams_t* params = (lightspriteparams_t*) context;

    dot = DOTPROD(vlight->vector, params->normal->xyz);
    dot += vlight->offset; // Shift a bit towards the light.

    if(!vlight->affectedByAmbient)
    {   // Won't be affected by ambient.
        dest = params->extra;
    }
    else
    {
        dest = params->color;
    }

    // Ability to both light and shade.
    if(dot > 0)
    {
        dot *= vlight->lightSide;
    }
    else
    {
        dot *= vlight->darkSide;
    }

    dot = MINMAX_OF(-1, dot, 1);

    dest[CR] += dot * vlight->color[CR];
    dest[CG] += dot * vlight->color[CG];
    dest[CB] += dot * vlight->color[CB];

    params->processedLights++;
    if(params->maxLights && !(params->processedLights < params->maxLights))
        return false; // Stop iteration.

    return true; // Continue iteration.
}

/**
 * Calculate vertex lighting.
 */
void Spr_VertexColors(int count, dgl_color_t *out, dgl_vertex_t *normal,
                      uint vLightListIdx, uint maxLights,
                      const float* ambient)
{
    int                 i, k;
    lightspriteparams_t params;

    for(i = 0; i < count; ++i, out++, normal++)
    {
        // Begin with total darkness.
        params.color[CR] = params.color[CG] = params.color[CB] = 0;
        params.extra[CR] = params.extra[CG] = params.extra[CB] = 0;
        params.normal = normal;
        params.processedLights = 0;
        params.maxLights = maxLights;

        VL_ListIterator(vLightListIdx, &params, lightSprite);

        // Check for ambient and convert to ubyte.
        for(k = 0; k < 3; ++k)
        {
            if(params.color[k] < ambient[k])
                params.color[k] = ambient[k];
            params.color[k] += params.extra[k];
            params.color[k] = MINMAX_OF(0, params.color[k], 1);

            // This is the final color.
            out->rgba[k] = (byte) (255 * params.color[k]);
        }

        out->rgba[CA] = (byte) (255 * ambient[CA]);
    }
}

static void setupPSpriteParams(rendpspriteparams_t* params, vispsprite_t* spr)
{
    ddpsprite_t* psp = spr->psp;
    int sprite = psp->statePtr->sprite;
    int frame = psp->statePtr->frame;
    float offScaleY = weaponOffsetScaleY / 1000.0f;
    const spritedef_t* sprDef;
    const patchtex_t* pTex;
    const spriteframe_t* sprFrame;
    const materialsnapshot_t* ms;
    const materialvariantspecification_t* spec;
    const variantspecification_t* texSpec;
    boolean flip;

#ifdef RANGECHECK
    if((unsigned) sprite >= (unsigned) numSprites)
        Con_Error("setupPSpriteParams: Invalid sprite number %i.\n", sprite);
#endif
    sprDef = &sprites[sprite];
#ifdef RANGECHECK
    if(frame >= sprDef->numFrames)
        Con_Error("setupPSpriteParams: Invalid frame number %i for sprite %i", frame, sprite);
#endif

    sprFrame = &sprDef->spriteFrames[frame];
    flip = sprFrame->flip[0];

    spec = Materials_VariantSpecificationForContext(MC_PSPRITE, 0, 1, 0, 0,
        GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 0, 1, 0, false, true, true, false);
    ms = Materials_Prepare(sprFrame->mats[0], spec, true);

#if _DEBUG
    if(Textures_Namespace(Textures_Id(MSU_texture(ms, MTU_PRIMARY))) != TN_SPRITES)
        Con_Error("setupPSpriteParams: Internal error, material snapshot's primary texture is not a SpriteTex!");
#endif

    pTex = (patchtex_t*)Texture_UserData(MSU_texture(ms, MTU_PRIMARY));
    assert(pTex);
    texSpec = TS_GENERAL(MSU_texturespec(ms, MTU_PRIMARY));
    assert(spec);

    params->pos[VX] = psp->pos[VX] - -pTex->offX + pspOffset[VX] + -texSpec->border;
    params->pos[VY] = offScaleY * (psp->pos[VY] - -pTex->offY) + pspOffset[VY] + -texSpec->border;
    params->width  = ms->size.width  + texSpec->border*2;
    params->height = ms->size.height + texSpec->border*2;

    TextureVariant_Coords(MST(ms, MTU_PRIMARY), &params->texOffset[0], &params->texOffset[1]);

    params->texFlip[0] = flip;
    params->texFlip[1] = false;

    params->mat = sprFrame->mats[0];
    params->ambientColor[CA] = spr->data.sprite.alpha;

    if(spr->data.sprite.isFullBright)
    {
        params->ambientColor[CR] = params->ambientColor[CG] =
            params->ambientColor[CB] = 1;
        params->vLightListIdx = 0;
    }
    else
    {
        collectaffectinglights_params_t lparams;

        if(useBias)
        {
            // Evaluate the position in the light grid.
            LG_Evaluate(spr->center, params->ambientColor);
        }
        else
        {
            float               lightLevel;
            const float*        secColor =
                R_GetSectorLightColor(spr->data.sprite.subsector->sector);

            // No need for distance attentuation.
            lightLevel = spr->data.sprite.subsector->sector->lightLevel;

            // Add extra light plus bonus.
            lightLevel += R_ExtraLightDelta();
            lightLevel *= pspLightLevelMultiplier;

            Rend_ApplyLightAdaptation(&lightLevel);

            // Determine the final ambientColor in affect.
            params->ambientColor[CR] = lightLevel * secColor[CR];
            params->ambientColor[CG] = lightLevel * secColor[CG];
            params->ambientColor[CB] = lightLevel * secColor[CB];
        }

        Rend_ApplyTorchLight(params->ambientColor, 0);

        lparams.starkLight = false;
        lparams.center[VX] = spr->center[VX];
        lparams.center[VY] = spr->center[VY];
        lparams.center[VZ] = spr->center[VZ];
        lparams.subsector = spr->data.sprite.subsector;
        lparams.ambientColor = params->ambientColor;

        params->vLightListIdx = R_CollectAffectingLights(&lparams);
    }
}

void Rend_DrawPSprite(const rendpspriteparams_t *params)
{
    float v1[2], v2[2], v3[2], v4[2];
    dgl_vertex_t quadNormals[4];
    dgl_color_t quadColors[4];
    int i;

    if(renderTextures == 1)
    {
        GL_SetPSprite(params->mat, 0, 0);
        glEnable(GL_TEXTURE_2D);
    }
    else if(renderTextures == 2)
    {   // For lighting debug, render all solid surfaces using the gray texture.
        material_t* mat = Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":gray"));
        const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
            MC_SPRITE, 0, 0, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 1, -2, 0, false, true, true, false);
        const materialsnapshot_t* ms = Materials_Prepare(mat, spec, true);

        GL_BindTexture(MST(ms, MTU_PRIMARY));
        glEnable(GL_TEXTURE_2D);
    }

    //  0---1
    //  |   |  Vertex layout.
    //  3---2

    v1[VX] = params->pos[VX];
    v1[VY] = params->pos[VY];

    v2[VX] = params->pos[VX] + params->width;
    v2[VY] = params->pos[VY];

    v3[VX] = params->pos[VX] + params->width;
    v3[VY] = params->pos[VY] + params->height;

    v4[VX] = params->pos[VX];
    v4[VY] = params->pos[VY] + params->height;

    // All psprite vertices are co-plannar, so just copy the view front vector.
    // \fixme: Can we do something better here?
    {
    const float* frontVec = R_ViewData(viewPlayer - ddPlayers)->frontVec;
    for(i = 0; i < 4; ++i)
    {
        quadNormals[i].xyz[VX] = frontVec[VX];
        quadNormals[i].xyz[VY] = frontVec[VZ];
        quadNormals[i].xyz[VZ] = frontVec[VY];
    }
    }

    if(!params->vLightListIdx)
    {   // Lit uniformly.
        Spr_UniformVertexColors(4, quadColors, params->ambientColor);
    }
    else
    {   // Lit normally.
        Spr_VertexColors(4, quadColors, quadNormals, params->vLightListIdx, spriteLight + 1, params->ambientColor);
    }

    {
    dgl_texcoord_t tcs[4], *tc = tcs;
    dgl_color_t* c = quadColors;

    tc[0].st[0] = params->texOffset[0] *  (params->texFlip[0]? 1:0);
    tc[0].st[1] = params->texOffset[1] *  (params->texFlip[1]? 1:0);
    tc[1].st[0] = params->texOffset[0] * (!params->texFlip[0]? 1:0);
    tc[1].st[1] = params->texOffset[1] *  (params->texFlip[1]? 1:0);
    tc[2].st[0] = params->texOffset[0] * (!params->texFlip[0]? 1:0);
    tc[2].st[1] = params->texOffset[1] * (!params->texFlip[1]? 1:0);
    tc[3].st[0] = params->texOffset[0] *  (params->texFlip[0]? 1:0);
    tc[3].st[1] = params->texOffset[1] * (!params->texFlip[1]? 1:0);

    glBegin(GL_QUADS);
        glColor4ubv(c[0].rgba);
        glTexCoord2fv(tc[0].st);
        glVertex2fv(v1);

        glColor4ubv(c[1].rgba);
        glTexCoord2fv(tc[1].st);
        glVertex2fv(v2);

        glColor4ubv(c[2].rgba);
        glTexCoord2fv(tc[2].st);
        glVertex2fv(v3);

        glColor4ubv(c[3].rgba);
        glTexCoord2fv(tc[3].st);
        glVertex2fv(v4);
    glEnd();
    }

    if(renderTextures)
        glDisable(GL_TEXTURE_2D);
}

/**
 * Draws 2D HUD sprites.
 *
 * \note If they were already drawn 3D, this won't do anything.
 */
void Rend_Draw2DPlayerSprites(void)
{
    int                 i;
    ddplayer_t         *ddpl = &viewPlayer->shared;
    ddpsprite_t        *psp;

    // Cameramen have no HUD sprites.
    if((ddpl->flags & DDPF_CAMERA) || (ddpl->flags & DDPF_CHASECAM))
        return;

    if(usingFog)
        glEnable(GL_FOG);

    // Check for fullbright.
    for(i = 0, psp = ddpl->pSprites; i < DDMAXPSPRITES; ++i, psp++)
    {
        vispsprite_t        *spr = &visPSprites[i];

        // Should this psprite be drawn?
        if(spr->type != VPSPR_SPRITE)
            continue; // No...

        // Draw as separate sprites.
        if(spr->psp && spr->psp->statePtr)
        {
            rendpspriteparams_t params;

            setupPSpriteParams(&params, spr);
            Rend_DrawPSprite(&params);
        }
    }

    if(usingFog)
        glDisable(GL_FOG);
}

/**
 * The first selected unit is active after this call.
 */
static void selectTexUnits(int count)
{
    int i;
    for(i = numTexUnits - 1; i >= count; i--)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glDisable(GL_TEXTURE_2D);
    }

    // Enable the selected units.
    for(i = count - 1; i >= 0; i--)
    {
        if(i >= numTexUnits) continue;

        glActiveTexture(GL_TEXTURE0 + i);
        glEnable(GL_TEXTURE_2D);
    }
}

/**
 * A sort of a sprite, I guess... Masked walls must be rendered sorted
 * with sprites, so no artifacts appear when sprites are seen behind
 * masked walls.
 */
void Rend_RenderMaskedWall(const rendmaskedwallparams_t* p)
{
    GLenum normalTarget, dynTarget;
    TextureVariant* tex = NULL;
    boolean withDyn = false;
    int normal = 0, dyn = 1;

    if(renderTextures)
    {
        const materialsnapshot_t* ms = Materials_PrepareVariant(p->material);
        tex = MST(ms, MTU_PRIMARY);
    }

    // Do we have a dynamic light to blend with?
    // This only happens when multitexturing is enabled.
    if(p->modTex && numTexUnits > 1)
    {
        if(IS_MUL)
        {
            normal = 1;
            dyn = 0;
        }
        else
        {
            normal = 0;
            dyn = 1;
        }

        selectTexUnits(2);
        GL_ModulateTexture(IS_MUL ? 4 : 5);

        // The dynamic light.
        glActiveTexture(IS_MUL ? GL_TEXTURE0 : GL_TEXTURE1);
        /// @fixme modTex may be the name of a "managed" texture.
        GL_BindTextureUnmanaged(renderTextures ? p->modTex : 0, GL_LINEAR);

        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, p->modColor);

        // The actual texture.
        glActiveTexture(IS_MUL ? GL_TEXTURE1 : GL_TEXTURE0);
        GL_BindTexture(tex);

        withDyn = true;
    }
    else
    {
        GL_ModulateTexture(1);
        glEnable(GL_TEXTURE_2D);
        GL_BindTexture(tex);
        normal = 0;
    }

    GL_BlendMode(p->blendMode);

    normalTarget = normal? GL_TEXTURE1 : GL_TEXTURE0;
    dynTarget = dyn? GL_TEXTURE1 : GL_TEXTURE0;

    // Draw one quad. This is obviously not a very efficient way to render
    // lots of masked walls, but since 3D models and sprites must be
    // rendered interleaved with masked walls, there's not much that can be
    // done about this.
    if(withDyn)
    {
        glBegin(GL_QUADS);
            glColor4fv(p->vertices[0].color);
            glMultiTexCoord2f(normalTarget, p->texCoord[0][0], p->texCoord[1][1]);

            glMultiTexCoord2f(dynTarget, p->modTexCoord[0][0], p->modTexCoord[1][1]);

            glVertex3f(p->vertices[0].pos[VX],
                       p->vertices[0].pos[VZ],
                       p->vertices[0].pos[VY]);

            glColor4fv(p->vertices[1].color);
            glMultiTexCoord2f(normalTarget, p->texCoord[0][0], p->texCoord[0][1]);

            glMultiTexCoord2f(dynTarget, p->modTexCoord[0][0], p->modTexCoord[1][0]);

            glVertex3f(p->vertices[1].pos[VX],
                       p->vertices[1].pos[VZ],
                       p->vertices[1].pos[VY]);

            glColor4fv(p->vertices[3].color);
            glMultiTexCoord2f(normalTarget, p->texCoord[1][0], p->texCoord[0][1]);

            glMultiTexCoord2f(dynTarget, p->modTexCoord[0][1], p->modTexCoord[1][0]);

            glVertex3f(p->vertices[3].pos[VX],
                       p->vertices[3].pos[VZ],
                       p->vertices[3].pos[VY]);

            glColor4fv(p->vertices[2].color);
            glMultiTexCoord2f(normalTarget, p->texCoord[1][0], p->texCoord[1][1]);

            glMultiTexCoord2f(dynTarget, p->modTexCoord[0][1], p->modTexCoord[1][1]);

            glVertex3f(p->vertices[2].pos[VX],
                       p->vertices[2].pos[VZ],
                       p->vertices[2].pos[VY]);
        glEnd();

        // Restore normal GL state.
        selectTexUnits(1);
        GL_ModulateTexture(1);
    }
    else
    {
        glBegin(GL_QUADS);
            glColor4fv(p->vertices[0].color);
            glTexCoord2f(p->texCoord[0][0], p->texCoord[1][1]);

            glVertex3f(p->vertices[0].pos[VX],
                       p->vertices[0].pos[VZ],
                       p->vertices[0].pos[VY]);

            glColor4fv(p->vertices[1].color);
            glTexCoord2f(p->texCoord[0][0], p->texCoord[0][1]);

            glVertex3f(p->vertices[1].pos[VX],
                       p->vertices[1].pos[VZ],
                       p->vertices[1].pos[VY]);

            glColor4fv(p->vertices[3].color);
            glTexCoord2f(p->texCoord[1][0], p->texCoord[0][1]);

            glVertex3f(p->vertices[3].pos[VX],
                       p->vertices[3].pos[VZ],
                       p->vertices[3].pos[VY]);

            glColor4fv(p->vertices[2].color);
            glTexCoord2f(p->texCoord[1][0], p->texCoord[1][1]);

            glVertex3f(p->vertices[2].pos[VX],
                       p->vertices[2].pos[VZ],
                       p->vertices[2].pos[VY]);
        glEnd();
    }

    glDisable(GL_TEXTURE_2D);
    GL_BlendMode(BM_NORMAL);
}

static void setupModelParamsForVisPSprite(rendmodelparams_t* params,
                                          vispsprite_t* spr)
{
    params->mf = spr->data.model.mf;
    params->nextMF = spr->data.model.nextMF;
    params->inter = spr->data.model.inter;
    params->alwaysInterpolate = false;
    params->id = spr->data.model.id;
    params->selector = spr->data.model.selector;
    params->flags = spr->data.model.flags;
    params->center[VX] = spr->center[VX];
    params->center[VY] = spr->center[VY];
    params->center[VZ] = spr->center[VZ];
    params->srvo[VX] = spr->data.model.visOff[VX];
    params->srvo[VY] = spr->data.model.visOff[VY];
    params->srvo[VZ] = spr->data.model.visOff[VZ] - spr->data.model.floorClip;
    params->gzt = spr->data.model.gzt;
    params->distance = -10;
    params->yaw = spr->data.model.yaw;
    params->extraYawAngle = 0;
    params->yawAngleOffset = spr->data.model.yawAngleOffset;
    params->pitch = spr->data.model.pitch;
    params->extraPitchAngle = 0;
    params->pitchAngleOffset = spr->data.model.pitchAngleOffset;
    params->extraScale = 0;
    params->viewAlign = spr->data.model.viewAligned;
    params->mirror = (mirrorHudModels? true : false);
    params->shineYawOffset = -vang;
    params->shinePitchOffset = vpitch + 90;
    params->shineTranslateWithViewerPos = false;
    params->shinepspriteCoordSpace = true;
    params->ambientColor[CA] = spr->data.model.alpha;

    if((levelFullBright || spr->data.model.stateFullBright) &&
       !(spr->data.model.mf->sub[0].flags & MFF_DIM))
    {
        params->ambientColor[CR] = params->ambientColor[CG] =
            params->ambientColor[CB] = 1;
        params->vLightListIdx = 0;
    }
    else
    {
        collectaffectinglights_params_t lparams;

        if(useBias)
        {
            LG_Evaluate(params->center, params->ambientColor);
        }
        else
        {
            float               lightLevel;
            const float*        secColor =
                R_GetSectorLightColor(spr->data.model.subsector->sector);

            // Diminished light (with compression).
            lightLevel = spr->data.model.subsector->sector->lightLevel;

            // No need for distance attentuation.

            // Add extra light.
            lightLevel += R_ExtraLightDelta();

            // The last step is to compress the resultant light value by
            // the global lighting function.
            Rend_ApplyLightAdaptation(&lightLevel);

            // Determine the final ambientColor in effect.
            params->ambientColor[CR] = lightLevel * secColor[CR];
            params->ambientColor[CG] = lightLevel * secColor[CG];
            params->ambientColor[CB] = lightLevel * secColor[CB];
        }

        Rend_ApplyTorchLight(params->ambientColor, params->distance);

        lparams.starkLight = true;
        lparams.center[VX] = spr->center[VX];
        lparams.center[VY] = spr->center[VY];
        lparams.center[VZ] = spr->center[VZ];
        lparams.subsector = spr->data.model.subsector;
        lparams.ambientColor = params->ambientColor;

        params->vLightListIdx = R_CollectAffectingLights(&lparams);
    }
}

static boolean generateHaloForVisSprite(const vissprite_t* spr, boolean primary)
{
    float occlussionFactor;

    if(primary && (spr->data.flare.flags & RFF_NO_PRIMARY))
        return false;

    if(spr->data.flare.isDecoration)
    {
        /**
         * \kludge surface decorations do not yet persist over frames,
         * thus we do not smoothly occlude their flares. Instead, we will
         * have to put up with them instantly appearing/disappearing.
         */
        occlussionFactor = (LO_IsClipped(spr->data.flare.lumIdx, viewPlayer - ddPlayers)? 0 : 1);
    }
    else
        occlussionFactor = (spr->data.flare.factor & 0x7f) / 127.0f;

    return H_RenderHalo(spr->center[VX], spr->center[VY], spr->center[VZ],
                        spr->data.flare.size,
                        spr->data.flare.tex,
                        spr->data.flare.color,
                        spr->distance,
                        occlussionFactor, spr->data.flare.mul,
                        spr->data.flare.xOff, primary,
                        (spr->data.flare.flags & RFF_NO_TURN));
}

/**
 * Render sprites, 3D models, masked wall segments and halos, ordered
 * back to front. Halos are rendered with Z-buffer tests and writes
 * disabled, so they don't go into walls or interfere with real objects.
 * It means that halos can be partly occluded by objects that are closer
 * to the viewpoint, but that's the price to pay for not having access to
 * the actual Z-buffer per-pixel depth information. The other option would
 * be for halos to shine through masked walls, sprites and models, which
 * looks even worse. (Plus, they are *halos*, not real lens flares...)
 */
void Rend_DrawMasked(void)
{
    boolean flareDrawn = false;
    vissprite_t* spr;

    if(devNoSprites)
        return;

    R_SortVisSprites();

    if(visSpriteP > visSprites)
    {
        // Draw all vissprites back to front.
        // Sprites look better with Z buffer writes turned off.
        for(spr = visSprSortedHead.next; spr != &visSprSortedHead; spr = spr->next)
        {
            switch(spr->type)
            {
            default:
                break;

            case VSPR_MASKED_WALL:
                // A masked wall is a specialized sprite.
                Rend_RenderMaskedWall(&spr->data.wall);
                break;

            case VSPR_SPRITE:
                // Render an old fashioned sprite, ah the nostalgia...
                Rend_RenderSprite(&spr->data.sprite);
                break;

            case VSPR_MODEL:
                Rend_RenderModel(&spr->data.model);
                break;

            case VSPR_FLARE:
                if(generateHaloForVisSprite(spr, true) && !flareDrawn)
                    flareDrawn = true;
                break;
            }
        }

        // Draw secondary halos.
        if(flareDrawn && haloMode > 1)
        {
            // Now we can setup the state only once.
            H_SetupState(true);

            for(spr = visSprSortedHead.next; spr != &visSprSortedHead;
                spr = spr->next)
            {
                if(spr->type == VSPR_FLARE)
                {
                    generateHaloForVisSprite(spr, false);
                }
            }

            // And we're done...
            H_SetupState(false);
        }
    }

    // Developer aid: visualize thinker IDs.
    if(devThinkerIds)
    {
        Rend_DrawThinkerIds();
    }
}

static materialvariant_t* chooseSpriteMaterial(const rendspriteparams_t* p)
{
    assert(p);

    if(!renderTextures) return NULL;
    if(renderTextures == 2)
    {
        // For lighting debug, render all solid surfaces using the gray texture.
        material_t* mat = Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":gray"));
        const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
            MC_SPRITE, 0, 0, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
            1, -2, -1, true, true, false, false);
        return Materials_ChooseVariant(mat, spec, true, true);
    }

    // Use the pre-chosen sprite.
    return p->material;
}

void Rend_RenderSprite(const rendspriteparams_t* params)
{
    float v1[3], v2[3], v3[3], v4[3];
    Point2Rawf viewOffset = { 0, 0 }; ///< View-aligned offset to center point.
    Size2Raw size = { 0, 0 };
    dgl_color_t quadColors[4];
    dgl_vertex_t quadNormals[4];
    boolean restoreMatrix = false;
    boolean restoreZ = false;
    float spriteCenter[3];
    float surfaceNormal[3];
    materialvariant_t* mat = NULL;
    const materialsnapshot_t* ms = NULL;
    float s = 1, t = 1; ///< Bottom right coords.
    int i;

    // Many sprite properties are inherited from the material.
    if(params->material)
    {
        const variantspecification_t* texSpec;
        patchtex_t* pTex;

        // Ensure this variant has been prepared.
        ms = Materials_PrepareVariant(params->material);

        texSpec = TS_GENERAL(MSU_texturespec(ms, MTU_PRIMARY));
        assert(texSpec);
        size.width  = ms->size.width  + texSpec->border*2;
        size.height = ms->size.height + texSpec->border*2;
        viewOffset.x = -size.width/2;

        TextureVariant_Coords(MST(ms, MTU_PRIMARY), &s, &t);

        if(Textures_Namespace(Textures_Id(MSU_texture(ms, MTU_PRIMARY))) == TN_SPRITES)
        {
            pTex = (patchtex_t*) Texture_UserData(MSU_texture(ms, MTU_PRIMARY));
            assert(pTex);
            viewOffset.x += (float) -pTex->offX;
        }
    }

    // We may want to draw using another material instead.
    mat = chooseSpriteMaterial(params);
    if(mat != params->material)
    {
        ms = mat? Materials_PrepareVariant(mat) : NULL;
    }

    if(ms)
    {
        GL_BindTexture(MST(ms, MTU_PRIMARY));
        glEnable(GL_TEXTURE_2D);
    }
    else
    {
        GL_SetNoTexture();
    }

    // Coordinates to the center of the sprite (game coords).
    spriteCenter[VX] = params->center[VX] + params->srvo[VX];
    spriteCenter[VY] = params->center[VY] + params->srvo[VY];
    spriteCenter[VZ] = params->center[VZ] + params->srvo[VZ];

    M_ProjectViewRelativeLine2D(spriteCenter, params->viewAligned,
                                size.width, viewOffset.x, v1, v4);

    v2[VX] = v1[VX];
    v2[VY] = v1[VY];
    v3[VX] = v4[VX];
    v3[VY] = v4[VY];

    v1[VZ] = v4[VZ] = spriteCenter[VZ] - size.height / 2 + viewOffset.y;
    v2[VZ] = v3[VZ] = spriteCenter[VZ] + size.height / 2 + viewOffset.y;

    // Calculate the surface normal.
    M_PointCrossProduct(v2, v1, v3, surfaceNormal);
    M_Normalize(surfaceNormal);

/*#if _DEBUG
// Draw the surface normal.
glBegin(GL_LINES);
glColor4f(1, 0, 0, 1);
glVertex3f(spriteCenter[VX], spriteCenter[VZ], spriteCenter[VY]);
glColor4f(1, 0, 0, 0);
glVertex3f(spriteCenter[VX] + surfaceNormal[VX] * 10,
           spriteCenter[VZ] + surfaceNormal[VZ] * 10,
           spriteCenter[VY] + surfaceNormal[VY] * 10);
glEnd();
#endif*/

    // All sprite vertices are co-plannar, so just copy the surface normal.
    // \fixme: Can we do something better here?
    for(i = 0; i < 4; ++i)
        memcpy(quadNormals[i].xyz, surfaceNormal, sizeof(surfaceNormal));

    if(!params->vLightListIdx)
    {
        // Lit uniformly.
        Spr_UniformVertexColors(4, quadColors, params->ambientColor);
    }
    else
    {
        // Lit normally.
        Spr_VertexColors(4, quadColors, quadNormals, params->vLightListIdx,
                         spriteLight + 1, params->ambientColor);
    }

    // Do we need to do some aligning?
    if(params->viewAligned || alwaysAlign >= 2)
    {
        // We must set up a modelview transformation matrix.
        restoreMatrix = true;
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        // Rotate around the center of the sprite.
        glTranslatef(spriteCenter[VX], spriteCenter[VZ], spriteCenter[VY]);
        if(!params->viewAligned)
        {
            float s_dx = v1[VX] - v2[VX];
            float s_dy = v1[VY] - v2[VY];

            if(alwaysAlign == 2)
            {   // Restricted camera alignment.
                float dx = spriteCenter[VX] - vx;
                float dy = spriteCenter[VY] - vz;
                float spriteAngle = BANG2DEG(
                    bamsAtan2(spriteCenter[VZ] - vy, sqrt(dx * dx + dy * dy)));

                if(spriteAngle > 180)
                    spriteAngle -= 360;

                if(fabs(spriteAngle) > maxSpriteAngle)
                {
                    float turnAngle = (spriteAngle > 0? spriteAngle - maxSpriteAngle :
                                                        spriteAngle + maxSpriteAngle);

                    // Rotate along the sprite edge.
                    glRotatef(turnAngle, s_dx, 0, s_dy);
                }
            }
            else
            {   // Restricted view plane alignment.
                // This'll do, for now... Really it should notice both the
                // sprite angle and vpitch.
                glRotatef(vpitch * .5f, s_dx, 0, s_dy);
            }
        }
        else
        {
            // Normal rotation perpendicular to the view plane.
            glRotatef(vpitch, viewsidex, 0, viewsidey);
        }
        glTranslatef(-spriteCenter[VX], -spriteCenter[VZ], -spriteCenter[VY]);
    }

    // Need to change blending modes?
    if(params->blendMode != BM_NORMAL)
        GL_BlendMode(params->blendMode);

    // Transparent sprites shouldn't be written to the Z buffer.
    if(params->noZWrite || params->ambientColor[CA] < .98f ||
       !(params->blendMode == BM_NORMAL || params->blendMode == BM_ZEROALPHA))
    {
        restoreZ = true;
        glDepthMask(GL_FALSE);
    }

    {
    dgl_vertex_t vs[4], *v = vs;
    dgl_texcoord_t tcs[4], *tc = tcs;

    //  1---2
    //  |   |  Vertex layout.
    //  0---3

    v[0].xyz[0] = v1[VX];
    v[0].xyz[1] = v1[VZ];
    v[0].xyz[2] = v1[VY];

    v[1].xyz[0] = v2[VX];
    v[1].xyz[1] = v2[VZ];
    v[1].xyz[2] = v2[VY];

    v[2].xyz[0] = v3[VX];
    v[2].xyz[1] = v3[VZ];
    v[2].xyz[2] = v3[VY];

    v[3].xyz[0] = v4[VX];
    v[3].xyz[1] = v4[VZ];
    v[3].xyz[2] = v4[VY];

    tc[0].st[0] = s *  (params->matFlip[0]? 1:0);
    tc[0].st[1] = t * (!params->matFlip[1]? 1:0);
    tc[1].st[0] = s *  (params->matFlip[0]? 1:0);
    tc[1].st[1] = t *  (params->matFlip[1]? 1:0);
    tc[2].st[0] = s * (!params->matFlip[0]? 1:0);
    tc[2].st[1] = t *  (params->matFlip[1]? 1:0);
    tc[3].st[0] = s * (!params->matFlip[0]? 1:0);
    tc[3].st[1] = t * (!params->matFlip[1]? 1:0);

    renderQuad(v, quadColors, tc);
    }

    if(ms) glDisable(GL_TEXTURE_2D);

    if(devMobjVLights && params->vLightListIdx)
    {   // Draw the vlight vectors, for debug.
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(params->center[VX], params->center[VZ], params->center[VY]);

        VL_ListIterator(params->vLightListIdx, (float*)&params->distance, R_DrawVLightVector);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    }

    // Need to restore the original modelview matrix?
    if(restoreMatrix)
        glPopMatrix();

    // Change back to normal blending?
    if(params->blendMode != BM_NORMAL)
        GL_BlendMode(BM_NORMAL);

    // Enable Z-writing again?
    if(restoreZ)
        glDepthMask(GL_TRUE);
}
