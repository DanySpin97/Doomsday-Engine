/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/**
 * p_sound.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"

#include "dmu_lib.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Start the song for the current map.
 */
void S_LevelMusic(void)
{
    ddmapinfo_t         info;
    char                id[10];
    int                 songID = 0;

    if(G_GetGameState() != GS_LEVEL)
        return;

    sprintf(id, "E%iM%i", gameEpisode, gameMap);
    if(Def_Get(DD_DEF_MAP_INFO, id, &info) && info.music >= 0)
    {
        songID = info.music;
        S_StartMusicNum(songID, true);
    }
    else
    {
        songID = (gameEpisode - 1) * 9 + gameMap - 1;
        S_StartMusicNum(songID, true);
    }

    // Set the map music game status cvar.
    gsvMapMusic = songID;
}

/**
 * Doom-like sector sounds: when a new sound starts, stop any old ones
 * from the same origin.
 *
 * @param sec           Sector in which the sound should be played.
 * @param origin        Origin of the sound (center/floor/ceiling).
 * @param id            ID number of the sound to be played.
 */
void S_SectorSound(sector_t *sec, int origin, int id)
{
    mobj_t             *centerorigin, *floororigin, *ceilingorigin;

    centerorigin = (mobj_t *) P_GetPtrp(sec, DMU_SOUND_ORIGIN);
    floororigin = (mobj_t *) P_GetPtrp(sec, DMU_FLOOR_SOUND_ORIGIN);
    ceilingorigin = (mobj_t *) P_GetPtrp(sec, DMU_CEILING_SOUND_ORIGIN);

    S_StopSound(0, centerorigin);
    S_StopSound(0, floororigin);
    S_StopSound(0, ceilingorigin);

    switch(origin)
    {
    case SORG_FLOOR:
        S_StartSound(id, floororigin);
        break;

    case SORG_CEILING:
        S_StartSound(id, ceilingorigin);
        break;

    case SORG_CENTER:
    default:
        S_StartSound(id, centerorigin);
        break;
    }
}
