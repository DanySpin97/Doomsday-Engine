/** @file render/rend_main.cpp Map Renderer.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <de/libdeng2.h>

#include <QtAlgorithms>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_ui.h"

#include "edit_bias.h" /// @todo remove me
#include "network/net_main.h" /// @todo remove me

#include "MaterialSnapshot"
#include "MaterialVariantSpec"
#include "Texture"

#include "BspNode"
#include "world/map.h"
#include "world/maputil.h"
#include "world/lineowner.h"
#include "world/p_object.h"
#include "world/p_objlink.h"
#include "world/p_players.h"
#include "world/thinkers.h"

#include "WallEdge"
#include "SkyFixEdge"
#include "TriangleStripBuilder"
#include "render/blockmapvisual.h"
#include "render/sprite.h"

#include "gl/sys_opengl.h"

#include "render/rend_main.h"

using namespace de;

// Surface (tangent-space) Vector Flags.
#define SVF_TANGENT             0x01
#define SVF_BITANGENT           0x02
#define SVF_NORMAL              0x04

/**
 * @defgroup soundOriginFlags  Sound Origin Flags
 * Flags for use with the sound origin debug display.
 * @ingroup flags
 */
///@{
#define SOF_SECTOR              0x01
#define SOF_PLANE               0x02
#define SOF_SIDE                0x04
///@}

void Rend_DrawBBox(Vector3d const &pos, coord_t w, coord_t l, coord_t h, float a,
    float const color[3], float alpha, float br, bool alignToBase = true);

void Rend_DrawArrow(Vector3d const &pos, float a, float s, float const color3f[3], float alpha);

boolean usingFog = false; // Is the fog in use?
float fogColor[4];
float fieldOfView = 95.0f;
byte smoothTexAnim = true;
int useShinySurfaces = true;

int useDynLights = true;
float dynlightFactor = .5f;
float dynlightFogBright = .15f;

int useWallGlow = true;
float glowFactor = .5f;
float glowHeightFactor = 3; // Glow height as a multiplier.
int glowHeightMax = 100; // 100 is the default (0-1024).

int useShadows = true;
float shadowFactor = 1.2f;
int shadowMaxRadius = 80;
int shadowMaxDistance = 1000;

float detailFactor = .5f;
float detailScale = 4;

coord_t vOrigin[3];
float vang, vpitch;
float viewsidex, viewsidey;

byte freezeRLs = false;
int devRendSkyMode = false;
byte devRendSkyAlways = false;

// Ambient lighting, rAmbient is used within the renderer, ambientLight is
// used to store the value of the ambient light cvar.
// The value chosen for rAmbient occurs in Rend_UpdateLightModMatrix
// for convenience (since we would have to recalculate the matrix anyway).
int rAmbient = 0, ambientLight = 0;

int viewpw, viewph; // Viewport size, in pixels.
int viewpx, viewpy; // Viewpoint top left corner, in pixels.

float yfov;

int gameDrawHUD = 1; // Set to zero when we advise that the HUD should not be drawn

/**
 * Implements a pre-calculated LUT for light level limiting and range
 * compression offsets, arranged such that it may be indexed with a
 * light level value. Return value is an appropriate delta (considering
 * all applicable renderer properties) which has been pre-clamped such
 * that when summed with the original light value the result remains in
 * the normalized range [0..1].
 */
float lightRangeCompression = 0;
float lightModRange[255];
byte devLightModRange = 0;

float rendLightDistanceAttenuation = 1024;
int rendLightAttenuateFixedColormap = 1;

float rendLightWallAngle = 1.2f; // Intensity of angle-based wall lighting.
byte rendLightWallAngleSmooth = true;

float rendSkyLight = .2f; // Intensity factor.
byte rendSkyLightAuto = true;

int extraLight; // Bumped light from gun blasts.
float extraLightDelta;

byte devMobjVLights = 0; // @c 1= Draw mobj vertex lighting vector.
int devMobjBBox = 0; // 1 = Draw mobj bounding boxes (for debug)
int devPolyobjBBox = 0; // 1 = Draw polyobj bounding boxes (for debug)
DGLuint dlBBox = 0; // Display list: active-textured bbox model.

byte devVertexIndices = 0; // @c 1= Draw world vertex indices (for debug).
byte devVertexBars = 0; // @c 1= Draw world vertex position bars.
byte devSoundOrigins = 0; ///< cvar @c 1= Draw sound origin debug display.
byte devSurfaceVectors = 0;
byte devNoTexFix = 0;

static void Rend_DrawBoundingBoxes();
static void Rend_DrawSoundOrigins();
static void Rend_DrawSurfaceVectors();
static void Rend_DrawVertexIndices();

static uint buildLeafPlaneGeometry(BspLeaf const &leaf, ClockDirection direction,
    coord_t height, rvertex_t **verts, uint *vertsSize);

// Draw state:
static Vector2d eyeOrigin; // Viewer origin.
static BspLeaf *currentBspLeaf; // BSP leaf currently being drawn.
static Vector3f currentSectorLightColor;
static float currentSectorLightLevel;
static bool firstBspLeaf; // No range checking for the first one.

static void markLightGridForFullUpdate()
{
    if(App_World().hasMap())
    {
        Map &map = App_World().map();
        if(map.hasLightGrid())
            map.lightGrid().markAllForUpdate();
    }
}

