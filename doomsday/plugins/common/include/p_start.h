/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * p_start.h: Common playsim code relating to the (re)spawn of map objects.
 */

#ifndef __COMMON_PLAYSTART_H__
#define __COMMON_PLAYSTART_H__

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__ || __WOLFTC__
# include "r_defs.h"
#else
# include "xddefs.h"
#endif

// Player spawn spots for deathmatch.
#define MAX_DM_STARTS       (16)

#if __JHERETIC__
extern spawnspot_t *maceSpots;
extern int maceSpotCount;
extern spawnspot_t *bossSpots;
extern int bossSpotCount;
#endif

extern spawnspot_t deathmatchStarts[];
extern spawnspot_t *deathmatchP;

extern spawnspot_t *playerStarts;
extern int numPlayerStarts;

void            P_Init(void);
int             P_RegisterPlayerStart(spawnspot_t *mthing);
void            P_FreePlayerStarts(void);
boolean         P_CheckSpot(int playernum, spawnspot_t *mthing,
                            boolean doTeleSpark);
boolean         P_FuzzySpawn(spawnspot_t *spot, int playernum,
                             boolean doTeleSpark);
spawnspot_t    *P_GetPlayerStart(int group, int pnum);
void            P_DealPlayerStarts(int group);
void            P_SpawnPlayers(void);

void            P_GetMapLumpName(int episode, int map, char *lumpName);

#endif
