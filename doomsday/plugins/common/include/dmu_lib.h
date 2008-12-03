/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * DMU_lib.h: Helper routines for accessing the DMU API
 */

#ifndef __DMU_LIB_H__
#define __DMU_LIB_H__

#include "doomsday.h"

// DMU property aliases. For short-hand purposes:
#define DMU_TOP_MATERIAL        (DMU_TOP_OF_SIDEDEF | DMU_MATERIAL)
#define DMU_TOP_MATERIAL_OFFSET_X (DMU_TOP_OF_SIDEDEF | DMU_MATERIAL_OFFSET_X)
#define DMU_TOP_MATERIAL_OFFSET_Y (DMU_TOP_OF_SIDEDEF | DMU_MATERIAL_OFFSET_Y)
#define DMU_TOP_MATERIAL_OFFSET_XY (DMU_TOP_OF_SIDEDEF | DMU_MATERIAL_OFFSET_XY)
#define DMU_TOP_FLAGS           (DMU_TOP_OF_SIDEDEF | DMU_FLAGS)
#define DMU_TOP_COLOR           (DMU_TOP_OF_SIDEDEF | DMU_COLOR)
#define DMU_TOP_COLOR_RED       (DMU_TOP_OF_SIDEDEF | DMU_COLOR_RED)
#define DMU_TOP_COLOR_GREEN     (DMU_TOP_OF_SIDEDEF | DMU_COLOR_GREEN)
#define DMU_TOP_COLOR_BLUE      (DMU_TOP_OF_SIDEDEF | DMU_COLOR_BLUE)

#define DMU_MIDDLE_MATERIAL     (DMU_MIDDLE_OF_SIDEDEF | DMU_MATERIAL)
#define DMU_MIDDLE_MATERIAL_OFFSET_X (DMU_MIDDLE_OF_SIDEDEF | DMU_MATERIAL_OFFSET_X)
#define DMU_MIDDLE_MATERIAL_OFFSET_Y (DMU_MIDDLE_OF_SIDEDEF | DMU_MATERIAL_OFFSET_Y)
#define DMU_MIDDLE_MATERIAL_OFFSET_XY (DMU_MIDDLE_OF_SIDEDEF | DMU_MATERIAL_OFFSET_XY)
#define DMU_MIDDLE_FLAGS        (DMU_MIDDLE_OF_SIDEDEF | DMU_FLAGS)
#define DMU_MIDDLE_COLOR        (DMU_MIDDLE_OF_SIDEDEF | DMU_COLOR)
#define DMU_MIDDLE_COLOR_RED    (DMU_MIDDLE_OF_SIDEDEF | DMU_COLOR_RED)
#define DMU_MIDDLE_COLOR_GREEN  (DMU_MIDDLE_OF_SIDEDEF | DMU_COLOR_GREEN)
#define DMU_MIDDLE_COLOR_BLUE   (DMU_MIDDLE_OF_SIDEDEF | DMU_COLOR_BLUE)
#define DMU_MIDDLE_ALPHA        (DMU_MIDDLE_OF_SIDEDEF | DMU_ALPHA)
#define DMU_MIDDLE_BLENDMODE    (DMU_MIDDLE_OF_SIDEDEF | DMU_BLENDMODE)

#define DMU_BOTTOM_MATERIAL     (DMU_BOTTOM_OF_SIDEDEF | DMU_MATERIAL)
#define DMU_BOTTOM_MATERIAL_OFFSET_X (DMU_BOTTOM_OF_SIDEDEF | DMU_MATERIAL_OFFSET_X)
#define DMU_BOTTOM_MATERIAL_OFFSET_Y (DMU_BOTTOM_OF_SIDEDEF | DMU_MATERIAL_OFFSET_Y)
#define DMU_BOTTOM_MATERIAL_OFFSET_XY (DMU_BOTTOM_OF_SIDEDEF | DMU_MATERIAL_OFFSET_XY)
#define DMU_BOTTOM_FLAGS        (DMU_BOTTOM_OF_SIDEDEF | DMU_FLAGS)
#define DMU_BOTTOM_COLOR        (DMU_BOTTOM_OF_SIDEDEF | DMU_COLOR)
#define DMU_BOTTOM_COLOR_RED    (DMU_BOTTOM_OF_SIDEDEF | DMU_COLOR_RED)
#define DMU_BOTTOM_COLOR_GREEN  (DMU_BOTTOM_OF_SIDEDEF | DMU_COLOR_GREEN)
#define DMU_BOTTOM_COLOR_BLUE   (DMU_BOTTOM_OF_SIDEDEF | DMU_COLOR_BLUE)