void Rend_Register()
{
    C_VAR_FLOAT ("rend-camera-fov",                 &fieldOfView,                   0, 1, 179);

    C_VAR_FLOAT ("rend-glow",                       &glowFactor,                    0, 0, 2);
    C_VAR_INT   ("rend-glow-height",                &glowHeightMax,                 0, 0, 1024);
    C_VAR_FLOAT ("rend-glow-scale",                 &glowHeightFactor,              0, 0.1f, 10);
    C_VAR_INT   ("rend-glow-wall",                  &useWallGlow,                   0, 0, 1);

    C_VAR_INT2  ("rend-light",                      &useDynLights,                  0, 0, 1, LO_UnlinkMobjLumobjs);
    C_VAR_INT2  ("rend-light-ambient",              &ambientLight,                  0, 0, 255, Rend_UpdateLightModMatrix);
    C_VAR_FLOAT ("rend-light-attenuation",          &rendLightDistanceAttenuation, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT ("rend-light-bright",               &dynlightFactor,                0, 0, 1);
    C_VAR_FLOAT2("rend-light-compression",          &lightRangeCompression,         0, -1, 1, Rend_UpdateLightModMatrix);
    C_VAR_FLOAT ("rend-light-fog-bright",           &dynlightFogBright,             0, 0, 1);
    C_VAR_FLOAT2("rend-light-sky",                  &rendSkyLight,                  0, 0, 1, markLightGridForFullUpdate);
    C_VAR_BYTE2 ("rend-light-sky-auto",             &rendSkyLightAuto,              0, 0, 1, markLightGridForFullUpdate);
    C_VAR_FLOAT ("rend-light-wall-angle",           &rendLightWallAngle,            CVF_NO_MAX, 0, 0);
    C_VAR_BYTE  ("rend-light-wall-angle-smooth",    &rendLightWallAngleSmooth,      0, 0, 1);

    C_VAR_BYTE  ("rend-map-material-precache",      &precacheMapMaterials,          0, 0, 1);

    C_VAR_INT   ("rend-shadow",                     &useShadows,                    0, 0, 1);
    C_VAR_FLOAT ("rend-shadow-darkness",            &shadowFactor,                  0, 0, 2);
    C_VAR_INT   ("rend-shadow-far",                 &shadowMaxDistance,             CVF_NO_MAX, 0, 0);
    C_VAR_INT   ("rend-shadow-radius-max",          &shadowMaxRadius,               CVF_NO_MAX, 0, 0);

    C_VAR_BYTE  ("rend-tex-anim-smooth",            &smoothTexAnim,                 0, 0, 1);
    C_VAR_INT   ("rend-tex-shiny",                  &useShinySurfaces,              0, 0, 1);

    C_VAR_INT   ("rend-dev-sky",                    &devRendSkyMode,                CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-sky-always",             &devRendSkyAlways,              CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-freeze",                 &freezeRLs,                     CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT   ("rend-dev-cull-leafs",             &devNoCulling,                  CVF_NO_ARCHIVE,0,1);
    C_VAR_INT   ("rend-dev-mobj-bbox",              &devMobjBBox,                   CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-mobj-show-vlights",      &devMobjVLights,                CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT   ("rend-dev-polyobj-bbox",           &devPolyobjBBox,                CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-light-mod",              &devLightModRange,              CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-tex-showfix",            &devNoTexFix,                   CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-blockmap-debug",         &bmapShowDebug,                 CVF_NO_ARCHIVE, 0, 4);
    C_VAR_FLOAT ("rend-dev-blockmap-debug-size",    &bmapDebugSize,                 CVF_NO_ARCHIVE, .1f, 100);
    C_VAR_BYTE  ("rend-dev-vertex-show-indices",    &devVertexIndices,              CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-vertex-show-bars",       &devVertexBars,                 CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE  ("rend-dev-surface-show-vectors",   &devSurfaceVectors,             CVF_NO_ARCHIVE, 0, 7);
    C_VAR_BYTE  ("rend-dev-soundorigins",           &devSoundOrigins,               CVF_NO_ARCHIVE, 0, 7);

    RL_Register();
    LO_Register();
    Rend_DecorRegister();
    SB_Register();
    LightGrid::consoleRegister();
    Sky_Register();
    Rend_ModelRegister();
    Rend_ParticleRegister();
    Rend_RadioRegister();
    Rend_SpriteRegister();
    //Rend_ConsoleRegister();
    Vignette_Register();
}

static void reportWallSectionDrawn(Line &line)
{
    // Already been here?
    int playerNum = viewPlayer - ddPlayers;
    if(line.isMappedByPlayer(playerNum)) return;

    // Mark as drawn.
    line.markMappedByPlayer(playerNum);

    // Send a status report.
    if(gx.HandleMapObjectStatusReport)
    {
        gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED, line.indexInMap(),
                                       DMU_LINE, &playerNum);
    }
}

static inline bool isNullLeaf(BspLeaf *leaf)
{
    return !leaf || !leaf->hasWorldVolume();
}

coord_t Rend_PointDist3D(coord_t const point[3])
{
    return M_ApproxDistance3(vOrigin[VX] - point[VX],
                             vOrigin[VZ] - point[VY],
                             1.2 * (vOrigin[VY] - point[VZ]));
}

void Rend_Init()
{
    C_Init();
    RL_Init();
    Sky_Init();
}

void Rend_Shutdown()
{
    RL_Shutdown();
}

/// World/map renderer reset.
void Rend_Reset()
{
    LO_Clear(); // Free lumobj stuff.
    if(dlBBox)
    {
        GL_DeleteLists(dlBBox, 1);
        dlBBox = 0;
    }
}

void Rend_ModelViewMatrix(bool useAngles)
{
    viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);

    vOrigin[VX] = viewData->current.origin[VX];
    vOrigin[VY] = viewData->current.origin[VZ];
    vOrigin[VZ] = viewData->current.origin[VY];
    vang = viewData->current.angle / (float) ANGLE_MAX *360 - 90;
    vpitch = viewData->current.pitch * 85.0 / 110.0;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if(useAngles)
    {
        glRotatef(vpitch, 1, 0, 0);
        glRotatef(vang, 0, 1, 0);
    }
    glScalef(1, 1.2f, 1);      // This is the aspect correction.
    glTranslatef(-vOrigin[VX], -vOrigin[VY], -vOrigin[VZ]);
}

static inline double viewFacingDot(Vector2d const &v1, Vector2d const &v2)
{
    // The dot product.
    return (v1.y - v2.y) * (v1.x - vOrigin[VX]) + (v2.x - v1.x) * (v1.y - vOrigin[VZ]);
}

static void Rend_VertexColorsGlow(ColorRawf *colors, uint num, float glow)
{
    for(uint i = 0; i < num; ++i)
    {
        ColorRawf *c = &colors[i];
        c->rgba[CR] = c->rgba[CG] = c->rgba[CB] = glow;
    }
}

static void Rend_VertexColorsAlpha(ColorRawf *colors, uint num, float alpha)
{
    for(uint i = 0; i < num; ++i)
    {
        colors[i].rgba[CA] = alpha;
    }
}

void Rend_ApplyTorchLight(float color[3], float distance)
{
    ddplayer_t *ddpl = &viewPlayer->shared;

    // Disabled?
    if(!ddpl->fixedColorMap) return;

    // Check for torch.
    if(!rendLightAttenuateFixedColormap || distance < 1024)
    {
        // Colormap 1 is the brightest. I'm guessing 16 would be
        // the darkest.
        float d = (16 - ddpl->fixedColorMap) / 15.0f;
        if(rendLightAttenuateFixedColormap)
        {
            d *= (1024 - distance) / 1024.0f;
        }

        if(torchAdditive)
        {
            color[CR] += d * torchColor[CR];
            color[CG] += d * torchColor[CG];
            color[CB] += d * torchColor[CB];
        }
        else
        {
            color[CR] += d * ((color[CR] * torchColor[CR]) - color[CR]);
            color[CG] += d * ((color[CG] * torchColor[CG]) - color[CG]);
            color[CB] += d * ((color[CB] * torchColor[CB]) - color[CB]);
        }
    }
}

float Rend_AttenuateLightLevel(float distToViewer, float lightLevel)
{
    if(distToViewer > 0 && rendLightDistanceAttenuation > 0)
    {
        float real = lightLevel -
            (distToViewer - 32) / rendLightDistanceAttenuation *
                (1 - lightLevel);

        float minimum = lightLevel * lightLevel + (lightLevel - .63f) * .5f;
        if(real < minimum)
            real = minimum; // Clamp it.

        return real;
    }

    return lightLevel;
}

float Rend_ExtraLightDelta()
{
    return extraLightDelta;
}

Vector3f const &Rend_SectorLightColor(Sector const &sector)
{
    static Vector3f skyLightColor;
    static Vector3f oldSkyAmbientColor(-1.f, -1.f, -1.f);
    static float oldRendSkyLight = -1;

    if(rendSkyLight > .001f && sector.hasSkyMaskedPlane())
    {
        ColorRawf const *ambientColor = Sky_AmbientColor();

        if(rendSkyLight != oldRendSkyLight ||
           !INRANGE_OF(ambientColor->red,   oldSkyAmbientColor.x, .001f) ||
           !INRANGE_OF(ambientColor->green, oldSkyAmbientColor.y, .001f) ||
           !INRANGE_OF(ambientColor->blue,  oldSkyAmbientColor.z, .001f))
        {
            skyLightColor = Vector3f(ambientColor->rgb);
            R_AmplifyColor(skyLightColor);

            // Apply the intensity factor cvar.
            for(int i = 0; i < 3; ++i)
            {
                skyLightColor[i] = skyLightColor[i] + (1 - rendSkyLight) * (1.f - skyLightColor[i]);
            }

            // When the sky light color changes we must update the light grid.
            markLightGridForFullUpdate();
            oldSkyAmbientColor = Vector3f(ambientColor->rgb);
        }

        oldRendSkyLight = rendSkyLight;
        return skyLightColor;
    }

    // A non-skylight sector (i.e., everything else!)
    // Return the sector's ambient light color.
    return sector.lightColor();
}

Material *Rend_ChooseMapSurfaceMaterial(Surface const &surface)
{
    switch(renderTextures)
    {
    case 0: // No texture mode.
    case 1: // Normal mode.
        if(devNoTexFix && surface.hasFixMaterial())
        {
            // Missing material debug mode -- use special "missing" material.
            return &App_Materials().find(de::Uri("System", Path("missing"))).material();
        }

        // Use the surface-bound material.
        return surface.materialPtr();

    case 2: // Lighting debug mode.
        if(surface.hasMaterial() && !(!devNoTexFix && surface.hasFixMaterial()))
        {
            if(!surface.hasSkyMaskedMaterial() || devRendSkyMode)
            {
                // Use the special "gray" material.
                return &App_Materials().find(de::Uri("System", Path("gray"))).material();
            }
        }
        break;

    default: break;
    }

    // No material, then.
    return 0;
}

static void lightVertex(ColorRawf &color, rvertex_t const &vtx, float lightLevel,
                        Vector3f const &ambientColor)
{
    float const dist = Rend_PointDist2D(vtx.pos);
    float lightVal = Rend_AttenuateLightLevel(dist, lightLevel);

    // Add extra light.
    lightVal += Rend_ExtraLightDelta();

    Rend_ApplyLightAdaptation(lightVal);

    // Mix with the surface color.
    for(int i = 0; i < 3; ++i)
    {
        color.rgba[i] = lightVal * ambientColor[i];
    }
}

static void lightVertices(uint num, ColorRawf *colors, rvertex_t const *verts,
                          float lightLevel, Vector3f const &ambientColor)
{
    for(uint i = 0; i < num; ++i)
    {
        lightVertex(colors[i], verts[i], lightLevel, ambientColor);
    }
}

static void torchLightVertices(uint num, ColorRawf *colors, rvertex_t const *verts)
{
    for(uint i = 0; i < num; ++i)
    {
        Rend_ApplyTorchLight(colors[i].rgba, Rend_PointDist2D(verts[i].pos));
    }
}

int RIT_FirstDynlightIterator(dynlight_t const *dyn, void *parameters)
{
    dynlight_t const **ptr = (dynlight_t const **)parameters;
    *ptr = dyn;
    return 1; // Stop iteration.
}

/**
 * This doesn't create a rendering primitive but a vissprite! The vissprite
 * represents the masked poly and will be rendered during the rendering
 * of sprites. This is necessary because all masked polygons must be
 * rendered back-to-front, or there will be alpha artifacts along edges.
 */
void Rend_AddMaskedPoly(rvertex_t const *rvertices, ColorRawf const *rcolors,
    coord_t wallLength, MaterialVariant *material, Vector2f const &materialOrigin,
    blendmode_t blendMode, uint lightListIdx, float glow)
{
    vissprite_t *vis = R_NewVisSprite();

    vis->type = VSPR_MASKED_WALL;
    vis->origin[VX] = (rvertices[0].pos[VX] + rvertices[3].pos[VX]) / 2;
    vis->origin[VY] = (rvertices[0].pos[VY] + rvertices[3].pos[VY]) / 2;
    vis->origin[VZ] = (rvertices[0].pos[VZ] + rvertices[3].pos[VZ]) / 2;
    vis->distance = Rend_PointDist2D(vis->origin);

    VS_WALL(vis)->texOffset[0] = materialOrigin[VX];
    VS_WALL(vis)->texOffset[1] = materialOrigin[VY];

    // Masked walls are sometimes used for special effects like arcs,
    // cobwebs and bottoms of sails. In order for them to look right,
    // we need to disable texture wrapping on the horizontal axis (S).
    // Most masked walls need wrapping, though. What we need to do is
    // look at the texture coordinates and see if they require texture
    // wrapping.
    if(renderTextures)
    {
        MaterialSnapshot const &ms = material->prepare();
        int wrapS = GL_REPEAT, wrapT = GL_REPEAT;

        VS_WALL(vis)->texCoord[0][VX] = VS_WALL(vis)->texOffset[0] / ms.width();
        VS_WALL(vis)->texCoord[1][VX] = VS_WALL(vis)->texCoord[0][VX] + wallLength / ms.width();
        VS_WALL(vis)->texCoord[0][VY] = VS_WALL(vis)->texOffset[1] / ms.height();
        VS_WALL(vis)->texCoord[1][VY] = VS_WALL(vis)->texCoord[0][VY] +
                (rvertices[3].pos[VZ] - rvertices[0].pos[VZ]) / ms.height();

        if(!ms.isOpaque())
        {
            if(!(VS_WALL(vis)->texCoord[0][VX] < 0 || VS_WALL(vis)->texCoord[0][VX] > 1 ||
                 VS_WALL(vis)->texCoord[1][VX] < 0 || VS_WALL(vis)->texCoord[1][VX] > 1))
            {
                // Visible portion is within the actual [0..1] range.
                wrapS = GL_CLAMP_TO_EDGE;
            }

            // Clamp on the vertical axis if the coords are in the normal [0..1] range.
            if(!(VS_WALL(vis)->texCoord[0][VY] < 0 || VS_WALL(vis)->texCoord[0][VY] > 1 ||
                 VS_WALL(vis)->texCoord[1][VY] < 0 || VS_WALL(vis)->texCoord[1][VY] > 1))
            {
                wrapT = GL_CLAMP_TO_EDGE;
            }
        }

        // Choose a specific variant for use as a middle wall section.
        material = material->generalCase()
                       .chooseVariant(Rend_MapSurfaceMaterialSpec(wrapS, wrapT),
                                      true /*can create variant*/);
    }

    VS_WALL(vis)->material = material;
    VS_WALL(vis)->blendMode = blendMode;

    for(int i = 0; i < 4; ++i)
    {
        VS_WALL(vis)->vertices[i].pos[VX] = rvertices[i].pos[VX];
        VS_WALL(vis)->vertices[i].pos[VY] = rvertices[i].pos[VY];
        VS_WALL(vis)->vertices[i].pos[VZ] = rvertices[i].pos[VZ];

        for(int c = 0; c < 4; ++c)
        {
            /// @todo Do not clamp here.
            VS_WALL(vis)->vertices[i].color[c] = MINMAX_OF(0, rcolors[i].rgba[c], 1);
        }
    }

    /// @todo Semitransparent masked polys arn't lit atm
    if(glow < 1 && lightListIdx && numTexUnits > 1 && envModAdd &&
       !(rcolors[0].rgba[CA] < 1))
    {
        dynlight_t const *dyn = 0;

        /**
         * The dynlights will have already been sorted so that the brightest
         * and largest of them is first in the list. So grab that one.
         */
        LO_IterateProjections(lightListIdx, RIT_FirstDynlightIterator, (void *)&dyn);

        VS_WALL(vis)->modTex = dyn->texture;
        VS_WALL(vis)->modTexCoord[0][0] = dyn->s[0];
        VS_WALL(vis)->modTexCoord[0][1] = dyn->t[0];
        VS_WALL(vis)->modTexCoord[1][0] = dyn->s[1];
        VS_WALL(vis)->modTexCoord[1][1] = dyn->t[1];
        for(int c = 0; c < 4; ++c)
        {
            VS_WALL(vis)->modColor[c] = dyn->color.rgba[c];
        }
    }
    else
    {
        VS_WALL(vis)->modTex = 0;
    }
}

static void quadTexCoords(rtexcoord_t *tc, rvertex_t const *rverts,
    coord_t wallLength, Vector3d const &topLeft)
{
    tc[0].st[0] = tc[1].st[0] = rverts[0].pos[VX] - topLeft.x;
    tc[3].st[1] = tc[1].st[1] = rverts[0].pos[VY] - topLeft.y;
    tc[3].st[0] = tc[2].st[0] = tc[0].st[0] + wallLength;
    tc[2].st[1] = tc[3].st[1] + (rverts[1].pos[VZ] - rverts[0].pos[VZ]);
    tc[0].st[1] = tc[3].st[1] + (rverts[3].pos[VZ] - rverts[2].pos[VZ]);
}

static void quadLightCoords(rtexcoord_t *tc, float const s[2], float const t[2])
{
    tc[1].st[0] = tc[0].st[0] = s[0];
    tc[1].st[1] = tc[3].st[1] = t[0];
    tc[3].st[0] = tc[2].st[0] = s[1];
    tc[2].st[1] = tc[0].st[1] = t[1];
}

static float shinyVertical(float dy, float dx)
{
    return ((atan(dy/dx) / (PI/2)) + 1) / 2;
}

static void quadShinyTexCoords(rtexcoord_t *tc, rvertex_t const *topLeft,
    rvertex_t const *bottomRight, coord_t wallLength)
{
    vec2f_t surface, normal, projected, s, reflected, view;
    float distance, angle, prevAngle = 0;
    uint i;

    // Quad surface vector.
    V2f_Set(surface, (bottomRight->pos[VX] - topLeft->pos[VX]) / wallLength,
                     (bottomRight->pos[VY] - topLeft->pos[VY]) / wallLength);

    V2f_Set(normal, surface[VY], -surface[VX]);

    // Calculate coordinates based on viewpoint and surface normal.
    for(i = 0; i < 2; ++i)
    {
        // View vector.
        V2f_Set(view, vOrigin[VX] - (i == 0? topLeft->pos[VX] : bottomRight->pos[VX]),
                      vOrigin[VZ] - (i == 0? topLeft->pos[VY] : bottomRight->pos[VY]));

        distance = V2f_Normalize(view);

        V2f_Project(projected, view, normal);
        V2f_Subtract(s, projected, view);
        V2f_Scale(s, 2);
        V2f_Sum(reflected, view, s);

        angle = acos(reflected[VY]) / PI;
        if(reflected[VX] < 0)
        {
            angle = 1 - angle;
        }

        if(i == 0)
        {
            prevAngle = angle;
        }
        else
        {
            if(angle > prevAngle)
                angle -= 1;
        }

        // Horizontal coordinates.
        tc[ (i == 0 ? 1 : 2) ].st[0] = tc[ (i == 0 ? 0 : 3) ].st[0] =
            angle + .3f; /*acos(-dot)/PI*/

        tc[ (i == 0 ? 0 : 2) ].st[1] =
            shinyVertical(vOrigin[VY] - bottomRight->pos[VZ], distance);

        // Vertical coordinates.
        tc[ (i == 0 ? 1 : 3) ].st[1] =
            shinyVertical(vOrigin[VY] - topLeft->pos[VZ], distance);
    }
}

static void flatShinyTexCoords(rtexcoord_t *tc, float const xyz[3])
{
    float distance, offset;
    vec2f_t view, start;

    // View vector.
    V2f_Set(view, vOrigin[VX] - xyz[VX], vOrigin[VZ] - xyz[VY]);

    distance = V2f_Normalize(view);
    if(distance < 10)
    {
        // Too small distances cause an ugly 'crunch' below and above
        // the viewpoint.
        distance = 10;
    }

    // Offset from the normal view plane.
    V2f_Set(start, vOrigin[VX], vOrigin[VZ]);

    offset = ((start[VY] - xyz[VY]) * sin(.4f)/*viewFrontVec[VX]*/ -
              (start[VX] - xyz[VX]) * cos(.4f)/*viewFrontVec[VZ]*/);

    tc->st[0] = ((shinyVertical(offset, distance) - .5f) * 2) + .5f;
    tc->st[1] = shinyVertical(vOrigin[VY] - xyz[VZ], distance);
}

struct rendworldpoly_params_t
{
    bool            isWall;
    int             flags; /// @ref rendpolyFlags
    blendmode_t     blendMode;
    Vector3d const *texTL;
    Vector3d const *texBR;
    Vector2f const *materialOrigin;
    Vector2f const *materialScale;
    Vector3f const *normal; // Surface normal.
    float           alpha;
    float           surfaceLightLevelDL;
    float           surfaceLightLevelDR;
    Vector3f const *surfaceColor;

    uint            lightListIdx; // List of lights that affect this poly.
    uint            shadowListIdx; // List of shadows that affect this poly.
    float           glowing;
    bool            forceOpaque;

// For bias:
    MapElement     *mapElement;
    int             elmIdx;
    BiasSurface    *bsuf;

// Wall only:
    struct {
        coord_t sectionWidth;
        Vector3f const *surfaceColor2; // Secondary color.
        WallEdge const *leftEdge;
        WallEdge const *rightEdge;
    } wall;
};

static bool renderWorldPoly(rvertex_t *rvertices, uint numVertices,
    rendworldpoly_params_t const &p, MaterialSnapshot const &ms)
{
    DENG_ASSERT(rvertices != 0);

    uint const realNumVertices = (p.isWall? 3 + p.wall.leftEdge->divisionCount() + 3 + p.wall.rightEdge->divisionCount() : numVertices);
    bool const mustSubdivide = (p.isWall && (p.wall.leftEdge->divisionCount() || p.wall.rightEdge->divisionCount()));

    bool const skyMaskedMaterial = ((p.flags & RPF_SKYMASK) || (ms.material().isSkyMasked()));
    bool const drawAsVisSprite   = (!p.forceOpaque && !(p.flags & RPF_SKYMASK) && (!ms.isOpaque() || p.alpha < 1 || p.blendMode > 0));

    bool useLights = false, useShadows = false, hasDynlights = false;

    // Map RTU configuration from prepared MaterialSnapshot(s).
    rtexmapunit_t const *primaryRTU       = (!(p.flags & RPF_SKYMASK))? &ms.unit(RTU_PRIMARY) : NULL;
    rtexmapunit_t const *primaryDetailRTU = (r_detail && !(p.flags & RPF_SKYMASK) && ms.unit(RTU_PRIMARY_DETAIL).hasTexture())? &ms.unit(RTU_PRIMARY_DETAIL) : NULL;
    rtexmapunit_t const *interRTU         = (!(p.flags & RPF_SKYMASK) && ms.unit(RTU_INTER).hasTexture())? &ms.unit(RTU_INTER) : NULL;
    rtexmapunit_t const *interDetailRTU   = (r_detail && !(p.flags & RPF_SKYMASK) && ms.unit(RTU_INTER_DETAIL).hasTexture())? &ms.unit(RTU_INTER_DETAIL) : NULL;
    rtexmapunit_t const *shinyRTU         = (useShinySurfaces && !(p.flags & RPF_SKYMASK) && ms.unit(RTU_REFLECTION).hasTexture())? &ms.unit(RTU_REFLECTION) : NULL;
    rtexmapunit_t const *shinyMaskRTU     = (useShinySurfaces && !(p.flags & RPF_SKYMASK) && ms.unit(RTU_REFLECTION).hasTexture() && ms.unit(RTU_REFLECTION_MASK).hasTexture())? &ms.unit(RTU_REFLECTION_MASK) : NULL;

    ColorRawf *rcolors          = !skyMaskedMaterial? R_AllocRendColors(realNumVertices) : 0;
    rtexcoord_t *primaryCoords  = R_AllocRendTexCoords(realNumVertices);
    rtexcoord_t *interCoords    = interRTU? R_AllocRendTexCoords(realNumVertices) : 0;

    ColorRawf *shinyColors      = 0;
    rtexcoord_t *shinyTexCoords = 0;
    rtexcoord_t *modCoords      = 0;

    DGLuint modTex = 0;
    float modTexSt[2][2] = {{ 0, 0 }, { 0, 0 }};
    ColorRawf modColor;

    if(!skyMaskedMaterial)
    {
        // ShinySurface?
        if(shinyRTU && !drawAsVisSprite)
        {
            // We'll reuse the same verts but we need new colors.
            shinyColors = R_AllocRendColors(realNumVertices);
            // The normal texcoords are used with the mask.
            // New texcoords are required for shiny texture.
            shinyTexCoords = R_AllocRendTexCoords(realNumVertices);
        }

        if(p.glowing < 1)
        {
            useLights  = (p.lightListIdx ? true : false);
            useShadows = (p.shadowListIdx? true : false);

            /**
             * If multitexturing is enabled and there is at least one
             * dynlight affecting this surface, grab the parameters
             * needed to draw it.
             */
            if(useLights && RL_IsMTexLights())
            {
                dynlight_t *dyn = 0;
                LO_IterateProjections(p.lightListIdx, RIT_FirstDynlightIterator, (void *)&dyn);

                modCoords = R_AllocRendTexCoords(realNumVertices);

                modTex = dyn->texture;
                modColor.red   = dyn->color.red;
                modColor.green = dyn->color.green;
                modColor.blue  = dyn->color.blue;
                modTexSt[0][0] = dyn->s[0];
                modTexSt[0][1] = dyn->s[1];
                modTexSt[1][0] = dyn->t[0];
                modTexSt[1][1] = dyn->t[1];
            }
        }
    }

    if(p.isWall)
    {
        // Primary texture coordinates.
        quadTexCoords(primaryCoords, rvertices, p.wall.sectionWidth, *p.texTL);

        // Blend texture coordinates.
        if(interRTU && !drawAsVisSprite)
            quadTexCoords(interCoords, rvertices, p.wall.sectionWidth, *p.texTL);

        // Shiny texture coordinates.
        if(shinyRTU && !drawAsVisSprite)
            quadShinyTexCoords(shinyTexCoords, &rvertices[1], &rvertices[2], p.wall.sectionWidth);

        // First light texture coordinates.
        if(modTex && RL_IsMTexLights())
            quadLightCoords(modCoords, modTexSt[0], modTexSt[1]);
    }
    else
    {
        for(uint i = 0; i < numVertices; ++i)
        {
            rvertex_t const &vtx = rvertices[i];

            float const xyz[3] = {
                vtx.pos[VX] - float(p.texTL->x),
                vtx.pos[VY] - float(p.texTL->y),
                vtx.pos[VZ] - float(p.texTL->z)
            };

            // Primary texture coordinates.
            if(primaryRTU)
            {
                rtexcoord_t &tc = primaryCoords[i];
                tc.st[0] =  xyz[VX];
                tc.st[1] = -xyz[VY];
            }

            // Blend primary texture coordinates.
            if(interRTU)
            {
                rtexcoord_t &tc = interCoords[i];
                tc.st[0] =  xyz[VX];
                tc.st[1] = -xyz[VY];
            }

            // Shiny texture coordinates.
            if(shinyRTU)
            {
                flatShinyTexCoords(&shinyTexCoords[i], vtx.pos);
            }

            // First light texture coordinates.
            if(modTex && RL_IsMTexLights())
            {
                rtexcoord_t &tc = modCoords[i];
                float const width  = p.texBR->x - p.texTL->x;
                float const height = p.texBR->y - p.texTL->y;

                tc.st[0] = ((p.texBR->x - vtx.pos[VX]) / width  * modTexSt[0][0]) + (xyz[VX] / width  * modTexSt[0][1]);
                tc.st[1] = ((p.texBR->y - vtx.pos[VY]) / height * modTexSt[1][0]) + (xyz[VY] / height * modTexSt[1][1]);
            }
        }
    }

    // Light this polygon.
    if(!skyMaskedMaterial)
    {
        if(levelFullBright || !(p.glowing < 1))
        {
            // Uniform color. Apply to all vertices.
            float glowStrength = currentSectorLightLevel + (levelFullBright? 1 : p.glowing);
            Rend_VertexColorsGlow(rcolors, numVertices, glowStrength);
        }
        else
        {
            // Non-uniform color.
            if(useBias && p.bsuf)
            {
                // Do BIAS lighting for this poly.
                SB_RendPoly(rcolors, p.bsuf, rvertices, numVertices, *p.normal,
                            currentSectorLightLevel, p.mapElement, p.elmIdx);

                if(p.glowing > 0)
                {
                    for(uint i = 0; i < numVertices; ++i)
                    {
                        rcolors[i].rgba[CR] = de::clamp(0.f, rcolors[i].rgba[CR] + p.glowing, 1.f);
                        rcolors[i].rgba[CG] = de::clamp(0.f, rcolors[i].rgba[CG] + p.glowing, 1.f);
                        rcolors[i].rgba[CB] = de::clamp(0.f, rcolors[i].rgba[CB] + p.glowing, 1.f);
                    }
                }
            }
            else
            {
                float llL = de::clamp(0.f, currentSectorLightLevel + p.surfaceLightLevelDL + p.glowing, 1.f);
                float llR = de::clamp(0.f, currentSectorLightLevel + p.surfaceLightLevelDR + p.glowing, 1.f);

                // Calculate the color for each vertex, blended with plane color?
                if(p.surfaceColor->x < 1 || p.surfaceColor->y < 1 || p.surfaceColor->z < 1)
                {
                    // Blend sector light+color+surfacecolor
                    Vector3f vColor = (*p.surfaceColor) * currentSectorLightColor;

                    if(p.isWall && llL != llR)
                    {
                        lightVertex(rcolors[0], rvertices[0], llL, vColor);
                        lightVertex(rcolors[1], rvertices[1], llL, vColor);
                        lightVertex(rcolors[2], rvertices[2], llR, vColor);
                        lightVertex(rcolors[3], rvertices[3], llR, vColor);
                    }
                    else
                    {
                        lightVertices(numVertices, rcolors, rvertices, llL, vColor);
                    }
                }
                else
                {
                    // Use sector light+color only.
                    if(p.isWall && llL != llR)
                    {
                        lightVertex(rcolors[0], rvertices[0], llL, currentSectorLightColor);
                        lightVertex(rcolors[1], rvertices[1], llL, currentSectorLightColor);
                        lightVertex(rcolors[2], rvertices[2], llR, currentSectorLightColor);
                        lightVertex(rcolors[3], rvertices[3], llR, currentSectorLightColor);
                    }
                    else
                    {
                        lightVertices(numVertices, rcolors, rvertices, llL, currentSectorLightColor);
                    }
                }

                // Bottom color (if different from top)?
                if(p.isWall && p.wall.surfaceColor2)
                {
                    // Blend sector light+color+surfacecolor
                    Vector3f vColor = (*p.wall.surfaceColor2) * currentSectorLightColor;

                    lightVertex(rcolors[0], rvertices[0], llL, vColor);
                    lightVertex(rcolors[2], rvertices[2], llR, vColor);
                }
            }

            // Apply torch light?
            if(viewPlayer->shared.fixedColorMap)
            {
                torchLightVertices(numVertices, rcolors, rvertices);
            }
        }

        if(shinyRTU && !drawAsVisSprite)
        {
            // Strength of the shine.
            Vector3f const &minColor = ms.shineMinColor();
            for(uint i = 0; i < numVertices; ++i)
            {
                ColorRawf &color = shinyColors[i];
                color.rgba[CR] = de::max(rcolors[i].rgba[CR], minColor.x);
                color.rgba[CG] = de::max(rcolors[i].rgba[CG], minColor.y);
                color.rgba[CB] = de::max(rcolors[i].rgba[CB], minColor.z);
                color.rgba[CA] = shinyRTU->opacity;
            }
        }

        // Apply uniform alpha.
        Rend_VertexColorsAlpha(rcolors, numVertices, p.alpha);
    }

    if(useLights || useShadows)
    {
        /*
         * Surfaces lit by dynamic lights may need to be rendered differently
         * than non-lit surfaces. Determine the average light level of this rend
         * poly, if too bright; do not bother with lights.
         */
        float avgLightlevel = 0;
        for(uint i = 0; i < numVertices; ++i)
        {
            avgLightlevel += rcolors[i].rgba[CR];
            avgLightlevel += rcolors[i].rgba[CG];
            avgLightlevel += rcolors[i].rgba[CB];
        }
        avgLightlevel /= numVertices * 3;

        if(avgLightlevel > 0.98f)
        {
            useLights = false;
        }
        if(avgLightlevel < 0.02f)
        {
            useShadows = false;
        }
    }

    if(drawAsVisSprite)
    {
        DENG_ASSERT(p.isWall);

        /**
         * Masked polys (walls) get a special treatment (=> vissprite).
         * This is needed because all masked polys must be sorted (sprites
         * are masked polys). Otherwise there will be artifacts.
         */
        Rend_AddMaskedPoly(rvertices, rcolors, p.wall.sectionWidth, &ms.materialVariant(),
                           *p.materialOrigin, p.blendMode, p.lightListIdx, p.glowing);

        R_FreeRendTexCoords(primaryCoords);
        R_FreeRendColors(rcolors);
        R_FreeRendTexCoords(interCoords);
        R_FreeRendTexCoords(modCoords);
        R_FreeRendTexCoords(shinyTexCoords);
        R_FreeRendColors(shinyColors);

        return false; // We HAD to use a vissprite, so it MUST not be opaque.
    }

    if(useLights)
    {
        // Render all lights projected onto this surface.
        renderlightprojectionparams_t parm;

        zap(parm);
        parm.rvertices       = rvertices;
        parm.numVertices     = numVertices;
        parm.realNumVertices = realNumVertices;
        parm.lastIdx         = 0;
        parm.texTL           = p.texTL;
        parm.texBR           = p.texBR;
        parm.isWall          = p.isWall;
        if(p.isWall)
        {
            parm.wall.leftEdge  = p.wall.leftEdge;
            parm.wall.rightEdge = p.wall.rightEdge;
        }

        hasDynlights = (0 != Rend_RenderLightProjections(p.lightListIdx, parm));
    }

    if(useShadows)
    {
        // Render all shadows projected onto this surface.
        rendershadowprojectionparams_t parm;

        zap(parm);
        parm.rvertices       = rvertices;
        parm.numVertices     = numVertices;
        parm.realNumVertices = realNumVertices;
        parm.texTL           = p.texTL;
        parm.texBR           = p.texBR;
        parm.isWall          = p.isWall;
        if(p.isWall)
        {
            parm.wall.leftEdge  = p.wall.leftEdge;
            parm.wall.rightEdge = p.wall.rightEdge;
        }

        Rend_RenderShadowProjections(p.shadowListIdx, parm);
    }

    // Map RTU state from the prepared texture units in the MaterialSnapshot.
    RL_LoadDefaultRtus();
    RL_MapRtu(RTU_PRIMARY,         primaryRTU);
    RL_MapRtu(RTU_PRIMARY_DETAIL,  primaryDetailRTU);
    RL_MapRtu(RTU_INTER,           interRTU);
    RL_MapRtu(RTU_INTER_DETAIL,    interDetailRTU);
    RL_MapRtu(RTU_REFLECTION,      shinyRTU);
    RL_MapRtu(RTU_REFLECTION_MASK, shinyMaskRTU);

    if(primaryRTU)
    {
        if(p.materialOrigin) RL_Rtu_TranslateOffset(RTU_PRIMARY, *p.materialOrigin);
        if(p.materialScale)  RL_Rtu_ScaleST(RTU_PRIMARY, *p.materialScale);
    }

    if(primaryDetailRTU)
    {
        if(p.materialOrigin) RL_Rtu_TranslateOffset(RTU_PRIMARY_DETAIL, *p.materialOrigin);
    }

    if(interRTU)
    {
        if(p.materialOrigin) RL_Rtu_TranslateOffset(RTU_INTER, *p.materialOrigin);
        if(p.materialScale)  RL_Rtu_ScaleST(RTU_INTER, *p.materialScale);
    }

    if(interDetailRTU)
    {
        if(p.materialOrigin) RL_Rtu_TranslateOffset(RTU_INTER_DETAIL, *p.materialOrigin);
    }

    if(shinyMaskRTU)
    {
        if(p.materialOrigin) RL_Rtu_TranslateOffset(RTU_REFLECTION_MASK, *p.materialOrigin);
        if(p.materialScale)  RL_Rtu_ScaleST(RTU_REFLECTION_MASK, *p.materialScale);
    }

    // Write multiple polys depending on rend params.
    if(mustSubdivide)
    {
        WallEdge const &leftEdge = *p.wall.leftEdge;
        WallEdge const &rightEdge = *p.wall.rightEdge;

        /*
         * Need to swap indices around into fans set the position
         * of the division vertices, interpolate texcoords and
         * color.
         */

        rvertex_t origVerts[4];
        std::memcpy(origVerts, rvertices, sizeof(origVerts));

        rtexcoord_t origTexCoords[4];
        std::memcpy(origTexCoords, primaryCoords, sizeof(origTexCoords));

        ColorRawf origColors[4];
        if(rcolors || shinyColors)
        {
            std::memcpy(origColors, rcolors, sizeof(origColors));
        }

        R_DivVerts(rvertices, origVerts, leftEdge, rightEdge);
        R_DivTexCoords(primaryCoords, origTexCoords, leftEdge, rightEdge);

        if(rcolors)
        {
            R_DivVertColors(rcolors, origColors, leftEdge, rightEdge);
        }

        if(interCoords)
        {
            rtexcoord_t origTexCoords2[4];
            std::memcpy(origTexCoords2, interCoords, sizeof(origTexCoords2));
            R_DivTexCoords(interCoords, origTexCoords2, leftEdge, rightEdge);
        }

        if(modCoords)
        {
            rtexcoord_t origTexCoords5[4];
            std::memcpy(origTexCoords5, modCoords, sizeof(origTexCoords5));
            R_DivTexCoords(modCoords, origTexCoords5, leftEdge, rightEdge);
        }

        if(shinyTexCoords)
        {
            rtexcoord_t origShinyTexCoords[4];
            std::memcpy(origShinyTexCoords, shinyTexCoords, sizeof(origShinyTexCoords));
            R_DivTexCoords(shinyTexCoords, origShinyTexCoords, leftEdge, rightEdge);
        }

        if(shinyColors)
        {
            ColorRawf origShinyColors[4];
            std::memcpy(origShinyColors, shinyColors, sizeof(origShinyColors));
            R_DivVertColors(shinyColors, origShinyColors, leftEdge, rightEdge);
        }

        RL_AddPolyWithCoordsModulationReflection(PT_FAN, p.flags | (hasDynlights? RPF_HAS_DYNLIGHTS : 0),
                                                 3 + rightEdge.divisionCount(),
                                                 rvertices + 3 + leftEdge.divisionCount(),
                                                 rcolors? rcolors + 3 + leftEdge.divisionCount() : 0,
                                                 primaryCoords + 3 + leftEdge.divisionCount(),
                                                 interCoords? interCoords + 3 + leftEdge.divisionCount() : 0,
                                                 modTex, &modColor, modCoords? modCoords + 3 + leftEdge.divisionCount() : 0,
                                                 shinyColors + 3 + leftEdge.divisionCount(),
                                                 shinyTexCoords? shinyTexCoords + 3 + leftEdge.divisionCount() : 0,
                                                 shinyMaskRTU? primaryCoords + 3 + leftEdge.divisionCount() : 0);

        RL_AddPolyWithCoordsModulationReflection(PT_FAN, p.flags | (hasDynlights? RPF_HAS_DYNLIGHTS : 0),
                                                 3 + leftEdge.divisionCount(),
                                                 rvertices, rcolors, primaryCoords, interCoords,
                                                 modTex, &modColor, modCoords,
                                                 shinyColors, shinyTexCoords,
                                                 shinyMaskRTU? primaryCoords : 0);
    }
    else
    {
        RL_AddPolyWithCoordsModulationReflection(p.isWall? PT_TRIANGLE_STRIP : PT_FAN,
                                                 p.flags | (hasDynlights? RPF_HAS_DYNLIGHTS : 0),
                                                 numVertices, rvertices, rcolors,
                                                 primaryCoords, interCoords,
                                                 modTex, &modColor, modCoords,
                                                 shinyColors, shinyTexCoords, shinyMaskRTU? primaryCoords : 0);
    }

    R_FreeRendTexCoords(primaryCoords);
    R_FreeRendTexCoords(interCoords);
    R_FreeRendTexCoords(modCoords);
    R_FreeRendTexCoords(shinyTexCoords);
    R_FreeRendColors(rcolors);
    R_FreeRendColors(shinyColors);

    return (p.forceOpaque || skyMaskedMaterial ||
            !(p.alpha < 1 || !ms.isOpaque() || p.blendMode > 0));
}

/**
 * Fade the specified @a opacity value to fully transparent the closer the view
 * player is to the geometry.
 *
 * @note When the viewer is close enough we should NOT try to occlude with this
 * section in the angle clipper, otherwise HOM would occur when directly on top
 * of the wall (e.g., passing through an opaque waterfall).
 *
 * @return  @c true= fading was applied (see above note), otherwise @c false.
 */
static bool nearFadeOpacity(WallEdge const &leftEdge, WallEdge const &rightEdge,
                            float &opacity)
{
    if(vOrigin[VY] < leftEdge.bottom().z() || vOrigin[VY] > rightEdge.top().z())
        return false;

    mobj_t const *mo = viewPlayer->shared.mo;

    Line const &line = leftEdge.mapSide().line();
    coord_t linePoint[2]     = { line.fromOrigin().x, line.fromOrigin().y };
    coord_t lineDirection[2] = {  line.direction().x,  line.direction().y };
    vec2d_t result;
    double pos = V2d_ProjectOnLine(result, mo->origin, linePoint, lineDirection);

    if(!(pos > 0 && pos < 1))
        return false;

    coord_t const maxDistance = mo->radius * .8f;

    Vector2d delta = Vector2d(result) - Vector2d(mo->origin);
    coord_t distance = delta.length();

    if(de::abs(distance) > maxDistance)
        return false;

    if(distance > 0)
    {
        opacity = (opacity / maxDistance) * distance;
        opacity = de::clamp(0.f, opacity, 1.f);
    }

    return true;
}

/**
 * The DOOM lighting model applies a sector light level delta when drawing
 * walls based on their 2D world angle.
 */
static float calcLightLevelDelta(Vector3f const &normal)
{
    return (1.0f / 255) * (normal.x * 18) * rendLightWallAngle;
}

static void wallSectionLightLevelDeltas(WallEdge const &leftEdge, WallEdge const &rightEdge,
    float &leftDelta, float &rightDelta)
{
    leftDelta = calcLightLevelDelta(leftEdge.normal());

    if(leftEdge.normal() == rightEdge.normal())
    {
        rightDelta = leftDelta;
    }
    else
    {
        rightDelta = calcLightLevelDelta(rightEdge.normal());

        // Linearly interpolate to find the light level delta values for the
        // vertical edges of this wall section.
        coord_t const lineLength = leftEdge.mapSide().line().length();
        coord_t const sectionOffset = leftEdge.mapSideOffset();
        coord_t const sectionWidth = de::abs(Vector2d(rightEdge.origin() - leftEdge.origin()).length());

        float deltaDiff = rightDelta - leftDelta;
        rightDelta = leftDelta + ((sectionOffset + sectionWidth) / lineLength) * deltaDiff;
        leftDelta += (sectionOffset / lineLength) * deltaDiff;
    }
}

/**
 * Project lights for the given map @a surface.
 *
 * @pre currentBspLeaf is set to the BSP leaf which "contains" the surface.
 *
 * @param surface          Map surface to project lights onto.
 * @param glowStrength     Surface glow strength (glow scale factor(s) are assumed to
 *                         have already been applied).
 * @param topLeft          Top left coordinates for the conceptual quad used for projection.
 * @param bottomRight      Bottom right coordinates for the conceptual quad used for projection.
 * @param sortProjections  @c true= instruct the projection algorithm to sort the resultant
 *                         projections by descending luminosity. Default = @c false.
 *
 * @return  Identifier for the resultant light projection list; otherwise @c 0.
 *
 * @see LO_ProjectToSurface()
 */
static uint projectSurfaceLights(Surface &surface, float glowStrength,
    Vector3d const &topLeft, Vector3d const &bottomRight, bool sortProjections = false)
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    // Is light projection disabled?
    if(glowStrength >= 1) return 0;
    if(!useDynLights && !useWallGlow) return 0;

    return LO_ProjectToSurface(0 | (sortProjections? PLF_SORT_LUMINOSITY_DESC : 0), leaf,
                               1, topLeft, bottomRight,
                               surface.tangent(), surface.bitangent(), surface.normal());
}

