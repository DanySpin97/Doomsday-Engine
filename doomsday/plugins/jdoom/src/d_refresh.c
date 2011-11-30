/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * d_refresh.c
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#include "jdoom.h"

#include "hu_menu.h"
#include "hu_stuff.h"
#include "hu_pspr.h"
#include "am_map.h"
#include "g_common.h"
#include "g_controls.h"
#include "r_common.h"
#include "d_net.h"
#include "f_infine.h"
#include "x_hair.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "rend_automap.h"

// MACROS ------------------------------------------------------------------

#define WINDOWHEIGHT        (Get(DD_VIEWWINDOW_HEIGHT))

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float quitDarkenOpacity = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Draws a special filter over the screen (e.g. the inversing filter used
 * when in god mode).
 */
void R_DrawSpecialFilter(int pnum)
{
    player_t*           player = &players[pnum];

    if(player->powers[PT_INVULNERABILITY])
    {
        float               x, y, w, h;
        float               max = 30;
        float               str, r, g, b;
        int                 t = player->powers[PT_INVULNERABILITY];

        if(t < max)
            str = t / max;
        else if(t < 4 * 32 && !(t & 8))
            str = .7f;
        else if(t > INVULNTICS - max)
            str = (INVULNTICS - t) / max;
        else
            str = 1; // Full inversion.

        // Draw an inversing filter.
        DGL_Disable(DGL_TEXTURING);
        DGL_BlendMode(BM_INVERSE);

        r = MINMAX_OF(0.f, str * 2, 1.f);
        g = MINMAX_OF(0.f, str * 2 - .4, 1.f);
        b = MINMAX_OF(0.f, str * 2 - .8, 1.f);

        R_GetViewWindow(&x, &y, &w, &h);
        DGL_DrawRect(x, y, w, h, r, g, b, 1);

        // Restore the normal rendering state.
        DGL_BlendMode(BM_NORMAL);
        DGL_Enable(DGL_TEXTURING);
    }
}

boolean R_GetFilterColor(float rgba[4], int filter)
{
    if(!rgba)
        return false;

    // We have to choose the right color and alpha.
    if(filter >= STARTREDPALS && filter < STARTREDPALS + NUMREDPALS)
    {   // Red.
        rgba[CR] = 1;
        rgba[CG] = 0;
        rgba[CB] = 0;
        rgba[CA] = (deathmatch? 1.0f : cfg.filterStrength) * filter / 9.f;
        return true;
    }

    if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS + NUMBONUSPALS)
    {   // Gold.
        rgba[CR] = 1;
        rgba[CG] = .8f;
        rgba[CB] = .5f;
        rgba[CA] = cfg.filterStrength * (filter - STARTBONUSPALS + 1) / 16.f;
        return true;
    }

    if(filter == 13) // RADIATIONPAL
    {   // Green.
        rgba[CR] = 0;
        rgba[CG] = .7f;
        rgba[CB] = 0;
        rgba[CA] = cfg.filterStrength * .25f;
        return true;
    }

    if(filter)
        Con_Message("R_GetFilterColor: Real strange filter number: %d.\n", filter);

    return false;
}

/**
 * Show map name and author.
 */
