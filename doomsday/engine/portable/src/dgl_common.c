/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2008 Daniel Swanson <danij@dengine.net>
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
 * dgl_common.c : Portable OpenGL init/state routines.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "de_base.h"
#include "de_dgl.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// A helpful macro that changes the origin of the screen
// coordinate system.
#define FLIP(y) (theWindow->height - (y+1))

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

dgl_state_t DGL_state;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initedGL = false;
static boolean firstTimeInit = true;

// CODE --------------------------------------------------------------------

/**
 * Set the currently active GL texture by name.
 *
 * @param texture       GL texture name to make active.
 */
void activeTexture(const GLenum texture)
{
#ifdef WIN32
    if(!glActiveTextureARB)
        return;
    glActiveTextureARB(texture);
#else
# ifdef USE_MULTITEXTURE
    glActiveTextureARB(texture);
# endif
#endif
}

static void checkExtensions(void)
{
    // Get the maximum texture size.
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint*) &DGL_state.maxTexSize);

    DGL_InitExtensions();

    glGetIntegerv(GL_MAX_TEXTURE_UNITS, (GLint*) &DGL_state.maxTexUnits);
#ifndef USE_MULTITEXTURE
    DGL_state.maxTexUnits = 1;
#endif
    // But sir, we are simple people; two units is enough.
    if(DGL_state.maxTexUnits > 2)
        DGL_state.maxTexUnits = 2;

    if(DGL_state_ext.aniso)
        glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint*) &DGL_state.maxAniso);
    else
        DGL_state.maxAniso = 1;

    // Decide whether vertex arrays should be done manually or with real
    // OpenGL calls.
    InitArrays();

    DGL_state.useAnisotropic =
        ((DGL_state_ext.aniso && !ArgExists("-noanifilter"))? true : false);

    DGL_state.allowCompression = true;
}

static void printGLUInfo(void)
{
    GLint           iVal;
    GLfloat         fVals[2];

    // Print some OpenGL information (console must be initialized by now).
    Con_Message("OpenGL information:\n");
    Con_Message("  Vendor: %s\n", glGetString(GL_VENDOR));
    Con_Message("  Renderer: %s\n", glGetString(GL_RENDERER));
    Con_Message("  Version: %s\n", glGetString(GL_VERSION));
    Con_Message("  GLU Version: %s\n", gluGetString(GLU_VERSION));

    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &iVal);
    Con_Message("  Available Texture units: %i\n", iVal);

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &iVal);
    Con_Message("  Maximum Texture Size: %i\n", iVal);
    if(DGL_state_ext.aniso)
    {
        glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &iVal);
        Con_Message("  Maximum Anisotropy: %i\n", iVal);
    }
    else
    {
        Con_Message("  Variable Texture Anisotropy Unavailable.\n");
    }

    if(DGL_state_ext.s3TC)
    {
        glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &iVal);
        Con_Message("  Num Texture Formats: %i\n", iVal);
    }

    glGetFloatv(GL_LINE_WIDTH_GRANULARITY, fVals);
    Con_Message("  Line Width Granularity: %3.1f\n",
                fVals[0]);
    glGetFloatv(GL_LINE_WIDTH_RANGE, fVals);
    Con_Message("  Line Width Range: %3.1f...%3.1f\n",
                fVals[0], fVals[1]);

    DGL_PrintExtensions();
}

static void printDGLConfiguration(void)
{
    static const char *yesNo[] = {"Disabled", "Enabled"};

    Con_Message("DGL Configuration:\n");

    Con_Message("  Texture Compression: %s\n", yesNo[DGL_state_texture.useCompr? 1:0]);
    Con_Message("  Variable Texture Anisotropy: %s\n", yesNo[DGL_state.useAnisotropic? 1:0]);
    Con_Message("  Utilized Texture Units: %i\n", DGL_state.maxTexUnits);
}

boolean DGL_PreInit(void)
{
    if(isDedicated)
        return true;

    if(initedGL)
        return true; // Already inited.

    memset(&DGL_state, 0, sizeof(DGL_state));
    memset(&DGL_state_ext, 0, sizeof(DGL_state_ext));
    memset(&DGL_state_texture, 0, sizeof(DGL_state_texture));

    DGL_state_texture.dumpTextures =
        (ArgCheck("-dumptextures")? true : false);

    if(DGL_state_texture.dumpTextures)
        Con_Message("  Dumping textures (mipmap level zero).\n");

    DGL_state.forceFinishBeforeSwap =
        (ArgExists("-glfinish")? true : false);

    if(DGL_state.forceFinishBeforeSwap)
        Con_Message("  glFinish() forced before swapping buffers.\n");

    initedGL = true;
    return true;
}

