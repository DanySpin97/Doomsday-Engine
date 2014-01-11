/**\file g_game.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * Top-level (common) game routines jHeretic - specific.
 */

#ifndef LIBJHERETIC_G_GAME_H
#define LIBJHERETIC_G_GAME_H

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "doomdef.h"
#include "h_event.h"
#include "h_player.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int gaSaveGameSaveSlot;
extern int gaLoadGameSaveSlot;

extern player_t players[MAXPLAYERS];

extern dd_bool gameInProgress;
extern skillmode_t gameSkill;
extern uint gameEpisode;
extern uint gameMap;
extern uint gameMapEntryPoint;

extern uint nextMap;
extern dd_bool secretExit;
extern int mapStartTic;
extern int totalKills, totalItems, totalSecret;
extern dd_bool deathmatch;
extern dd_bool respawnMonsters;
extern dd_bool paused;
extern dd_bool precache;
extern wbstartstruct_t wmInfo;
extern dd_bool customPal;
extern dd_bool briefDisabled;

extern int gsvMapMusic;

void            G_Register(void);
void            G_CommonPreInit(void);
void            G_CommonPostInit(void);
void            G_CommonShutdown(void);
void            R_InitRefresh(void);

void            G_DeathMatchSpawnPlayer(int playernum);

void            G_PrintMapList(void);

void            G_DeferredPlayDemo(char* demo);

void            G_QuitGame(void);

/// @return  @c true = loading is presently possible.
dd_bool G_IsLoadGamePossible(void);

/**
 * To be called to schedule a load game-save action.
 * @param slot  Logical identifier of the save slot to use.
 * @return  @c true iff @a saveSlot is in use and loading is presently possible.
 */
dd_bool G_LoadGame(int slot);

/// @return  @c true = saving is presently possible.
dd_bool G_IsSaveGamePossible(void);

/**
 * To be called to schedule a save game-save action.
 * @param slot  Logical identifier of the save slot to use.
 * @param name  New name for the game-save. Can be @c NULL in which case
 *      the name will not change if the slot has already been used.
 *      If an empty string a new name will be generated automatically.
 * @return  @c true iff @a saveSlot is valid and saving is presently possible.
 */
dd_bool G_SaveGame2(int slot, const char* name);
dd_bool G_SaveGame(int slot);

void            G_StopDemo(void);

int             G_BriefingEnabled(uint episode, uint map, ddfinale_t* fin);
int             G_DebriefingEnabled(uint episode, uint map, ddfinale_t* fin);

void            G_DoReborn(int playernum);
void            G_PlayerReborn(int player);

void            G_IntermissionDone(void);

void            G_Ticker(timespan_t ticLength);

/// @return  @c true if the input event @a ev was eaten.
int G_PrivilegedResponder(event_t* ev);

/// @return  @c true if the input event @a ev was eaten.
int G_Responder(event_t* ev);

void            G_ScreenShot(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBJHERETIC_G_GAME_H */
