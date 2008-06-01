/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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
 * h_refresh.c: - jHeretic specific.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "jheretic.h"

#include "hu_stuff.h"
#include "hu_menu.h"
#include "hu_pspr.h"
#include "am_map.h"
#include "g_common.h"
#include "r_common.h"
#include "d_net.h"
#include "f_infine.h"
#include "x_hair.h"
#include "g_controls.h"
#include "p_mapsetup.h"
#include "p_tick.h"

// MACROS ------------------------------------------------------------------

#define viewheight          Get(DD_VIEWWINDOW_HEIGHT)
#define SIZEFACT            (4)
#define SIZEFACT2           (16)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    R_SetAllDoomsdayFlags(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean finalestage;

extern float lookOffset;

extern const float defFontRGB[];
extern const float defFontRGB2[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

player_t *viewplayer;

gamestate_t wipegamestate = GS_DEMOSCREEN;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Creates the translation tables to map the green color ramp to gray,
 * brown, red.
 *
 * \note Assumes a given structure of the PLAYPAL. Could be read from a
 * lump instead.
 */
static void initTranslation(void)
{
    int         i;
    byte       *translationtables = (byte *)
                    DD_GetVariable(DD_TRANSLATIONTABLES_ADDRESS);

    // Fill out the translation tables.
    for(i = 0; i < 256; ++i)
    {
        if(i >= 225 && i <= 240)
        {
            translationtables[i] = 114 + (i - 225); // yellow
            translationtables[i + 256] = 145 + (i - 225); // red
            translationtables[i + 512] = 190 + (i - 225); // blue
        }
        else
        {
            translationtables[i] = translationtables[i + 256] =
                translationtables[i + 512] = i;
        }
    }
}

void R_InitRefresh(void)
{
    initTranslation();
}

/**
 * Draws a special filter over the screen.
 */
void R_DrawSpecialFilter(void)
{
    float       x, y, w, h;
    player_t   *player = &players[DISPLAYPLAYER];

    if(player->powers[PT_INVULNERABILITY] <= BLINKTHRESHOLD &&
       !(player->powers[PT_INVULNERABILITY] & 8))
        return;

    R_GetViewWindow(&x, &y, &w, &h);
    DGL_Disable(DGL_TEXTURING);
    if(cfg.ringFilter == 1)
    {
        DGL_BlendFunc(DGL_SRC_COLOR, DGL_SRC_COLOR);
        GL_DrawRect(x, y, w, h, .5f, .35f, .1f, 1);
    }
    else
    {
        DGL_BlendFunc(DGL_DST_COLOR, DGL_SRC_COLOR);
        GL_DrawRect(x, y, w, h, 0, 0, .6f, 1);
    }

    // Restore the normal rendering state.
    GL_BlendMode(BM_NORMAL);
    DGL_Enable(DGL_TEXTURING);
}

void R_DrawLevelTitle(int x, int y, float alpha, dpatch_t *font,
                      boolean center)
{
    int         strX;
    char       *lname, *lauthor;

    lname = P_GetMapNiceName();
    if(lname)
    {
        strX = x;
        if(center)
            strX -= M_StringWidth(lname, font) / 2;

        M_WriteText3(strX, y, lname, font,
                     defFontRGB[0], defFontRGB[1], defFontRGB[2], alpha,
                     false, 0);
        y += 20;
    }

    lauthor = (char *) DD_GetVariable(DD_MAP_AUTHOR);
    if(lauthor && stricmp(lauthor, "raven software"))
    {
        strX = x;
        if(center)
            strX -= M_StringWidth(lauthor, huFontA) / 2;

        M_WriteText3(strX, y, lauthor, huFontA, .5f, .5f, .5f, alpha,
                     false, 0);
    }
}

/**
 * Do not really change anything here, because Doomsday might be in the
 * middle of a refresh. The change will take effect next refresh.
 */
void R_SetViewSize(int blocks, int detail)
{
    cfg.setSizeNeeded = true;
    if(cfg.setBlocks != blocks && blocks > 10 && blocks < 13)
    {   // When going fullscreen, force a hud show event (to reset the timer).
        ST_HUDUnHide(HUE_FORCE);
    }
    cfg.setBlocks = blocks;
}

void H_Display(void)
{
    static boolean viewactivestate = false;
    static boolean menuactivestate = false;
    static gamestate_t oldgamestate = -1;
    player_t   *vplayer = &players[DISPLAYPLAYER];
    boolean     iscam = (vplayer->plr->flags & DDPF_CAMERA) != 0; // $democam
    float       x, y, w, h;
    boolean     mapHidesView;

    // $democam: can be set on every frame
    if(cfg.setBlocks > 10 || iscam)
    {
        // Full screen.
        R_SetViewWindowTarget(0, 0, 320, 200);
    }
    else
    {
        int w = cfg.setBlocks * 32;
        int h = cfg.setBlocks * (200 - SBARHEIGHT * cfg.statusbarScale / 20) / 10;
        R_SetViewWindowTarget(160 - (w / 2),
                              (200 - SBARHEIGHT * cfg.statusbarScale / 20 - h) / 2,
                              w, h);
    }

    R_GetViewWindow(&x, &y, &w, &h);
    R_ViewWindow((int) x, (int) y, (int) w, (int) h);

    switch(G_GetGameState())
    {
    case GS_LEVEL:
        if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
            break;

        if(!IS_CLIENT && levelTime < 2)
        {
            // Don't render too early; the first couple of frames
            // might be a bit unstable -- this should be considered
            // a bug, but since there's an easy fix...
            break;
        }

        mapHidesView =
            R_MapObscures(DISPLAYPLAYER, (int) x, (int) y, (int) w, (int) h);

        if(!(MN_CurrentMenuHasBackground() && Hu_MenuAlpha() >= 1) &&
           !mapHidesView)
        {   // Draw the player view.
            int         viewAngleOffset =
                ANGLE_MAX * -G_GetLookOffset(DISPLAYPLAYER);
            boolean     isFullbright =
                (vplayer->powers[PT_INVULNERABILITY] > BLINKTHRESHOLD) ||
                               (vplayer->powers[PT_INVULNERABILITY] & 8);

            if(IS_CLIENT)
            {
                // Server updates mobj flags in NetSv_Ticker.
                R_SetAllDoomsdayFlags();
            }

            // The view angle offset.
            DD_SetVariable(DD_VIEWANGLE_OFFSET, &viewAngleOffset);
            GL_SetFilter(vplayer->plr->filter);

            // How about fullbright?
            Set(DD_FULLBRIGHT, isFullbright);

            // Render the view with possible custom filters.
            R_RenderPlayerView(DISPLAYPLAYER);

            R_DrawSpecialFilter();

            // Crosshair.
            if(!iscam)
                X_Drawer();
        }

        // Draw the automap?
        AM_Drawer(DISPLAYPLAYER);
        break;

    default:
        break;
    }

    menuactivestate = Hu_MenuIsActive();
    viewactivestate = viewActive;
    oldgamestate = wipegamestate = G_GetGameState();
}

void H_Display2(void)
{
    switch(G_GetGameState())
    {
    case GS_LEVEL:
        if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
            break;

        if(!IS_CLIENT && levelTime < 2)
        {
            // Don't render too early; the first couple of frames
            // might be a bit unstable -- this should be considered
            // a bug, but since there's an easy fix...
            break;
        }

        // These various HUD's will be drawn unless Doomsday advises not to.
        if(DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
        {
            boolean     redrawsbar = false;

            // Draw HUD displays only visible when the automap is open.
            if(AM_IsMapActive(DISPLAYPLAYER))
                HU_DrawMapCounters();

            // Level information is shown for a few seconds in the
            // beginning of a level.
            if(cfg.levelTitle || actualLevelTime <= 6 * TICSPERSEC)
            {
                int         x, y;
                float       alpha = 1;

                if(actualLevelTime < 35)
                    alpha = actualLevelTime / 35.0f;
                if(actualLevelTime > 5 * 35)
                    alpha = 1 - (actualLevelTime - 5 * 35) / 35.0f;

                x = SCREENWIDTH / 2;
                y = 13;
                Draw_BeginZoom((1 + cfg.hudScale)/2, x, y);
                R_DrawLevelTitle(x, y, alpha, huFontB, true);
                Draw_EndZoom();
            }

            if((viewheight != 200))
                redrawsbar = true;

            // Do we need to render a full status bar at this point?
            if(!(AM_IsMapActive(DISPLAYPLAYER) && cfg.automapHudDisplay == 0))
            {
                player_t   *player = &players[DISPLAYPLAYER];
                boolean     iscam = (player->plr->flags & DDPF_CAMERA) != 0; // $democam

                if(!iscam)
                {
                    int         viewmode =
                        ((viewheight == 200)? (cfg.setBlocks - 10) : 0);

                    ST_Drawer(viewmode, redrawsbar); // $democam
                }
            }

            HU_Drawer();
        }
        break;

    case GS_INTERMISSION:
        IN_Drawer();
        break;

    case GS_WAITING:
        // Clear the screen while waiting, doesn't mess up the menu.
        //gl.Clear(DGL_COLOR_BUFFER_BIT);
        break;

    default:
        break;
    }

    // Draw pause pic (but not if InFine active).
    if(paused && !fiActive)
    {
        GL_DrawPatch(160, 4, W_GetNumForName("PAUSED"));
    }

    // InFine is drawn whenever active.
    FI_Drawer();

    // The menu is drawn whenever active.
    Hu_MenuDrawer();
}

/**
 * Updates the mobj flags used by Doomsday with the state of our local flags
 * for the given mobj.
 */
void R_SetDoomsdayFlags(mobj_t *mo)
{
    // Client mobjs can't be set here.
    if(IS_CLIENT && mo->ddFlags & DDMF_REMOTE)
        return;

    // Reset the flags for a new frame.
    mo->ddFlags &= DDMF_CLEAR_MASK;

    // Local objects aren't sent to clients.
    if(mo->flags & MF_LOCAL)
        mo->ddFlags |= DDMF_LOCAL;
    if(mo->flags & MF_SOLID)
        mo->ddFlags |= DDMF_SOLID;
    if(mo->flags & MF_NOGRAVITY)
        mo->ddFlags |= DDMF_NOGRAVITY;
    if(mo->flags2 & MF2_FLOATBOB)
        mo->ddFlags |= DDMF_NOGRAVITY | DDMF_BOB;
    if(mo->flags & MF_MISSILE)
    {   // Mace death balls are controlled by the server.
        //if(mo->type != MT_MACEFX4)
        mo->ddFlags |= DDMF_MISSILE;
    }
    if(mo->info && (mo->info->flags2 & MF2_ALWAYSLIT))
        mo->ddFlags |= DDMF_ALWAYSLIT;

    if(mo->flags2 & MF2_FLY)
        mo->ddFlags |= DDMF_FLY | DDMF_NOGRAVITY;

    // $democam: cameramen are invisible.
    if(P_IsCamera(mo))
        mo->ddFlags |= DDMF_DONTDRAW;

    if((mo->flags & MF_CORPSE) && cfg.corpseTime && mo->corpseTics == -1)
        mo->ddFlags |= DDMF_DONTDRAW;

    // Choose which ddflags to set.
    if(mo->flags2 & MF2_DONTDRAW)
    {
        mo->ddFlags |= DDMF_DONTDRAW;
        return; // No point in checking the other flags.
    }

    if(mo->flags2 & MF2_LOGRAV)
        mo->ddFlags |= DDMF_LOWGRAVITY;

    if(mo->flags & MF_BRIGHTSHADOW)
        mo->ddFlags |= DDMF_BRIGHTSHADOW;
    else if(mo->flags & MF_SHADOW)
        mo->ddFlags |= DDMF_ALTSHADOW;

    if(((mo->flags & MF_VIEWALIGN) && !(mo->flags & MF_MISSILE)) ||
       (mo->flags & MF_FLOAT) ||
       ((mo->flags & MF_MISSILE) && !(mo->flags & MF_VIEWALIGN)))
        mo->ddFlags |= DDMF_VIEWALIGN;

    mo->ddFlags |= (mo->flags & MF_TRANSLATION);
}

/**
 * Updates the status flags for all visible things.
 */
void R_SetAllDoomsdayFlags(void)
{
    uint        i;
    mobj_t     *iter;

    // Only visible things are in the sector thinglists, so this is good.
    for(i = 0; i < numsectors; ++i)
    {
        iter = P_GetPtr(DMU_SECTOR, i, DMT_MOBJS);

        while(iter)
        {
            R_SetDoomsdayFlags(iter);
            iter = iter->sNext;
        }
    }
}