/**
 * Project shadows for the given map @a surface.
 *
 * @pre currentBspLeaf is set to the BSP leaf which "contains" the surface.
 *
 * @param surface       Map surface to project shadows onto.
 * @param glowStrength  Surface glow strength (glow scale factor(s) are assumed to
 *                      have already been applied).
 * @param topLeft       Top left coordinates for the conceptual quad used for projection.
 * @param bottomRight   Bottom right coordinates for the conceptual quad used for projection.
 *
 * @return  Identifier for the resultant shadow projection list; otherwise @c 0.
 *
 * @see R_ProjectShadowsToSurface()
 */
static uint projectSurfaceShadows(Surface &surface, float glowStrength,
    Vector3d const &topLeft, Vector3d const &bottomRight)
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    // Is shadow projection disabled?
    if(glowStrength >= 1) return 0;
    if(!Rend_MobjShadowsEnabled()) return 0;

    // Glow inversely diminishes shadow strength.
    float const shadowStrength = 1 - glowStrength;

    return R_ProjectShadowsToSurface(leaf, shadowStrength, topLeft, bottomRight,
                                     surface.tangent(), surface.bitangent(), surface.normal());
}

static void writeWallSection(Segment &segment, int section,
    bool *retWroteOpaque = 0, coord_t *retBottomZ = 0, coord_t *retTopZ = 0)
{
    DENG_ASSERT(!isNullLeaf(currentBspLeaf));
    DENG_ASSERT(segment.isFlagged(Segment::FacingFront));
    DENG_ASSERT(segment.hasLineSide() && segment.lineSide().hasSections());

    if(retWroteOpaque) *retWroteOpaque = false;
    if(retBottomZ)     *retBottomZ     = 0;
    if(retTopZ)        *retTopZ        = 0;

    Line::Side &side = segment.lineSide();
    Surface &surface = side.surface(section);

    // Skip nearly transparent surfaces.
    float opacity = surface.opacity();
    if(opacity < .001f)
        return;

    // Determine which Material to use.
    Material *material = Rend_ChooseMapSurfaceMaterial(surface);

    // A drawable material is required.
    if(!material || !material->isDrawable())
        return;

    // Generate edge geometries.
    WallSpec const wallSpec = WallSpec::fromMapSide(side, section);

    WallEdge leftEdge(wallSpec, segment, Line::From);
    WallEdge rightEdge(wallSpec, segment, Line::To);

    // Do the edge geometries describe a valid polygon?
    if(!leftEdge.isValid() || !rightEdge.isValid() ||
       de::fequal(leftEdge.bottom().z(), rightEdge.top().z()))
        return;

    // Apply a fade out when the viewer is near to this geometry?
    bool didNearFade = false;
    if(wallSpec.flags.testFlag(WallSpec::NearFade))
    {
        didNearFade = nearFadeOpacity(leftEdge, rightEdge, opacity);
    }

    bool wroteOpaque = false;
    if(opacity >= .001f)
    {
        bool const skyMasked       = material->isSkyMasked() && !devRendSkyMode;
        bool const twoSidedMiddle  = (wallSpec.section == Line::Side::Middle && !side.considerOneSided());

        MaterialSnapshot const &ms = material->prepare(Rend_MapSurfaceMaterialSpec());

        Vector2f const materialScale((surface.flags() & DDSUF_MATERIAL_FLIPH)? -1 : 1,
                                     (surface.flags() & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        Vector3d const texQuad[]   = { leftEdge.top().origin(), rightEdge.bottom().origin() };

        rendworldpoly_params_t parm; zap(parm);

        parm.flags               = RPF_DEFAULT | (skyMasked? RPF_SKYMASK : 0);
        parm.forceOpaque         = wallSpec.flags.testFlag(WallSpec::ForceOpaque);
        parm.alpha               = parm.forceOpaque? 1 : opacity;
        parm.mapElement          = &segment;
        parm.elmIdx              = wallSpec.section;
        parm.bsuf                = &segment.biasSurface(wallSpec.section);
        parm.normal              = &surface.normal();
        parm.texTL               = &texQuad[0];
        parm.texBR               = &texQuad[1];

        // Calculate the light level deltas for this wall section?
        if(!wallSpec.flags.testFlag(WallSpec::NoLightDeltas))
        {
            wallSectionLightLevelDeltas(leftEdge, rightEdge,
                                        parm.surfaceLightLevelDL, parm.surfaceLightLevelDR);
        }

        parm.blendMode           = BM_NORMAL;
        Vector2f materialOrigin  = leftEdge.materialOrigin();
        parm.materialOrigin      = &materialOrigin;
        parm.materialScale       = &materialScale;

        parm.isWall = true;
        parm.wall.sectionWidth   = de::abs(Vector2d(rightEdge.origin() - leftEdge.origin()).length());
        parm.wall.leftEdge       = &leftEdge;
        parm.wall.rightEdge      = &rightEdge;

        if(!skyMasked)
        {
            if(twoSidedMiddle)
            {
                parm.blendMode = surface.blendMode();
                if(parm.blendMode == BM_NORMAL && noSpriteTrans)
                    parm.blendMode = BM_ZEROALPHA; // "no translucency" mode
            }

            if(glowFactor > .0001f)
            {
                if(material == surface.materialPtr())
                {
                    parm.glowing = ms.glowStrength();
                }
                else
                {
                    Material *actualMaterial =
                        surface.hasMaterial()? surface.materialPtr()
                                             : &App_Materials().find(de::Uri("System", Path("missing"))).material();

                    MaterialSnapshot const &ms = actualMaterial->prepare(Rend_MapSurfaceMaterialSpec());
                    parm.glowing = ms.glowStrength();
                }

                parm.glowing *= glowFactor; // Global scale factor.
            }

            side.chooseSurfaceTintColors(wallSpec.section, &parm.surfaceColor, &parm.wall.surfaceColor2);
        }

        // Project dynamic Lights?
        if(!wallSpec.flags.testFlag(WallSpec::NoDynLights) && !skyMasked)
        {
            parm.lightListIdx =
                projectSurfaceLights(surface, parm.glowing, texQuad[0], texQuad[1],
                                     wallSpec.flags.testFlag(WallSpec::SortDynLights));
        }

        // Project dynamic shadows?
        if(!wallSpec.flags.testFlag(WallSpec::NoDynShadows) && !skyMasked)
        {
            parm.shadowListIdx =
                projectSurfaceShadows(surface, parm.glowing, texQuad[0], texQuad[1]);
        }

        /*
         * Geometry write/drawing begins.
         */

        if(twoSidedMiddle && side.sectorPtr() != currentBspLeaf->sectorPtr())
        {
            // Temporarily modify the draw state.
            currentSectorLightColor = Rend_SectorLightColor(side.sector());
            currentSectorLightLevel = side.sector().lightLevel();
        }

        // Allocate enough vertices for the divisions too.
        rvertex_t *rvertices;
        if(leftEdge.divisionCount() || rightEdge.divisionCount())
        {
            // Use two fans.
            rvertices = R_AllocRendVertices(3 + leftEdge.divisionCount() +
                                            3 + rightEdge.divisionCount());
        }
        else
        {
            // Use a quad.
            rvertices = R_AllocRendVertices(4);
        }

        // Vertex coords.
        rvertex_t &bottomLeft  = rvertices[0];
        rvertex_t &topLeft     = rvertices[1];
        rvertex_t &bottomRight = rvertices[2];
        rvertex_t &topRight    = rvertices[3];

        V3f_Set(bottomLeft.pos,   leftEdge.bottom().origin().x,  leftEdge.bottom().origin().y,  leftEdge.bottom().origin().z);
        V3f_Set(   topLeft.pos,      leftEdge.top().origin().x,     leftEdge.top().origin().y,     leftEdge.top().origin().z);

        V3f_Set(bottomRight.pos, rightEdge.bottom().origin().x, rightEdge.bottom().origin().y, rightEdge.bottom().origin().z);
        V3f_Set(   topRight.pos,    rightEdge.top().origin().x,    rightEdge.top().origin().y,    rightEdge.top().origin().z);

        // Draw this section.
        wroteOpaque = renderWorldPoly(rvertices, 4, parm, ms);
        if(wroteOpaque)
        {
            // Render FakeRadio for this section?
            if(!wallSpec.flags.testFlag(WallSpec::NoFakeRadio) && !skyMasked &&
               !(parm.glowing > 0) && currentSectorLightLevel > 0)
            {
                Rend_RadioUpdateForLineSide(side);

                // Determine the shadow properties.
                /// @todo Make cvars out of constants.
                float shadowSize = 2 * (8 + 16 - currentSectorLightLevel * 16);
                float shadowDark = Rend_RadioCalcShadowDarkness(currentSectorLightLevel);

                Rend_RadioWallSection(leftEdge, rightEdge, shadowDark, shadowSize);
            }
        }

        if(twoSidedMiddle && side.sectorPtr() != currentBspLeaf->sectorPtr())
        {
            // Undo temporary draw state changes.
            currentSectorLightColor = Rend_SectorLightColor(currentBspLeaf->sector());
            currentSectorLightLevel = currentBspLeaf->sector().lightLevel();
        }

        R_FreeRendVertices(rvertices);
    }

    if(retWroteOpaque) *retWroteOpaque = wroteOpaque && !didNearFade;
    if(retBottomZ)     *retBottomZ     = leftEdge.bottom().z();
    if(retTopZ)        *retTopZ        = rightEdge.top().z();
}

/**
 * Prepare the trifan rvertex_t buffer specified according to the edges of this
 * BSP leaf. If a fan base HEdge has been chosen it will be used as the center of
 * the trifan, else the mid point of this leaf will be used instead.
 *
 * @param leaf  BspLeaf instance.
 * @param direction  Vertex winding direction.
 * @param height  Z map space height coordinate to be set for each vertex.
 * @param verts  Built vertices are written here.
 * @param vertsSize  Number of built vertices is written here. Can be @c NULL.
 *
 * @return  Number of built vertices (same as written to @a vertsSize).
 */
static uint buildLeafPlaneGeometry(BspLeaf const &leaf, ClockDirection direction,
    coord_t height, rvertex_t **verts, uint *vertsSize)
{
    DENG_ASSERT(!leaf.isDegenerate());
    DENG_ASSERT(verts != 0);

    Face const &face = leaf.poly();

    HEdge *fanBase  = leaf.fanBase();
    uint totalVerts = face.hedgeCount() + (!fanBase? 2 : 0);

    *verts = R_AllocRendVertices(totalVerts);

    int n = 0;
    if(!fanBase)
    {
        V3f_Set((*verts)[n].pos, face.center().x, face.center().y, height);
        n++;
    }

    // Add the vertices for each hedge.
    HEdge *baseNode = fanBase? fanBase : face.hedge();
    HEdge *node = baseNode;
    do
    {
        V3f_Set((*verts)[n].pos, node->origin().x, node->origin().y, height);
        n++;
    } while((node = &node->neighbor(direction)) != baseNode);

    // The last vertex is always equal to the first.
    if(!fanBase)
    {
        V3f_Set((*verts)[n].pos, face.hedge()->origin().x, face.hedge()->origin().y, height);
    }

    if(vertsSize) *vertsSize = totalVerts;
    return totalVerts;
}

static void writeLeafPlane(Plane &plane)
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    Face const &face = leaf->poly();

    Surface const &surface = plane.surface();
    Vector3f eyeToSurface(vOrigin[VX] - face.center().x,
                          vOrigin[VZ] - face.center().y,
                          vOrigin[VY] - plane.visHeight());

    // Skip planes facing away from the viewer.
    if(eyeToSurface.dot(surface.normal()) < 0)
        return;

    // Determine which Material to use.
    Material *material = Rend_ChooseMapSurfaceMaterial(surface);

    // A drawable material is required.
    if(!material || !material->isDrawable())
        return;

    // Skip planes with a sky-masked material?
    if(!devRendSkyMode)
    {
        if(surface.hasSkyMaskedMaterial() && plane.indexInSector() <= Sector::Ceiling)
            return; // Not handled here (drawn with the mask geometry).
    }

    Vector2f materialOrigin = leaf->worldGridOffset() // Align to the worldwide grid.
                            + surface.visMaterialOrigin();

    // Add the Y offset to orient the Y flipped material.
    /// @todo fixme: What is this meant to do? -ds
    if(plane.indexInSector() == Sector::Ceiling)
        materialOrigin.y -= face.aaBox().maxY - face.aaBox().minY;
    materialOrigin.y = -materialOrigin.y;

    Vector2f materialScale((surface.flags() & DDSUF_MATERIAL_FLIPH)? -1 : 1,
                           (surface.flags() & DDSUF_MATERIAL_FLIPV)? -1 : 1);

    // Set the texture origin, Y is flipped for the ceiling.
    Vector3d texTL(face.aaBox().minX,
                   face.aaBox().arvec2[plane.indexInSector() == Sector::Floor? 1 : 0][VY],
                   plane.visHeight());
    Vector3d texBR(face.aaBox().maxX,
                   face.aaBox().arvec2[plane.indexInSector() == Sector::Floor? 0 : 1][VY],
                   plane.visHeight());

    rendworldpoly_params_t parm; zap(parm);

    parm.flags               = RPF_DEFAULT;
    parm.isWall              = false;
    parm.mapElement          = leaf;
    parm.elmIdx              = plane.indexInSector();
    parm.bsuf                = &leaf->biasSurface(plane.indexInSector());
    parm.normal              = &surface.normal();
    parm.texTL               = &texTL;
    parm.texBR               = &texBR;
    parm.surfaceLightLevelDL = parm.surfaceLightLevelDR = 0;
    parm.surfaceColor        = &surface.tintColor();
    parm.materialOrigin      = &materialOrigin;
    parm.materialScale       = &materialScale;

    if(material->isSkyMasked())
    {
        // In devRendSkyMode mode we render all polys destined for the
        // skymask as regular world polys (with a few obvious properties).
        if(devRendSkyMode)
        {
            parm.blendMode = BM_NORMAL;
            parm.forceOpaque = true;
        }
        else
        {   // We'll mask this.
            parm.flags |= RPF_SKYMASK;
        }
    }
    else if(plane.indexInSector() <= Sector::Ceiling)
    {
        parm.blendMode = BM_NORMAL;
        parm.forceOpaque = true;
    }
    else
    {
        parm.blendMode = surface.blendMode();
        if(parm.blendMode == BM_NORMAL && noSpriteTrans)
            parm.blendMode = BM_ZEROALPHA; // "no translucency" mode

        parm.alpha = surface.opacity();
    }

    uint numVertices;
    rvertex_t *rvertices;
    buildLeafPlaneGeometry(*leaf, (plane.indexInSector() == Sector::Ceiling)? Anticlockwise : Clockwise,
                           plane.visHeight(),
                           &rvertices, &numVertices);

    MaterialSnapshot const &ms = material->prepare(Rend_MapSurfaceMaterialSpec());

    if(!(parm.flags & RPF_SKYMASK))
    {
        if(glowFactor > .0001f)
        {
            if(material == surface.materialPtr())
            {
                parm.glowing = ms.glowStrength();
            }
            else
            {
                Material *actualMaterial = surface.hasMaterial()? surface.materialPtr()
                                                                : &App_Materials().find(de::Uri("System", Path("missing"))).material();

                MaterialSnapshot const &ms = actualMaterial->prepare(Rend_MapSurfaceMaterialSpec());
                parm.glowing = ms.glowStrength();
            }

            parm.glowing *= glowFactor; // Global scale factor.
        }

        // Dynamic lights?
        if(parm.glowing < 1 && !(!useDynLights && !useWallGlow))
        {
            /// @ref projectLightFlags
            int plFlags = (PLF_NO_PLANE | (plane.indexInSector() == Sector::Floor? PLF_TEX_FLOOR : PLF_TEX_CEILING));

            parm.lightListIdx =
                LO_ProjectToSurface(plFlags, leaf, 1, *parm.texTL, *parm.texBR,
                                    surface.tangent(), surface.bitangent(), surface.normal());
        }

        // Mobj shadows?
        if(plane.indexInSector() == Sector::Floor && parm.glowing < 1 && Rend_MobjShadowsEnabled())
        {
            // Glowing planes inversely diminish shadow strength.
            float blendFactor = 1 - parm.glowing;

            parm.shadowListIdx =
                R_ProjectShadowsToSurface(leaf, blendFactor, *parm.texTL, *parm.texBR,
                                          surface.tangent(), surface.bitangent(), surface.normal());
        }
    }

    renderWorldPoly(rvertices, numVertices, parm, ms);

    R_FreeRendVertices(rvertices);
}

static void writeSkyFixStrip(int numElements, rvertex_t const *positions,
    rtexcoord_t const *texcoords, Material *material)
{
    DENG_ASSERT(positions != 0);

    int const rendPolyFlags = RPF_DEFAULT | (!devRendSkyMode? RPF_SKYMASK : 0);
    if(!devRendSkyMode)
    {
        RL_AddPoly(PT_TRIANGLE_STRIP, rendPolyFlags, numElements, positions, 0);
    }
    else
    {
        DENG_ASSERT(texcoords != 0);

        if(renderTextures != 2)
        {
            DENG_ASSERT(material != 0);

            // Map RTU configuration from the sky surface material.
            MaterialSnapshot const &ms = material->prepare(Rend_MapSurfaceMaterialSpec());
            RL_LoadDefaultRtus();
            RL_MapRtu(RTU_PRIMARY, &ms.unit(RTU_PRIMARY));
        }

        RL_AddPolyWithCoords(PT_TRIANGLE_STRIP, rendPolyFlags, numElements,
                             positions, NULL, texcoords, NULL);
    }
}

static void writeLeafSkyMaskStrips(SkyFixEdge::FixType fixType)
{
    BspLeaf *bspLeaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(bspLeaf));

    // Determine strip generation behavior.
    ClockDirection direction         = Clockwise;
    bool const buildTexCoords        = CPP_BOOL(devRendSkyMode);
    bool const splitOnMaterialChange = (devRendSkyMode && renderTextures != 2);

    // Configure the strip builder wrt vertex attributes.
    TriangleStripBuilder stripBuilder(buildTexCoords);

    // Configure the strip build state (we'll most likely need to break
    // edge loop into multiple strips).
    Segment *startNode        = 0;
    coord_t startZBottom      = 0;
    coord_t startZTop         = 0;
    Material *startMaterial   = 0;
    float startMaterialOffset = 0;

    // Determine the relative sky plane (for monitoring material changes).
    int relPlane = fixType == SkyFixEdge::Upper? Sector::Ceiling : Sector::Floor;

    // Begin generating geometry.
    BspLeaf::Segments const &segments = bspLeaf->clockwiseSegments();
    int const segCount = segments.count();
    int i = 0;
    forever
    {
        Segment &seg = *segments.at(i);

        // Are we monitoring material changes?
        Material *skyMaterial = 0;
        if(splitOnMaterialChange)
        {
            skyMaterial = seg.sector().planeSurface(relPlane).materialPtr();
        }

        // Add a first (left) edge to the current strip?
        if(startNode == 0)
        {
            startMaterialOffset = seg.lineSideOffset();

            // Prepare the edge geometry
            SkyFixEdge skyEdge(seg, fixType, (direction == Anticlockwise)? Line::To : Line::From,
                               startMaterialOffset);

            if(skyEdge.isValid() && skyEdge.bottom().z() < skyEdge.top().z())
            {
                // A new strip begins.
                stripBuilder.begin(direction);
                stripBuilder << skyEdge;

                // Update the strip build state.
                startNode     = &seg;
                startZBottom  = skyEdge.bottom().z();
                startZTop     = skyEdge.top().z();
                startMaterial = skyMaterial;
            }
        }

        bool beginNewStrip = false;

        // Add the i'th (right) edge to the current strip?
        if(startNode != 0)
        {
            startMaterialOffset += seg.length() * (direction == Anticlockwise? -1 : 1);

            // Prepare the edge geometry
            SkyFixEdge skyEdge(seg, fixType, (direction == Anticlockwise)? Line::From : Line::To,
                               startMaterialOffset);

            // Stop if we've reached a "null" edge.
            bool endStrip = false;
            if(!(skyEdge.isValid() && skyEdge.bottom().z() < skyEdge.top().z()))
            {
                endStrip = true;
            }
            // Must we split the strip here?
            else if(&seg != startNode &&
                    (!de::fequal(skyEdge.bottom().z(), startZBottom) ||
                     !de::fequal(skyEdge.top().z(), startZTop) ||
                     (splitOnMaterialChange && skyMaterial != startMaterial)))
            {
                endStrip = true;
                beginNewStrip = true; // We'll continue from here.
            }
            else
            {
                // Extend the strip geometry.
                stripBuilder << skyEdge;
            }

            if(endStrip || de::wrap(i + (direction == Clockwise? 1 : -1), 0, segCount) == 0)
            {
                // End the current strip.
                startNode = 0;

                // Take ownership of the built geometry.
                PositionBuffer *positions = 0;
                TexCoordBuffer *texcoords = 0;
                int numVerts = stripBuilder.take(&positions, &texcoords);

                // Write the strip geometry to the render lists.
                writeSkyFixStrip(numVerts, positions->constData(),
                                 texcoords? texcoords->constData() : 0, startMaterial);

                delete positions;
                delete texcoords;
            }
        }

        // Start a new strip from the current node?
        if(beginNewStrip) continue;

        // On to the next node.
        i = de::wrap(i + (direction == Clockwise? 1 : -1), 0, segCount);

        // Are we done?
        if(i == 0) break;
    }
}

