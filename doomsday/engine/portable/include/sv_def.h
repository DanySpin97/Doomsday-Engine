/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * sv_def.h: Server Definitions
 */

#ifndef __DOOMSDAY_SERVER_H__
#define __DOOMSDAY_SERVER_H__

#include "dd_def.h"
#include "m_string.h"

#define SV_VERSION			7
#define SV_WELCOME_STRING   "Doomsday "DOOMSDAY_VERSION_TEXT" Server (R7)"

// Anything closer than this is always taken into consideration when
// deltas are being generated.
#define CLOSE_MOBJ_DIST		512

// Anything farther than this will never be taken into consideration.
#define FAR_MOBJ_DIST		1500

extern int      sv_maxPlayers;
extern int      allowFrames;	   // Allow sending of frames.
extern int      send_all_players;
extern int      frameInterval;	   // In tics.
extern int      net_remoteuser;	   // The client who is currently logged in.
extern char    *net_password;	   // Remote login password.

void            Sv_Shutdown(void);
void            Sv_StartNetGame(void);
boolean         Sv_PlayerArrives(unsigned int nodeID, char *name);
void            Sv_PlayerLeaves(unsigned int nodeID);
void            Sv_Handshake(int playernum, boolean newplayer);
void            Sv_GetPackets(void);
void            Sv_SendText(int to, int con_flags, char *text);
//void            Sv_FixLocalAngles(boolean clearFixAnglesFlag);
void            Sv_Ticker(void);
int             Sv_Latency(byte cmdtime);
void            Sv_Kick(int who);
void            Sv_GetInfo(serverinfo_t *info);
size_t          Sv_InfoToString(serverinfo_t *info, ddstring_t *msg);
boolean         Sv_StringToInfo(const char *valuePair, serverinfo_t *info);
int             Sv_GetNumPlayers(void);
int             Sv_GetNumConnected(void);
void            Sv_PlaceMobj(struct mobj_s *mo, float x, float y, float z, boolean onFloor);

boolean         Sv_CheckBandwidth(int playerNumber);

#endif