/**
 * Initializes DGL.
 */
boolean DGL_Init(void)
{
    if(isDedicated)
        return true;

    if(!initedGL)
        return false;

    if(firstTimeInit)
    {
        checkExtensions();

        printGLUInfo();
        printDGLConfiguration();

        firstTimeInit = false;
    }

    return true;
}

/**
 * Shutdown the DGL drawing facilities.
 */
void DGL_Shutdown(void)
{
    if(!initedGL)
        return;

    initedGL = false;
}

void initState(void)
{
    GLfloat         fogcol[4] = { .54f, .54f, .54f, 1 };

    DGL_state.nearClip = 5;
    DGL_state.farClip = 8000;
    polyCounter = 0;

    DGL_state_texture.usePalTex = false;
    DGL_state_texture.dumpTextures = false;
    DGL_state_texture.useCompr = false;

    // Here we configure the OpenGL state and set projection matrix.
    glFrontFace(GL_CW);
    glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
#ifndef DRMESA
    glEnable(GL_TEXTURE_2D);
#else
    glDisable(GL_TEXTURE_2D);
#endif

    // The projection matrix.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Initialize the modelview matrix.
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Also clear the texture matrix.
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();

#if DRMESA
    glDisable(GL_DITHER);
    glDisable(GL_LIGHTING);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_POINT_SMOOTH);
    glDisable(GL_POLYGON_SMOOTH);
    glShadeModel(GL_FLAT);
#else
    // Setup for antialiased lines/points.
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    DGL_state.currentLineWidth = 1.5f;
    glLineWidth(DGL_state.currentLineWidth);

    glShadeModel(GL_SMOOTH);
#endif

    // Alpha blending is a go!
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0);

    // Default state for the white fog is off.
    DGL_state.useFog = 0;
    glDisable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogi(GL_FOG_END, 2100);   // This should be tweaked a bit.
    glFogfv(GL_FOG_COLOR, fogcol);

    // Default state for vsync is off
    DGL_state.useVSync = 0;
#ifdef WIN32
    if(wglSwapIntervalEXT != NULL)
        wglSwapIntervalEXT(0);
#endif

#if DRMESA
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
#else
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
#endif

    // Prefer good quality in texture compression.
    glHint(GL_TEXTURE_COMPRESSION_HINT, GL_NICEST);

    // Clear the buffers.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/**
 * Requires a texture environment mode that can add and multiply.
 * Nvidia's and ATI's appropriate extensions are supported, other cards will
 * not be able to utilize multitextured lights.
 */
void envAddColoredAlpha(int activate, GLenum addFactor)
{
    if(activate)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
                  DGL_state_ext.nvTexEnvComb ? GL_COMBINE4_NV : GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);

        // Combine: texAlpha * constRGB + 1 * prevRGB.
        if(DGL_state_ext.nvTexEnvComb)
        {
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_ADD);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, addFactor);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_ZERO);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_ONE_MINUS_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_SRC_COLOR);
        }
        else if(DGL_state_ext.atiTexEnvComb)
        {   // MODULATE_ADD_ATI: Arg0 * Arg2 + Arg1.
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE_ADD_ATI);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, addFactor);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        }
        else
        {   // This doesn't look right.
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_ADD);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, addFactor);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        }
    }
    else
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }
}

/**
 * Setup the texture environment for single-pass multiplicative lighting.
 * The last texture unit is always used for the texture modulation.
 * TUs 1...n-1 are used for dynamic lights.
 */
void envModMultiTex(int activate)
{
    // Setup TU 2: The modulated texture.
    activeTexture(GL_TEXTURE1);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    // Setup TU 1: The dynamic light.
    activeTexture(GL_TEXTURE0);
    envAddColoredAlpha(activate, GL_SRC_ALPHA);

    // This is a single-pass mode. The alpha should remain unmodified
    // during the light stage.
    if(activate)
    {   // Replace: primAlpha.
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
    }
}

void DGL_Viewport(int x, int y, int width, int height)
{
    glViewport(x, FLIP(y + height - 1), width, height);
}

void DGL_Scissor(int x, int y, int width, int height)
{
    glScissor(x, FLIP(y + height - 1), width, height);
}

