/**
 * @file sector.h
 * Map Sector. @ingroup map
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_SECTOR
#define LIBDENG_MAP_SECTOR

#include "r_data.h"
#include "p_dmu.h"

/**
 * Update the origin of the sector according to the point defined by the center of
 * the sector's axis-aligned bounding box (which must be initialized before calling).
 *
 * @param sector  Sector instance.
 */
void Sector_UpdateOrigin(Sector* sector);

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param sector  Sector instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Sector_GetProperty(const Sector* sector, setargs_t* args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param sector  Sector instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Sector_SetProperty(Sector* sector, const setargs_t* args);

#endif /// LIBDENG_MAP_SECTOR