void R_DrawMapTitle(void)
{
    float alpha;
    int y = 12, mapnum;
    const char* lname, *lauthor;

    if(!cfg.mapTitle || actualMapTime > 6 * TICSPERSEC)
        return;

    // Make the text a bit smaller.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(160, y, 0);
    DGL_Scalef(.7f, .7f, 1);
    DGL_Translatef(-160, -y, 0);

    alpha = 1;
    if(actualMapTime < 35)
        alpha = actualMapTime / 35.0f;
    if(actualMapTime > 5 * 35)
        alpha = 1 - (actualMapTime - 5 * 35) / 35.0f;

    lname = P_GetMapNiceName();
    lauthor = P_GetMapAuthor(cfg.hideIWADAuthor);

    // Compose the mapnumber used to check the map name patches array.
    if(gameMode == commercial)
        mapnum = gameMap;
    else
        mapnum = (gameEpisode * 9) + gameMap;

    WI_DrawPatch(SCREENWIDTH / 2, y, 1, 1, 1, alpha,
                 &mapNamePatches[mapnum], lname, false, ALIGN_CENTER);
    y += 14;

    if(lauthor)
    {
        M_WriteText3(160 - M_StringWidth(lauthor, GF_FONTA) / 2, y,
                     lauthor, GF_FONTA, .5f, .5f, .5f, alpha, false,
                     true, 0);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

/**
 * Do not really change anything here, because Doomsday might be in the
 * middle of a refresh. The change will take effect next refresh.
 */
void R_SetViewSize(int blocks)
{
    cfg.setSizeNeeded = true;

    if(cfg.setBlocks != blocks && blocks > 10 && blocks < 13)
    {
        int                 i;

        // When going fullscreen, force a hud show event (to reset the timer).
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            ST_HUDUnHide(i, HUE_FORCE);
        }
    }

    cfg.setBlocks = blocks;
}

static void rendPlayerView(int player)
{
    player_t* plr = &players[player];
    float pspriteOffsetY;
    int isFullBright = ((plr->powers[PT_INFRARED] > 4 * 32) ||
                        (plr->powers[PT_INFRARED] & 8) ||
                        plr->powers[PT_INVULNERABILITY] > 30);

    if(IS_CLIENT)
    {
        // Server updates mobj flags in NetSv_Ticker.
        R_SetAllDoomsdayFlags();
    }

    /// @todo Each player needs their own view variables.
    /// @see R_UpdateConsoleView()
    /*
    viewPos[VX] = plr->plr->mo->pos[VX] + plr->viewOffset[VX];
    viewPos[VY] = plr->plr->mo->pos[VY] + plr->viewOffset[VY];
    viewPos[VZ] = plr->viewZ + plr->viewOffset[VZ];
    viewAngle = plr->plr->mo->angle + (int) (ANGLE_MAX * -G_GetLookOffset(player));
    viewPitch = plr->plr->lookDir;

    DD_SetVariable(DD_VIEW_X, &viewPos[VX]);
    DD_SetVariable(DD_VIEW_Y, &viewPos[VY]);
    DD_SetVariable(DD_VIEW_Z, &viewPos[VZ]);
    DD_SetVariable(DD_VIEW_ANGLE, &viewAngle);
    DD_SetVariable(DD_VIEW_PITCH, &viewPitch);
    */

    pspriteOffsetY = HU_PSpriteYOffset(plr);
    DD_SetVariable(DD_PSPRITE_OFFSET_Y, &pspriteOffsetY);

    // $democam
    GL_SetFilter((plr->plr->flags & DDPF_USE_VIEW_FILTER)? true : false);
    if(plr->plr->flags & DDPF_USE_VIEW_FILTER)
    {
        const float* color = plr->plr->filterColor;
        GL_SetFilterColor(color[CR], color[CG], color[CB], color[CA]);
    }

    // How about fullbright?
    DD_SetInteger(DD_FULLBRIGHT, isFullBright);

    // Render the view with possible custom filters.
    R_RenderPlayerView(player);
}

static void rendHUD(int player)
{
    player_t*           plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    if(G_GetGameState() != GS_MAP)
        return;

    if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
        return;

    plr = &players[player];

    // These various HUD's will be drawn unless Doomsday advises not to
    if(DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
    {
        automapid_t         map = AM_MapForPlayer(player);
        boolean             redrawsbar = false;

        // Draw HUD displays only visible when the automap is open.
        if(AM_IsActive(map))
            HU_DrawMapCounters();

        if(WINDOWHEIGHT != 200)
            redrawsbar = true;

        // Do we need to render a full status bar at this point?
        if(!(AM_IsActive(map) && cfg.automapHudDisplay == 0) &&
           !(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK)))
        {
            int viewmode = ((WINDOWHEIGHT == 200)? cfg.setBlocks - 10 : 0);

            ST_Drawer(player, viewmode, redrawsbar);
        }

        HU_Drawer(player);
    }
}

/**
 * Draws the in-viewport display.
 *
 * @param layer         @c 0 = bottom layer (before the viewport border).
 *                      @c 1 = top layer (after the viewport border).
 */
void D_Display(int layer)
{
    int player = DISPLAYPLAYER;
    player_t* plr = &players[player];
    float x, y, w, h;

    if(layer != 0)
    {
        rendHUD(player);
        return;
    }

    if(G_GetGameState() == GS_MAP)
    {
        // $democam: can be set on every frame.
        if(cfg.setBlocks > 10 || (P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK)))
        {   // Full screen.
            R_SetViewWindowTarget(0, 0, SCREENWIDTH, SCREENHEIGHT);
        }
        else
        {
            int w = cfg.setBlocks * 32;
            int h = cfg.setBlocks * (SCREENHEIGHT - ST_HEIGHT * cfg.statusbarScale/20) / 10;
            R_SetViewWindowTarget(SCREENWIDTH/2 - w/2, (SCREENHEIGHT - ST_HEIGHT * cfg.statusbarScale / 20 - h) / 2, w, h);
        }

        R_GetViewWindow(&x, &y, &w, &h);
    }
    else
    {
        x = 0;
        y = 0;
        w = SCREENWIDTH;
        h = SCREENHEIGHT;
    }

    R_SetViewWindow((int) x, (int) y, (int) w, (int) h);

    switch(G_GetGameState())
    {
    case GS_MAP:
        if(!(MN_CurrentMenuHasBackground() && Hu_MenuAlpha() >= 1) &&
           !R_MapObscures(player, (int) x, (int) y, (int) w, (int) h))
        {
            if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
                return;

            rendPlayerView(player);

            R_DrawSpecialFilter(player);

            // Crosshair.
            if(!(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))) // $democam
                X_Drawer(player);
        }

        // Draw the automap.
        AM_Drawer(player);
        break;
    case GS_STARTUP:
        DGL_Disable(DGL_TEXTURING);
        DGL_DrawRect(x, y, w, h, 0, 0, 0, 1);
        DGL_Enable(DGL_TEXTURING);
        break;
    default:
        break;
    }
}

