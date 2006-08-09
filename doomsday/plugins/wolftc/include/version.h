/* DE1: $Id: version.h 3305 2006-06-11 17:00:36Z skyjake $
 * Copyright (C) 2005 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

#ifndef __JDOOM_VERSION_H__
#define __JDOOM_VERSION_H__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

// DOOM version

#ifndef WOLFTC_VER_ID
#  ifdef _DEBUG
#    define WOLFTC_VER_ID "+D Doomsday"
#  else
#    define WOLFTC_VER_ID "Doomsday"
#  endif
#endif

#define GAMENAMETEXT "WolfTC"

// My my, the names of these #defines are really well chosen...
#define VERSION_TEXT "1.15."DOOMSDAY_RELEASE_NAME
#define VERSIONTEXT "Version "VERSION_TEXT" "__DATE__" ("WOLFTC_VER_ID")"

// All the versions of Doom have different savegame IDs, but
// 500 will be the savegame base from now on.
#define SAVE_VERSION_BASE   500
#define SAVE_VERSION        (SAVE_VERSION_BASE + gamemode)

#endif
