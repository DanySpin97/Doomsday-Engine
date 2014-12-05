/** @file st_stuff.h  Heretic specific HUD and statusbar widgets.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBHERETIC_STUFF_H
#define LIBHERETIC_STUFF_H

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "hu_lib.h"
#include "h_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Size of statusbar, now sensitive for scaling.
#define ST_HEIGHT                   (42 * SCREEN_MUL)
#define ST_WIDTH                    (SCREENWIDTH)
#define ST_Y                        (SCREENHEIGHT - ST_HEIGHT)

#define ST_AUTOMAP_OBSCURE_TOLERANCE (.9999f)

/// Register the console commands, variables, etc..., of this module.
void ST_Register(void);

void ST_Init(void);
void ST_Shutdown(void);

int ST_Responder(event_t *ev);
void ST_Ticker(timespan_t ticLength);
void ST_Drawer(int player);

void ST_Start(int player);
void ST_Stop(int player);

void ST_CloseAll(int player, dd_bool fast);

uiwidget_t *ST_UIChatForPlayer(int player);
uiwidget_t *ST_UILogForPlayer(int player);
uiwidget_t *ST_UIAutomapForPlayer(int player);

dd_bool ST_ChatIsActive(int player);
dd_bool ST_StatusBarIsActive(int player);

/**
 * Post a message to the specified player's log.
 *
 * @param player  Player (local) number whose log to post to.
 * @param flags   @ref logMessageFlags
 * @param text    Message Text to be posted. Messages may include encoded parameters
 *                to control how the messae is presented.
 */
void ST_LogPost(int player, byte flags, char const *text);

/**
 * Rewind the message log of the specified player, making the last few messages
 * visible once again.
 *
 * @param player  Local player number whose message log to refresh.
 */
void ST_LogRefresh(int player);

/**
 * Empty the message log of the specified player.
 *
 * @param player  Local player number whose message log to empty.
 */
void ST_LogEmpty(int player);

void ST_LogUpdateAlignment(void);
void ST_LogPostVisibilityChangeNotification(void);

/**
 * Start the automap.
 */
void ST_AutomapOpen(int player, dd_bool yes, dd_bool fast);

dd_bool ST_AutomapIsActive(int player);

void ST_ToggleAutomapPanMode(int player);
void ST_ToggleAutomapMaxZoom(int player);

float ST_AutomapOpacity(int player);

/**
 * Does the player's automap obscure this region completely?
 * @pre Window dimensions use the fixed coordinate space {x} 0 - 320, {y} 0 - 200.
 *
 * @param player  Local player number whose automap to check.
 * @param region  Window region.
 *
 * @return  @true= there is no point even partially visible.
 */
dd_bool ST_AutomapObscures2(int player, RectRaw const *region);
dd_bool ST_AutomapObscures(int player, int x, int y, int width, int height);

int ST_AutomapAddPoint(int player, coord_t x, coord_t y, coord_t z);
void ST_AutomapClearPoints(int player);
dd_bool ST_AutomapPointOrigin(int player, int point, coord_t *x, coord_t *y, coord_t *z);

void ST_SetAutomapCameraRotation(int player, dd_bool on);

int ST_AutomapCheatLevel(int player);
void ST_SetAutomapCheatLevel(int player, int level);
void ST_CycleAutomapCheatLevel(int player);

void ST_RevealAutomap(int player, dd_bool on);
dd_bool ST_AutomapHasReveal(int player);

void ST_RebuildAutomap(int player);

/**
 * Unhides the current HUD display if hidden.
 *
 * @param player  Player whoose HUD to (maybe) unhide.
 * @param event  Event type trigger.
 */
void ST_HUDUnHide(int player, hueevent_t ev);

void ST_FlashCurrentItem(int player);

#ifdef __cplusplus
} // extern "C"
#endif

#endif  // LIBHERETIC_STUFF_H