/**
 * Draws the over-viewport display.
 */
void D_Display2(void)
{
    switch(G_GetGameState())
    {
    case GS_MAP:
        if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
            break;

        if(DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
        {
            // Map information is shown for a few seconds in the
            // beginning of a map.
            R_DrawMapTitle();
        }
        break;

    case GS_INTERMISSION:
        WI_Drawer();
        break;

    case GS_WAITING:
        //gl.Clear(DGL_COLOR_BUFFER_BIT);
        //M_WriteText2(5, 188, "WAITING... PRESS ESC FOR MENU", GF_FONTA, 1, 0, 0, 1);
        break;

    default:
        break;
    }

    // Draw pause pic (but not if InFine active).
    if(paused && !fiActive)
    {
        WI_DrawPatch(SCREENWIDTH /2, 4, 1, 1, 1, 1, &m_pause, NULL, false,
                     ALIGN_CENTER);
    }

    // InFine is drawn whenever active.
    FI_Drawer();

    // Draw HUD displays; menu, messages.
    Hu_Drawer();

    if(G_GetGameAction() == GA_QUIT)
    {
        DGL_Disable(DGL_TEXTURING);
        DGL_DrawRect(0, 0, 320, 200, 0, 0, 0, quitDarkenOpacity);
        DGL_Enable(DGL_TEXTURING);
    }
}

/**
 * Updates the mobj flags used by Doomsday with the state of our local flags
 * for the given mobj.
 */
void P_SetDoomsdayFlags(mobj_t *mo)
{
    // Client mobjs can't be set here.
    if(IS_CLIENT && (mo->ddFlags & DDMF_REMOTE))
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
    {
        mo->ddFlags |= DDMF_MISSILE;
    }
    if(mo->type == MT_LIGHTSOURCE)
        mo->ddFlags |= DDMF_ALWAYSLIT | DDMF_DONTDRAW;
    if(mo->info && (mo->info->flags2 & MF2_ALWAYSLIT))
        mo->ddFlags |= DDMF_ALWAYSLIT;

    if(mo->flags2 & MF2_FLY)
        mo->ddFlags |= DDMF_FLY | DDMF_NOGRAVITY;

    // $democam: cameramen are invisible
    if(P_MobjIsCamera(mo))
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

    /**
     * The torches often go into the ceiling. This'll prevent them from
     * 'jumping' when they do.
     *
     * \todo Add a thing def flag for this.
     */
    if(mo->type == MT_MISC41 || mo->type == MT_MISC42 || mo->type == MT_MISC43 || // tall torches
       mo->type == MT_MISC44 || mo->type == MT_MISC45 || mo->type == MT_MISC46)  // short torches
        mo->ddFlags |= DDMF_NOFITBOTTOM;

    if(mo->flags & MF_BRIGHTSHADOW)
        mo->ddFlags |= DDMF_BRIGHTSHADOW;
    else if(mo->flags & MF_SHADOW)
        mo->ddFlags |= DDMF_SHADOW;

    if(((mo->flags & MF_VIEWALIGN) && !(mo->flags & MF_MISSILE)) ||
       ((mo->flags & MF_MISSILE) && !(mo->flags & MF_VIEWALIGN)) ||
       (mo->flags & MF_FLOAT))
        mo->ddFlags |= DDMF_VIEWALIGN;

    if(mo->flags & MF_TRANSLATION)
        mo->tmap = (mo->flags & MF_TRANSLATION) >> MF_TRANSSHIFT;
}

/**
 * Updates the status flags for all visible things.
 */
void R_SetAllDoomsdayFlags(void)
{
    uint                i;
    mobj_t             *iter;

    // Only visible things are in the sector thinglists, so this is good.
    for(i = 0; i < numsectors; ++i)
    {
        for(iter = P_GetPtr(DMU_SECTOR, i, DMT_MOBJS); iter;
            iter = iter->sNext)
        {
            P_SetDoomsdayFlags(iter);
        }
    }
}
