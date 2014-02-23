/**\file g_game.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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
 * Top-level (common) game routines jHexen - specific.
 */

#ifndef LIBJHEXEN_G_GAME_H
#define LIBJHEXEN_G_GAME_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "gamerules.h"
#include "p_mobj.h"
#include "x_player.h"

DENG_EXTERN_C int gaSaveGameSaveSlot;
DENG_EXTERN_C int gaLoadGameSaveSlot;

DENG_EXTERN_C player_t players[MAXPLAYERS];

DENG_EXTERN_C dd_bool gameInProgress;
DENG_EXTERN_C uint gameEpisode;
DENG_EXTERN_C uint gameMap;
DENG_EXTERN_C Uri *gameMapUri;
DENG_EXTERN_C uint gameMapEntrance;
DENG_EXTERN_C GameRuleset gameRules;

DENG_EXTERN_C dd_bool paused;
DENG_EXTERN_C dd_bool precache;
DENG_EXTERN_C dd_bool customPal;

DENG_EXTERN_C uint nextMap;
DENG_EXTERN_C uint nextMapEntrance;
DENG_EXTERN_C dd_bool briefDisabled;

DENG_EXTERN_C int gsvMapMusic;

#ifdef __cplusplus
extern "C" {
#endif

void G_CommonShutdown(void);

void R_InitRefresh(void);
void R_GetTranslation(int plrClass, int plrColor, int *tclass, int *tmap);
void Mobj_UpdateTranslationClassAndMap(mobj_t *mo);

void G_PrintMapList(void);

int G_BriefingEnabled(Uri const *mapUri, ddfinale_t *fin);
int G_DebriefingEnabled(Uri const *mapUri, ddfinale_t *fin);

void G_QuitGame(void);

/// @return  @c true = loading is presently possible.
dd_bool G_IsLoadGamePossible(void);

/// @return  @c true = saving is presently possible.
dd_bool G_IsSaveGamePossible(void);

void G_CommonPreInit(void);
void G_CommonPostInit(void);

int G_GetInteger(int id);
void *G_GetVariable(int id);

void G_PlayerReborn(int player);
void G_DeathMatchSpawnPlayer(int playernum);
void G_DeferredPlayDemo(char *demo);
void G_DoPlayDemo(void);

void G_PlayDemo(char *name);
void G_TimeDemo(char *name);
void G_IntermissionDone(void);
void G_ScreenShot(void);
void G_DoReborn(int playernum);
void G_StopDemo(void);

void G_Ticker(timespan_t ticLength);

/// @return  @c true if the input event @a ev was eaten.
int G_PrivilegedResponder(event_t *ev);

/// @return  @c true if the input event @a ev was eaten.
int G_Responder(event_t *ev);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBJHEXEN_G_GAME_H */
