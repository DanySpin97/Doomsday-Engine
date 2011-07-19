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

/**
 * p_setup.c: Common map setup routines.
 */

#ifndef LIBCOMMON_PLAYSETUP_H
#define LIBCOMMON_PLAYSETUP_H

#define numvertexes (*(uint*) DD_GetVariable(DD_VERTEX_COUNT))
#define numsegs     (*(uint*) DD_GetVariable(DD_SEG_COUNT))
#define numsectors  (*(uint*) DD_GetVariable(DD_SECTOR_COUNT))
#define numsubsectors (*(uint*) DD_GetVariable(DD_SUBSECTOR_COUNT))
#define numnodes    (*(uint*) DD_GetVariable(DD_NODE_COUNT))
#define numlines    (*(uint*) DD_GetVariable(DD_LINE_COUNT))
#define numsides    (*(uint*) DD_GetVariable(DD_SIDE_COUNT))
#define nummaterials (*(uint*) DD_GetVariable(DD_MATERIAL_COUNT))

#if __JHEXEN__
#define numpolyobjs (*(uint*) DD_GetVariable(DD_POLYOBJ_COUNT))
#endif

// If true we are in the process of setting up a map.
extern boolean mapSetup;

void P_SetupForMapData(int type, uint num);

void P_SetupMap(uint episode, uint map, int playermask, skillmode_t skill);

boolean P_IsMapFromIWAD(uint episode, uint map);
const char* P_GetMapNiceName(void);
patchid_t P_FindMapTitlePatch(uint episode, uint map);
const char* P_GetMapAuthor(boolean supressGameAuthor);

#endif /* LIBCOMMON_PLAYSETUP_H */
