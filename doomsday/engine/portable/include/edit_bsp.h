/**
 * @file edit_bsp.h
 * Editable map BSP Builder. @ingroup map
 *
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_EDIT_BSP_H
#define LIBDENG_MAP_EDIT_BSP_H

#include "dd_types.h"

struct vertex_s;
struct gamemap_s;

#ifdef __cplusplus
extern "C" {
#endif

struct bspbuilder_c_s;
typedef struct bspbuilder_c_s BspBuilder_c;

// CVar for tuning the BSP edge split cost factor.
extern int bspFactor;

void BspBuilder_Register(void);

BspBuilder_c* BspBuilder_New(struct gamemap_s* map);

void BspBuilder_Delete(BspBuilder_c* builder);

BspBuilder_c* BspBuilder_SetSplitCostFactor(BspBuilder_c* builder, int factor);

/**
 * Build the BSP for the given map.
 *
 * @param map  The map to build the BSP for.
 *
 * @return  @c true= iff completed successfully.
 */
boolean BspBuilder_Build(BspBuilder_c* builder);

void MPE_SaveBsp(BspBuilder_c* builder, struct gamemap_s* map, struct vertex_s*** editableVertexes,
    uint* numEditableVertexes);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_MAP_EDIT_BSP_H