/**
 * @defgroup skyCapFlags  Sky Cap Flags
 * @ingroup flags
 */
///@{
#define SKYCAP_LOWER                0x1
#define SKYCAP_UPPER                0x2
///@}

static coord_t skyPlaneZ(BspLeaf *bspLeaf, int skyCap)
{
    DENG_ASSERT(bspLeaf);
    int const relPlane = (skyCap & SKYCAP_UPPER)? Sector::Ceiling : Sector::Floor;
    if(!bspLeaf->hasSector() || !P_IsInVoid(viewPlayer))
        return bspLeaf->map().skyFix(relPlane == Sector::Ceiling);
    return bspLeaf->sector().plane(relPlane).visHeight();
}

/// @param skyCap  @ref skyCapFlags.
static void writeLeafSkyMaskCap(int skyCap)
{
    BspLeaf *bspLeaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(bspLeaf));

    // Caps are unnecessary in sky debug mode (will be drawn as regular planes).
    if(devRendSkyMode) return;
    if(!skyCap) return;

    rvertex_t *verts;
    uint numVerts;
    buildLeafPlaneGeometry(*bspLeaf, (skyCap & SKYCAP_UPPER)? Anticlockwise : Clockwise,
                           skyPlaneZ(bspLeaf, skyCap),
                           &verts, &numVerts);

    RL_AddPoly(PT_FAN, RPF_DEFAULT | RPF_SKYMASK, numVerts, verts, 0);
    R_FreeRendVertices(verts);
}

