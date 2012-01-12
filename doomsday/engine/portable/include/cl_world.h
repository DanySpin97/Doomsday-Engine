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
 * cl_world.h: Clientside World Management
 */

#ifndef __DOOMSDAY_CLIENT_WORLD_H__
#define __DOOMSDAY_CLIENT_WORLD_H__

typedef enum {
    MVT_FLOOR,
    MVT_CEILING
} clmovertype_t;

void            Cl_WorldInit(void);
void            Cl_WorldReset(void);

/**
 * Handles the PSV_MATERIAL_ARCHIVE packet sent by the server. The list of
 * materials is stored until the client disconnects.
 */
void Cl_ReadServerMaterials(void);

void            Cl_SetPolyMover(uint number, int move, int rotate);
void            Cl_AddMover(uint sectornum, clmovertype_t type, float dest,
                            float speed);

void            Cl_ReadSectorDelta2(int deltaType, boolean skip);
void            Cl_ReadSideDelta2(int deltaType, boolean skip);
void            Cl_ReadPolyDelta2(boolean skip);

int             Cl_ReadSectorDelta(void); // obsolete
int             Cl_ReadSideDelta(void); // obsolete
int             Cl_ReadPolyDelta(void); // obsolete

#endif