boolean DGL_GetIntegerv(int name, int *v)
{
    int         i;
    float       color[4];

    switch(name)
    {
    case DGL_MAX_TEXTURE_SIZE:
        *v = DGL_state.maxTexSize;
        break;

    case DGL_MAX_TEXTURE_UNITS:
        *v = DGL_state.maxTexUnits;
        break;

    case DGL_MODULATE_ADD_COMBINE:
        *v = DGL_state_ext.nvTexEnvComb || DGL_state_ext.atiTexEnvComb;
        break;

    case DGL_PALETTED_TEXTURES:
        *v = DGL_state_texture.usePalTex;
        break;

    case DGL_PALETTED_GENMIPS:
        // We are unable to generate mipmaps for paletted textures.
        *v = 0;
        break;

    case DGL_SCISSOR_TEST:
        glGetIntegerv(GL_SCISSOR_TEST, (GLint*) v);
        break;

    case DGL_SCISSOR_BOX:
        glGetIntegerv(GL_SCISSOR_BOX, (GLint*) v);
        v[1] = FLIP(v[1] + v[3] - 1);
        break;

    case DGL_FOG:
        *v = DGL_state.useFog;
        break;

    case DGL_CURRENT_COLOR_R:
        glGetFloatv(GL_CURRENT_COLOR, color);
        *v = (int) (color[0] * 255);
        break;

    case DGL_CURRENT_COLOR_G:
        glGetFloatv(GL_CURRENT_COLOR, color);
        *v = (int) (color[1] * 255);
        break;

    case DGL_CURRENT_COLOR_B:
        glGetFloatv(GL_CURRENT_COLOR, color);
        *v = (int) (color[2] * 255);
        break;

    case DGL_CURRENT_COLOR_A:
        glGetFloatv(GL_CURRENT_COLOR, color);
        *v = (int) (color[3] * 255);
        break;

    case DGL_CURRENT_COLOR_RGBA:
        glGetFloatv(GL_CURRENT_COLOR, color);
        for(i = 0; i < 4; i++)
            v[i] = (int) (color[i] * 255);
        break;

    case DGL_POLY_COUNT:
        *v = polyCounter;
        polyCounter = 0;
        break;

    case DGL_TEXTURE_BINDING:
        glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint*) v);
        break;

    case DGL_VSYNC:
#if WIN32
        *v = DGL_state_ext.wglSwapIntervalEXT? DGL_state.useVSync : -1;
#else
        *v = 1;
#endif
        break;

    case DGL_GRAY_MIPMAP:
        *v = (int) (DGL_state_texture.grayMipmapFactor * 255);
        break;

    default:
        return false;
    }

    return true;
}

int DGL_GetInteger(int name)
{
    int         values[10];

    DGL_GetIntegerv(name, values);
    return values[0];
}

boolean DGL_SetInteger(int name, int value)
{
    switch(name)
    {
    case DGL_ACTIVE_TEXTURE:
        activeTexture(GL_TEXTURE0 + value);
        break;

    case DGL_MODULATE_TEXTURE:
        switch(value)
        {
        case 0:
            // No modulation: just replace with texture.
            activeTexture(GL_TEXTURE0);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            break;

        case 1:
            // Normal texture modulation with primary color.
            activeTexture(GL_TEXTURE0);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            break;

        case 12:
            // Normal texture modulation on both stages. TU 1 modulates with
            // primary color, TU 2 with TU 1.
            activeTexture(GL_TEXTURE1);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            activeTexture(GL_TEXTURE0);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            break;

        case 2:
        case 3:
            // Texture modulation and interpolation.
            activeTexture(GL_TEXTURE1);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
            if(value == 2)
            {   // Used with surfaces that have a color.
                // TU 2: Modulate previous with primary color.
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PRIMARY_COLOR);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

            }
            else
            {   // Mode 3: Used with surfaces with no primary color.
                // TU 2: Pass through.
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
            }
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

            // TU 1: Interpolate between texture 1 and 2, using the constant
            // alpha as the factor.
            activeTexture(GL_TEXTURE0);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE1);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE0);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
            glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);

            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
            break;

        case 4:
            // Apply sector light, dynamic light and texture.
            envModMultiTex(true);
            break;

        case 5:
        case 10:
            // Sector light * texture + dynamic light.
            activeTexture(GL_TEXTURE1);
            envAddColoredAlpha(true, value == 5 ? GL_SRC_ALPHA : GL_SRC_COLOR);

            // Alpha remains unchanged.
            if(DGL_state_ext.nvTexEnvComb)
            {
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_ADD);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_ZERO);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA,
                          GL_ONE_MINUS_SRC_ALPHA);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PREVIOUS);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA, GL_ZERO);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_ALPHA, GL_SRC_ALPHA);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_ZERO);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_SRC_ALPHA);
            }
            else
            {
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
            }

            activeTexture(GL_TEXTURE0);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            break;

        case 6:
            // Simple dynlight addition (add to primary color).
            activeTexture(GL_TEXTURE0);
            envAddColoredAlpha(true, GL_SRC_ALPHA);
            break;

        case 7:
            // Dynlight addition without primary color.
            activeTexture(GL_TEXTURE0);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_ALPHA);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
            break;

        case 8:
        case 9:
            // Texture and Detail.
            activeTexture(GL_TEXTURE1);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 2);

            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

            activeTexture(GL_TEXTURE0);
            if(value == 8)
            {
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            }
            else
            {   // Mode 9: Ignore primary color.
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            }
            break;

        case 11:
            // Normal modulation, alpha of 2nd stage.
            // Tex0: texture
            // Tex1: shiny texture
            activeTexture(GL_TEXTURE1);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

            activeTexture(GL_TEXTURE0);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE1);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_TEXTURE0);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
            break;

        default:
            break;
        }
        break;

    case DGL_GRAY_MIPMAP:
        DGL_state_texture.grayMipmapFactor = value / 255.0f;
        break;

    default:
        return false;
    }

    return true;
}