/// @param skyCap  @ref skyCapFlags
static void writeLeafSkyMask(int skyCap = SKYCAP_LOWER|SKYCAP_UPPER)
{
    BspLeaf *bspLeaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(bspLeaf));

    // Any work to do?
    if(!bspLeaf->sector().hasSkyMaskedPlane())
        return;

    // Sky caps are only necessary in sectors with sky-masked planes.
    if((skyCap & SKYCAP_LOWER) && !bspLeaf->sector().floorSurface().hasSkyMaskedMaterial())
        skyCap &= ~SKYCAP_LOWER;
    if((skyCap & SKYCAP_UPPER) && !bspLeaf->sector().ceilingSurface().hasSkyMaskedMaterial())
        skyCap &= ~SKYCAP_UPPER;

    if(!skyCap) return;

    if(!devRendSkyMode || renderTextures == 2)
    {
        // All geometry uses the same RTU write state.
        RL_LoadDefaultRtus();
        if(renderTextures == 2)
        {
            // Map RTU configuration from the special "gray" material.
            MaterialSnapshot const &ms =
                App_Materials().find(de::Uri("System", Path("gray")))
                    .material().prepare(Rend_MapSurfaceMaterialSpec());
            RL_MapRtu(RTU_PRIMARY, &ms.unit(RTU_PRIMARY));
        }
    }

    // Lower?
    if(skyCap & SKYCAP_LOWER)
    {
        writeLeafSkyMaskStrips(SkyFixEdge::Lower);
        writeLeafSkyMaskCap(SKYCAP_LOWER);
    }

    // Upper?
    if(skyCap & SKYCAP_UPPER)
    {
        writeLeafSkyMaskStrips(SkyFixEdge::Upper);
        writeLeafSkyMaskCap(SKYCAP_UPPER);
    }
}