#define DMU_FLOOR_HEIGHT        (DMU_FLOOR_OF_SECTOR | DMU_HEIGHT)
#define DMU_FLOOR_TARGET_HEIGHT (DMU_FLOOR_OF_SECTOR | DMU_TARGET_HEIGHT)
#define DMU_FLOOR_SPEED         (DMU_FLOOR_OF_SECTOR | DMU_SPEED)
#define DMU_FLOOR_MATERIAL      (DMU_FLOOR_OF_SECTOR | DMU_MATERIAL)
#define DMU_FLOOR_SOUND_ORIGIN  (DMU_FLOOR_OF_SECTOR | DMU_SOUND_ORIGIN)
#define DMU_FLOOR_FLAGS         (DMU_FLOOR_OF_SECTOR | DMU_FLAGS)
#define DMU_FLOOR_COLOR         (DMU_FLOOR_OF_SECTOR | DMU_COLOR)
#define DMU_FLOOR_COLOR_RED     (DMU_FLOOR_OF_SECTOR | DMU_COLOR_RED)
#define DMU_FLOOR_COLOR_GREEN   (DMU_FLOOR_OF_SECTOR | DMU_COLOR_GREEN)
#define DMU_FLOOR_COLOR_BLUE    (DMU_FLOOR_OF_SECTOR | DMU_COLOR_BLUE)
#define DMU_FLOOR_MATERIAL_OFFSET_X (DMU_FLOOR_OF_SECTOR | DMU_MATERIAL_OFFSET_X)
#define DMU_FLOOR_MATERIAL_OFFSET_Y (DMU_FLOOR_OF_SECTOR | DMU_MATERIAL_OFFSET_Y)
#define DMU_FLOOR_MATERIAL_OFFSET_XY (DMU_FLOOR_OF_SECTOR | DMU_MATERIAL_OFFSET_XY)

#define DMU_CEILING_HEIGHT      (DMU_CEILING_OF_SECTOR | DMU_HEIGHT)
#define DMU_CEILING_TARGET_HEIGHT (DMU_CEILING_OF_SECTOR | DMU_TARGET_HEIGHT)
#define DMU_CEILING_SPEED       (DMU_CEILING_OF_SECTOR | DMU_SPEED)
#define DMU_CEILING_MATERIAL    (DMU_CEILING_OF_SECTOR | DMU_MATERIAL)
#define DMU_CEILING_SOUND_ORIGIN (DMU_CEILING_OF_SECTOR | DMU_SOUND_ORIGIN)
#define DMU_CEILING_FLAGS       (DMU_CEILING_OF_SECTOR | DMU_FLAGS)
#define DMU_CEILING_COLOR       (DMU_CEILING_OF_SECTOR | DMU_COLOR)
#define DMU_CEILING_COLOR_RED   (DMU_CEILING_OF_SECTOR | DMU_COLOR_RED)
#define DMU_CEILING_COLOR_GREEN (DMU_CEILING_OF_SECTOR | DMU_COLOR_GREEN)
#define DMU_CEILING_COLOR_BLUE  (DMU_CEILING_OF_SECTOR | DMU_COLOR_BLUE)
#define DMU_CEILING_MATERIAL_OFFSET_X (DMU_CEILING_OF_SECTOR | DMU_MATERIAL_OFFSET_X)
#define DMU_CEILING_MATERIAL_OFFSET_Y (DMU_CEILING_OF_SECTOR | DMU_MATERIAL_OFFSET_Y)
#define DMU_CEILING_MATERIAL_OFFSET_XY (DMU_CEILING_OF_SECTOR | DMU_MATERIAL_OFFSET_XY)

linedef_t*  P_AllocDummyLine(void);
void        P_FreeDummyLine(linedef_t* line);

void        P_CopyLine(linedef_t* dest, linedef_t* src);
void        P_CopySector(sector_t* dest, sector_t* src);

float       P_SectorLight(sector_t* sector);
void        P_SectorSetLight(sector_t* sector, float level);
void        P_SectorModifyLight(sector_t* sector, float value);
void        P_SectorModifyLightx(sector_t* sector, fixed_t value);
void*       P_SectorSoundOrigin(sector_t* sector);

#endif