float DGL_GetFloat(int name)
{
    switch(name)
    {
    case DGL_LINE_WIDTH:
        return DGL_state.currentLineWidth;

    default:
        return 0;
    }

    return 1;
}

boolean DGL_SetFloat(int name, float value)
{
    switch(name)
    {
    case DGL_LINE_WIDTH:
        DGL_state.currentLineWidth = value;
        glLineWidth(value);
        break;

    default:
        return false;
    }

    return true;
}

void DGL_EnableTexUnit(byte id)
{
    activeTexture(GL_TEXTURE0 + id);
    glEnable(GL_TEXTURE_2D);
}

void DGL_DisableTexUnit(byte id)
{
    activeTexture(GL_TEXTURE0 + id);
    glDisable(GL_TEXTURE_2D);

    // Implicit disabling of texcoord array.
    if(DGL_state.noArrays)
    {
        DGL_DisableArrays(0, 0, 1 << id);
    }
}

int DGL_Enable(int cap)
{
    switch(cap)
    {
    case DGL_TEXTURING:
#ifndef DRMESA
        glEnable(GL_TEXTURE_2D);
#endif
        break;

    case DGL_TEXTURE_COMPRESSION:
        DGL_state.allowCompression = true;
        break;

    case DGL_FOG:
        glEnable(GL_FOG);
        DGL_state.useFog = true;
        break;

    case DGL_SCISSOR_TEST:
        glEnable(GL_SCISSOR_TEST);
        break;

    case DGL_PALETTED_TEXTURES:
        enablePalTexExt(true);
        break;

    case DGL_VSYNC:
#ifdef WIN32
        if(DGL_state_ext.wglSwapIntervalEXT)
        {
            wglSwapIntervalEXT(1);
            DGL_state.useVSync = true;
        }
#endif
        break;

    default:
        return 0;
    }

    return 1;
}

void DGL_Disable(int cap)
{
    switch(cap)
    {
    case DGL_TEXTURING:
        glDisable(GL_TEXTURE_2D);
        break;

    case DGL_TEXTURE_COMPRESSION:
        DGL_state.allowCompression = false;
        break;

    case DGL_FOG:
        glDisable(GL_FOG);
        DGL_state.useFog = false;
        break;

    case DGL_SCISSOR_TEST:
        glDisable(GL_SCISSOR_TEST);
        break;

    case DGL_PALETTED_TEXTURES:
        enablePalTexExt(false);
        break;

    case DGL_VSYNC:
#ifdef WIN32
        if(DGL_state_ext.wglSwapIntervalEXT)
        {
            wglSwapIntervalEXT(0);
            DGL_state.useVSync = false;
        }
#endif
        break;

    default:
        break;
    }
}

void DGL_BlendOp(int op)
{
#ifndef UNIX
    if(!glBlendEquationEXT)
        return;
#endif
    glBlendEquationEXT(op ==
                       DGL_SUBTRACT ? GL_FUNC_SUBTRACT : op ==
                       DGL_REVERSE_SUBTRACT ? GL_FUNC_REVERSE_SUBTRACT :
                       GL_FUNC_ADD);
}