static bool coveredOpenRange(Segment &segment, coord_t middleBottomZ, coord_t middleTopZ,
    bool wroteOpaqueMiddle)
{
    if(segment.lineSide().considerOneSided())
    {
        return wroteOpaqueMiddle;
    }

    //if(segment.line().isSelfReferencing())
    //   return false; /// @todo Why? -ds

    Sector const &frontSec = segment.sector();
    Sector const &backSec  = segment.back().sector();

    coord_t const ffloor   = frontSec.floor().visHeight();
    coord_t const fceil    = frontSec.ceiling().visHeight();
    coord_t const bfloor   = backSec.floor().visHeight();
    coord_t const bceil    = backSec.ceiling().visHeight();

    bool middleCoversOpening = false;
    if(wroteOpaqueMiddle)
    {
        coord_t xbottom = de::max(bfloor, ffloor);
        coord_t xtop    = de::min(bceil,  fceil);

        Surface const &middle = segment.lineSide().middle();
        xbottom += middle.visMaterialOrigin().y;
        xtop    += middle.visMaterialOrigin().y;

        middleCoversOpening = (middleTopZ >= xtop &&
                               middleBottomZ <= xbottom);
    }

    if(wroteOpaqueMiddle && middleCoversOpening)
        return true;

    Line::Side const &front = segment.lineSide();

    if(   (bceil <= ffloor &&
               (front.top().hasMaterial()    || front.middle().hasMaterial()))
       || (bfloor >= fceil &&
               (front.bottom().hasMaterial() || front.middle().hasMaterial())))
    {
        Surface const &ffloorSurface = frontSec.floor().surface();
        Surface const &fceilSurface  = frontSec.ceiling().surface();
        Surface const &bfloorSurface = backSec.floor().surface();
        Surface const &bceilSurface  = backSec.ceiling().surface();

        // A closed gap?
        if(de::fequal(fceil, bfloor))
        {
            return (bceil <= bfloor) ||
                   !(fceilSurface.hasSkyMaskedMaterial() &&
                     bceilSurface.hasSkyMaskedMaterial());
        }

        if(de::fequal(ffloor, bceil))
        {
            return (bfloor >= bceil) ||
                   !(ffloorSurface.hasSkyMaskedMaterial() &&
                     bfloorSurface.hasSkyMaskedMaterial());
        }

        return true;
    }

    /// @todo Is this still necessary?
    if(bceil <= bfloor ||
       (!(bceil - bfloor > 0) && bfloor > ffloor && bceil < fceil &&
        front.top().hasMaterial() && front.bottom().hasMaterial()))
    {
        // A zero height back segment
        return true;
    }

    return false;
}

static void writeLeafWallSections()
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    foreach(Segment *seg, leaf->allSegments())
    {
        // We are only interested in front facing line segments.
        if(!seg->isFlagged(Segment::FacingFront))
            continue;

        // Only line segments with wall sections will be drawn.
        if(!seg->hasLineSide() || !seg->lineSide().hasSections())
            continue;

        // Done here because of the logic of doom.exe wrt the automap.
        reportWallSectionDrawn(seg->line());

        bool wroteOpaqueMiddle = false;
        coord_t middleBottomZ = 0, middleTopZ = 0;

        writeWallSection(*seg, Line::Side::Bottom);
        writeWallSection(*seg, Line::Side::Top);
        writeWallSection(*seg, Line::Side::Middle,
                         &wroteOpaqueMiddle, &middleBottomZ, &middleTopZ);

        // We can occlude the angle range defined by the X|Y origins of the
        // line segment if the open range has been covered (when the viewer
        // is not in the void).
        if(!P_IsInVoid(viewPlayer) &&
           coveredOpenRange(*seg, middleBottomZ, middleTopZ, wroteOpaqueMiddle))
        {
            C_AddRangeFromViewRelPoints(seg->from().origin(), seg->to().origin());
        }
    }
}

static void writeLeafPolyobjs()
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    foreach(Polyobj *po, leaf->polyobjs())
    foreach(Line *line, po->lines())
    {
        Segment *seg = line->front().leftSegment();

        // We are only interested in front facing line segments.
        if(!seg->isFlagged(Segment::FacingFront))
            continue;

        // Only line segments with wall sections will be drawn.
        if(!seg->hasLineSide() || !seg->lineSide().hasSections())
            continue;

        // Done here because of the logic of doom.exe wrt the automap.
        reportWallSectionDrawn(seg->line());

        bool wroteOpaqueMiddle = false;

        writeWallSection(*seg, Line::Side::Middle, &wroteOpaqueMiddle);

        // We can occlude the angle range defined by the X|Y origins of the
        // line segment if the open range has been covered (when the viewer
        // is not in the void).
        if(!P_IsInVoid(viewPlayer) && wroteOpaqueMiddle)
        {
            C_AddRangeFromViewRelPoints(seg->from().origin(), seg->to().origin());
        }
    }
}

static void writeLeafPlanes()
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    foreach(Plane *plane, leaf->sector().planes())
    {
        writeLeafPlane(*plane);
    }
}

static void markFrontFacingSegments()
{
    BspLeaf *bspLeaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(bspLeaf));

    foreach(Segment *seg, bspLeaf->allSegments())
    {
        if(!seg->hasLineSide())
            continue;

        // Which way is it facing?
        double dot = viewFacingDot(seg->from().origin(), seg->to().origin());
        seg->setFlags(Segment::FacingFront, dot < 0? UnsetFlags : SetFlags);
    }

    foreach(Polyobj *po, bspLeaf->polyobjs())
    foreach(Line *line, po->lines())
    {
        Segment *seg = line->front().leftSegment();

        // Which way is it facing?
        double dot = viewFacingDot(seg->from().origin(), seg->to().origin());
        seg->setFlags(Segment::FacingFront, dot < 0? UnsetFlags : SetFlags);
    }
}

static inline bool canOccludeSectorPairBoundary(Sector const &frontSec,
    Sector const &backSec, bool upward)
{
    Plane const &frontPlane = frontSec.plane(upward? Sector::Ceiling : Sector::Floor);
    Plane const &backPlane  =  backSec.plane(upward? Sector::Ceiling : Sector::Floor);

    // Do not create an occlusion between two sky-masked planes.
    // Only because the open range does not account for the sky plane height? -ds
    return !(frontPlane.surface().hasSkyMaskedMaterial() &&
              backPlane.surface().hasSkyMaskedMaterial());
}

/**
 * Add angle clipper occlusion ranges for the edges of the current leaf.
 */
static void occludeLeaf(bool frontFacing)
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    if(devNoCulling) return;
    if(P_IsInVoid(viewPlayer)) return;

    foreach(Segment *seg, leaf->clockwiseSegments())
    {
        DENG_ASSERT(seg->hasBspLeaf() && seg->bspLeaf().hasSector());

        if(frontFacing != seg->isFlagged(Segment::FacingFront))
            continue;

        // Edges without map line sections can never occlude.
        if(!seg->hasLineSide() || !seg->lineSide().hasSections())
            continue;

        // Occlusions should only happen where two sectors meet.
        if(!seg->hasBack() || !seg->back().hasBspLeaf())
            continue;

        BspLeaf &backLeaf = seg->back().bspLeaf();
        if(backLeaf.isDegenerate() || !backLeaf.hasSector())
            continue;

        Sector const &frontSec = seg->sector();
        Sector const &backSec  = seg->back().sector();

        // Choose start and end vertexes so that it's facing forward.
        Vertex const &from = frontFacing? seg->from() : seg->to();
        Vertex const &to   = frontFacing? seg->to()   : seg->from();

        coord_t openBottom, openTop;
        R_VisOpenRange(seg->lineSide(), &frontSec, &backSec, &openBottom, &openTop);

        // Does the floor create an occlusion?
        if(((openBottom > frontSec.floor().visHeight() && vOrigin[VY] <= openBottom)
            || (openBottom >  backSec.floor().visHeight() && vOrigin[VY] >= openBottom))
           && canOccludeSectorPairBoundary(frontSec, backSec, false))
        {
            C_AddViewRelOcclusion(from.origin(), to.origin(), openBottom, false);
        }

        // Does the ceiling create an occlusion?
        if(((openTop < frontSec.ceiling().visHeight() && vOrigin[VY] >= openTop)
            || (openTop <  backSec.ceiling().visHeight() && vOrigin[VY] <= openTop))
           && canOccludeSectorPairBoundary(frontSec, backSec, true))
        {
            C_AddViewRelOcclusion(from.origin(), to.origin(), openTop, true);
        }
    }
}

/// If @a segment is @em not front facing this is no-op.
static inline void clipFrontFacingSegment(Segment &segment)
{
    if(segment.isFlagged(Segment::FacingFront))
    {
        if(!C_CheckRangeFromViewRelPoints(segment.from().origin(), segment.to().origin()))
        {
            segment.setFlags(Segment::FacingFront, UnsetFlags);
        }
    }
}

static void clipFrontFacingSegments()
{
    BspLeaf *bspLeaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(bspLeaf));

    foreach(Segment *seg, bspLeaf->allSegments())
    {
        if(!seg->hasLineSide())
            continue;

        clipFrontFacingSegment(*seg);
    }

    foreach(Polyobj *po, bspLeaf->polyobjs())
    foreach(Line *line, po->lines())
    {
        clipFrontFacingSegment(*line->front().leftSegment());
    }
}

/**
 * @pre Assumes the leaf is at least partially visible.
 */
static void drawCurrentLeaf()
{
    BspLeaf *leaf = currentBspLeaf;
    DENG_ASSERT(!isNullLeaf(leaf));

    // Mark the sector visible for this frame.
    leaf->sector()._frameFlags |= SIF_VISIBLE;

    markFrontFacingSegments();
    R_InitForBspLeaf(*leaf);
    Rend_RadioBspLeafEdges(*leaf);

    /*
     * Before clip testing lumobjs (for halos), range-occlude the back facing edges.
     * After testing, range-occlude the front facing edges. Done before drawing wall
     * sections so that opening occlusions cut out unnecessary oranges.
     */

    occludeLeaf(false /* back facing */);
    LO_ClipInBspLeaf(leaf->indexInMap());
    occludeLeaf(true /* front facing */);

    clipFrontFacingSegments();

    if(leaf->hasPolyobj())
    {
        // Polyobjs don't obstruct - clip lights with another algorithm.
        LO_ClipInBspLeafBySight(leaf->indexInMap());
    }

    // Mark particle generators in the sector visible.
    Rend_ParticleMarkInSectorVisible(leaf->sectorPtr());

    /*
     * Sprites for this BSP leaf have to be drawn.
     *
     * Must be done BEFORE the segments of this BspLeaf are added to the clipper.
     * Otherwise the sprites would get clipped by them, and that wouldn't be right.
     *
     * Must be done AFTER the lumobjs have been clipped as this affects the projection
     * of halos.
     */
    R_AddSprites(leaf);

    writeLeafSkyMask();
    writeLeafWallSections();
    writeLeafPolyobjs();
    writeLeafPlanes();
}

/**
 * Change the current BspLeaf (updating any relevant draw state properties
 * accordingly).
 *
 * @param bspLeaf  The new BSP leaf to make current.
 */
static void makeCurrent(BspLeaf *bspLeaf)
{
    DENG_ASSERT(bspLeaf != 0);
    bool sectorChanged = (!currentBspLeaf || currentBspLeaf->sectorPtr() != bspLeaf->sectorPtr());

    currentBspLeaf = bspLeaf;

    // Update draw state.
    if(sectorChanged)
    {
        currentSectorLightColor = Rend_SectorLightColor(bspLeaf->sector());
        currentSectorLightLevel = bspLeaf->sector().lightLevel();
    }
}

static void traverseBspAndDrawLeafs(MapElement *bspElement)
{
    DENG_ASSERT(bspElement != 0);

    while(bspElement->type() != DMU_BSPLEAF)
    {
        // Descend deeper into the nodes.
        BspNode const *bspNode = bspElement->as<BspNode>();

        // Decide which side the view point is on.
        int eyeSide = bspNode->partition().pointOnSide(eyeOrigin) < 0;

        // Recursively divide front space.
        traverseBspAndDrawLeafs(bspNode->childPtr(eyeSide));

        // If the clipper is full we're pretty much done. This means no geometry
        // will be visible in the distance because every direction has already
        // been fully covered by geometry.
        if(!firstBspLeaf && C_IsFull())
            return;

        // ...and back space.
        bspElement = bspNode->childPtr(eyeSide ^ 1);
    }

    // We've arrived at a leaf.
    BspLeaf *bspLeaf = bspElement->as<BspLeaf>();

    // Skip null leafs (those with zero volume). Neighbors handle adding the
    // solid clipper segments.
    if(isNullLeaf(bspLeaf))
        return;

    // Is this leaf visible?
    if(!firstBspLeaf && !C_CheckBspLeaf(*bspLeaf))
        return;

    // This is now the current leaf.
    makeCurrent(bspLeaf);

    drawCurrentLeaf();

    // This is no longer the first leaf.
    firstBspLeaf = false;
}

void Rend_RenderMap()
{
    DENG_ASSERT(App_World().hasMap());

    Map &map = App_World().map();

    // Set to true if dynlights are inited for this frame.
    loInited = false;

    GL_SetMultisample(true);

    // Setup the modelview matrix.
    Rend_ModelViewMatrix();

    if(!freezeRLs)
    {
        // Prepare for rendering.
        RL_ClearLists(); // Clear the lists for new quads.
        C_ClearRanges(); // Clear the clipper.

        // Recycle the vlight lists. Currently done here as the lists are
        // not shared by all viewports.
        VL_InitForNewFrame();

        // Make vissprites of all the visible decorations.
        Rend_DecorProject();

        LO_BeginFrame();

        // Clear particle generator visibilty info.
        Rend_ParticleInitForNewFrame();

        R_InitShadowProjectionListsForNewFrame();

        viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
        eyeOrigin = Vector2d(viewData->current.origin);

        // Add the backside clipping range (if vpitch allows).
        if(vpitch <= 90 - yfov / 2 && vpitch >= -90 + yfov / 2)
        {
            float a = de::abs(vpitch) / (90 - yfov / 2);
            binangle_t startAngle = binangle_t(BANG_45 * fieldOfView / 90) * (1 + a);
            binangle_t angLen = BANG_180 - startAngle;

            binangle_t viewside = (viewData->current.angle >> (32 - BAMS_BITS)) + startAngle;
            C_SafeAddRange(viewside, viewside + angLen);
            C_SafeAddRange(viewside + angLen, viewside + 2 * angLen);
        }

        // The viewside line for the depth cue.
        viewsidex = -viewData->viewSin;
        viewsidey = viewData->viewCos;

        // We don't want BSP clip checking for the first BSP leaf.
        firstBspLeaf = true;

        // No current BSP leaf as of yet.
        currentBspLeaf = 0;

        // Draw the world!
        traverseBspAndDrawLeafs(&map.bspRoot());

        Rend_RenderMobjShadows();
    }
    RL_RenderAllLists();

    // Draw various debugging displays:
    Rend_DrawSurfaceVectors();
    LO_DrawLumobjs(); // Lumobjs.
    Rend_DrawBoundingBoxes(); // Mobj bounding boxes.
    Rend_DrawVertexIndices();
    Rend_DrawSoundOrigins();
    Rend_RenderGenerators();

    // Draw the Source Bias Editor's draw that identifies the current light.
    SBE_DrawCursor();

    GL_SetMultisample(false);
}

