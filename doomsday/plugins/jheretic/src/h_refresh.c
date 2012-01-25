/**\file h_refresh.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * Refresh - Heretic specific.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#include "jheretic.h"

#include "hu_stuff.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "hu_pspr.h"
#include "hu_log.h"
#include "am_map.h"
#include "g_common.h"
#include "r_common.h"
#include "d_net.h"
#include "x_hair.h"
#include "g_controls.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "hu_automap.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void R_SetAllDoomsdayFlags(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern const float defFontRGB[];
extern const float defFontRGB2[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float quitDarkenOpacity = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Draws a special filter over the screen.
 */
static void rendSpecialFilter(int player, const RectRaw* region)
{
    player_t* plr = players + player;
    const struct filter_s {
        float colorRGB[3];
        int blendArg1, blendArg2;
    } filters[] = {
        { { 0.0f, 0.0f,  0.6f }, DGL_DST_COLOR, DGL_SRC_COLOR },
        { { 0.5f, 0.35f, 0.1f }, DGL_SRC_COLOR, DGL_SRC_COLOR }
    };

    if(plr->powers[PT_INVULNERABILITY] <= BLINKTHRESHOLD &&
       !(plr->powers[PT_INVULNERABILITY] & 8))
        return;

    DGL_BlendFunc(filters[cfg.ringFilter == 1].blendArg1,
        filters[cfg.ringFilter == 1].blendArg2);
    DGL_DrawRectf2Color(region->origin.x, region->origin.y,
        region->size.width, region->size.height,
        filters[cfg.ringFilter == 1].colorRGB[CR],
        filters[cfg.ringFilter == 1].colorRGB[CG],
        filters[cfg.ringFilter == 1].colorRGB[CB], cfg.filterStrength);

    // Restore the normal rendering state.
    DGL_BlendMode(BM_NORMAL);
}

boolean R_ViewFilterColor(float rgba[4], int filter)
{
    if(!rgba)
        return false;

    // We have to choose the right color and alpha.
    if(filter >= STARTREDPALS && filter < STARTREDPALS + NUMREDPALS)
    {   // Red.
        rgba[CR] = 1;
        rgba[CG] = 0;
        rgba[CB] = 0;
        rgba[CA] = (deathmatch? 1.0f : cfg.filterStrength) * filter / 8.f; // Full red with filter 8.
        return true;
    }
    else if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS + NUMBONUSPALS)
    {   // Light Yellow.
        rgba[CR] = 1;
        rgba[CG] = 1;
        rgba[CB] = .5f;
        rgba[CA] = cfg.filterStrength * (filter - STARTBONUSPALS + 1) / 16.f;
        return true;
    }

    if(filter)
        Con_Message("R_ViewFilterColor: Real strange filter number: %d.\n", filter);

    return false;
}

/**
 * Sets the new palette based upon current values of player->damageCount
 * and player->bonusCount
 */
void R_UpdateViewFilter(int player)
{
    player_t* plr = players + player;
    int palette = 0;

    if(player < 0 || player >= MAXPLAYERS)
    {
#if _DEBUG
        Con_Message("Warning:R_UpdateViewFilter: Invalid player #%i, ignoring.\n", player);
#endif
        return;
    }

    // Not currently present?
    if(!plr->plr->inGame) return;

    if(plr->damageCount)
    {
        palette = (plr->damageCount + 7) >> 3;
        if(palette >= NUMREDPALS)
        {
            palette = NUMREDPALS - 1;
        }
        palette += STARTREDPALS;
    }
    else if(plr->bonusCount)
    {
        palette = (plr->bonusCount + 7) >> 3;
        if(palette >= NUMBONUSPALS)
        {
            palette = NUMBONUSPALS - 1;
        }
        palette += STARTBONUSPALS;
    }

    // $democam
    if(palette)
    {
        plr->plr->flags |= DDPF_VIEW_FILTER;
        R_ViewFilterColor(plr->plr->filterColor, palette);
    }
    else
    {
        plr->plr->flags &= ~DDPF_VIEW_FILTER;
    }
}

static void rendPlayerView(int player)
{
    player_t* plr = &players[player];
    float pspriteOffsetY;
    boolean isFullBright = (plr->powers[PT_INVULNERABILITY] > BLINKTHRESHOLD) ||
                            (plr->powers[PT_INVULNERABILITY] & 8);

    if(IS_CLIENT)
    {
        // Server updates mobj flags in NetSv_Ticker.
        R_SetAllDoomsdayFlags();
    }

    // View angles are updated with fractional ticks, so we can just use the current values.
    R_SetViewAngle(player, plr->plr->mo->angle + (int) (ANGLE_MAX * -G_GetLookOffset(player)));
    R_SetViewPitch(player, plr->plr->lookDir);

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

static void rendHUD(int player, const RectRaw* portGeometry)
{
    if(player < 0 || player >= MAXPLAYERS) return;
    if(G_GameState() != GS_MAP) return;
    if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME))) return;
    if(!DD_GetInteger(DD_GAME_DRAW_HUD_HINT)) return; // The engine advises not to draw any HUD displays.

    ST_Drawer(player);
    HU_DrawScoreBoard(player);
    Hu_MapTitleDrawer(portGeometry);
}

void H_DrawViewPort(int port, const RectRaw* portGeometry,
    const RectRaw* windowGeometry, int player, int layer)
{
    player_t* plr = players + player;

    if(layer != 0)
    {
        rendHUD(player, portGeometry);
        return;
    }

    switch(G_GameState())
    {
    case GS_MAP:
        if(!ST_AutomapObscures2(player, windowGeometry))
        {
            if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME))) return;

            rendPlayerView(player);
            rendSpecialFilter(player, windowGeometry);

            // Crosshair.
            if(!(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))) // $democam
                X_Drawer(player);
        }
        break;

    case GS_STARTUP:
        DGL_DrawRectf2Color(0, 0, portGeometry->size.width, portGeometry->size.height, 0, 0, 0, 1);
        break;

    default:
        break;
    }
}

void H_DrawWindow(const Size2Raw* windowSize)
{
    if(G_GameState() == GS_INTERMISSION)
    {
        IN_Drawer();
    }

    // Draw HUD displays; menu, messages.
    Hu_Drawer();

    if(G_QuitInProgress())
    {
        DGL_DrawRectf2Color(0, 0, 320, 200, 0, 0, 0, quitDarkenOpacity);
    }
}

/**
 * Updates the mobj flags used by Doomsday with the state of our local flags
 * for the given mobj.
 */
void R_SetDoomsdayFlags(mobj_t* mo)
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

    if(mo->flags & MF_BRIGHTSHADOW)
        mo->ddFlags |= DDMF_BRIGHTSHADOW;
    else if(mo->flags & MF_SHADOW)
        mo->ddFlags |= DDMF_ALTSHADOW;

    if(((mo->flags & MF_VIEWALIGN) && !(mo->flags & MF_MISSILE)) ||
       (mo->flags & MF_FLOAT) ||
       ((mo->flags & MF_MISSILE) && !(mo->flags & MF_VIEWALIGN)))
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
    mobj_t*             iter;

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