void DGL_BlendFunc(int param1, int param2)
{
    glBlendFunc(param1 == DGL_ZERO ? GL_ZERO : param1 ==
                DGL_ONE ? GL_ONE : param1 ==
                DGL_DST_COLOR ? GL_DST_COLOR : param1 ==
                DGL_ONE_MINUS_DST_COLOR ? GL_ONE_MINUS_DST_COLOR : param1
                == DGL_SRC_ALPHA ? GL_SRC_ALPHA : param1 ==
                DGL_ONE_MINUS_SRC_ALPHA ? GL_ONE_MINUS_SRC_ALPHA : param1
                == DGL_DST_ALPHA ? GL_DST_ALPHA : param1 ==
                DGL_ONE_MINUS_DST_ALPHA ? GL_ONE_MINUS_DST_ALPHA : param1
                ==
                DGL_SRC_ALPHA_SATURATE ? GL_SRC_ALPHA_SATURATE : GL_ZERO,
                param2 == DGL_ZERO ? GL_ZERO : param2 ==
                DGL_ONE ? GL_ONE : param2 ==
                DGL_SRC_COLOR ? GL_SRC_COLOR : param2 ==
                DGL_ONE_MINUS_SRC_COLOR ? GL_ONE_MINUS_SRC_COLOR : param2
                == DGL_SRC_ALPHA ? GL_SRC_ALPHA : param2 ==
                DGL_ONE_MINUS_SRC_ALPHA ? GL_ONE_MINUS_SRC_ALPHA : param2
                == DGL_DST_ALPHA ? GL_DST_ALPHA : param2 ==
                DGL_ONE_MINUS_DST_ALPHA ? GL_ONE_MINUS_DST_ALPHA :
                GL_ZERO);
}

void DGL_MatrixMode(int mode)
{
    glMatrixMode(mode == DGL_PROJECTION ? GL_PROJECTION :
                 mode == DGL_TEXTURE ? GL_TEXTURE :
                 GL_MODELVIEW);
}

void DGL_PushMatrix(void)
{
    glPushMatrix();

    if(glGetError() == GL_STACK_OVERFLOW)
    {
        Con_Error("DG_PushMatrix: Stack overflow.\nEnsure you have current OpenGL Drivers installed on you system.\n");
    }
}

void DGL_PopMatrix(void)
{
    glPopMatrix();

    if(glGetError() == GL_STACK_UNDERFLOW)
    {
        Con_Error("DG_PopMatrix: Stack underflow.\nEnsure you have current OpenGL Drivers installed on you system.\n");
    }
}

void DGL_LoadIdentity(void)
{
    glLoadIdentity();
}

void DGL_Translatef(float x, float y, float z)
{
    glTranslatef(x, y, z);
}

void DGL_Rotatef(float angle, float x, float y, float z)
{
    glRotatef(angle, x, y, z);
}

void DGL_Scalef(float x, float y, float z)
{
    glScalef(x, y, z);
}

void DGL_Ortho(float left, float top, float right, float bottom, float znear,
               float zfar)
{
    glOrtho(left, right, bottom, top, znear, zfar);
}

boolean DGL_Grab(int x, int y, int width, int height, gltexformat_t format, void *buffer)
{
    if(format != DGL_RGB)
        return false;

    // y+height-1 is the bottom edge of the rectangle. It's
    // flipped to change the origin.
    glReadPixels(x, FLIP(y + height - 1), width, height, GL_RGB,
                 GL_UNSIGNED_BYTE, buffer);
    return true;
}

#if 0 // Currently unused.
int DGL_Project(int num, gl_fc3vertex_t *inVertices,
               gl_fc3vertex_t *outVertices)
{
    GLdouble    modelMatrix[16], projMatrix[16];
    GLint       viewport[4];
    GLdouble    x, y, z;
    int         i, numOut;
    gl_fc3vertex_t *in = inVertices, *out = outVertices;

    if(num == 0)
        return 0;

    // Get the data we'll need in the operation.
    glGetDoublev(GL_MODELVIEW_MATRIX, modelMatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);
    glGetIntegerv(GL_VIEWPORT, viewport);
    for(i = numOut = 0; i < num; ++i, in++)
    {
        if(gluProject
           (in->pos[VX], in->pos[VY], in->pos[VZ], modelMatrix, projMatrix,
            viewport, &x, &y, &z) == GL_TRUE)
        {
            // A success: add to the out vertices.
            out->pos[VX] = (float) x;
            out->pos[VY] = (float) FLIP(y);
            out->pos[VZ] = (float) z;

            // Check that it's truly visible.
            if(out->pos[VX] < 0 || out->pos[VY] < 0 ||
               out->pos[VX] >= theWindow->width ||
               out->pos[VY] >= theWindow->height)
                continue;

            memcpy(out->color, in->color, sizeof(in->color));
            numOut++;
            out++;
        }
    }
    return numOut;
}
#endif