void Rend_UpdateLightModMatrix()
{
    if(novideo) return;

    std::memset(lightModRange, 0, sizeof(float) * 255);

    if(!App_World().hasMap())
    {
        rAmbient = 0;
        return;
    }

    int mapAmbient = App_World().map().ambientLightLevel();
    if(mapAmbient > ambientLight)
        rAmbient = mapAmbient;
    else
        rAmbient = ambientLight;

    for(int i = 0; i < 255; ++i)
    {
        // Adjust the white point/dark point?
        float lightlevel = 0;
        if(lightRangeCompression != 0)
        {
            if(lightRangeCompression >= 0)
            {
                // Brighten dark areas.
                lightlevel = float(255 - i) * lightRangeCompression;
            }
            else
            {
                // Darken bright areas.
                lightlevel = float(-i) * -lightRangeCompression;
            }
        }

        // Lower than the ambient limit?
        if(rAmbient != 0 && i+lightlevel <= rAmbient)
            lightlevel = rAmbient - i;

        // Clamp the result as a modifier to the light value (j).
        if((i + lightlevel) >= 255)
            lightlevel = 255 - i;
        else if((i + lightlevel) <= 0)
            lightlevel = -i;

        // Insert it into the matrix
        lightModRange[i] = lightlevel / 255.0f;
    }
}

float Rend_LightAdaptationDelta(float val)
{
    int clampedVal = de::clamp(0, ROUND(255.0f * val), 254);
    return lightModRange[clampedVal];
}

void Rend_ApplyLightAdaptation(float &val)
{
    val += Rend_LightAdaptationDelta(val);
}

void Rend_DrawLightModMatrix()
{
#define BLOCK_WIDTH             (1.0f)
#define BLOCK_HEIGHT            (BLOCK_WIDTH * 255.0f)
#define BORDER                  (20)

    // Disabled?
    if(!devLightModRange) return;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_WINDOW->width(), DENG_WINDOW->height(), 0, -1, 1);

    glTranslatef(BORDER, BORDER, 0);

    /*
    color.red = 0.2f;
    color.green = 0;
    color.blue = 0.6f;
    */

    // Draw an outside border.
    glColor4f(1, 1, 0, 1);
    glBegin(GL_LINES);
        glVertex2f(-1, -1);
        glVertex2f(255 + 1, -1);
        glVertex2f(255 + 1, -1);
        glVertex2f(255 + 1, BLOCK_HEIGHT + 1);
        glVertex2f(255 + 1, BLOCK_HEIGHT + 1);
        glVertex2f(-1, BLOCK_HEIGHT + 1);
        glVertex2f(-1, BLOCK_HEIGHT + 1);
        glVertex2f(-1, -1);
    glEnd();

    glBegin(GL_QUADS);
    float c = 0;
    for(int i = 0; i < 255; ++i, c += (1.0f/255.0f))
    {
        // Get the result of the source light level + offset.
        float off = lightModRange[i];

        glColor4f(c + off, c + off, c + off, 1);
        glVertex2f(i * BLOCK_WIDTH, 0);
        glVertex2f(i * BLOCK_WIDTH + BLOCK_WIDTH, 0);
        glVertex2f(i * BLOCK_WIDTH + BLOCK_WIDTH, BLOCK_HEIGHT);
        glVertex2f(i * BLOCK_WIDTH, BLOCK_HEIGHT);
    }
    glEnd();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

#undef BORDER
#undef BLOCK_HEIGHT
#undef BLOCK_WIDTH
}

static DGLuint constructBBox(DGLuint name, float br)
{
    if(GL_NewList(name, GL_COMPILE))
    {
        glBegin(GL_QUADS);
        {
            // Top
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f+br, 1.0f,-1.0f-br);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f-br, 1.0f,-1.0f-br);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f-br, 1.0f, 1.0f+br);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f+br, 1.0f, 1.0f+br);  // BR
            // Bottom
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f+br,-1.0f, 1.0f+br);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f-br,-1.0f, 1.0f+br);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f-br,-1.0f,-1.0f-br);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f+br,-1.0f,-1.0f-br);  // BR
            // Front
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f+br, 1.0f+br, 1.0f);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f-br, 1.0f+br, 1.0f);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f-br,-1.0f-br, 1.0f);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f+br,-1.0f-br, 1.0f);  // BR
            // Back
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f+br,-1.0f-br,-1.0f);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f-br,-1.0f-br,-1.0f);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f-br, 1.0f+br,-1.0f);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f+br, 1.0f+br,-1.0f);  // BR
            // Left
            glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, 1.0f+br, 1.0f+br);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f, 1.0f+br,-1.0f-br);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f,-1.0f-br,-1.0f-br);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f,-1.0f-br, 1.0f+br);  // BR
            // Right
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f, 1.0f+br,-1.0f-br);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f, 1.0f+br, 1.0f+br);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f,-1.0f-br, 1.0f+br);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f,-1.0f-br,-1.0f-br);  // BR
        }
        glEnd();
        return GL_EndList();
    }
    return 0;
}

/**
 * Draws a textured cube using the currently bound gl texture.
 * Used to draw mobj bounding boxes.
 *
 * @param pos           Coordinates of the center of the box (in map space units).
 * @param w             Width of the box.
 * @param l             Length of the box.
 * @param h             Height of the box.
 * @param a             Angle of the box.
 * @param color         Color to make the box (uniform vertex color).
 * @param alpha         Alpha to make the box (uniform vertex color).
 * @param br            Border amount to overlap box faces.
 * @param alignToBase   If @c true, align the base of the box
 *                      to the Z coordinate.
 */
void Rend_DrawBBox(Vector3d const &pos, coord_t w, coord_t l, coord_t h,
    float a, float const color[3], float alpha, float br, bool alignToBase)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    if(alignToBase)
        // The Z coordinate is to the bottom of the object.
        glTranslated(pos.x, pos.z + h, pos.y);
    else
        glTranslated(pos.x, pos.z, pos.y);

    glRotatef(0, 0, 0, 1);
    glRotatef(0, 1, 0, 0);
    glRotatef(a, 0, 1, 0);

    glScaled(w - br - br, h - br - br, l - br - br);
    glColor4f(color[CR], color[CG], color[CB], alpha);

    GL_CallList(dlBBox);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

/**
 * Draws a textured triangle using the currently bound gl texture.
 * Used to draw mobj angle direction arrow.
 *
 * @param pos           Coordinates of the center of the base of the
 *                      triangle (in "world" coordinates [VX, VY, VZ]).
 * @param a             Angle to point the triangle in.
 * @param s             Scale of the triangle.
 * @param color         Color to make the box (uniform vertex color).
 * @param alpha         Alpha to make the box (uniform vertex color).
 */
void Rend_DrawArrow(Vector3d const &pos, float a, float s, float const color[3],
    float alpha)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glTranslated(pos.x, pos.z, pos.y);

    glRotatef(0, 0, 0, 1);
    glRotatef(0, 1, 0, 0);
    glRotatef(a, 0, 1, 0);

    glScalef(s, 0, s);

    glBegin(GL_TRIANGLES);
    {
        glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f( 1.0f, 1.0f,-1.0f);  // L

        glColor4f(color[0], color[1], color[2], alpha);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(-1.0f, 1.0f,-1.0f);  // Point

        glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-1.0f, 1.0f, 1.0f);  // R
    }
    glEnd();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

static int drawMobjBBox(thinker_t *th, void * /*context*/)
{
    static float const red[3]    = { 1, 0.2f, 0.2f}; // non-solid objects
    static float const green[3]  = { 0.2f, 1, 0.2f}; // solid objects
    static float const yellow[3] = {0.7f, 0.7f, 0.2f}; // missiles

    mobj_t *mo = (mobj_t *) th;

    // We don't want the console player.
    if(mo == ddPlayers[consolePlayer].shared.mo)
        return false; // Continue iteration.
    // Is it vissible?
    if(!(mo->bspLeaf && mo->bspLeaf->sector().frameFlags() & SIF_VISIBLE))
        return false; // Continue iteration.

    Vector3d eye(vOrigin[VX], vOrigin[VZ], vOrigin[VY]);

    float alpha = 1 - ((Vector3d(eye - Vector3d(mo->origin)).length() / (DENG_WINDOW->width()/2)) / 4);
    if(alpha < .25f)
        alpha = .25f; // Don't make them totally invisible.

    // Draw a bounding box in an appropriate color.
    coord_t size = mo->radius;
    Rend_DrawBBox(mo->origin, size, size, mo->height/2, 0,
                  (mo->ddFlags & DDMF_MISSILE)? yellow :
                  (mo->ddFlags & DDMF_SOLID)? green : red,
                  alpha, .08f);

    Rend_DrawArrow(mo->origin, ((mo->angle + ANG45 + ANG90) / (float) ANGLE_MAX *-360), size*1.25,
                   (mo->ddFlags & DDMF_MISSILE)? yellow :
                   (mo->ddFlags & DDMF_SOLID)? green : red, alpha);
    return false; // Continue iteration.
}

/**
 * Renders bounding boxes for all mobj's (linked in sec->mobjList, except
 * the console player) in all sectors that are currently marked as vissible.
 *
 * Depth test is disabled to show all mobjs that are being rendered, regardless
 * if they are actually vissible (hidden by previously drawn map geometry).
 */
static void Rend_DrawBoundingBoxes()
{
    //static float const red[3]   = { 1, 0.2f, 0.2f}; // non-solid objects
    static float const green[3]  = { 0.2f, 1, 0.2f}; // solid objects
    static float const yellow[3] = {0.7f, 0.7f, 0.2f}; // missiles

    Map &map = App_World().map();

    if(!devMobjBBox && !devPolyobjBBox) return;

#ifndef _DEBUG
    // Bounding boxes are not allowed in non-debug netgames.
    if(netGame) return;
#endif

    if(!dlBBox)
        dlBBox = constructBBox(0, .08f);

    Vector3d eye(vOrigin[VX], vOrigin[VZ], vOrigin[VY]);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    MaterialSnapshot const &ms = App_Materials().
            find(de::Uri("System", Path("bbox"))).material().prepare(Rend_SpriteMaterialSpec());

    GL_BindTexture(&ms.texture(MTU_PRIMARY));
    GL_BlendMode(BM_ADD);

    if(devMobjBBox)
    {
        map.thinkers().iterate(reinterpret_cast<thinkfunc_t>(gx.MobjThinker),
                               0x1, drawMobjBBox, NULL);
    }

    if(devPolyobjBBox)
    {
        foreach(Polyobj const *polyobj, map.polyobjs())
        {
            Sector const &sec = polyobj->sector();
            coord_t width  = (polyobj->aaBox.maxX - polyobj->aaBox.minX)/2;
            coord_t length = (polyobj->aaBox.maxY - polyobj->aaBox.minY)/2;
            coord_t height = (sec.ceiling().height() - sec.floor().height())/2;

            Vector3d pos(polyobj->aaBox.minX + width,
                         polyobj->aaBox.minY + length,
                         sec.floor().height());

            float alpha = 1 - ((Vector3d(eye - pos).length() / (DENG_WINDOW->width()/2)) / 4);
            if(alpha < .25f)
                alpha = .25f; // Don't make them totally invisible.

            Rend_DrawBBox(pos, width, length, height, 0, yellow, alpha, .08f);

            foreach(Line *line, polyobj->lines())
            {
                Vector3d pos(line->center(), sec.floor().height());

                Rend_DrawBBox(pos, 0, line->length() / 2, height,
                              BANG2DEG(BANG_90 - line->angle()),
                              green, alpha, 0);
            }
        }
    }

    GL_BlendMode(BM_NORMAL);

    glEnable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
}

static void drawVector(Vector3f const &vector, float scalar, const float color[3])
{
    static const float black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(scalar * vector.x, scalar * vector.z, scalar * vector.y);
        glColor3fv(color);
        glVertex3f(0, 0, 0);
    glEnd();
}

