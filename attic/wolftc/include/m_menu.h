/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * m_menu.h: Menu widget stuff, episode selection and such.
 */

#ifndef __M_MENU__
#define __M_MENU__

#ifndef __WOLFTC__
#  error "Using WolfTC headers without __WOLFTC__"
#endif

#include "hu_stuff.h"
#include "d_event.h"

void            M_LoadData(void);
void            M_UnloadData(void);

void            M_StartMessage(char *string, void *routine, boolean input);
void            M_StopMessage(void);

boolean         MN_CurrentMenuHasBackground(void);

#endif
