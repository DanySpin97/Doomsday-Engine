/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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
 * p_maputl.h: Movement/collision utility functions jDoom64 - specific.
 */

#ifndef __P_MAPUTL_H__
#define __P_MAPUTL_H__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "p_mobj.h"

#define OPENRANGE           (*(float*) DD_GetVariable(DD_OPENRANGE))
#define OPENTOP             (*(float*) DD_GetVariable(DD_OPENTOP))
#define OPENBOTTOM          (*(float*) DD_GetVariable(DD_OPENBOTTOM))
#define LOWFLOOR            (*(float*) DD_GetVariable(DD_LOWFLOOR))

void            P_ApplyTorque(mobj_t *mo);

#endif