static void drawSurfaceTangentSpaceVectors(Surface *suf, Vector3d const &origin)
{
    int const VISUAL_LENGTH = 20;

    static float const red[3]   = { 1, 0, 0 };
    static float const green[3] = { 0, 1, 0 };
    static float const blue[3]  = { 0, 0, 1 };

    DENG_ASSERT(suf != 0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(origin.x, origin.z, origin.y);

    if(devSurfaceVectors & SVF_TANGENT)   drawVector(suf->tangent(),   VISUAL_LENGTH, red);
    if(devSurfaceVectors & SVF_BITANGENT) drawVector(suf->bitangent(), VISUAL_LENGTH, green);
    if(devSurfaceVectors & SVF_NORMAL)    drawVector(suf->normal(),    VISUAL_LENGTH, blue);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

/**
 * Draw the surface tangent space vectors, primarily for debug.
 */
static void Rend_DrawSurfaceVectors()
{
    if(!devSurfaceVectors) return;
    if(!App_World().hasMap()) return;

    Map &map = App_World().map();

    glDisable(GL_CULL_FACE);

    Vector3d origin;
    foreach(BspLeaf *bspLeaf, map.bspLeafs())
    foreach(Segment *segment, bspLeaf->allSegments())
    {
        if(!bspLeaf->hasSector())
            continue;

        if(!segment->hasLineSide() || segment->line().definesPolyobj())
            continue;

        if(!segment->back().hasBspLeaf() || segment->back().bspLeaf().isDegenerate() ||
           !segment->back().bspLeaf().hasSector())
        {
            coord_t const bottom = segment->sector().floor().visHeight();
            coord_t const top = segment->sector().ceiling().visHeight();
            Vector2d center = (segment->to().origin() + segment->from().origin()) / 2;
            Surface *suf = &segment->lineSide().middle();

            origin = Vector3d(center, bottom + (top - bottom) / 2);
            drawSurfaceTangentSpaceVectors(suf, origin);
        }
        else
        {
            Sector *backSec  = segment->back().sectorPtr();
            Line::Side &side = segment->lineSide();

            if(side.middle().hasMaterial())
            {
                coord_t const bottom = segment->sector().floor().visHeight();
                coord_t const top = segment->sector().ceiling().visHeight();
                Vector2d center = (segment->to().origin() + segment->from().origin()) / 2;
                Surface *suf = &side.middle();

                origin = Vector3d(center, bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }

            if(backSec->ceiling().visHeight() <
               segment->sector().ceiling().visHeight() &&
               !(segment->sector().ceilingSurface().hasSkyMaskedMaterial() &&
                 backSec->ceilingSurface().hasSkyMaskedMaterial()))
            {
                coord_t const bottom = backSec->ceiling().visHeight();
                coord_t const top = segment->sector().ceiling().visHeight();
                Vector2d center = (segment->to().origin() + segment->from().origin()) / 2;
                Surface *suf = &side.top();

                origin = Vector3d(center, bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }

            if(backSec->floor().visHeight() >
               segment->sector().floor().visHeight() &&
               !(segment->sector().floorSurface().hasSkyMaskedMaterial() &&
                 backSec->floorSurface().hasSkyMaskedMaterial()))
            {
                coord_t const bottom = segment->sector().floor().visHeight();
                coord_t const top = backSec->floor().visHeight();
                Vector2d center = (segment->to().origin() + segment->from().origin()) / 2;
                Surface *suf = &side.bottom();

                origin = Vector3d(center, bottom + (top - bottom) / 2);
                drawSurfaceTangentSpaceVectors(suf, origin);
            }
        }
    }

    foreach(BspLeaf *bspLeaf, map.bspLeafs())
    {
        if(bspLeaf->isDegenerate()) continue;
        if(!bspLeaf->hasSector()) continue;
        Sector &sector = bspLeaf->sector();

        foreach(Plane *plane, sector.planes())
        {
            origin = Vector3d(bspLeaf->poly().center(), plane->visHeight());

            if(plane->surface().hasSkyMaskedMaterial() && plane->indexInSector() <= Sector::Ceiling)
                origin.z = plane->map().skyFix(plane->indexInSector() == Sector::Ceiling);

            drawSurfaceTangentSpaceVectors(&plane->surface(), origin);
        }
    }

    foreach(Polyobj *polyobj, map.polyobjs())
    {
        Sector const &sector = polyobj->sector();
        float zPos = sector.floor().height() + (sector.ceiling().height() - sector.floor().height())/2;

        foreach(Line *line, polyobj->lines())
        {
            origin = Vector3d(line->center(), zPos);
            drawSurfaceTangentSpaceVectors(&line->front().middle(), origin);
        }
    }

    glEnable(GL_CULL_FACE);
}

static void drawSoundOrigin(Vector3d const &origin, char const *label, Vector3d const &eye)
{
    coord_t const MAX_SOUNDORIGIN_DIST = 384; ///< Maximum distance from origin to eye in map coordinates.

    if(!label) return;

    coord_t dist = Vector3d(eye - origin).length();
    float alpha = 1.f - de::min(dist, MAX_SOUNDORIGIN_DIST) / MAX_SOUNDORIGIN_DIST;

    if(alpha > 0)
    {
        float scale = dist / (DENG_WINDOW->width() / 2);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(origin.x, origin.z, origin.y);
        glRotatef(-vang + 180, 0, 1, 0);
        glRotatef(vpitch, 1, 0, 0);
        glScalef(-scale, -scale, 1);

        Point2Raw const labelOrigin(2, 2);
        UI_TextOutEx(label, &labelOrigin, UI_Color(UIC_TITLE), alpha);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }
}

/**
 * Debugging aid for visualizing sound origins.
 */
static void Rend_DrawSoundOrigins()
{
    if(!devSoundOrigins) return;
    if(!App_World().hasMap()) return;

    Map &map = App_World().map();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    Vector3d eye(vOrigin[VX], vOrigin[VZ], vOrigin[VY]);

    if(devSoundOrigins & SOF_SIDE)
    {
        foreach(Line *line, map.lines())
        for(int i = 0; i < 2; ++i)
        {
            Line::Side &side = line->side(i);
            if(!side.hasSections()) continue;

            char buf[80];

            dd_snprintf(buf, 80, "Line #%d (%s, middle)", line->indexInMap(), (i? "back" : "front"));
            drawSoundOrigin(Vector3d(side.middleSoundEmitter().origin), buf, eye);

            dd_snprintf(buf, 80, "Line #%d (%s, bottom)", line->indexInMap(), (i? "back" : "front"));
            drawSoundOrigin(Vector3d(side.bottomSoundEmitter().origin), buf, eye);

            dd_snprintf(buf, 80, "Line #%d (%s, top)", line->indexInMap(), (i? "back" : "front"));
            drawSoundOrigin(Vector3d(side.topSoundEmitter().origin), buf, eye);
        }
    }

    if(devSoundOrigins & (SOF_SECTOR|SOF_PLANE))
    {
        foreach(Sector *sec, map.sectors())
        {
            char buf[80];

            if(devSoundOrigins & SOF_PLANE)
            {
                for(int i = 0; i < sec->planeCount(); ++i)
                {
                    Plane &plane = sec->plane(i);
                    dd_snprintf(buf, 80, "Sector #%i (pln:%i)", sec->indexInMap(), i);
                    drawSoundOrigin(Vector3d(plane.soundEmitter().origin), buf, eye);
                }
            }

            if(devSoundOrigins & SOF_SECTOR)
            {
                dd_snprintf(buf, 80, "Sector #%i", sec->indexInMap());
                drawSoundOrigin(Vector3d(sec->soundEmitter().origin), buf, eye);
            }
        }
    }

    // Restore previous state.
    glEnable(GL_DEPTH_TEST);
}

static void getVertexPlaneMinMax(Vertex const *vtx, coord_t *min, coord_t *max)
{
    if(!vtx || (!min && !max)) return;

    LineOwner const *base = vtx->firstLineOwner();
    LineOwner const *own  = base;
    do
    {
        Line *li = &own->line();

        if(li->hasFrontSector())
        {
            if(min && li->frontSector().floor().visHeight() < *min)
                *min = li->frontSector().floor().visHeight();

            if(max && li->frontSector().ceiling().visHeight() > *max)
                *max = li->frontSector().ceiling().visHeight();
        }

        if(li->hasBackSector())
        {
            if(min && li->backSector().floor().visHeight() < *min)
                *min = li->backSector().floor().visHeight();

            if(max && li->backSector().ceiling().visHeight() > *max)
                *max = li->backSector().ceiling().visHeight();
        }

        own = &own->next();
    } while(own != base);
}

static void drawVertexPoint(const Vertex* vtx, coord_t z, float alpha)
{
    glBegin(GL_POINTS);
        glColor4f(.7f, .7f, .2f, alpha * 2);
        glVertex3f(vtx->origin().x, z, vtx->origin().y);
    glEnd();
}

static void drawVertexBar(Vertex const *vtx, coord_t bottom, coord_t top, float alpha)
{
    int const EXTEND_DIST = 64;

    static const float black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(vtx->origin().x, bottom - EXTEND_DIST, vtx->origin().y);
        glColor4f(1, 1, 1, alpha);
        glVertex3f(vtx->origin().x, bottom, vtx->origin().y);
        glVertex3f(vtx->origin().x, bottom, vtx->origin().y);
        glVertex3f(vtx->origin().x, top, vtx->origin().y);
        glVertex3f(vtx->origin().x, top, vtx->origin().y);
        glColor4fv(black);
        glVertex3f(vtx->origin().x, top + EXTEND_DIST, vtx->origin().y);
    glEnd();
}

static void drawVertexIndex(Vertex const *vtx, coord_t z, float scale, float alpha)
{
    DENG_ASSERT(vtx != 0);
    Point2Raw const origin(2, 2);
    char buf[80];

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    sprintf(buf, "%i", vtx->indexInMap());

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(vtx->origin().x, z, vtx->origin().y);
    glRotatef(-vang + 180, 0, 1, 0);
    glRotatef(vpitch, 1, 0, 0);
    glScalef(-scale, -scale, 1);

    glEnable(GL_TEXTURE_2D);

    UI_TextOutEx(buf, &origin, UI_Color(UIC_TITLE), alpha);

    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

#define MAX_VERTEX_POINT_DIST 1280

static int drawVertex1(Line *li, void *context)
{
    Vertex *vtx = &li->from();
    Polyobj *po = (Polyobj *) context;
    coord_t dist2D = M_ApproxDistance(vOrigin[VX] - vtx->origin().x, vOrigin[VZ] - vtx->origin().y);

    if(dist2D < MAX_VERTEX_POINT_DIST)
    {
        float alpha = 1 - dist2D / MAX_VERTEX_POINT_DIST;

        if(alpha > 0)
        {
            coord_t bottom = po->sector().floor().visHeight();
            coord_t top    = po->sector().ceiling().visHeight();

            if(devVertexBars)
                drawVertexBar(vtx, bottom, top, MIN_OF(alpha, .15f));

            drawVertexPoint(vtx, bottom, alpha * 2);
        }
    }

    if(devVertexIndices)
    {
        coord_t eye[3], pos[3], dist3D;

        eye[VX] = vOrigin[VX];
        eye[VY] = vOrigin[VZ];
        eye[VZ] = vOrigin[VY];

        pos[VX] = vtx->origin().x;
        pos[VY] = vtx->origin().y;
        pos[VZ] = po->sector().floor().visHeight();

        dist3D = V3d_Distance(pos, eye);

        if(dist3D < MAX_VERTEX_POINT_DIST)
        {
            drawVertexIndex(vtx, pos[VZ], dist3D / (DENG_WINDOW->width() / 2),
                            1 - dist3D / MAX_VERTEX_POINT_DIST);
        }
    }

    return false; // Continue iteration.
}

static int drawPolyObjVertexes(Polyobj *po, void * /*context*/)
{
    DENG_ASSERT(po != 0);
    foreach(Line *line, po->lines())
    {
        if(line->validCount() == validCount)
            continue;

        line->setValidCount(validCount);
        int result = drawVertex1(line, po);
        if(result) return result;
    }
    return false; // Continue iteration.
}

/**
 * Draw the various vertex debug aids.
 */
static void Rend_DrawVertexIndices()
{
    float oldLineWidth = -1;

    if(!devVertexBars && !devVertexIndices) return;
    if(!App_World().hasMap()) return;

    Map &map = App_World().map();

    glDisable(GL_DEPTH_TEST);

    if(devVertexBars)
    {
        glEnable(GL_LINE_SMOOTH);
        oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
        DGL_SetFloat(DGL_LINE_WIDTH, 2);

        foreach(Vertex *vertex, map.vertexes())
        {
            // Not a line vertex?
            LineOwner const *own = vertex->firstLineOwner();
            if(!own) continue;

            // Ignore polyobj vertexes.
            if(own->line().definesPolyobj()) continue;

            float alpha = 1 - M_ApproxDistance(vOrigin[VX] - vertex->origin().x,
                                               vOrigin[VZ] - vertex->origin().y) / MAX_VERTEX_POINT_DIST;
            alpha = de::min(alpha, .15f);

            if(alpha > 0)
            {
                coord_t bottom = DDMAXFLOAT;
                coord_t top    = DDMINFLOAT;
                getVertexPlaneMinMax(vertex, &bottom, &top);

                drawVertexBar(vertex, bottom, top, alpha);
            }
        }
    }

    // Draw the vertex point nodes.
    float const oldPointSize = DGL_GetFloat(DGL_POINT_SIZE);

    glEnable(GL_POINT_SMOOTH);
    DGL_SetFloat(DGL_POINT_SIZE, 6);

    foreach(Vertex *vertex, map.vertexes())
    {
        // Not a line vertex?
        LineOwner const *own = vertex->firstLineOwner();
        if(!own) continue;

        // Ignore polyobj vertexes.
        if(own->line().definesPolyobj()) continue;

        coord_t dist = M_ApproxDistance(vOrigin[VX] - vertex->origin().x,
                                       vOrigin[VZ] - vertex->origin().y);

        if(dist < MAX_VERTEX_POINT_DIST)
        {
            coord_t bottom = DDMAXFLOAT;
            getVertexPlaneMinMax(vertex, &bottom, NULL);

            drawVertexPoint(vertex, bottom, (1 - dist / MAX_VERTEX_POINT_DIST) * 2);
        }
    }

    if(devVertexIndices)
    {
        coord_t eye[3];

        eye[VX] = vOrigin[VX];
        eye[VY] = vOrigin[VZ];
        eye[VZ] = vOrigin[VY];

        foreach(Vertex *vertex, map.vertexes())
        {
            coord_t pos[3], dist;

            // Not a line vertex?
            LineOwner const *own = vertex->firstLineOwner();
            if(!own) continue;

            // Ignore polyobj vertexes.
            if(own->line().definesPolyobj()) continue;

            pos[VX] = vertex->origin().x;
            pos[VY] = vertex->origin().y;
            pos[VZ] = DDMAXFLOAT;
            getVertexPlaneMinMax(vertex, &pos[VZ], NULL);

            dist = V3d_Distance(pos, eye);

            if(dist < MAX_VERTEX_POINT_DIST)
            {
                float const alpha = 1 - dist / MAX_VERTEX_POINT_DIST;
                float const scale = dist / (DENG_WINDOW->width() / 2);

                drawVertexIndex(vertex, pos[VZ], scale, alpha);
            }
        }
    }

    // Next, the vertexes of all nearby polyobjs.
    AABoxd box;
    box.minX = vOrigin[VX] - MAX_VERTEX_POINT_DIST;
    box.minY = vOrigin[VY] - MAX_VERTEX_POINT_DIST;
    box.maxX = vOrigin[VX] + MAX_VERTEX_POINT_DIST;
    box.maxY = vOrigin[VY] + MAX_VERTEX_POINT_DIST;
    P_PolyobjsBoxIterator(&box, drawPolyObjVertexes, NULL);

    // Restore previous state.
    if(devVertexBars)
    {
        DGL_SetFloat(DGL_LINE_WIDTH, oldLineWidth);
        glDisable(GL_LINE_SMOOTH);
    }
    DGL_SetFloat(DGL_POINT_SIZE, oldPointSize);
    glDisable(GL_POINT_SMOOTH);
    glEnable(GL_DEPTH_TEST);
}

MaterialVariantSpec const &Rend_MapSurfaceMaterialSpec(int wrapS, int wrapT)
{
    return App_Materials().variantSpec(MapSurfaceContext, 0, 0, 0, 0, wrapS, wrapT,
                                       -1, -1, -1, true, true, false, false);
}

MaterialVariantSpec const &Rend_MapSurfaceMaterialSpec()
{
    return Rend_MapSurfaceMaterialSpec(GL_REPEAT, GL_REPEAT);
}

texturevariantspecification_t &Rend_MapSurfaceShinyTextureSpec()
{
    return GL_TextureVariantSpec(TC_MAPSURFACE_REFLECTION, TSF_NO_COMPRESSION,
                                 0, 0, 0, GL_REPEAT, GL_REPEAT, 1, 1, -1,
                                 false, false, false, false);
}

texturevariantspecification_t &Rend_MapSurfaceShinyMaskTextureSpec()
{
    return GL_TextureVariantSpec(TC_MAPSURFACE_REFLECTIONMASK, 0,
                                 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1,
                                 true, false, false, false);
}
