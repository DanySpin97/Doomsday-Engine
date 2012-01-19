/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Server Definitions.
 */

#ifndef __DOOMSDAY_SERVER_H__
#define __DOOMSDAY_SERVER_H__

#include "dd_def.h"

#define SV_VERSION          19
#define SV_WELCOME_STRING   "Doomsday "DOOMSDAY_VERSION_TEXT" Server (R19)"

// Anything closer than this is always taken into consideration when
// deltas are being generated.
#define CLOSE_MOBJ_DIST     512

// Anything farther than this will never be taken into consideration.
#define FAR_MOBJ_DIST       1500

extern int svMaxPlayers;
extern int allowFrames; // Allow sending of frames.
extern int frameInterval; // In tics.
extern int netRemoteUser; // The client who is currently logged in.
extern char* netPassword; // Remote login password.

void            Sv_Shutdown(void);
void            Sv_StartNetGame(void);
void            Sv_StopNetGame(void);
boolean         Sv_PlayerArrives(unsigned int nodeID, char* name);
void            Sv_PlayerLeaves(unsigned int nodeID);
void            Sv_Handshake(int playernum, boolean newplayer);
void            Sv_GetPackets(void);

/**
 * @param flags  @see consolePrintFlags
 */
void            Sv_SendText(int to, int flags, const char* text);

void            Sv_Ticker(timespan_t ticLength);
int             Sv_Latency(byte cmdTime);
void            Sv_Kick(int who);
void            Sv_GetInfo(serverinfo_t* info);
size_t          Sv_InfoToString(serverinfo_t* info, ddstring_t* msg);
boolean         Sv_StringToInfo(const char* valuePair, serverinfo_t* info);
int             Sv_GetNumPlayers(void);
int             Sv_GetNumConnected(void);
boolean         Sv_CheckBandwidth(int playerNumber);
boolean         Sv_CanTrustClientPos(int plrNum);

/**
 * Returns a unique id for material @a mat that can be passed on to clients.
 */
unsigned int Sv_IdForMaterial(material_t* mat);

#endif
