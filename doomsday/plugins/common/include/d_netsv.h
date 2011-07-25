/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_NETSV_H
#define LIBCOMMON_NETSV_H

#include "common.h"

extern char cyclingMaps, mapCycleNoExit;
extern int netSvAllowCheats;
extern char* mapCycle;
extern char gameConfigString[];

void            P_Telefrag(mobj_t* thing);

void            NetSv_NewPlayerEnters(int plrNum);
void            NetSv_ResetPlayerFrags(int plrNum);
void            NetSv_SendGameState(int flags, int to);
void            NetSv_SendPlayerSpawnPosition(int plrNum, float x, float y, float z, int angle);
void            NetSv_SendMessage(int plrNum, const char *msg);
void            NetSv_SendYellowMessage(int plrNum, const char *msg);
void            NetSv_SendPlayerState(int srcPlrNum, int destPlrNum, int flags,
                                      boolean reliable);
void            NetSv_SendPlayerState2(int srcPlrNum, int destPlrNum,
                                       int flags, boolean reliable);
void            NetSv_TellCycleRulesToPlayerAfterTics(int destPlr, int tics);
void            NetSv_Sound(mobj_t *origin, int sound_id, int toPlr);   // toPlr=0: broadcast.
void            NetSv_SoundAtVolume(mobj_t *origin, int sound_id, int volume,
                                    int toPlr);
void            NetSv_Intermission(int flags, int state, int time);

void            NetSv_SendPlayerInfo(int whose, int toWhom);
void            NetSv_ChangePlayerInfo(int from, byte* data);
void            NetSv_Ticker(void);
void            NetSv_SaveGame(unsigned int game_id);
void            NetSv_LoadGame(unsigned int game_id);
void            NetSv_LoadReply(int plnum, int console);
void            NetSv_FragsForAll(player_t* player);
void            NetSv_KillMessage(player_t* killer, player_t* fragged, boolean stomping);
void            NetSv_UpdateGameConfig(void);
void            NetSv_Paused(boolean isPaused);
void            NetSv_DoCheat(int player, const char *data);
void            NetSv_DoAction(int player, const char *data);
void            NetSv_DoDamage(int player, const char *data);
void            NetSv_SendJumpPower(int target, float power);

D_CMD(MapCycle);

#endif /* LIBCOMMON_NETSV_H */
