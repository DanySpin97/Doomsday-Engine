/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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

/*
 * r_common.c : Common routines for refresh.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "am_map.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// View window current position.
static float windowX = 0;
static float windowY = 0;
static float windowWidth = 320;
static float windowHeight = 200;
static int targetX = 0, targetY = 0, targetWidth = 320, targetHeight = 200;
static int oldWindowX = 0, oldWindowY = 0, oldWindowWidth = 320, oldWindowHeight = 200;
static float windowPos = 0;

// CODE --------------------------------------------------------------------

void R_PrecachePSprites(void)
{
    int     i, k;
    int     pclass = players[consoleplayer].class;

    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        for(k = 0; k < NUMWEAPLEVELS; ++k)
        {
            pclass = players[consoleplayer].class;
            R_PrecacheSkinsForState(weaponinfo[i][pclass].mode[k].upstate);
            R_PrecacheSkinsForState(weaponinfo[i][pclass].mode[k].downstate);
            R_PrecacheSkinsForState(weaponinfo[i][pclass].mode[k].readystate);
            R_PrecacheSkinsForState(weaponinfo[i][pclass].mode[k].atkstate);
            R_PrecacheSkinsForState(weaponinfo[i][pclass].mode[k].flashstate);
#if __JHERETIC__ || __JHEXEN__
            R_PrecacheSkinsForState(weaponinfo[i][pclass].mode[k].holdatkstate);
#endif
        }
    }
}

void R_SetViewWindowTarget(int x, int y, int w, int h)
{
    if(x == targetX && y == targetY && w == targetWidth && h == targetHeight)
    {
        return;
    }

    oldWindowX = windowX;
    oldWindowY = windowY;
    oldWindowWidth = windowWidth;
    oldWindowHeight = windowHeight;

    // Restart the timer.
    windowPos = 0;

    targetX = x;
    targetY = y;
    targetWidth = w;
    targetHeight = h;
}

/*
 * Animates the game view window towards the target values.
 */
void R_ViewWindowTicker(void)
{
    windowPos += .4f;
    if(windowPos >= 1)
    {
        windowX = targetX;
        windowY = targetY;
        windowWidth = targetWidth;
        windowHeight = targetHeight;
    }
    else
    {
#define LERP(start, end, pos) (end * pos + start * (1 - pos))
        windowX = LERP(oldWindowX, targetX, windowPos);
        windowY = LERP(oldWindowY, targetY, windowPos);
        windowWidth = LERP(oldWindowWidth, targetWidth, windowPos);
        windowHeight = LERP(oldWindowHeight, targetHeight, windowPos);
#undef LERP
    }
}

void R_GetViewWindow(float* x, float* y, float* w, float* h)
{
    if(x) *x = windowX;
    if(y) *y = windowY;
    if(w) *w = windowWidth;
    if(h) *h = windowHeight;
}

boolean R_IsFullScreenViewWindow(void)
{
    return (windowWidth >= 320 && windowHeight >= 200 && windowX <= 0 &&
            windowY <= 0);
}

/**
 * Does the given display player's automap obscure the window completely?
 * NOTE: Window dimensions are in fixed scale {x} 0 - 320, {y} 0 - 200.
 *
 * @param playerid  Index of the player whose map to check.
 * @param x         Top left X coordinate.
 * @param y         Top left Y coordinate.
 * @param w         Width.
 * @param h         Height.
 *
 * @return          <code>true</code> if there is no point in the given
 *                  window which is even partially visible.
 */
boolean R_MapObscures(int playerid, int x, int y, int w, int h)
{
#define FIXXTOSCREENX(x) (scrwidth * (x / (float) SCREENWIDTH))
#define FIXYTOSCREENY(y) (scrheight * (y / (float) SCREENHEIGHT))

    boolean     retVal = false;

    // Perhaps the automap completely obscures the view?
    if(AM_IsMapActive(displayplayer))
    {
        float   alpha;

        AM_GetColorAndAlpha(displayplayer, AMO_BACKGROUND,
                                    NULL, NULL, NULL, &alpha);
        if(!(alpha < 1) && !(AM_GetGlobalAlpha(displayplayer) < 1))
        {
            if(AM_IsMapWindowInFullScreenMode(displayplayer))
            {
                retVal = true;
            }
            else
            {
                // We'll have to compare the dimensions.
                float       scrwidth = Get(DD_SCREEN_WIDTH);
                float       scrheight = Get(DD_SCREEN_HEIGHT);
                float       fx = FIXXTOSCREENX(x);
                float       fy = FIXYTOSCREENY(x);
                float       fw = FIXXTOSCREENX(w);
                float       fh = FIXYTOSCREENY(h);
                float       mx, my, mw, mh;

                AM_GetWindow(displayplayer, &mx, &my, &mw, &mh);
                if(mx >= fx && my >= fy && mw >= fw && mh >= fh)
                    retVal = true;
            }
        }
    }

    return retVal;

#undef FIXXTOSCREENX
#undef FIXYTOSCREENY
}
