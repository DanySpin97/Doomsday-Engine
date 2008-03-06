/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2008 Daniel Swanson <danij@dengine.net>
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
 * d_refresh.h:
 */

#ifndef __D_REFRESH_H__
#define __D_REFRESH_H__

#ifndef __DOOM64TC__
#  error "Using Doom64TC headers without __DOOM64TC__"
#endif

#include "p_mobj.h"

void            R_InitRefresh(void);

void            D_Display(void);
void            D_Display2(void);
void            R_SetViewSize(int blocks, int detail);

void            R_DrawSpecialFilter(void);
void            R_DrawLevelTitle(void);

void            P_SetDoomsdayFlags(mobj_t *mo);
void            R_SetAllDoomsdayFlags();

#endif
