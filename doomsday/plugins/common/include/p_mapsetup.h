/** @file p_mapsetup.h Common map setup routines.
 *
 * Management of extended map data objects (e.g., xlines) is done here.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_PLAYSIM_MAPSETUP_H
#define LIBCOMMON_PLAYSIM_MAPSETUP_H

#include "common.h"

// If true we are in the process of setting up a map.
DENG_EXTERN_C dd_bool mapSetup;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Doomsday calls this once a map change has completed to allow us (the game)
 * to do any finalization we need (such as spawning thinkers or cataloguing
 * secret areas).
 */
void P_FinalizeMapChange(Uri const *uri);

/**
 * Load the specified map.
 *
 * @param uri  URI e.g., "E1M1".
 */
void P_SetupMap(Uri const *uri);

/**
 * To be called to reset the local world state (e.g., when leaving a networked game).
 * Note that @ref P_SetupMap() calls this automatically when the current map changes.
 */
void P_ResetWorldState();

/**
 * @param mapUri  Identifier of the map to lookup the author of. Can be @c 0 in which
 *                case the author for the @em current map will be returned (if set).
 */
char const *P_MapAuthor(Uri const *mapUri, dd_bool supressGameAuthor);

/**
 * @param mapUri  Identifier of the map to lookup the title of. Can be @c 0 in which
 *                case the title for the @em current map will be returned (if set).
 */
char const *P_MapTitle(Uri const *mapUri);

/**
 * @param mapUri  Identifier of the map to lookup the title of. Can be @c 0 in which
 *                case the title for the @em current map will be returned (if set).
 */
patchid_t P_MapTitlePatch(Uri const *mapUri);

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
void P_FindSecrets(void);
#endif

void P_SpawnSectorMaterialOriginScrollers(void);

void P_SpawnSideMaterialOriginScrollers(void);

void P_SpawnAllMaterialOriginScrollers(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBCOMMON_PLAYSIM_MAPSETUP_H */
