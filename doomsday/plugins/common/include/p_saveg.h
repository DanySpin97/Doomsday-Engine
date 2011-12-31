/**\file p_saveg.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * Common save game handling.
 */

#ifndef LIBCOMMON_SAVESTATE_H
#define LIBCOMMON_SAVESTATE_H

#include "p_svtexarc.h"

typedef struct gamesaveinfo_s {
    ddstring_t filePath;
    ddstring_t name;
    int slot;
} gamesaveinfo_t;

/// Register the console commands and variables of this module.
void SV_Register(void);

/// Initialize this module.
void SV_Init(void);

/// Shutdown this module.
void SV_Shutdown(void);

/**
 * Parse the given string and determine whether it references a logical
 * game-save slot.
 *
 * @param str  String to be parsed. Parse is divided into three passes.
 *   Pass 1: Check for a known game-save name which matches this.
 *      Search is in ascending logical slot order 0...N (where N is
 *      the number of available save slots in the current game).
 *   Pass 2: Check for keyword identifiers.
 *      <quick> = The currently nominated "quick save" slot.
 *   Pass 3: Check for a logical save slot identifier.
 *
 * @return  Save slot identifier of the slot else @c -1
 */
int SV_ParseGameSaveSlot(const char* str);

/**
 * Force an update of the cached game-save info. To be called (sparingly)
 * at strategic points when an update is necessary (e.g., the game-save
 * paths have changed).
 *
 * \note It is not necessary to call this after a game-save is made,
 * this module will do so automatically.
 */
void SV_UpdateGameSaveInfo(void);

/**
 * Lookup a save slot by searching for a match on game-save name.
 * Search is in ascending logical slot order 0...N (where N is the number
 * of available save slots in the current game).
 *
 * @param name  Name of the game-save to look for. Case insensitive.
 * @return  Logical slot number of the found game-save else @c -1
 */
int SV_FindGameSaveSlotForName(const char* name);

/**
 * @return  @c true iff a game-save is present for logical save @a slot.
 */
boolean SV_IsGameSaveSlotUsed(int slot);

/**
 * @return  Game-save info for logical save @a slot. Always returns valid
 *      info even if supplied with an invalid or unused slot identifer.
 */
const gamesaveinfo_t* SV_GetGameSaveInfoForSlot(int slot);

#if !__JHEXEN__
/**
 * Compose the (possibly relative) path to the game-save associated with
 * the unique @a gameId. If the game-save path is unreachable then
 * @a path will be made empty.
 *
 * @param gameId  Unique game identifier.
 * @param path  String buffer to populate with the game-save path.
 * @return  @c true if @a path was set.
 */
boolean SV_GetClientGameSavePathForGameId(uint gameId, ddstring_t* path);
#endif

boolean SV_SaveGame(int slot, const char* name);

boolean SV_LoadGame(int slot);

#if __JHEXEN__
void            SV_MapTeleport(uint map, uint position);
void            SV_HxInitBaseSlot(void);
void            SV_HxUpdateRebornSlot(void);
void            SV_HxClearRebornSlot(void);
boolean         SV_HxRebornSlotAvailable(void);
int             SV_HxGetRebornSlot(void);
#else
/**
 * Saves a snapshot of the world, a still image.
 * No data of movement is included (server sends it).
 */
void SV_SaveClient(uint gameId);

void SV_LoadClient(uint gameId);
#endif

typedef enum gamearchivesegment_e {
    ASEG_GAME_HEADER = 101, //jhexen only
    ASEG_MAP_HEADER, //jhexen only
    ASEG_WORLD,
    ASEG_POLYOBJS, //jhexen only
    ASEG_MOBJS, //jhexen < ver 4 only
    ASEG_THINKERS,
    ASEG_SCRIPTS, //jhexen only
    ASEG_PLAYERS,
    ASEG_SOUNDS, //jhexen only
    ASEG_MISC, //jhexen only
    ASEG_END,
    ASEG_MATERIAL_ARCHIVE,
    ASEG_MAP_HEADER2, //jhexen only
    ASEG_PLAYER_HEADER,
    ASEG_GLOBALSCRIPTDATA //jhexen only
} gamearchivesegment_t;

/**
 * Original indices must remain unchanged!
 * Added new think classes to the end.
 */
typedef enum thinkclass_e {
    TC_NULL = -1,
    TC_END,
    TC_MOBJ,
    TC_XGMOVER,
    TC_CEILING,
    TC_DOOR,
    TC_FLOOR,
    TC_PLAT,
#if __JHEXEN__
    TC_INTERPRET_ACS,
    TC_FLOOR_WAGGLE,
    TC_LIGHT,
    TC_PHASE,
    TC_BUILD_PILLAR,
    TC_ROTATE_POLY,
    TC_MOVE_POLY,
    TC_POLY_DOOR,
#else
    TC_FLASH,
    TC_STROBE,
# if __JDOOM__ || __JDOOM64__
    TC_GLOW,
    TC_FLICKER,
#  if __JDOOM64__
    TC_BLINK,
#  endif
# else
    TC_GLOW,
# endif
#endif
    TC_MATERIALCHANGER,
    NUMTHINKERCLASSES
} thinkerclass_t;

#if __JHEXEN__
int             SV_ThingArchiveNum(mobj_t* mo);
#else
unsigned short  SV_ThingArchiveNum(mobj_t* mo);
#endif
mobj_t*         SV_GetArchiveThing(int thingid, void* address);

materialarchive_t* SV_MaterialArchive(void);
material_t*     SV_GetArchiveMaterial(materialarchive_serialid_t serialId, int group);

/**
 * Exit with a fatal error if the value at the current location in the
 * game-save file does not match that associated with the segment type.
 *
 * @param segType  Segment type identifier to check alignment of.
 */
void SV_AssertSegment(int segType);

void            SV_BeginSegment(int segType);

void            SV_Write(const void* data, int len);
void            SV_WriteByte(byte val);
#if __JHEXEN__
void            SV_WriteShort(unsigned short val);
#else
void            SV_WriteShort(short val);
#endif
#if __JHEXEN__
void            SV_WriteLong(unsigned int val);
#else
void            SV_WriteLong(long val);
#endif
void            SV_WriteFloat(float val);
void            SV_Read(void* data, int len);
byte            SV_ReadByte(void);
short           SV_ReadShort(void);
long            SV_ReadLong(void);
float           SV_ReadFloat(void);

/**
 * Update mobj flag values from those used in legacy game-save formats
 * to their current values.
 *
 * To be called after loading a legacy game-save for each mobj loaded.
 *
 * @param mo  Mobj whoose flags are to be updated.
 * @param ver  The MOBJ save version to update from.
 */
void SV_UpdateReadMobjFlags(mobj_t* mo, int ver);

#endif /* LIBCOMMON_SAVESTATE_H */
