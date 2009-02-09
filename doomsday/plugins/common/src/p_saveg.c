/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * p_saveg.c: Save Game I/O
 *
 * \bug Not 64bit clean: In function 'SV_ReadPlayer': cast from pointer to integer of different size
 * \bug Not 64bit clean: In function 'SV_WriteMobj': cast from pointer to integer of different size
 * \bug Not 64bit clean: In function 'RestoreMobj': cast from pointer to integer of different size
 * \bug Not 64bit clean: In function 'SV_ReadMobj': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'P_UnArchiveThinkers': cast from pointer to integer of different size
 * \bug Not 64bit clean: In function 'P_UnArchiveBrain': cast to pointer from integer of different size, cast from pointer to integer of different size
 * \bug Not 64bit clean: In function 'P_UnArchiveSoundTargets': cast to pointer from integer of different size, cast from pointer to integer of different size
 */

// HEADER FILES ------------------------------------------------------------

#include <lzss.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "p_saveg.h"
#include "f_infine.h"
#include "d_net.h"
#include "p_svtexarc.h"
#include "dmu_lib.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_player.h"
#include "p_inventory.h"
#include "am_map.h"
#include "p_tick.h"
#include "p_actor.h"
#include "p_ceiling.h"
#include "p_door.h"
#include "p_floor.h"
#include "p_plat.h"
#include "hu_log.h"

// MACROS ------------------------------------------------------------------

#if __JDOOM__
# define MY_SAVE_MAGIC         0x1DEAD666
# define MY_CLIENT_SAVE_MAGIC  0x2DEAD666
# define MY_SAVE_VERSION       6
# define SAVESTRINGSIZE        24
# define CONSISTENCY           0x2c
# define SAVEGAMENAME          "DoomSav"
# define CLIENTSAVEGAMENAME    "DoomCl"
# define SAVEGAMEEXTENSION     "dsg"
#elif __JDOOM64__
# define MY_SAVE_MAGIC         0x1D6420F4
# define MY_CLIENT_SAVE_MAGIC  0x2D6420F4
# define MY_SAVE_VERSION       6
# define SAVESTRINGSIZE        24
# define CONSISTENCY           0x2c
# define SAVEGAMENAME          "D64Sav"
# define CLIENTSAVEGAMENAME    "D64Cl"
# define SAVEGAMEEXTENSION     "6sg"
#elif __JHERETIC__
# define MY_SAVE_MAGIC         0x7D9A12C5
# define MY_CLIENT_SAVE_MAGIC  0x1062AF43
# define MY_SAVE_VERSION       6
# define SAVESTRINGSIZE        24
# define CONSISTENCY           0x9d
# define SAVEGAMENAME          "HticSav"
# define CLIENTSAVEGAMENAME    "HticCl"
# define SAVEGAMEEXTENSION     "hsg"
#elif __JHEXEN__
# define HXS_VERSION_TEXT      "HXS Ver " // Do not change me!
# define HXS_VERSION_TEXT_LENGTH 16

# define MY_SAVE_VERSION       6
# define SAVESTRINGSIZE        24
# define SAVEGAMENAME          "Hex"
# define CLIENTSAVEGAMENAME    "HexenCl"
# define SAVEGAMEEXTENSION     "hxs"

# define MOBJ_XX_PLAYER        -2
# define MAX_MAPS              99
# define BASE_SLOT             6
# define REBORN_SLOT           7
# define REBORN_DESCRIPTION    "TEMP GAME"
#endif

#if !__JHEXEN__
# define PRE_VER5_END_SPECIALS   7
#endif

#define FF_FULLBRIGHT       0x8000 // used to be flag in thing->frame
#define FF_FRAMEMASK        0x7fff

// TYPES -------------------------------------------------------------------

#if !__JHEXEN__
typedef struct saveheader_s {
    int             magic;
    int             version;
    int             gameMode;
    char            description[SAVESTRINGSIZE];
    byte            skill;
    byte            episode;
    byte            map;
    byte            deathmatch;
    byte            noMonsters;
    byte            respawnMonsters;
    int             mapTime;
    byte            players[MAXPLAYERS];
    unsigned int    gameID;
} saveheader_t;
#endif

typedef struct playerheader_s {
    int             numPowers;
    int             numKeys;
    int             numFrags;
    int             numWeapons;
    int             numAmmoTypes;
    int             numPSprites;
#if __JHERETIC__ || __JHEXEN__
    int             numInvSlots;
#endif
#if __JHEXEN__
    int             numArmorTypes;
#endif
#if __JDOOM64__
    int             numArtifacts;
#endif
} playerheader_t;

typedef enum gamearchivesegment_e {
    ASEG_GAME_HEADER = 101, //jhexen only
    ASEG_MAP_HEADER,        //jhexen only
    ASEG_WORLD,
    ASEG_POLYOBJS,          //jhexen only
    ASEG_MOBJS,             //jhexen < ver 4 only
    ASEG_THINKERS,
    ASEG_SCRIPTS,           //jhexen only
    ASEG_PLAYERS,
    ASEG_SOUNDS,            //jhexen only
    ASEG_MISC,              //jhexen only
    ASEG_END,
    ASEG_MATERIAL_ARCHIVE,
    ASEG_MAP_HEADER2,       //jhexen only
    ASEG_PLAYER_HEADER
} gamearchivesegment_t;

#if __JHEXEN__
static union saveptr_u {
    byte        *b;
    short       *w;
    int         *l;
    float       *f;
} saveptr;

typedef struct targetplraddress_s {
    void     **address;
    struct targetplraddress_s *next;
} targetplraddress_t;
#endif

// Thinker Save flags
#define TSF_SERVERONLY      0x01    // Only saved by servers.
#define TSF_SPECIAL         0x02    // Its a special (eg T_MoveCeiling)

typedef struct thinkerinfo_s {
    thinkerclass_t  thinkclass;
    think_t         function;
    int             flags;
    void          (*Write) ();
    int           (*Read) ();
    size_t          size;
} thinkerinfo_t;

typedef enum sectorclass_e {
    sc_normal,
    sc_ploff,                   // plane offset
#if !__JHEXEN__
    sc_xg1
#endif
} sectorclass_t;

typedef enum lineclass_e {
    lc_normal,
#if !__JHEXEN__
    lc_xg1
#endif
} lineclass_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void SV_WriteMobj(mobj_t *mobj);
static int  SV_ReadMobj(thinker_t *th);
static void SV_WriteCeiling(ceiling_t* ceiling);
static int  SV_ReadCeiling(ceiling_t* ceiling);
static void SV_WriteDoor(door_t* door);
static int  SV_ReadDoor(door_t* door);
static void SV_WriteFloor(floor_t* floor);
static int  SV_ReadFloor(floor_t* floor);
static void SV_WritePlat(plat_t* plat);
static int  SV_ReadPlat(plat_t* plat);

#if __JHEXEN__
static void SV_WriteLight(light_t *light);
static int  SV_ReadLight(light_t *light);
static void SV_WritePhase(phase_t *phase);
static int  SV_ReadPhase(phase_t *phase);
static void SV_WriteScript(acs_t *script);
static int  SV_ReadScript(acs_t *script);
static void SV_WriteDoorPoly(polydoor_t *polydoor);
static int  SV_ReadDoorPoly(polydoor_t *polydoor);
static void SV_WriteMovePoly(polyevent_t *movepoly);
static int  SV_ReadMovePoly(polyevent_t *movepoly);
static void SV_WriteRotatePoly(polyevent_t *rotatepoly);
static int  SV_ReadRotatePoly(polyevent_t *rotatepoly);
static void SV_WritePillar(pillar_t *pillar);
static int  SV_ReadPillar(pillar_t *pillar);
static void SV_WriteFloorWaggle(waggle_t *floorwaggle);
static int  SV_ReadFloorWaggle(waggle_t *floorwaggle);
#else
static void SV_WriteFlash(lightflash_t* flash);
static int  SV_ReadFlash(lightflash_t* flash);
static void SV_WriteStrobe(strobe_t* strobe);
static int  SV_ReadStrobe(strobe_t* strobe);
static void SV_WriteGlow(glow_t* glow);
static int  SV_ReadGlow(glow_t* glow);
# if __JDOOM__ || __JDOOM64__
static void SV_WriteFlicker(fireflicker_t* flicker);
static int  SV_ReadFlicker(fireflicker_t* flicker);
# endif

# if __JDOOM64__
static void SV_WriteBlink(lightblink_t* flicker);
static int  SV_ReadBlink(lightblink_t* flicker);
# endif
#endif

#if __JHEXEN__
static void OpenStreamOut(char *fileName);
static void CloseStreamOut(void);

static void ClearSaveSlot(int slot);
static void CopySaveSlot(int sourceSlot, int destSlot);
static void CopyFile(char *sourceName, char *destName);
static boolean ExistingFile(char *name);

static void SV_HxLoadMap(void);
#else
static void SV_DMLoadMap(void);
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

#if __JHEXEN__
extern int ACScriptCount;
extern byte *ActionCodeBase;
extern acsinfo_t *ACSInfo;
#endif

// PUBLIC DATA DEFINITIONS -------------------------------------------------

LZFILE *savefile;
#if __JHEXEN__
char    savePath[256];        /* = "hexndata\\"; */
char    clientSavePath[256];  /* = "hexndata\\client\\"; */
#else
char    savePath[128];         /* = "savegame\\"; */
char    clientSavePath[128];  /* = "savegame\\client\\"; */
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#if __JHEXEN__
static int saveVersion;
#else
static saveheader_t hdr;
#endif
static playerheader_t playerHeader;
static boolean playerHeaderOK = false;
static mobj_t **thing_archive = NULL;
static uint thing_archiveSize = 0;
static int saveToRealPlayerNum[MAXPLAYERS];
#if __JHEXEN__
static targetplraddress_t *targetPlayerAddrs = NULL;
static byte *saveBuffer;
static boolean savingPlayers;
#else
static int numSoundTargets = 0;
#endif

static byte *junkbuffer;        // Old save data is read into here.

static thinkerinfo_t thinkerInfo[] = {
    {
      TC_MOBJ,
      P_MobjThinker,
      TSF_SERVERONLY,
      SV_WriteMobj,
      SV_ReadMobj,
      sizeof(mobj_t)
    },
#if !__JHEXEN__
    {
      TC_XGMOVER,
      XS_PlaneMover,
      0,
      SV_WriteXGPlaneMover,
      SV_ReadXGPlaneMover,
      sizeof(xgplanemover_t)
    },
#endif
    {
      TC_CEILING,
      T_MoveCeiling,
      TSF_SPECIAL,
      SV_WriteCeiling,
      SV_ReadCeiling,
      sizeof(ceiling_t)
    },
    {
      TC_DOOR,
      T_Door,
      TSF_SPECIAL,
      SV_WriteDoor,
      SV_ReadDoor,
      sizeof(door_t)
    },
    {
      TC_FLOOR,
      T_MoveFloor,
      TSF_SPECIAL,
      SV_WriteFloor,
      SV_ReadFloor,
      sizeof(floor_t)
    },
    {
      TC_PLAT,
      T_PlatRaise,
      TSF_SPECIAL,
      SV_WritePlat,
      SV_ReadPlat,
      sizeof(plat_t)
    },
#if __JHEXEN__
    {
     TC_INTERPRET_ACS,
     T_InterpretACS,
     0,
     SV_WriteScript,
     SV_ReadScript,
     sizeof(acs_t)
    },
    {
     TC_FLOOR_WAGGLE,
     T_FloorWaggle,
     0,
     SV_WriteFloorWaggle,
     SV_ReadFloorWaggle,
     sizeof(waggle_t)
    },
    {
     TC_LIGHT,
     T_Light,
     0,
     SV_WriteLight,
     SV_ReadLight,
     sizeof(light_t)
    },
    {
     TC_PHASE,
     T_Phase,
     0,
     SV_WritePhase,
     SV_ReadPhase,
     sizeof(phase_t)
    },
    {
     TC_BUILD_PILLAR,
     T_BuildPillar,
     0,
     SV_WritePillar,
     SV_ReadPillar,
     sizeof(pillar_t)
    },
    {
     TC_ROTATE_POLY,
     T_RotatePoly,
     0,
     SV_WriteRotatePoly,
     SV_ReadRotatePoly,
     sizeof(polyevent_t)
    },
    {
     TC_MOVE_POLY,
     T_MovePoly,
     0,
     SV_WriteMovePoly,
     SV_ReadMovePoly,
     sizeof(polyevent_t)
    },
    {
     TC_POLY_DOOR,
     T_PolyDoor,
     0,
     SV_WriteDoorPoly,
     SV_ReadDoorPoly,
     sizeof(polydoor_t)
    },
#else
    {
      TC_FLASH,
      T_LightFlash,
      TSF_SPECIAL,
      SV_WriteFlash,
      SV_ReadFlash,
      sizeof(lightflash_t)
    },
    {
      TC_STROBE,
      T_StrobeFlash,
      TSF_SPECIAL,
      SV_WriteStrobe,
      SV_ReadStrobe,
      sizeof(strobe_t)
    },
    {
      TC_GLOW,
      T_Glow,
      TSF_SPECIAL,
      SV_WriteGlow,
      SV_ReadGlow,
      sizeof(glow_t)
    },
# if __JDOOM__ || __JDOOM64__
    {
      TC_FLICKER,
      T_FireFlicker,
      TSF_SPECIAL,
      SV_WriteFlicker,
      SV_ReadFlicker,
      sizeof(fireflicker_t)
    },
# endif
# if __JDOOM64__
    {
      TC_BLINK,
      T_LightBlink,
      TSF_SPECIAL,
      SV_WriteBlink,
      SV_ReadBlink,
      sizeof(lightblink_t)
    },
# endif
#endif
    // Terminator
    { TC_NULL, NULL, 0, NULL, NULL, 0 }
};

// CODE --------------------------------------------------------------------

/**
 * Exit with a fatal error if the value at the current location in the save
 * file does not match that associated with the specified segment type.
 *
 * @param segType       Value by which to check for alignment.
 */
static void AssertSegment(int segType)
{
#if __JHEXEN__
    if(SV_ReadLong() != segType)
    {
        Con_Error("Corrupt save game: Segment [%d] failed alignment check",
                  segType);
    }
#endif
}

static void SV_BeginSegment(int segType)
{
#if __JHEXEN__
    SV_WriteLong(segType);
#endif
}

/**
 * @return              Ptr to the thinkerinfo for the given thinker.
 */
static thinkerinfo_t* infoForThinker(thinker_t* th)
{
    thinkerinfo_t*      thInfo = thinkerInfo;

    if(!th)
        return NULL;

    while(thInfo->thinkclass != TC_NULL)
    {
        if(thInfo->function == th->function)
            return thInfo;

        thInfo++;
    }

    return NULL;
}

static boolean removeThinker(thinker_t* th, void* context)
{
    if(th->function == P_MobjThinker)
        P_MobjRemove((mobj_t *) th, true);
    else
        Z_Free(th);

    return true; // Continue iteration.
}

typedef struct {
    uint                count;
    boolean             savePlayers;
} countmobjsparams_t;

static boolean countMobjs(thinker_t* th, void* context)
{
    countmobjsparams_t* params = (countmobjsparams_t*) context;
    mobj_t*             mo = (mobj_t*) th;

    if(!(mo->player && !params->savePlayers))
        params->count++;

    return true; // Continue iteration.
}

/**
 * Must be called before saving or loading any data.
 */
static uint SV_InitThingArchive(boolean load, boolean savePlayers)
{
    countmobjsparams_t  params;

    params.count = 0;
    params.savePlayers = savePlayers;

    if(load)
    {
#if !__JHEXEN__
        if(hdr.version < 5)
            params.count = 1024; // Limit in previous versions.
        else
#endif
            params.count = SV_ReadLong();
    }
    else
    {
        // Count the number of mobjs we'll be writing.
        P_IterateThinkers(P_MobjThinker, countMobjs, &params);
    }

    thing_archive = calloc(params.count, sizeof(mobj_t*));
    return thing_archiveSize = params.count;
}

/**
 * Used by the read code when mobjs are read.
 */
static void SV_SetArchiveThing(mobj_t *mo, int num)
{
#if __JHEXEN__
    if(saveVersion >= 4)
#endif
        num -= 1;

    if(num < 0)
        return;

    if(!thing_archive)
        Con_Error("SV_SetArchiveThing: Thing archive uninitialized.");

    thing_archive[num] = mo;
}

/**
 * Free the thing archive. Called when load is complete.
 */
static void SV_FreeThingArchive(void)
{
    if(thing_archive)
    {
        free(thing_archive);
        thing_archive = NULL;
        thing_archiveSize = 0;
    }
}

/**
 * Called by the write code to get archive numbers.
 * If the mobj is already archived, the existing number is returned.
 * Number zero is not used.
 */
#if __JHEXEN__
int SV_ThingArchiveNum(mobj_t *mo)
#else
unsigned short SV_ThingArchiveNum(mobj_t *mo)
#endif
{
    uint        i, first_empty = 0;
    boolean     found;

    // We only archive valid mobj thinkers.
    if(mo == NULL || ((thinker_t *) mo)->function != P_MobjThinker)
        return 0;

#if __JHEXEN__
    if(mo->player && !savingPlayers)
        return MOBJ_XX_PLAYER;
#endif

    if(!thing_archive)
        Con_Error("SV_ThingArchiveNum: Thing archive uninitialized.");

    found = false;
    for(i = 0; i < thing_archiveSize; ++i)
    {
        if(!thing_archive[i] && !found)
        {
            first_empty = i;
            found = true;
            continue;
        }
        if(thing_archive[i] == mo)
            return i + 1;
    }

    if(!found)
    {
        Con_Error("SV_ThingArchiveNum: Thing archive exhausted!\n");
        return 0;               // No number available!
    }

    // OK, place it in an empty pos.
    thing_archive[first_empty] = mo;
    return first_empty + 1;
}

#if __JHEXEN__
static void SV_FreeTargetPlayerList(void)
{
    targetplraddress_t *p = targetPlayerAddrs, *np;

    while(p != NULL)
    {
        np = p->next;
        free(p);
        p = np;
    }
    targetPlayerAddrs = NULL;
}
#endif

/**
 * Called by the read code to resolve mobj ptrs from archived thing ids
 * after all thinkers have been read and spawned into the map.
 */
mobj_t *SV_GetArchiveThing(int thingid, void *address)
{
#if __JHEXEN__
    if(thingid == MOBJ_XX_PLAYER)
    {
        targetplraddress_t *tpa = malloc(sizeof(targetplraddress_t));

        tpa->address = address;

        tpa->next = targetPlayerAddrs;
        targetPlayerAddrs = tpa;

        return NULL;
    }
#endif

    if(!thing_archive)
        Con_Error("SV_GetArchiveThing: Thing archive uninitialized.");

    // Check that the thing archive id is valid.
#if __JHEXEN__
    if(saveVersion < 4)
    {   // Old format is base 0.
        if(thingid == -1)
            return NULL; // A NULL reference.

        if(thingid < 0 || (uint) thingid > thing_archiveSize - 1)
            return NULL;
    }
    else
#endif
    {   // New format is base 1.
        if(thingid == 0)
            return NULL; // A NULL reference.

        if(thingid < 1 || (uint) thingid > thing_archiveSize)
        {
            Con_Message("SV_GetArchiveThing: Invalid NUM %i??\n", thingid);
            return NULL;
        }

        thingid -= 1;
    }

    return thing_archive[thingid];
}

static playerheader_t *GetPlayerHeader(void)
{
#if _DEBUG
    if(!playerHeaderOK)
        Con_Error("GetPlayerHeader: Attempted to read before init!");
#endif
    return &playerHeader;
}

unsigned int SV_GameID(void)
{
    return Sys_GetRealTime() + (mapTime << 24);
}

void SV_Write(void *data, int len)
{
    lzWrite(data, len, savefile);
}

void SV_WriteByte(byte val)
{
    lzPutC(val, savefile);
}

#if __JHEXEN__
void SV_WriteShort(unsigned short val)
#else
void SV_WriteShort(short val)
#endif
{
    lzPutW(val, savefile);
}

#if __JHEXEN__
void SV_WriteLong(unsigned int val)
#else
void SV_WriteLong(long val)
#endif
{
    lzPutL(val, savefile);
}

void SV_WriteFloat(float val)
{
#if __JHEXEN__
    lzPutL(*(int *) &val, savefile);
#else
    long temp = 0;
    memcpy(&temp, &val, 4);
    lzPutL(temp, savefile);
#endif
}

void SV_Read(void *data, int len)
{
#if __JHEXEN__
    memcpy(data, saveptr.b, len);
    saveptr.b += len;
#else
    lzRead(data, len, savefile);
#endif
}

byte SV_ReadByte(void)
{
#if __JHEXEN__
    return (*saveptr.b++);
#else
    return lzGetC(savefile);
#endif
}

short SV_ReadShort(void)
{
#if __JHEXEN__
    return (SHORT(*saveptr.w++));
#else
    return lzGetW(savefile);
#endif
}

long SV_ReadLong(void)
{
#if __JHEXEN__
    return (LONG(*saveptr.l++));
#else
    return lzGetL(savefile);
#endif
}

float SV_ReadFloat(void)
{
#if __JHEXEN__
    return (FLOAT(*saveptr.f++));
#else
    long    val = lzGetL(savefile);
    float   returnValue = 0;

    memcpy(&returnValue, &val, 4);
    return returnValue;
#endif
}

#if __JHEXEN__
static void OpenStreamOut(char *fileName)
{
    savefile = lzOpen(fileName, "wp");
}

static void CloseStreamOut(void)
{
    if(savefile)
    {
        lzClose(savefile);
    }
}

static boolean ExistingFile(char *name)
{
    FILE       *fp;

    if((fp = fopen(name, "rb")) != NULL)
    {
        fclose(fp);
        return true;
    }
    else
    {
        return false;
    }
}

/**
 * Deletes all save game files associated with a slot number.
 */
static void ClearSaveSlot(int slot)
{
    int         i;
    char        fileName[100];

    for(i = 0; i < MAX_MAPS; ++i)
    {
        sprintf(fileName, "%shex%d%02d.hxs", savePath, slot, i);
        M_TranslatePath(fileName, fileName);
        remove(fileName);
    }
    sprintf(fileName, "%shex%d.hxs", savePath, slot);
    M_TranslatePath(fileName, fileName);
    remove(fileName);
}

static void CopyFile(char *sourceName, char *destName)
{
    size_t      length;
    byte       *buffer;
    LZFILE     *outf;

    length = M_ReadFile(sourceName, &buffer);
    outf = lzOpen(destName, "wp");
    if(outf)
    {
        lzWrite(buffer, length, outf);
        lzClose(outf);
    }
    Z_Free(buffer);
}

/**
 * Copies all the save game files from one slot to another.
 */
static void CopySaveSlot(int sourceSlot, int destSlot)
{
    int         i;
    char        sourceName[100];
    char        destName[100];

    for(i = 0; i < MAX_MAPS; ++i)
    {
        sprintf(sourceName, "%shex%d%02d.hxs", savePath, sourceSlot, i);
        M_TranslatePath(sourceName, sourceName);

        if(ExistingFile(sourceName))
        {
            sprintf(destName, "%shex%d%02d.hxs", savePath, destSlot, i);
            M_TranslatePath(destName, destName);
            CopyFile(sourceName, destName);
        }
    }

    sprintf(sourceName, "%shex%d.hxs", savePath, sourceSlot);
    M_TranslatePath(sourceName, sourceName);

    if(ExistingFile(sourceName))
    {
        sprintf(destName, "%shex%d.hxs", savePath, destSlot);
        M_TranslatePath(destName, destName);
        CopyFile(sourceName, destName);
    }
}

/**
 * Copies the base slot to the reborn slot.
 */
void SV_HxUpdateRebornSlot(void)
{
    ClearSaveSlot(REBORN_SLOT);
    CopySaveSlot(BASE_SLOT, REBORN_SLOT);
}

void SV_HxClearRebornSlot(void)
{
    ClearSaveSlot(REBORN_SLOT);
}

int SV_HxGetRebornSlot(void)
{
    return (REBORN_SLOT);
}

/**
 * @return              @c true, if the reborn slot is available.
 */
boolean SV_HxRebornSlotAvailable(void)
{
    char        fileName[100];

    sprintf(fileName, "%shex%d.hxs", savePath, REBORN_SLOT);
    M_TranslatePath(fileName, fileName);
    return ExistingFile(fileName);
}

void SV_HxInitBaseSlot(void)
{
    ClearSaveSlot(BASE_SLOT);
}
#endif

/**
 * Writes the given player's data (not including the ID number).
 */
static void SV_WritePlayer(int playernum)
{
    int                 i, numPSprites = GetPlayerHeader()->numPSprites;
    player_t            temp, *p = &temp;
    ddplayer_t          ddtemp, *dp = &ddtemp;

    // Make a copy of the player.
    memcpy(p, &players[playernum], sizeof(temp));
    memcpy(dp, players[playernum].plr, sizeof(ddtemp));
    temp.plr = &ddtemp;

    // Convert the psprite states.
    for(i = 0; i < numPSprites; ++i)
    {
        pspdef_t       *pspDef = &temp.pSprites[i];

        if(pspDef->state)
        {
            pspDef->state = (state_t *) (pspDef->state - states);
        }
    }

    // Version number. Increase when you make changes to the player data
    // segment format.
    SV_WriteByte(5);

#if __JHEXEN__
    // Class.
    SV_WriteByte(cfg.playerClass[playernum]);
#endif

    SV_WriteLong(p->playerState);
#if __JHEXEN__
    SV_WriteLong(p->class);    // 2nd class...?
#endif
    SV_WriteLong(FLT2FIX(dp->viewZ));
    SV_WriteLong(FLT2FIX(dp->viewHeight));
    SV_WriteLong(FLT2FIX(dp->viewHeightDelta));
#if !__JHEXEN__
    SV_WriteFloat(dp->lookDir);
#endif
    SV_WriteLong(FLT2FIX(p->bob));
#if __JHEXEN__
    SV_WriteLong(p->flyHeight);
    SV_WriteFloat(dp->lookDir);
    SV_WriteLong(p->centering);
#endif
    SV_WriteLong(p->health);

#if __JHEXEN__
    for(i = 0; i < GetPlayerHeader()->numArmorTypes; ++i)
    {
        SV_WriteLong(p->armorPoints[i]);
    }
#else
    SV_WriteLong(p->armorPoints);
    SV_WriteLong(p->armorType);
#endif

#if __JHEXEN__
    for(i = 0; i < GetPlayerHeader()->numInvSlots; ++i)
    {
        SV_WriteLong(p->inventory[i].type);
        SV_WriteLong(p->inventory[i].count);
    }
    SV_WriteLong(p->readyArtifact);
    SV_WriteLong(p->inventorySlotNum);
#endif

    for(i = 0; i < GetPlayerHeader()->numPowers; ++i)
    {
        SV_WriteLong(p->powers[i]);
    }

#if __JHEXEN__
    SV_WriteLong(p->keys);
#else
    for(i = 0; i < GetPlayerHeader()->numKeys; ++i)
    {
        SV_WriteLong(p->keys[i]);
    }
#endif

#if __JHEXEN__
    SV_WriteLong(p->pieces);
#else
# if __JDOOM64__
    for(i = 0; i < GetPlayerHeader()->numArtifacts; ++i)
    {
        SV_WriteLong(p->artifacts[i]);
    }
# endif

    SV_WriteLong(p->backpack);
#endif

    for(i = 0; i < GetPlayerHeader()->numFrags; ++i)
    {
        SV_WriteLong(p->frags[i]);
    }

    SV_WriteLong(p->readyWeapon);
    SV_WriteLong(p->pendingWeapon);

    for(i = 0; i < GetPlayerHeader()->numWeapons; ++i)
    {
        SV_WriteLong(p->weapons[i].owned);
    }

    for(i = 0; i < GetPlayerHeader()->numAmmoTypes; ++i)
    {
        SV_WriteLong(p->ammo[i].owned);
#if !__JHEXEN__
        SV_WriteLong(p->ammo[i].max);
#endif
    }

    SV_WriteLong(p->attackDown);
    SV_WriteLong(p->useDown);

    SV_WriteLong(p->cheats);

    SV_WriteLong(p->refire);

    SV_WriteLong(p->killCount);
    SV_WriteLong(p->itemCount);
    SV_WriteLong(p->secretCount);

    SV_WriteLong(p->damageCount);
    SV_WriteLong(p->bonusCount);
#if __JHEXEN__
    SV_WriteLong(p->poisonCount);
#endif

    SV_WriteLong(dp->extraLight);
    SV_WriteLong(dp->fixedColorMap);
    SV_WriteLong(p->colorMap);

    for(i = 0; i < numPSprites; ++i)
    {
        pspdef_t       *psp = &p->pSprites[i];

        SV_WriteLong((int)psp->state);
        SV_WriteLong(psp->tics);
        SV_WriteLong(FLT2FIX(psp->pos[VX]));
        SV_WriteLong(FLT2FIX(psp->pos[VY]));
    }

#if !__JHEXEN__
    SV_WriteLong(p->didSecret);

    // Added in ver 2 with __JDOOM__
    SV_WriteLong(p->flyHeight);
#endif

#if __JHERETIC__
    for(i = 0; i < GetPlayerHeader()->numInvSlots; ++i)
    {
        SV_WriteLong(p->inventory[i].type);
        SV_WriteLong(p->inventory[i].count);
    }
    SV_WriteLong(p->readyArtifact);
    SV_WriteLong(p->inventorySlotNum);
    SV_WriteLong(p->chickenPeck);
#endif

#if __JHERETIC__ || __JHEXEN__
    SV_WriteLong(p->morphTics);
#endif

    SV_WriteLong(p->airCounter);

#if __JHEXEN__
    SV_WriteLong(p->jumpTics);
    SV_WriteLong(p->worldTimer);
#elif __JHERETIC__
    SV_WriteLong(p->flameCount);

    // Added in ver 2
    SV_WriteByte(p->class);
#endif
}

/**
 * Reads a player's data (not including the ID number).
 */
static void SV_ReadPlayer(player_t *p)
{
    int                 i, numPSprites = GetPlayerHeader()->numPSprites;
    byte                ver;
    ddplayer_t         *dp = p->plr;

    ver = SV_ReadByte();

#if __JHEXEN__
    cfg.playerClass[p - players] = SV_ReadByte();

    memset(p, 0, sizeof(*p));   // Force everything NULL,
    p->plr = dp;                // but restore the ddplayer pointer.
#endif

    p->playerState = SV_ReadLong();
#if __JHEXEN__
    p->class = SV_ReadLong();        // 2nd class...?
#endif
    dp->viewZ = FIX2FLT(SV_ReadLong());
    dp->viewHeight = FIX2FLT(SV_ReadLong());
    dp->viewHeightDelta = FIX2FLT(SV_ReadLong());
#if !__JHEXEN__
    dp->lookDir = SV_ReadFloat();
#endif
    p->bob = FIX2FLT(SV_ReadLong());
#if __JHEXEN__
    p->flyHeight = SV_ReadLong();
    dp->lookDir = SV_ReadFloat();
    p->centering = SV_ReadLong();
#endif

    p->health = SV_ReadLong();

#if __JHEXEN__
    for(i = 0; i < GetPlayerHeader()->numArmorTypes; ++i)
    {
        p->armorPoints[i] = SV_ReadLong();
    }
#else
    p->armorPoints = SV_ReadLong();
    p->armorType = SV_ReadLong();
#endif

#if __JHEXEN__
    for(i = 0; i < GetPlayerHeader()->numInvSlots; ++i)
    {
        p->inventory[i].type  = SV_ReadLong();
        p->inventory[i].count = SV_ReadLong();
    }
    p->readyArtifact = SV_ReadLong();
    if(ver < 5)
    {
        /*p->artifactCount =*/ SV_ReadLong();
    }
    p->inventorySlotNum = SV_ReadLong();
#endif

    for(i = 0; i < GetPlayerHeader()->numPowers; ++i)
    {
        p->powers[i] = SV_ReadLong();
    }
    if(p->powers[PT_ALLMAP])
        AM_RevealMap(AM_MapForPlayer(p - players), true);

#if __JHEXEN__
    p->keys = SV_ReadLong();
#else
    for(i = 0; i < GetPlayerHeader()->numKeys; ++i)
    {
        p->keys[i] = SV_ReadLong();
    }
#endif

#if __JHEXEN__
    p->pieces = SV_ReadLong();
#else
# if __JDOOM64__
    for(i = 0; i < GetPlayerHeader()->numArtifacts; ++i)
    {
        p->artifacts[i] = SV_ReadLong();
    }
# endif
    p->backpack = SV_ReadLong();
#endif

    for(i = 0; i < GetPlayerHeader()->numFrags; ++i)
    {
        p->frags[i] = SV_ReadLong();
    }

    p->readyWeapon = SV_ReadLong();
#if __JHEXEN__
    if(ver < 5)
        p->pendingWeapon = WT_NOCHANGE;
    else
#endif
        p->pendingWeapon = SV_ReadLong();

    for(i = 0; i < GetPlayerHeader()->numWeapons; ++i)
    {
        p->weapons[i].owned = (SV_ReadLong()? true : false);
    }

    for(i = 0; i < GetPlayerHeader()->numAmmoTypes; ++i)
    {
        p->ammo[i].owned = SV_ReadLong();

#if !__JHEXEN__
        p->ammo[i].max = SV_ReadLong();
#endif
    }

    p->attackDown = SV_ReadLong();
    p->useDown = SV_ReadLong();

    p->cheats = SV_ReadLong();

    p->refire = SV_ReadLong();

    p->killCount = SV_ReadLong();
    p->itemCount = SV_ReadLong();
    p->secretCount = SV_ReadLong();

#if __JHEXEN__
    if(ver <= 1)
    {
        /*p->messageTics =*/ SV_ReadLong();
        /*p->ultimateMessage =*/ SV_ReadLong();
        /*p->yellowMessage =*/ SV_ReadLong();
    }
#endif

    p->damageCount = SV_ReadLong();
    p->bonusCount = SV_ReadLong();
#if __JHEXEN__
    p->poisonCount = SV_ReadLong();
#endif

    dp->extraLight = SV_ReadLong();
    dp->fixedColorMap = SV_ReadLong();
    p->colorMap = SV_ReadLong();

    for(i = 0; i < numPSprites; ++i)
    {
        pspdef_t *psp = &p->pSprites[i];

        psp->state = (state_t*) SV_ReadLong();
        psp->tics = SV_ReadLong();
        psp->pos[VX] = FIX2FLT(SV_ReadLong());
        psp->pos[VY] = FIX2FLT(SV_ReadLong());
    }

#if !__JHEXEN__
    p->didSecret = SV_ReadLong();

# if __JDOOM__ || __JDOOM64__
    if(ver == 2) // nolonger used in >= ver 3
        /*p->messageTics =*/ SV_ReadLong();

    if(ver >= 2)
        p->flyHeight = SV_ReadLong();

# elif __JHERETIC__
    if(ver < 3) // nolonger used in >= ver 3
        /*p->messageTics =*/ SV_ReadLong();

    p->flyHeight = SV_ReadLong();
    for(i = 0; i < GetPlayerHeader()->numInvSlots; ++i)
    {
        p->inventory[i].type = SV_ReadLong();
        p->inventory[i].count = SV_ReadLong();
    }

    p->readyArtifact = SV_ReadLong();
    if(ver < 5)
    {
        /*p->artifactCount =*/ SV_ReadLong();
    }
    p->inventorySlotNum = SV_ReadLong();

    p->chickenPeck = SV_ReadLong();
# endif
#endif

#if __JHERETIC__ || __JHEXEN__
    p->morphTics = SV_ReadLong();
#endif

    if(ver >= 2)
        p->airCounter = SV_ReadLong();

#if __JHEXEN__
    p->jumpTics = SV_ReadLong();
    p->worldTimer = SV_ReadLong();
#elif __JHERETIC__
    p->flameCount = SV_ReadLong();

    if(ver >= 2)
        p->class = SV_ReadByte();
#endif

#if !__JHEXEN__
    // Will be set when unarc thinker.
    p->plr->mo = NULL;
    p->attacker = NULL;
#endif

    // Demangle it.
    for(i = 0; i < numPSprites; ++i)
        if(p->pSprites[i].state)
        {
            p->pSprites[i].state = &states[(int) p->pSprites[i].state];
        }

    // Mark the player for fixpos and fixangles.
    dp->flags |= DDPF_FIXPOS | DDPF_FIXANGLES | DDPF_FIXMOM;
    p->update |= PSF_REBORN;
}

#if __JHEXEN__
# define MOBJ_SAVEVERSION 6
#elif __JHERETIC__
# define MOBJ_SAVEVERSION 8
#else
# define MOBJ_SAVEVERSION 7
#endif

static void SV_WriteMobj(mobj_t *original)
{
    mobj_t              temp, *mo = &temp;

    memcpy(mo, original, sizeof(*mo));
    // Mangle it!
    mo->state = (state_t *) (mo->state - states);
    if(mo->player)
        mo->player = (player_t *) ((mo->player - players) + 1);

    // Version.
    // JHEXEN
    // 2: Added the 'translucency' byte.
    // 3: Added byte 'vistarget'
    // 4: Added long 'tracer'
    // 4: Added long 'lastenemy'
    // 5: Added flags3
    // 6: Floor material removed.
    //
    // JDOOM || JHERETIC || JDOOM64
    // 4: Added byte 'translucency'
    // 5: Added byte 'vistarget'
    // 5: Added tracer in jDoom
    // 5: Added dropoff fix in jHeretic
    // 5: Added long 'floorclip'
    // 6: Added proper respawn data
    // 6: Added flags 2 in jDoom
    // 6: Added damage
    // 7: Added generator in jHeretic
    // 7: Added flags3
    //
    // JHERETIC
    // 8: Added special3
    SV_WriteByte(MOBJ_SAVEVERSION);

#if !__JHEXEN__
    // A version 2 features: archive number and target.
    SV_WriteShort(SV_ThingArchiveNum(original));
    SV_WriteShort(SV_ThingArchiveNum(mo->target));

# if __JDOOM__ || __JDOOM64__
    // Ver 5 features: Save tracer (fixes Archvile, Revenant bug)
    SV_WriteShort(SV_ThingArchiveNum(mo->tracer));
# endif

    SV_WriteShort(SV_ThingArchiveNum(mo->onMobj));
#endif

    // Info for drawing: position.
    SV_WriteLong(FLT2FIX(mo->pos[VX]));
    SV_WriteLong(FLT2FIX(mo->pos[VY]));
    SV_WriteLong(FLT2FIX(mo->pos[VZ]));

    //More drawing info: to determine current sprite.
    SV_WriteLong(mo->angle); // Orientation.
    SV_WriteLong(mo->sprite); // Used to find patch_t and flip value.
    SV_WriteLong(mo->frame);

#if !__JHEXEN__
    // The closest interval over all contacted Sectors.
    SV_WriteLong(FLT2FIX(mo->floorZ));
    SV_WriteLong(FLT2FIX(mo->ceilingZ));
#endif

    // For movement checking.
    SV_WriteLong(FLT2FIX(mo->radius));
    SV_WriteLong(FLT2FIX(mo->height));

    // Momentums, used to update position.
    SV_WriteLong(FLT2FIX(mo->mom[MX]));
    SV_WriteLong(FLT2FIX(mo->mom[MY]));
    SV_WriteLong(FLT2FIX(mo->mom[MZ]));

    // If == VALIDCOUNT, already checked.
    SV_WriteLong(mo->valid);

    SV_WriteLong(mo->type);
#if __JHEXEN__
    SV_WriteLong((int) mo->info);
#endif

    SV_WriteLong(mo->tics); // State tic counter.
    SV_WriteLong((int) mo->state);

#if __JHEXEN__
    SV_WriteLong(mo->damage);
#endif

    SV_WriteLong(mo->flags);
#if __JHEXEN__
    SV_WriteLong(mo->flags2);
    SV_WriteLong(mo->flags3);

    if(mo->type == MT_KORAX)
        SV_WriteLong(0); // Searching index.
    else
        SV_WriteLong(mo->special1);

    switch(mo->type)
    {
    case MT_LIGHTNING_FLOOR:
    case MT_LIGHTNING_ZAP:
    case MT_HOLY_TAIL:
    case MT_LIGHTNING_CEILING:
        if(mo->flags & MF_CORPSE)
            SV_WriteLong(0);
        else
            SV_WriteLong(SV_ThingArchiveNum((mobj_t *) mo->special2));
        break;

    default:
        SV_WriteLong(mo->special2);
        break;
    }
#endif
    SV_WriteLong(mo->health);

    // Movement direction, movement generation (zig-zagging).
    SV_WriteLong(mo->moveDir); // 0-7
    SV_WriteLong(mo->moveCount); // When 0, select a new dir.

#if __JHEXEN__
    if(mo->flags & MF_CORPSE)
        SV_WriteLong(0);
    else
        SV_WriteLong((int) SV_ThingArchiveNum(mo->target));
#endif

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    SV_WriteLong(mo->reactionTime);

    // If >0, the target will be chased no matter what (even if shot).
    SV_WriteLong(mo->threshold);

    // Additional info record for player avatars only (only valid if type
    // == MT_PLAYER).
    SV_WriteLong((int) mo->player);

    // Player number last looked for.
    SV_WriteLong(mo->lastLook);

#if !__JHEXEN__
    // For nightmare/multiplayer respawn.
    SV_WriteLong(FLT2FIX(mo->spawnSpot.pos[VX]));
    SV_WriteLong(FLT2FIX(mo->spawnSpot.pos[VY]));
    SV_WriteLong(FLT2FIX(mo->spawnSpot.pos[VZ]));
    SV_WriteLong(mo->spawnSpot.angle);
    SV_WriteLong(mo->spawnSpot.type);
    SV_WriteLong(mo->spawnSpot.flags);

    SV_WriteLong(mo->intFlags); // $dropoff_fix: internal flags.
    SV_WriteLong(FLT2FIX(mo->dropOffZ)); // $dropoff_fix
    SV_WriteLong(mo->gear); // Used in torque simulation.

    SV_WriteLong(mo->damage);
    SV_WriteLong(mo->flags2);
    SV_WriteLong(mo->flags3);
# ifdef __JHERETIC__
    SV_WriteLong(mo->special1);
    SV_WriteLong(mo->special2);
    SV_WriteLong(mo->special3);
# endif

    SV_WriteByte(mo->translucency);
    SV_WriteByte((byte)(mo->visTarget +1));
#endif

    SV_WriteLong(FLT2FIX(mo->floorClip));
#if __JHEXEN__
    SV_WriteLong(SV_ThingArchiveNum(original));
    SV_WriteLong(mo->tid);
    SV_WriteLong(mo->special);
    SV_Write(mo->args, sizeof(mo->args));
    SV_WriteByte(mo->translucency);
    SV_WriteByte((byte)(mo->visTarget +1));

    switch(mo->type)
    {
    case MT_BISH_FX:
    case MT_HOLY_FX:
    case MT_DRAGON:
    case MT_THRUSTFLOOR_UP:
    case MT_THRUSTFLOOR_DOWN:
    case MT_MINOTAUR:
    case MT_SORCFX1:
    case MT_MSTAFF_FX2:
    case MT_HOLY_TAIL:
    case MT_LIGHTNING_CEILING:
        if(mo->flags & MF_CORPSE)
            SV_WriteLong(0);
        else
            SV_WriteLong(SV_ThingArchiveNum(mo->tracer));
        break;

    default:
# if _DEBUG
if(mo->tracer != NULL)
Con_Error("SV_WriteMobj: Mobj using tracer. Possibly saved incorrectly.");
# endif
        SV_WriteLong((int) mo->tracer);
        break;
    }

    SV_WriteLong((int) mo->lastEnemy);
#elif __JHERETIC__
    // Ver 7 features: generator
    SV_WriteShort(SV_ThingArchiveNum(mo->generator));
#endif
}

/**
 * Fixes the mobj flags in older save games to the current values.
 *
 * Called after loading a save game where the mobj format is older than
 * the current version.
 *
 * @param mo            Ptr to the mobj whoose flags are to be updated.
 * @param ver           The MOBJ save version to update from.
 */
#if !__JDOOM64__
void SV_UpdateReadMobjFlags(mobj_t *mo, int ver)
{
#if __JDOOM__ || __JHERETIC__
    if(ver < 6)
    {
        // mobj.flags
# if __JDOOM__
        // switched values for MF_BRIGHTSHADOW <> MF_BRIGHTEXPLODE
        if((mo->flags & MF_BRIGHTEXPLODE) != (mo->flags & MF_BRIGHTSHADOW))
        {
            if(mo->flags & MF_BRIGHTEXPLODE) // previously MF_BRIGHTSHADOW
            {
                mo->flags |= MF_BRIGHTSHADOW;
                mo->flags &= ~MF_BRIGHTEXPLODE;
            }
            else // previously MF_BRIGHTEXPLODE
            {
                mo->flags |= MF_BRIGHTEXPLODE;
                mo->flags &= ~MF_BRIGHTSHADOW;
            }
        } // else they were both on or off so it doesn't matter.
# endif
        // Remove obsoleted flags in earlier save versions.
        mo->flags &= ~MF_V6OBSOLETE;

        // mobj.flags2
# if __JDOOM__
        // jDoom only gained flags2 in ver 6 so all we can do is to
        // apply the values as set in the mobjinfo.
        // Non-persistent flags might screw things up a lot worse otherwise.
        mo->flags2 = mo->info->flags2;
# endif
    }
#endif

#if __JHEXEN__
    if(ver < 5)
#else
    if(ver < 7)
#endif
    {
        // flags3 was introduced in a latter version so all we can do is to
        // apply the values as set in the mobjinfo.
        // Non-persistent flags might screw things up a lot worse otherwise.
        mo->flags3 = mo->info->flags3;
    }
}
#endif

static void RestoreMobj(mobj_t *mo, int ver)
{
    mo->info = &mobjInfo[mo->type];

    P_MobjSetState(mo, (int) mo->state);

    if(mo->player)
    {
        // The player number translation table is used to find out the
        // *current* (actual) player number of the referenced player.
        int     pNum = saveToRealPlayerNum[(int) mo->player - 1];

#if __JHEXEN__
        if(pNum < 0)
        {
            // This saved player does not exist in the current game!
            // This'll make the mobj unarchiver destroy this mobj.
            Z_Free(mo);

            return;  // Don't add this thinker.
        }
#endif

        mo->player = &players[pNum];
        mo->dPlayer = mo->player->plr;
        mo->dPlayer->mo = mo;
        //mo->dPlayer->clAngle = mo->angle; /* $unifiedangles */
        mo->dPlayer->lookDir = 0; /* $unifiedangles */
    }

    mo->visAngle = mo->angle >> 16;

#if !__JHEXEN__
    if(mo->dPlayer && !mo->dPlayer->inGame)
    {
        if(mo->dPlayer)
            mo->dPlayer->mo = NULL;
        P_MobjDestroy(mo);

        return;
    }
#endif

#if !__JDOOM64__
    // Do we need to update this mobj's flag values?
    if(ver < MOBJ_SAVEVERSION)
        SV_UpdateReadMobjFlags(mo, ver);
#endif

    P_MobjSetPosition(mo);
    mo->floorZ =
        P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT);
    mo->ceilingZ =
        P_GetFloatp(mo->subsector, DMU_CEILING_HEIGHT);

    return;
}

/**
 * Always returns @c false as a thinker will have already been allocated in
 * the mobj creation process.
 */
static int SV_ReadMobj(thinker_t *th)
{
    int         ver;
    mobj_t     *mo = (mobj_t*) th;

    ver = SV_ReadByte();

#if !__JHEXEN__
    if(ver >= 2)
    {
        // Version 2 has mobj archive numbers.
        SV_SetArchiveThing(mo, SV_ReadShort());
        // The reference will be updated after all mobjs are loaded.
        mo->target = (mobj_t *) (int) SV_ReadShort();
    }
    else
        mo->target = NULL;

    // Ver 5 features:
    if(ver >= 5)
    {
# if __JDOOM__ || __JDOOM64__
        // Tracer for enemy attacks (updated after all mobjs are loaded).
        mo->tracer = (mobj_t *) (int) SV_ReadShort();
# endif
        // mobj this one is on top of (updated after all mobjs are loaded).
        mo->onMobj = (mobj_t *) (int) SV_ReadShort();
    }
    else
    {
# if __JDOOM__ || __JDOOM64__
        mo->tracer = NULL;
# endif
        mo->onMobj = NULL;
    }
#endif

    // Info for drawing: position.
    mo->pos[VX] = FIX2FLT(SV_ReadLong());
    mo->pos[VY] = FIX2FLT(SV_ReadLong());
    mo->pos[VZ] = FIX2FLT(SV_ReadLong());

    //More drawing info: to determine current sprite.
    mo->angle = SV_ReadLong();  // orientation
    mo->sprite = SV_ReadLong(); // used to find patch_t and flip value
    mo->frame = SV_ReadLong();  // might be ORed with FF_FULLBRIGHT
    if(mo->frame & FF_FULLBRIGHT)
        mo->frame &= FF_FRAMEMASK; // not used anymore.

#if __JHEXEN__
    if(ver < 6)
        SV_ReadLong(); // Used to be floorflat.
#else
    // The closest interval over all contacted Sectors.
    mo->floorZ = FIX2FLT(SV_ReadLong());
    mo->ceilingZ = FIX2FLT(SV_ReadLong());
#endif

    // For movement checking.
    mo->radius = FIX2FLT(SV_ReadLong());
    mo->height = FIX2FLT(SV_ReadLong());

    // Momentums, used to update position.
    mo->mom[MX] = FIX2FLT(SV_ReadLong());
    mo->mom[MY] = FIX2FLT(SV_ReadLong());
    mo->mom[MZ] = FIX2FLT(SV_ReadLong());

    // If == VALIDCOUNT, already checked.
    mo->valid = SV_ReadLong();
    mo->type = SV_ReadLong();
#if __JHEXEN__
    mo->info = (mobjinfo_t *) SV_ReadLong();
#else
    mo->info = &mobjInfo[mo->type];
#endif

    if(mo->info->flags & MF_SOLID)
        mo->ddFlags |= DDMF_SOLID;
    if(mo->info->flags & MF_NOBLOCKMAP)
        mo->ddFlags |= DDMF_NOBLOCKMAP;
    if(mo->info->flags2 & MF2_DONTDRAW)
        mo->ddFlags |= DDMF_DONTDRAW;

    mo->tics = SV_ReadLong();   // state tic counter
    mo->state = (state_t *) SV_ReadLong();

#if __JHEXEN__
    mo->damage = SV_ReadLong();
#endif

    mo->flags = SV_ReadLong();

#if __JHEXEN__
    mo->flags2 = SV_ReadLong();
    if(ver >= 5)
        mo->flags3 = SV_ReadLong();
    mo->special1 = SV_ReadLong();
    mo->special2 = SV_ReadLong();
#endif

    mo->health = SV_ReadLong();
#if __JHERETIC__
    if(ver < 8)
    {
        // Fix a bunch of kludges in the original Heretic.
        switch(mo->type)
        {
        case MT_MACEFX1:
        case MT_MACEFX2:
        case MT_MACEFX3:
        case MT_HORNRODFX2:
        case MT_HEADFX3:
        case MT_WHIRLWIND:
        case MT_TELEGLITTER:
        case MT_TELEGLITTER2:
            mo->special3 = mo->health;
            if(mo->type == MT_HORNRODFX2 && mo->special3 > 16)
                mo->special3 = 16;
            mo->health = mobjInfo[mo->type].spawnHealth;
            break;

        default:
            break;
        }
    }
#endif

    // Movement direction, movement generation (zig-zagging).
    mo->moveDir = SV_ReadLong();    // 0-7
    mo->moveCount = SV_ReadLong();  // when 0, select a new dir

#if __JHEXEN__
    mo->target = (mobj_t *) SV_ReadLong();
#endif

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    mo->reactionTime = SV_ReadLong();

    // If >0, the target will be chased
    // no matter what (even if shot)
    mo->threshold = SV_ReadLong();

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    mo->player = (player_t *) SV_ReadLong();

    // Player number last looked for.
    mo->lastLook = SV_ReadLong();

#if __JHEXEN__
    mo->floorClip = FIX2FLT(SV_ReadLong());
    SV_SetArchiveThing(mo, SV_ReadLong());
    mo->tid = SV_ReadLong();
#else
    // For nightmare respawn.
    if(ver >= 6)
    {
        mo->spawnSpot.pos[VX] = FIX2FLT(SV_ReadLong());
        mo->spawnSpot.pos[VY] = FIX2FLT(SV_ReadLong());
        mo->spawnSpot.pos[VZ] = FIX2FLT(SV_ReadLong());
        mo->spawnSpot.angle = SV_ReadLong();
        mo->spawnSpot.type = SV_ReadLong();
        mo->spawnSpot.flags = SV_ReadLong();
    }
    else
    {
        mo->spawnSpot.pos[VX] = (float) SV_ReadShort();
        mo->spawnSpot.pos[VY] = (float) SV_ReadShort();
        mo->spawnSpot.pos[VZ] = ONFLOORZ;
        mo->spawnSpot.angle = (angle_t) (ANG45 * (SV_ReadShort() / 45));
        mo->spawnSpot.type = (int) SV_ReadShort();
        mo->spawnSpot.flags = (int) SV_ReadShort();
    }

# if __JDOOM__ || __JDOOM64__
    if(ver >= 3)
# elif __JHERETIC__
    if(ver >= 5)
# endif
    {
        mo->intFlags = SV_ReadLong();   // killough $dropoff_fix: internal flags
        mo->dropOffZ = FIX2FLT(SV_ReadLong());   // killough $dropoff_fix
        mo->gear = SV_ReadLong();   // killough used in torque simulation
    }

# if __JDOOM__ || __JDOOM64__
    if(ver >= 6)
    {
        mo->damage = SV_ReadLong();
        mo->flags2 = SV_ReadLong();
    }// Else flags2 will be applied from the defs.
    else
        mo->damage = DDMAXINT; // Use the value set in mo->info->damage

# elif __JHERETIC__
    mo->damage = SV_ReadLong();
    mo->flags2 = SV_ReadLong();
# endif

    if(ver >= 7)
        mo->flags3 = SV_ReadLong();
    // Else flags3 will be applied from the defs.
#endif

#if __JHEXEN__
    mo->special = SV_ReadLong();
    SV_Read(mo->args, 1 * 5);
#elif __JHERETIC__
    mo->special1 = SV_ReadLong();
    mo->special2 = SV_ReadLong();
    if(ver >= 8)
        mo->special3 = SV_ReadLong();
#endif

#if __JHEXEN__
    if(ver >= 2)
#else
    if(ver >= 4)
#endif
        mo->translucency = SV_ReadByte();

#if __JHEXEN__
    if(ver >= 3)
#else
    if(ver >= 5)
#endif
        mo->visTarget = (short) (SV_ReadByte()) -1;

#if __JHEXEN__
    if(ver >= 4)
        mo->tracer = (mobj_t *) SV_ReadLong();

    if(ver >= 4)
        mo->lastEnemy = (mobj_t *) SV_ReadLong();
#else
    if(ver >= 5)
        mo->floorClip = FIX2FLT(SV_ReadLong());
#endif

#if __JHERETIC__
    if(ver >= 7)
        mo->generator = (mobj_t *) (int) SV_ReadShort();
    else
        mo->generator = NULL;
#endif

    // Restore! (unmangle)
    RestoreMobj(mo, ver);

    return false;
}

/**
 * Prepare and write the player header info.
 */
static void P_ArchivePlayerHeader(void)
{
    playerheader_t *ph = &playerHeader;

    SV_BeginSegment(ASEG_PLAYER_HEADER);
    SV_WriteByte(2); // version byte

    ph->numPowers = NUM_POWER_TYPES;
    ph->numKeys = NUM_KEY_TYPES;
    ph->numFrags = MAXPLAYERS;
    ph->numWeapons = NUM_WEAPON_TYPES;
    ph->numAmmoTypes = NUM_AMMO_TYPES;
    ph->numPSprites = NUMPSPRITES;
#if __JHERETIC__ || __JHEXEN__
    ph->numInvSlots = NUMINVENTORYSLOTS;
#endif
#if __JHEXEN__
    ph->numArmorTypes = NUMARMOR;
#endif
#if __JDOOM64__
    ph->numArtifacts = NUM_ARTIFACT_TYPES;
#endif

    SV_WriteLong(ph->numPowers);
    SV_WriteLong(ph->numKeys);
    SV_WriteLong(ph->numFrags);
    SV_WriteLong(ph->numWeapons);
    SV_WriteLong(ph->numAmmoTypes);
    SV_WriteLong(ph->numPSprites);
#if __JHERETIC__ || __JHEXEN__
    SV_WriteLong(ph->numInvSlots);
#endif
#if __JHEXEN__
    SV_WriteLong(ph->numArmorTypes);
#endif
#if __JDOOM64__
    SV_WriteLong(ph->numArtifacts);
#endif
    playerHeaderOK = true;
}

/**
 * Read archived player header info.
 */
static void P_UnArchivePlayerHeader(void)
{
#if __JHEXEN__
    if(saveVersion >= 4)
#else
    if(hdr.version >= 5)
#endif
    {
        int     ver;

        AssertSegment(ASEG_PLAYER_HEADER);
        ver = SV_ReadByte();

        playerHeader.numPowers = SV_ReadLong();
        playerHeader.numKeys = SV_ReadLong();
        playerHeader.numFrags = SV_ReadLong();
        playerHeader.numWeapons = SV_ReadLong();
        playerHeader.numAmmoTypes = SV_ReadLong();
        playerHeader.numPSprites = SV_ReadLong();
#if __JHERETIC__
        if(ver >= 2)
            playerHeader.numInvSlots = SV_ReadLong();
        else
            playerHeader.numInvSlots = NUMINVENTORYSLOTS;
#endif
#if __JHEXEN__
        playerHeader.numInvSlots = SV_ReadLong();
        playerHeader.numArmorTypes = SV_ReadLong();
#endif
#if __JDOOM64__
        playerHeader.numArtifacts = SV_ReadLong();
#endif
    }
    else // The old format didn't save the counts.
    {
#if __JHEXEN__
        playerHeader.numPowers = 9;
        playerHeader.numKeys = 11;
        playerHeader.numFrags = 8;
        playerHeader.numWeapons = 4;
        playerHeader.numAmmoTypes = 2;
        playerHeader.numPSprites = 2;
        playerHeader.numInvSlots = 33;
        playerHeader.numArmorTypes = 4;
#elif __JDOOM__ || __JDOOM64__
        playerHeader.numPowers = 6;
        playerHeader.numKeys = 6;
        playerHeader.numFrags = 4; // Why was this only 4?
        playerHeader.numWeapons = 9;
        playerHeader.numAmmoTypes = 4;
        playerHeader.numPSprites = 2;
#elif __JHERETIC__
        playerHeader.numPowers = 9;
        playerHeader.numKeys = 3;
        playerHeader.numFrags = 4; // ?
        playerHeader.numWeapons = 8;
        playerHeader.numInvSlots = 14;
        playerHeader.numAmmoTypes = 6;
        playerHeader.numPSprites = 2;
#endif
    }
    playerHeaderOK = true;
}

static void P_ArchivePlayers(void)
{
    int                 i;

    SV_BeginSegment(ASEG_PLAYERS);
#if __JHEXEN__
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        SV_WriteByte(players[i].plr->inGame);
    }
#endif

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        SV_WriteLong(Net_GetPlayerID(i));
        SV_WritePlayer(i);
    }
}

static void P_UnArchivePlayers(boolean *infile, boolean *loaded)
{
    int                 i, j;
    unsigned int        pid;
    player_t            dummyPlayer;
    ddplayer_t          dummyDDPlayer;
    player_t           *player;

    // Setup the dummy.
    dummyPlayer.plr = &dummyDDPlayer;

    // Load the players.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        // By default a saved player translates to nothing.
        saveToRealPlayerNum[i] = -1;

        if(!infile[i])
            continue;

        // The ID number will determine which player this actually is.
        pid = SV_ReadLong();
        for(player = 0, j = 0; j < MAXPLAYERS; ++j)
            if((IS_NETGAME && Net_GetPlayerID(j) == pid) ||
               (!IS_NETGAME && j == 0))
            {
                // This is our guy.
                player = players + j;
                loaded[j] = true;
                // Later references to the player number 'i' must be
                // translated!
                saveToRealPlayerNum[i] = j;
#if _DEBUG
Con_Printf("P_UnArchivePlayers: Saved %i is now %i.\n", i, j);
#endif
                break;
            }

        if(!player)
        {
            // We have a missing player. Use a dummy to load the data.
            player = &dummyPlayer;
        }

        // Read the data.
        SV_ReadPlayer(player);
    }
}

static void SV_WriteSector(sector_t *sec)
{
    int         i, type;
    float       flooroffx = P_GetFloatp(sec, DMU_FLOOR_MATERIAL_OFFSET_X);
    float       flooroffy = P_GetFloatp(sec, DMU_FLOOR_MATERIAL_OFFSET_Y);
    float       ceiloffx = P_GetFloatp(sec, DMU_CEILING_MATERIAL_OFFSET_X);
    float       ceiloffy = P_GetFloatp(sec, DMU_CEILING_MATERIAL_OFFSET_Y);
    byte        lightlevel = (byte) (255.f * P_GetFloatp(sec, DMU_LIGHT_LEVEL));
    short       floorheight = (short) P_GetIntp(sec, DMU_FLOOR_HEIGHT);
    short       ceilingheight = (short) P_GetIntp(sec, DMU_CEILING_HEIGHT);
    short       floorFlags = (short) P_GetIntp(sec, DMU_FLOOR_FLAGS);
    short       ceilingFlags = (short) P_GetIntp(sec, DMU_CEILING_FLAGS);
    material_t* floorMaterial = P_GetPtrp(sec, DMU_FLOOR_MATERIAL);
    material_t* ceilingMaterial = P_GetPtrp(sec, DMU_CEILING_MATERIAL);
    xsector_t*  xsec = P_ToXSector(sec);
    float       rgb[3];

#if !__JHEXEN__
    // Determine type.
    if(xsec->xg)
        type = sc_xg1;
    else
#endif
        if(flooroffx || flooroffy || ceiloffx || ceiloffy)
        type = sc_ploff;
    else
        type = sc_normal;

    // Type byte.
    SV_WriteByte(type);

    // Version.
    // 2: Surface colors.
    // 3: Surface flags.
    SV_WriteByte(3); // write a version byte.

    SV_WriteShort(floorheight);
    SV_WriteShort(ceilingheight);
    SV_WriteShort(SV_MaterialArchiveNum(floorMaterial));
    SV_WriteShort(SV_MaterialArchiveNum(ceilingMaterial));
    SV_WriteShort(floorFlags);
    SV_WriteShort(ceilingFlags);
#if __JHEXEN__
    SV_WriteShort((short) lightlevel);
#else
    SV_WriteByte(lightlevel);
#endif

    P_GetFloatpv(sec, DMU_COLOR, rgb);
    for(i = 0; i < 3; ++i)
        SV_WriteByte((byte)(255.f * rgb[i]));

    P_GetFloatpv(sec, DMU_FLOOR_COLOR, rgb);
    for(i = 0; i < 3; ++i)
        SV_WriteByte((byte)(255.f * rgb[i]));

    P_GetFloatpv(sec, DMU_CEILING_COLOR, rgb);
    for(i = 0; i < 3; ++i)
        SV_WriteByte((byte)(255.f * rgb[i]));

    SV_WriteShort(xsec->special);
    SV_WriteShort(xsec->tag);

#if __JHEXEN__
    SV_WriteShort(xsec->seqType);
#endif

    if(type == sc_ploff
#if !__JHEXEN__
       || type == sc_xg1
#endif
       )
    {
        SV_WriteFloat(flooroffx);
        SV_WriteFloat(flooroffy);
        SV_WriteFloat(ceiloffx);
        SV_WriteFloat(ceiloffy);
    }

#if !__JHEXEN__
    if(xsec->xg)                 // Extended General?
    {
        SV_WriteXGSector(sec);
    }

    // Count the number of sound targets
    if(xsec->soundTarget)
        numSoundTargets++;
#endif
}

/**
 * Reads all versions of archived sectors.
 * Including the old Ver1.
 */
static void SV_ReadSector(sector_t *sec)
{
    int                 i, ver = 1;
    int                 type = 0;
    material_t*         floorMaterial, *ceilingMaterial;
    byte                rgb[3], lightlevel;
    xsector_t*          xsec = P_ToXSector(sec);
    int                 fh, ch;

    // A type byte?
#if __JHEXEN__
    if(saveVersion < 4)
        type = sc_ploff;
    else
#else
    if(hdr.version <= 1)
        type = sc_normal;
    else
#endif
        type = SV_ReadByte();

    // A version byte?
#if __JHEXEN__
    if(saveVersion > 2)
#else
    if(hdr.version > 4)
#endif
        ver = SV_ReadByte();

    fh = SV_ReadShort();
    ch = SV_ReadShort();

    P_SetIntp(sec, DMU_FLOOR_HEIGHT, fh);
    P_SetIntp(sec, DMU_CEILING_HEIGHT, ch);
#if __JHEXEN__
    // Update the "target heights" of the planes.
    P_SetIntp(sec, DMU_FLOOR_TARGET_HEIGHT, fh);
    P_SetIntp(sec, DMU_CEILING_TARGET_HEIGHT, ch);
    // The move speed is not saved; can cause minor problems.
    P_SetIntp(sec, DMU_FLOOR_SPEED, 0);
    P_SetIntp(sec, DMU_CEILING_SPEED, 0);
#endif

#if !__JHEXEN__
    if(hdr.version == 1)
    {   // Flat numbers are the original flat lump indices - (lump) "F_START".
        floorMaterial = P_ToPtr(DMU_MATERIAL,
            P_MaterialNumForIndex(SV_ReadShort(), MN_FLATS));
        ceilingMaterial = P_ToPtr(DMU_MATERIAL,
            P_MaterialNumForIndex(SV_ReadShort(), MN_FLATS));
    }
    else if(hdr.version >= 4)
#endif
    {
        // The flat numbers are actually archive numbers.
        floorMaterial = SV_GetArchiveMaterial(SV_ReadShort(), 0);
        ceilingMaterial = SV_GetArchiveMaterial(SV_ReadShort(), 0);
    }

    P_SetPtrp(sec, DMU_FLOOR_MATERIAL, floorMaterial);
    P_SetPtrp(sec, DMU_CEILING_MATERIAL, ceilingMaterial);

    if(ver >= 3)
    {
        P_SetIntp(sec, DMU_FLOOR_FLAGS, SV_ReadShort());
        P_SetIntp(sec, DMU_CEILING_FLAGS, SV_ReadShort());
    }

#if __JHEXEN__
    lightlevel = (byte) SV_ReadShort();
#else
    // In Ver1 the light level is a short
    if(hdr.version == 1)
        lightlevel = (byte) SV_ReadShort();
    else
        lightlevel = SV_ReadByte();
#endif
    P_SetFloatp(sec, DMU_LIGHT_LEVEL, (float) lightlevel / 255.f);

#if !__JHEXEN__
    if(hdr.version > 1)
#endif
    {
        SV_Read(rgb, 3);
        for(i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_COLOR_RED + i, rgb[i] / 255.f);
    }

    // Ver 2 includes surface colours
    if(ver >= 2)
    {
        SV_Read(rgb, 3);
        for(i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_FLOOR_COLOR_RED + i, rgb[i] / 255.f);

        SV_Read(rgb, 3);
        for(i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_CEILING_COLOR_RED + i, rgb[i] / 255.f);
    }

    xsec->special = SV_ReadShort();
    /*xsec->tag =*/ SV_ReadShort();

#if __JHEXEN__
    xsec->seqType = SV_ReadShort();
#endif

    if(type == sc_ploff
#if !__JHEXEN__
       || type == sc_xg1
#endif
       )
    {
        P_SetFloatp(sec, DMU_FLOOR_MATERIAL_OFFSET_X, SV_ReadFloat());
        P_SetFloatp(sec, DMU_FLOOR_MATERIAL_OFFSET_Y, SV_ReadFloat());
        P_SetFloatp(sec, DMU_CEILING_MATERIAL_OFFSET_X, SV_ReadFloat());
        P_SetFloatp(sec, DMU_CEILING_MATERIAL_OFFSET_Y, SV_ReadFloat());
    }

#if !__JHEXEN__
    if(type == sc_xg1)
        SV_ReadXGSector(sec);
#endif

#if !__JHEXEN__
    if(hdr.version <= 1)
#endif
    {
        xsec->specialData = 0;
    }

    // We'll restore the sound targets latter on
    xsec->soundTarget = 0;
}

static void SV_WriteLine(linedef_t* li)
{
    uint                i, j;
    float               rgba[4];
    lineclass_t         type;
    xline_t*            xli = P_ToXLine(li);

#if !__JHEXEN__
    if(xli->xg)
        type =  lc_xg1;
    else
#endif
        type = lc_normal;
    SV_WriteByte(type);

    // Version.
    // 2: Per surface texture offsets.
    // 2: Surface colors.
    // 3: "Mapped by player" values.
    // 3: Surface flags.
    SV_WriteByte(3); // Write a version byte

    SV_WriteShort(xli->flags);

    for(i = 0; i < MAXPLAYERS; ++i)
        SV_WriteByte(xli->mapped[i]);

#if __JHEXEN__
    SV_WriteByte(xli->special);
    SV_WriteByte(xli->arg1);
    SV_WriteByte(xli->arg2);
    SV_WriteByte(xli->arg3);
    SV_WriteByte(xli->arg4);
    SV_WriteByte(xli->arg5);
#else
    SV_WriteShort(xli->special);
    SV_WriteShort(xli->tag);
#endif

    // For each side
    for(i = 0; i < 2; ++i)
    {
        sidedef_t *si = P_GetPtrp(li, (i? DMU_SIDEDEF1:DMU_SIDEDEF0));
        if(!si)
            continue;

        SV_WriteShort(P_GetIntp(si, DMU_TOP_MATERIAL_OFFSET_X));
        SV_WriteShort(P_GetIntp(si, DMU_TOP_MATERIAL_OFFSET_Y));
        SV_WriteShort(P_GetIntp(si, DMU_MIDDLE_MATERIAL_OFFSET_X));
        SV_WriteShort(P_GetIntp(si, DMU_MIDDLE_MATERIAL_OFFSET_Y));
        SV_WriteShort(P_GetIntp(si, DMU_BOTTOM_MATERIAL_OFFSET_X));
        SV_WriteShort(P_GetIntp(si, DMU_BOTTOM_MATERIAL_OFFSET_Y));

        SV_WriteShort(P_GetIntp(si, DMU_TOP_FLAGS));
        SV_WriteShort(P_GetIntp(si, DMU_MIDDLE_FLAGS));
        SV_WriteShort(P_GetIntp(si, DMU_BOTTOM_FLAGS));

        SV_WriteShort(SV_MaterialArchiveNum(P_GetPtrp(si, DMU_TOP_MATERIAL)));
        SV_WriteShort(SV_MaterialArchiveNum(P_GetPtrp(si, DMU_BOTTOM_MATERIAL)));
        SV_WriteShort(SV_MaterialArchiveNum(P_GetPtrp(si, DMU_MIDDLE_MATERIAL)));

        P_GetFloatpv(si, DMU_TOP_COLOR, rgba);
        for(j = 0; j < 3; ++j)
            SV_WriteByte((byte)(255 * rgba[j]));

        P_GetFloatpv(si, DMU_BOTTOM_COLOR, rgba);
        for(j = 0; j < 3; ++j)
            SV_WriteByte((byte)(255 * rgba[j]));

        P_GetFloatpv(si, DMU_MIDDLE_COLOR, rgba);
        for(j = 0; j < 4; ++j)
            SV_WriteByte((byte)(255 * rgba[j]));

        SV_WriteLong(P_GetIntp(si, DMU_MIDDLE_BLENDMODE));
        SV_WriteShort(P_GetIntp(si, DMU_FLAGS));
    }

#if !__JHEXEN__
    // Extended General?
    if(xli->xg)
    {
        SV_WriteXGLine(li);
    }
#endif
}

/**
 * Reads all versions of archived lines.
 * Including the old Ver1.
 */
static void SV_ReadLine(linedef_t *li)
{
    int                 i, j;
    lineclass_t         type;
    int                 ver;
    material_t*         topMaterial, *bottomMaterial, *middleMaterial;
    short               flags;
    xline_t*            xli = P_ToXLine(li);

    // A type byte?
#if __JHEXEN__
    if(saveVersion < 4)
#else
    if(hdr.version < 2)
#endif
        type = lc_normal;
    else
        type = (int) SV_ReadByte();

    // A version byte?
#if __JHEXEN__
    if(saveVersion < 3)
#else
    if(hdr.version < 5)
#endif
        ver = 1;
    else
        ver = (int) SV_ReadByte();

    flags = SV_ReadShort();
    if(ver < 3 && (flags & 0x0100)) // the old ML_MAPPED flag
    {
        uint                lineIDX = P_ToIndex(li);

        // Set line as having been seen by all players..
        memset(xli->mapped, 0, sizeof(xli->mapped));
        for(i = 0; i < MAXPLAYERS; ++i)
            AM_UpdateLinedef(AM_MapForPlayer(i), lineIDX, true);
        flags &= ~0x0100; // remove the old flag.
    }
    xli->flags = flags;

    if(ver >= 3)
    {
        for(i = 0; i < MAXPLAYERS; ++i)
            xli->mapped[i] = SV_ReadByte();
    }

#if __JHEXEN__
    xli->special = SV_ReadByte();
    xli->arg1 = SV_ReadByte();
    xli->arg2 = SV_ReadByte();
    xli->arg3 = SV_ReadByte();
    xli->arg4 = SV_ReadByte();
    xli->arg5 = SV_ReadByte();
#else
    xli->special = SV_ReadShort();
    /*xli->tag =*/ SV_ReadShort();
#endif

    // For each side
    for(i = 0; i < 2; ++i)
    {
        sidedef_t*          si = P_GetPtrp(li, (i? DMU_SIDEDEF1:DMU_SIDEDEF0));

        if(!si)
            continue;

        // Versions latter than 2 store per surface texture offsets.
        if(ver >= 2)
        {
            float               offset[2];

            offset[VX] = (float) SV_ReadShort();
            offset[VY] = (float) SV_ReadShort();
            P_SetFloatpv(si, DMU_TOP_MATERIAL_OFFSET_XY, offset);

            offset[VX] = (float) SV_ReadShort();
            offset[VY] = (float) SV_ReadShort();
            P_SetFloatpv(si, DMU_MIDDLE_MATERIAL_OFFSET_XY, offset);

            offset[VX] = (float) SV_ReadShort();
            offset[VY] = (float) SV_ReadShort();
            P_SetFloatpv(si, DMU_BOTTOM_MATERIAL_OFFSET_XY, offset);
        }
        else
        {
            float       offset[2];

            offset[VX] = (float) SV_ReadShort();
            offset[VY] = (float) SV_ReadShort();

            P_SetFloatpv(si, DMU_TOP_MATERIAL_OFFSET_XY, offset);
            P_SetFloatpv(si, DMU_MIDDLE_MATERIAL_OFFSET_XY, offset);
            P_SetFloatpv(si, DMU_BOTTOM_MATERIAL_OFFSET_XY, offset);
        }

        if(ver >= 3)
        {
            P_SetIntp(si, DMU_TOP_FLAGS, SV_ReadShort());
            P_SetIntp(si, DMU_MIDDLE_FLAGS, SV_ReadShort());
            P_SetIntp(si, DMU_BOTTOM_FLAGS, SV_ReadShort());
        }

#if !__JHEXEN__
        if(hdr.version >= 4)
#endif
        {
            // The texture numbers are archive numbers.
            topMaterial = SV_GetArchiveMaterial(SV_ReadShort(), 1);
            bottomMaterial = SV_GetArchiveMaterial(SV_ReadShort(), 1);
            middleMaterial = SV_GetArchiveMaterial(SV_ReadShort(), 1);
        }

        P_SetPtrp(si, DMU_TOP_MATERIAL, topMaterial);
        P_SetPtrp(si, DMU_BOTTOM_MATERIAL, bottomMaterial);
        P_SetPtrp(si, DMU_MIDDLE_MATERIAL, middleMaterial);

        // Ver2 includes surface colours
        if(ver >= 2)
        {
            float           rgba[4];

            for(j = 0; j < 3; ++j)
                rgba[j] = (float) SV_ReadByte() / 255.f;
            rgba[3] = 1;
            P_SetFloatpv(si, DMU_TOP_COLOR, rgba);

            for(j = 0; j < 3; ++j)
                rgba[j] = (float) SV_ReadByte() / 255.f;
            rgba[3] = 1;
            P_SetFloatpv(si, DMU_BOTTOM_COLOR, rgba);

            for(j = 0; j < 4; ++j)
                rgba[j] = (float) SV_ReadByte() / 255.f;
            P_SetFloatpv(si, DMU_MIDDLE_COLOR, rgba);

            P_SetIntp(si, DMU_MIDDLE_BLENDMODE, SV_ReadLong());
            P_SetIntp(si, DMU_FLAGS, SV_ReadShort());
        }
    }

#if !__JHEXEN__
    if(type == lc_xg1)
        SV_ReadXGLine(li);
#endif
}

#if __JHEXEN__
static void SV_WritePolyObj(polyobj_t* po)
{
    SV_WriteByte(1); // write a version byte.

    SV_WriteLong(po->tag);
    SV_WriteLong(po->angle);
    SV_WriteLong(FLT2FIX(po->pos[VX]));
    SV_WriteLong(FLT2FIX(po->pos[VY]));
}

static int SV_ReadPolyObj(void)
{
    int             ver;
    float           deltaX;
    float           deltaY;
    angle_t         angle;
    polyobj_t*      po;

    if(saveVersion >= 3)
        ver = SV_ReadByte();

    po = P_GetPolyobj(SV_ReadLong()); // Get polyobj by tag.
    if(!po)
        Con_Error("UnarchivePolyobjs: Invalid polyobj tag");

    angle = (angle_t) SV_ReadLong();
    P_PolyobjRotate(po, angle);
    po->destAngle = angle;
    deltaX = FIX2FLT(SV_ReadLong()) - po->pos[VX];
    deltaY = FIX2FLT(SV_ReadLong()) - po->pos[VY];
    P_PolyobjMove(po, deltaX, deltaY);

    //// \fixme What about speed? It isn't saved at all?

    return true;
}
#endif

/**
 * Only write world in the latest format.
 */
static void P_ArchiveWorld(void)
{
    uint                i;

    SV_BeginSegment(ASEG_MATERIAL_ARCHIVE);
    SV_WriteMaterialArchive();

    SV_BeginSegment(ASEG_WORLD);
    for(i = 0; i < numsectors; ++i)
        SV_WriteSector(P_ToPtr(DMU_SECTOR, i));

    for(i = 0; i < numlines; ++i)
        SV_WriteLine(P_ToPtr(DMU_LINEDEF, i));

#if __JHEXEN__
    SV_BeginSegment(ASEG_POLYOBJS);
    SV_WriteLong(numpolyobjs);
    for(i = 0; i < numpolyobjs; ++i)
        SV_WritePolyObj(P_GetPolyobj(i | 0x80000000));
#endif
}

static void P_UnArchiveWorld(void)
{
    uint                i;
    int                 matArchiveVer = 0;

    AssertSegment(ASEG_MATERIAL_ARCHIVE);

#if __JHEXEN__
    if(saveVersion >= 6)
#else
    if(hdr.version >= 6)
#endif
        matArchiveVer = 1;

    // Load texturearchive?
#if !__JHEXEN__
    if(hdr.version >= 4)
#endif
        SV_ReadMaterialArchive(matArchiveVer);

    AssertSegment(ASEG_WORLD);
    // Load sectors.
    for(i = 0; i < numsectors; ++i)
        SV_ReadSector(P_ToPtr(DMU_SECTOR, i));
    // Load lines.
    for(i = 0; i < numlines; ++i)
        SV_ReadLine(P_ToPtr(DMU_LINEDEF, i));

#if __JHEXEN__
    // Load polyobjects.
    AssertSegment(ASEG_POLYOBJS);
    if(SV_ReadLong() != numpolyobjs)
        Con_Error("UnarchivePolyobjs: Bad polyobj count");

    for(i = 0; i < numpolyobjs; ++i)
        SV_ReadPolyObj();
#endif
}

static void SV_WriteCeiling(ceiling_t* ceiling)
{
    SV_WriteByte(2); // Write a version byte.

    SV_WriteByte((byte) ceiling->type);
    SV_WriteLong(P_ToIndex(ceiling->sector));

    SV_WriteShort((int)ceiling->bottomHeight);
    SV_WriteShort((int)ceiling->topHeight);
    SV_WriteLong(FLT2FIX(ceiling->speed));

    SV_WriteByte(ceiling->crush);

    SV_WriteByte((byte) ceiling->state);
    SV_WriteLong(ceiling->tag);
    SV_WriteByte((byte) ceiling->oldState);
}

static int SV_ReadCeiling(ceiling_t* ceiling)
{
    sector_t*           sector;

#if __JHEXEN__
    if(saveVersion >= 4)
#else
    if(hdr.version >= 5)
#endif
    {   // Note: the thinker class byte has already been read.
        int                 ver = SV_ReadByte(); // version byte.

        ceiling->thinker.function = T_MoveCeiling;

#if !__JHEXEN__
        // Should we put this into stasis?
        if(hdr.version == 5)
        {
            if(!SV_ReadByte())
                P_ThinkerSetStasis(&ceiling->thinker, true);
        }
#endif

        ceiling->type = (ceilingtype_e) SV_ReadByte();

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

        if(!sector)
            Con_Error("TC_CEILING: bad sector number\n");

        ceiling->sector = sector;

        ceiling->bottomHeight = (float) SV_ReadShort();
        ceiling->topHeight = (float) SV_ReadShort();
        ceiling->speed = FIX2FLT((fixed_t) SV_ReadLong());

        ceiling->crush = SV_ReadByte();

        if(ver == 2)
            ceiling->state = SV_ReadByte();
        else
            ceiling->state = (SV_ReadLong() == -1? CS_DOWN : CS_UP);
        ceiling->tag = SV_ReadLong();
        if(ver == 2)
            ceiling->oldState = SV_ReadByte();
        else
            ceiling->state = (SV_ReadLong() == -1? CS_DOWN : CS_UP);
    }
    else
    {
        // Its in the old format which serialized ceiling_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
#if __JHEXEN__
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());
        if(!sector)
            Con_Error("TC_CEILING: bad sector number\n");
        ceiling->sector = sector;

        ceiling->type = SV_ReadLong();
#else
        ceiling->type = SV_ReadLong();

        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());
        if(!sector)
            Con_Error("TC_CEILING: bad sector number\n");
        ceiling->sector = sector;
#endif

        ceiling->bottomHeight = FIX2FLT((fixed_t) SV_ReadLong());
        ceiling->topHeight = FIX2FLT((fixed_t) SV_ReadLong());
        ceiling->speed = FIX2FLT((fixed_t) SV_ReadLong());

        ceiling->crush = SV_ReadLong();
        ceiling->state = (SV_ReadLong() == -1? CS_DOWN : CS_UP);
        ceiling->tag = SV_ReadLong();
        ceiling->oldState = (SV_ReadLong() == -1? CS_DOWN : CS_UP);

        ceiling->thinker.function = T_MoveCeiling;
#if !__JHEXEN__
        if(!junk.function)
            P_ThinkerSetStasis(&ceiling->thinker, true);
#endif
    }

    P_ToXSector(ceiling->sector)->specialData = ceiling;
    return true; // Add this thinker.
}

static void SV_WriteDoor(door_t *door)
{
    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteByte((byte) door->type);

    SV_WriteLong(P_ToIndex(door->sector));

    SV_WriteShort((int)door->topHeight);
    SV_WriteLong(FLT2FIX(door->speed));

    SV_WriteLong(door->state);
    SV_WriteLong(door->topWait);
    SV_WriteLong(door->topCountDown);
}

static int SV_ReadDoor(door_t *door)
{
    sector_t *sector;

#if __JHEXEN__
    if(saveVersion >= 4)
#else
    if(hdr.version >= 5)
#endif
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        door->type = (doortype_e) SV_ReadByte();

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

        if(!sector)
            Con_Error("TC_DOOR: bad sector number\n");

        door->sector = sector;

        door->topHeight = (float) SV_ReadShort();
        door->speed = FIX2FLT((fixed_t) SV_ReadLong());

        door->state = SV_ReadLong();
        door->topWait = SV_ReadLong();
        door->topCountDown = SV_ReadLong();
    }
    else
    {
        // Its in the old format which serialized door_t
        // Padding at the start (an old thinker_t struct)
        SV_Read(junkbuffer, (size_t) 16);

        // Start of used data members.
#if __JHEXEN__
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_DOOR: bad sector number\n");
        door->sector = sector;

        door->type = SV_ReadLong();
#else
        door->type = SV_ReadLong();

        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_DOOR: bad sector number\n");
        door->sector = sector;
#endif
        door->topHeight = FIX2FLT((fixed_t) SV_ReadLong());
        door->speed = FIX2FLT((fixed_t) SV_ReadLong());

        door->state = SV_ReadLong();
        door->topWait = SV_ReadLong();
        door->topCountDown = SV_ReadLong();
    }

    P_ToXSector(door->sector)->specialData = door;
    door->thinker.function = T_Door;

    return true; // Add this thinker.
}

static void SV_WriteFloor(floor_t *floor)
{
    SV_WriteByte(3); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteByte((byte) floor->type);

    SV_WriteLong(P_ToIndex(floor->sector));

    SV_WriteByte((byte) floor->crush);

    SV_WriteLong((int) floor->state);
    SV_WriteLong(floor->newSpecial);

    SV_WriteShort(SV_MaterialArchiveNum(floor->material));

    SV_WriteShort((int) floor->floorDestHeight);
    SV_WriteLong(FLT2FIX(floor->speed));

#if __JHEXEN__
    SV_WriteLong(floor->delayCount);
    SV_WriteLong(floor->delayTotal);
    SV_WriteLong(FLT2FIX(floor->stairsDelayHeight));
    SV_WriteLong(FLT2FIX(floor->stairsDelayHeightDelta));
    SV_WriteLong(FLT2FIX(floor->resetHeight));
    SV_WriteShort(floor->resetDelay);
    SV_WriteShort(floor->resetDelayCount);
#endif
}

static int SV_ReadFloor(floor_t* floor)
{
    sector_t*           sector;

#if __JHEXEN__
    if(saveVersion >= 4)
#else
    if(hdr.version >= 5)
#endif
    {   // Note: the thinker class byte has already been read.
        byte                ver = SV_ReadByte(); // version byte.

        floor->type = (floortype_e) SV_ReadByte();

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

        if(!sector)
            Con_Error("TC_FLOOR: bad sector number\n");

        floor->sector = sector;

        floor->crush = (boolean) SV_ReadByte();

        floor->state = (int) SV_ReadLong();
        floor->newSpecial = SV_ReadLong();

        if(ver >= 2)
            floor->material = SV_GetArchiveMaterial(SV_ReadShort(), 0);
        else
            floor->material = P_ToPtr(DMU_MATERIAL,
                P_MaterialNumForName(W_LumpName(SV_ReadShort()), MN_FLATS));

        floor->floorDestHeight = (float) SV_ReadShort();
        floor->speed = FIX2FLT(SV_ReadLong());

#if __JHEXEN__
        floor->delayCount = SV_ReadLong();
        floor->delayTotal = SV_ReadLong();
        floor->stairsDelayHeight = FIX2FLT(SV_ReadLong());
        floor->stairsDelayHeightDelta = FIX2FLT(SV_ReadLong());
        floor->resetHeight = FIX2FLT(SV_ReadLong());
        floor->resetDelay = SV_ReadShort();
        floor->resetDelayCount = SV_ReadShort();
        /*floor->textureChange =*/ SV_ReadByte();
#endif
    }
    else
    {
        // Its in the old format which serialized floor_t
        // Padding at the start (an old thinker_t struct)
        SV_Read(junkbuffer, (size_t) 16);

        // Start of used data members.
#if __JHEXEN__
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_FLOOR: bad sector number\n");
        floor->sector = sector;

        floor->type = SV_ReadLong();
        floor->crush = SV_ReadLong();
#else
        floor->type = SV_ReadLong();

        floor->crush = SV_ReadLong();

        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_FLOOR: bad sector number\n");
        floor->sector = sector;
#endif
        floor->state = (int) SV_ReadLong();
        floor->newSpecial = SV_ReadLong();
        floor->material = P_ToPtr(DMU_MATERIAL,
            P_MaterialNumForName(W_LumpName(SV_ReadShort()), MN_FLATS));

        floor->floorDestHeight = FIX2FLT((fixed_t) SV_ReadLong());
        floor->speed = FIX2FLT((fixed_t) SV_ReadLong());

#if __JHEXEN__
        floor->delayCount = SV_ReadLong();
        floor->delayTotal = SV_ReadLong();
        floor->stairsDelayHeight = FIX2FLT((fixed_t) SV_ReadLong());
        floor->stairsDelayHeightDelta = FIX2FLT((fixed_t) SV_ReadLong());
        floor->resetHeight = FIX2FLT((fixed_t) SV_ReadLong());
        floor->resetDelay = SV_ReadShort();
        floor->resetDelayCount = SV_ReadShort();
        /*floor->textureChange =*/ SV_ReadByte();
#endif
    }

    P_ToXSector(floor->sector)->specialData = floor;
    floor->thinker.function = T_MoveFloor;

    return true; // Add this thinker.
}

static void SV_WritePlat(plat_t *plat)
{
    SV_WriteByte(1); // Write a version byte.

    SV_WriteByte((byte) plat->type);

    SV_WriteLong(P_ToIndex(plat->sector));

    SV_WriteLong(FLT2FIX(plat->speed));
    SV_WriteShort((int)plat->low);
    SV_WriteShort((int)plat->high);

    SV_WriteLong(plat->wait);
    SV_WriteLong(plat->count);

    SV_WriteByte((byte) plat->state);
    SV_WriteByte((byte) plat->oldState);
    SV_WriteByte((byte) plat->crush);

    SV_WriteLong(plat->tag);
}

static int SV_ReadPlat(plat_t *plat)
{
    sector_t *sector;

#if __JHEXEN__
    if(saveVersion >= 4)
#else
    if(hdr.version >= 5)
#endif
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        plat->thinker.function = T_PlatRaise;

#if !__JHEXEN__
        // Should we put this into stasis?
        if(hdr.version == 5)
        {
        if(!SV_ReadByte())
            P_ThinkerSetStasis(&plat->thinker, true);
        }
#endif

        plat->type = (plattype_e) SV_ReadByte();

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

        if(!sector)
            Con_Error("TC_PLAT: bad sector number\n");

        plat->sector = sector;

        plat->speed = FIX2FLT(SV_ReadLong());
        plat->low = (float) SV_ReadShort();
        plat->high = (float) SV_ReadShort();

        plat->wait = SV_ReadLong();
        plat->count = SV_ReadLong();

        plat->state = (platstate_e) SV_ReadByte();
        plat->oldState = (platstate_e) SV_ReadByte();
        plat->crush = (boolean) SV_ReadByte();

        plat->tag = SV_ReadLong();
    }
    else
    {
        // Its in the old format which serialized plat_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_PLAT: bad sector number\n");
        plat->sector = sector;

        plat->speed = FIX2FLT((fixed_t) SV_ReadLong());
        plat->low = FIX2FLT((fixed_t) SV_ReadLong());
        plat->high = FIX2FLT((fixed_t) SV_ReadLong());

        plat->wait = SV_ReadLong();
        plat->count = SV_ReadLong();
        plat->state = SV_ReadLong();
        plat->oldState = SV_ReadLong();
        plat->crush = SV_ReadLong();
        plat->tag = SV_ReadLong();
        plat->type = SV_ReadLong();

        plat->thinker.function = T_PlatRaise;
#if !__JHEXEN__
        if(!junk.function)
            P_ThinkerSetStasis(&plat->thinker, true);
#endif
    }

    P_ToXSector(plat->sector)->specialData = plat;
    return true; // Add this thinker.
}

#if __JHEXEN__
static void SV_WriteLight(light_t *th)
{
    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteByte((byte) th->type);

    SV_WriteLong(P_ToIndex(th->sector));

    SV_WriteLong((int) (255.0f * th->value1));
    SV_WriteLong((int) (255.0f * th->value2));
    SV_WriteLong(th->tics1);
    SV_WriteLong(th->tics2);
    SV_WriteLong(th->count);
}

static int SV_ReadLight(light_t *th)
{
    sector_t *sector;

    if(saveVersion >= 4)
    {
        /*int ver =*/ SV_ReadByte(); // version byte.

        th->type = (lighttype_t) SV_ReadByte();

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());
        if(!sector)
            Con_Error("TC_LIGHT: bad sector number\n");
        th->sector = sector;

        th->value1 = (float) SV_ReadLong() / 255.0f;
        th->value2 = (float) SV_ReadLong() / 255.0f;
        th->tics1 = SV_ReadLong();
        th->tics2 = SV_ReadLong();
        th->count = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V4 format which serialized light_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_LIGHT: bad sector number\n");
        th->sector = sector;

        th->type = (lighttype_t) SV_ReadLong();
        th->value1 = (float) SV_ReadLong() / 255.0f;
        th->value2 = (float) SV_ReadLong() / 255.0f;
        th->tics1 = SV_ReadLong();
        th->tics2 = SV_ReadLong();
        th->count = SV_ReadLong();
    }

    th->thinker.function = T_Light;

    return true; // Add this thinker.
}

static void SV_WritePhase(phase_t *th)
{
    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(th->sector));

    SV_WriteLong(th->index);
    SV_WriteLong((int) (255.0f * th->baseValue));
}

static int SV_ReadPhase(phase_t *th)
{
    sector_t *sector;

    if(saveVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_PHASE: bad sector number\n");
        th->sector = sector;

        th->index = SV_ReadLong();
        th->baseValue = (float) SV_ReadLong() / 255.0f;
    }
    else
    {
        // Its in the old pre V4 format which serialized phase_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_PHASE: bad sector number\n");
        th->sector = sector;

        th->index = SV_ReadLong();
        th->baseValue = (float) SV_ReadLong() / 255.0f;
    }

    th->thinker.function = T_Phase;

    return true; // Add this thinker.
}

static void SV_WriteScript(acs_t *th)
{
    uint        i;

    SV_WriteByte(1); // Write a version byte.

    SV_WriteLong(SV_ThingArchiveNum(th->activator));
    SV_WriteLong(th->line ? P_ToIndex(th->line) : -1);
    SV_WriteLong(th->side);
    SV_WriteLong(th->number);
    SV_WriteLong(th->infoIndex);
    SV_WriteLong(th->delayCount);
    for(i = 0; i < ACS_STACK_DEPTH; ++i)
        SV_WriteLong(th->stack[i]);
    SV_WriteLong(th->stackPtr);
    for(i = 0; i < MAX_ACS_SCRIPT_VARS; ++i)
        SV_WriteLong(th->vars[i]);
    SV_WriteLong((int) (th->ip) - (int) ActionCodeBase);
}

static int SV_ReadScript(acs_t *th)
{
    int         temp;
    uint        i;

    if(saveVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        th->activator = (mobj_t*) SV_ReadLong();
        th->activator = SV_GetArchiveThing((int) th->activator, &th->activator);
        temp = SV_ReadLong();
        if(temp == -1)
            th->line = NULL;
        else
            th->line = P_ToPtr(DMU_LINEDEF, temp);
        th->side = SV_ReadLong();
        th->number = SV_ReadLong();
        th->infoIndex = SV_ReadLong();
        th->delayCount = SV_ReadLong();
        for(i = 0; i < ACS_STACK_DEPTH; ++i)
            th->stack[i] = SV_ReadLong();
        th->stackPtr = SV_ReadLong();
        for(i = 0; i < MAX_ACS_SCRIPT_VARS; ++i)
            th->vars[i] = SV_ReadLong();
        th->ip = (int *) (ActionCodeBase + SV_ReadLong());
    }
    else
    {
        // Its in the old pre V4 format which serialized acs_t
        // Padding at the start (an old thinker_t struct)
        thinker_t   junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        th->activator = (mobj_t*) SV_ReadLong();
        th->activator = SV_GetArchiveThing((int) th->activator, &th->activator);
        temp = SV_ReadLong();
        if(temp == -1)
            th->line = NULL;
        else
            th->line = P_ToPtr(DMU_LINEDEF, temp);
        th->side = SV_ReadLong();
        th->number = SV_ReadLong();
        th->infoIndex = SV_ReadLong();
        th->delayCount = SV_ReadLong();
        for(i = 0; i < ACS_STACK_DEPTH; ++i)
            th->stack[i] = SV_ReadLong();
        th->stackPtr = SV_ReadLong();
        for(i = 0; i < MAX_ACS_SCRIPT_VARS; ++i)
            th->vars[i] = SV_ReadLong();
        th->ip = (int *) (ActionCodeBase + SV_ReadLong());
    }

    return true; // Add this thinker.
}

static void SV_WriteDoorPoly(polydoor_t *th)
{
    SV_WriteByte(1); // Write a version byte.

    SV_WriteByte(th->type);

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(th->polyobj);
    SV_WriteLong(th->intSpeed);
    SV_WriteLong(th->dist);
    SV_WriteLong(th->totalDist);
    SV_WriteLong(th->direction);
    SV_WriteLong(FLT2FIX(th->speed[VX]));
    SV_WriteLong(FLT2FIX(th->speed[VY]));
    SV_WriteLong(th->tics);
    SV_WriteLong(th->waitTics);
    SV_WriteByte(th->close);
}

static int SV_ReadDoorPoly(polydoor_t *th)
{
    if(saveVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Start of used data members.
        th->type = SV_ReadByte();

        th->polyobj = SV_ReadLong();
        th->intSpeed = SV_ReadLong();
        th->dist = SV_ReadLong();
        th->totalDist = SV_ReadLong();
        th->direction = SV_ReadLong();
        th->speed[VX] = FIX2FLT(SV_ReadLong());
        th->speed[VY] = FIX2FLT(SV_ReadLong());
        th->tics = SV_ReadLong();
        th->waitTics = SV_ReadLong();
        th->close = SV_ReadByte();
    }
    else
    {
        // Its in the old pre V4 format which serialized polydoor_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        th->polyobj = SV_ReadLong();
        th->intSpeed = SV_ReadLong();
        th->dist = SV_ReadLong();
        th->totalDist = SV_ReadLong();
        th->direction = SV_ReadLong();
        th->speed[VX] = FIX2FLT(SV_ReadLong());
        th->speed[VY] = FIX2FLT(SV_ReadLong());
        th->tics = SV_ReadLong();
        th->waitTics = SV_ReadLong();
        th->type = SV_ReadByte();
        th->close = SV_ReadByte();
    }

    th->thinker.function = T_PolyDoor;

    return true; // Add this thinker.
}

static void SV_WriteMovePoly(polyevent_t *th)
{
    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(th->polyobj);
    SV_WriteLong(th->intSpeed);
    SV_WriteLong(th->dist);
    SV_WriteLong(th->fangle);
    SV_WriteLong(FLT2FIX(th->speed[VX]));
    SV_WriteLong(FLT2FIX(th->speed[VY]));
}

static int SV_ReadMovePoly(polyevent_t *th)
{
    if(saveVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Start of used data members.
        th->polyobj = SV_ReadLong();
        th->intSpeed = SV_ReadLong();
        th->dist = SV_ReadLong();
        th->fangle = SV_ReadLong();
        th->speed[VX] = FIX2FLT(SV_ReadLong());
        th->speed[VY] = FIX2FLT(SV_ReadLong());
    }
    else
    {
        // Its in the old pre V4 format which serialized polyevent_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        th->polyobj = SV_ReadLong();
        th->intSpeed = SV_ReadLong();
        th->dist = SV_ReadLong();
        th->fangle = SV_ReadLong();
        th->speed[VX] = FIX2FLT(SV_ReadLong());
        th->speed[VY] = FIX2FLT(SV_ReadLong());
    }

    th->thinker.function = T_MovePoly;

    return true; // Add this thinker.
}

static void SV_WriteRotatePoly(polyevent_t *th)
{
    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(th->polyobj);
    SV_WriteLong(th->intSpeed);
    SV_WriteLong(th->dist);
    SV_WriteLong(th->fangle);
    SV_WriteLong(FLT2FIX(th->speed[VX]));
    SV_WriteLong(FLT2FIX(th->speed[VY]));
}

static int SV_ReadRotatePoly(polyevent_t *th)
{
    if(saveVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Start of used data members.
        th->polyobj = SV_ReadLong();
        th->intSpeed = SV_ReadLong();
        th->dist = SV_ReadLong();
        th->fangle = SV_ReadLong();
        th->speed[VX] = FIX2FLT(SV_ReadLong());
        th->speed[VY] = FIX2FLT(SV_ReadLong());
    }
    else
    {
        // Its in the old pre V4 format which serialized polyevent_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        th->polyobj = SV_ReadLong();
        th->intSpeed = SV_ReadLong();
        th->dist = SV_ReadLong();
        th->fangle = SV_ReadLong();
        th->speed[VX] = FIX2FLT(SV_ReadLong());
        th->speed[VY] = FIX2FLT(SV_ReadLong());
    }

    th->thinker.function = T_RotatePoly;
    return true; // Add this thinker.
}

static void SV_WritePillar(pillar_t *th)
{
    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(th->sector));

    SV_WriteLong(FLT2FIX(th->ceilingSpeed));
    SV_WriteLong(FLT2FIX(th->floorSpeed));
    SV_WriteLong(FLT2FIX(th->floorDest));
    SV_WriteLong(FLT2FIX(th->ceilingDest));
    SV_WriteLong(th->direction);
    SV_WriteLong(th->crush);
}

static int SV_ReadPillar(pillar_t *th)
{
    sector_t *sector;

    if(saveVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Start of used data members.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_BUILD_PILLAR: bad sector number\n");
        th->sector = sector;

        th->ceilingSpeed = FIX2FLT((fixed_t) SV_ReadLong());
        th->floorSpeed = FIX2FLT((fixed_t) SV_ReadLong());
        th->floorDest = FIX2FLT((fixed_t) SV_ReadLong());
        th->ceilingDest = FIX2FLT((fixed_t) SV_ReadLong());
        th->direction = SV_ReadLong();
        th->crush = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V4 format which serialized pillar_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_BUILD_PILLAR: bad sector number\n");
        th->sector = sector;

        th->ceilingSpeed = FIX2FLT((fixed_t) SV_ReadLong());
        th->floorSpeed = FIX2FLT((fixed_t) SV_ReadLong());
        th->floorDest = FIX2FLT((fixed_t) SV_ReadLong());
        th->ceilingDest = FIX2FLT((fixed_t) SV_ReadLong());
        th->direction = SV_ReadLong();
        th->crush = SV_ReadLong();
    }

    th->thinker.function = T_BuildPillar;

    P_ToXSector(th->sector)->specialData = th;
    return true; // Add this thinker.
}

static void SV_WriteFloorWaggle(waggle_t *th)
{
    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(th->sector));

    SV_WriteLong(FLT2FIX(th->originalHeight));
    SV_WriteLong(FLT2FIX(th->accumulator));
    SV_WriteLong(FLT2FIX(th->accDelta));
    SV_WriteLong(FLT2FIX(th->targetScale));
    SV_WriteLong(FLT2FIX(th->scale));
    SV_WriteLong(FLT2FIX(th->scaleDelta));
    SV_WriteLong(th->ticker);
    SV_WriteLong(th->state);
}

static int SV_ReadFloorWaggle(waggle_t *th)
{
    sector_t *sector;

    if(saveVersion >= 4)
    {
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Start of used data members.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_FLOOR_WAGGLE: bad sector number\n");
        th->sector = sector;

        th->originalHeight = FIX2FLT((fixed_t) SV_ReadLong());
        th->accumulator = FIX2FLT((fixed_t) SV_ReadLong());
        th->accDelta = FIX2FLT((fixed_t) SV_ReadLong());
        th->targetScale = FIX2FLT((fixed_t) SV_ReadLong());
        th->scale = FIX2FLT((fixed_t) SV_ReadLong());
        th->scaleDelta = FIX2FLT((fixed_t) SV_ReadLong());
        th->ticker = SV_ReadLong();
        th->state = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V4 format which serialized waggle_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_FLOOR_WAGGLE: bad sector number\n");
        th->sector = sector;

        th->originalHeight = FIX2FLT((fixed_t) SV_ReadLong());
        th->accumulator = FIX2FLT((fixed_t) SV_ReadLong());
        th->accDelta = FIX2FLT((fixed_t) SV_ReadLong());
        th->targetScale = FIX2FLT((fixed_t) SV_ReadLong());
        th->scale = FIX2FLT((fixed_t) SV_ReadLong());
        th->scaleDelta = FIX2FLT((fixed_t) SV_ReadLong());
        th->ticker = SV_ReadLong();
        th->state = SV_ReadLong();
    }

    th->thinker.function = T_FloorWaggle;

    P_ToXSector(th->sector)->specialData = th;
    return true; // Add this thinker.
}
#endif // __JHEXEN__

#if !__JHEXEN__
static void SV_WriteFlash(lightflash_t* flash)
{
    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(flash->sector));

    SV_WriteLong(flash->count);
    SV_WriteLong((int) (255.0f * flash->maxLight));
    SV_WriteLong((int) (255.0f * flash->minLight));
    SV_WriteLong(flash->maxTime);
    SV_WriteLong(flash->minTime);
}

static int SV_ReadFlash(lightflash_t* flash)
{
    sector_t* sector;

    if(hdr.version >= 5)
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Start of used data members.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_FLASH: bad sector number\n");
        flash->sector = sector;

        flash->count = SV_ReadLong();
        flash->maxLight = (float) SV_ReadLong() / 255.0f;
        flash->minLight = (float) SV_ReadLong() / 255.0f;
        flash->maxTime = SV_ReadLong();
        flash->minTime = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V5 format which serialized lightflash_t
        // Padding at the start (an old thinker_t struct)
        SV_Read(junkbuffer, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_FLASH: bad sector number\n");
        flash->sector = sector;

        flash->count = SV_ReadLong();
        flash->maxLight = (float) SV_ReadLong() / 255.0f;
        flash->minLight = (float) SV_ReadLong() / 255.0f;
        flash->maxTime = SV_ReadLong();
        flash->minTime = SV_ReadLong();
    }

    flash->thinker.function = T_LightFlash;
    return true; // Add this thinker.
}

static void SV_WriteStrobe(strobe_t* strobe)
{
    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(strobe->sector));

    SV_WriteLong(strobe->count);
    SV_WriteLong((int) (255.0f * strobe->maxLight));
    SV_WriteLong((int) (255.0f * strobe->minLight));
    SV_WriteLong(strobe->darkTime);
    SV_WriteLong(strobe->brightTime);
}

static int SV_ReadStrobe(strobe_t *strobe)
{
    sector_t* sector;

    if(hdr.version >= 5)
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_STROBE: bad sector number\n");
        strobe->sector = sector;

        strobe->count = SV_ReadLong();
        strobe->maxLight = (float) SV_ReadLong() / 255.0f;
        strobe->minLight = (float) SV_ReadLong() / 255.0f;
        strobe->darkTime = SV_ReadLong();
        strobe->brightTime = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V5 format which serialized strobe_t
        // Padding at the start (an old thinker_t struct)
        SV_Read(junkbuffer, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_STROBE: bad sector number\n");
        strobe->sector = sector;

        strobe->count = SV_ReadLong();
        strobe->minLight = (float) SV_ReadLong() / 255.0f;
        strobe->maxLight = (float) SV_ReadLong() / 255.0f;
        strobe->darkTime = SV_ReadLong();
        strobe->brightTime = SV_ReadLong();
    }

    strobe->thinker.function = T_StrobeFlash;
    return true; // Add this thinker.
}

static void SV_WriteGlow(glow_t *glow)
{
    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(glow->sector));

    SV_WriteLong((int) (255.0f * glow->maxLight));
    SV_WriteLong((int) (255.0f * glow->minLight));
    SV_WriteLong(glow->direction);
}

static int SV_ReadGlow(glow_t *glow)
{
    sector_t* sector;

    if(hdr.version >= 5)
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_GLOW: bad sector number\n");
        glow->sector = sector;

        glow->maxLight = (float) SV_ReadLong() / 255.0f;
        glow->minLight = (float) SV_ReadLong() / 255.0f;
        glow->direction = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V5 format which serialized strobe_t
        // Padding at the start (an old thinker_t struct)
        SV_Read(junkbuffer, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_GLOW: bad sector number\n");
        glow->sector = sector;

        glow->minLight = (float) SV_ReadLong() / 255.0f;
        glow->maxLight = (float) SV_ReadLong() / 255.0f;
        glow->direction = SV_ReadLong();
    }

    glow->thinker.function = T_Glow;
    return true; // Add this thinker.
}

# if __JDOOM__ || __JDOOM64__
static void SV_WriteFlicker(fireflicker_t *flicker)
{
    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(flicker->sector));

    SV_WriteLong((int) (255.0f * flicker->maxLight));
    SV_WriteLong((int) (255.0f * flicker->minLight));
}

/**
 * T_FireFlicker was added to save games in ver5, therefore we don't have
 * an old format to support.
 */
static int SV_ReadFlicker(fireflicker_t *flicker)
{
    sector_t* sector;
    /*int ver =*/ SV_ReadByte(); // version byte.

    // Note: the thinker class byte has already been read.
    sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
    if(!sector)
        Con_Error("TC_FLICKER: bad sector number\n");
    flicker->sector = sector;

    flicker->maxLight = (float) SV_ReadLong() / 255.0f;
    flicker->minLight = (float) SV_ReadLong() / 255.0f;

    flicker->thinker.function = T_FireFlicker;
    return true; // Add this thinker.
}
# endif

# if __JDOOM64__
static void SV_WriteBlink(lightblink_t *blink)
{
    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(blink->sector));

    SV_WriteLong(blink->count);
    SV_WriteLong((int) (255.0f * blink->maxLight));
    SV_WriteLong((int) (255.0f * blink->minLight));
    SV_WriteLong(blink->maxTime);
    SV_WriteLong(blink->minTime);
}

/**
 * T_LightBlink was added to save games in ver5, therefore we don't have an
 * old format to support
 */
static int SV_ReadBlink(lightblink_t *blink)
{
    sector_t* sector;
    /*int ver =*/ SV_ReadByte(); // version byte.

    // Note: the thinker class byte has already been read.
    sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
    if(!sector)
        Con_Error("tc_lightblink: bad sector number\n");
    blink->sector = sector;

    blink->count = SV_ReadLong();
    blink->maxLight = (float) SV_ReadLong() / 255.0f;
    blink->minLight = (float) SV_ReadLong() / 255.0f;
    blink->maxTime = SV_ReadLong();
    blink->minTime = SV_ReadLong();

    blink->thinker.function = T_LightBlink;
    return true; // Add this thinker.
}
# endif
#endif // !__JHEXEN__

/**
 * Archives the specified thinker.
 *
 * @param th        The thinker to be archived.
 */
static boolean archiveThinker(thinker_t* th, void* context)
{
    boolean             savePlayers = *(boolean*) context;

    // Are we archiving players?
    if(!(th->function == P_MobjThinker && ((mobj_t *) th)->player &&
       !savePlayers))
    {
        thinkerinfo_t*      thInfo = infoForThinker(th);

        if(!thInfo)
            return true; // This is not a thinker we need to save.

        // Only the server saves this class of thinker?
        if(!((thInfo->flags & TSF_SERVERONLY) && IS_CLIENT))
        {
#if _DEBUG
assert(thInfo->Write);
#endif
            // Write the header block for this thinker.
            SV_WriteByte(thInfo->thinkclass); // Thinker type byte.
            SV_WriteByte(th->inStasis? 1 : 0); // In stasis?

            // Write the thinker data.
            thInfo->Write(th);
        }
    }

    return true; // Continue iteration.
}

/**
 * Archives thinkers for both client and server.
 * Clients do not save all data for all thinkers (the server will send
 * it to us anyway so saving it would just bloat client save games).
 *
 * \note Some thinker classes are NEVER saved by clients.
 */
static void P_ArchiveThinkers(boolean savePlayers)
{
    boolean             localSavePlayers = savePlayers;

    SV_BeginSegment(ASEG_THINKERS);
#if __JHEXEN__
    SV_WriteLong(thing_archiveSize); // number of mobjs.
#endif

    // Save off the current thinkers.
    P_IterateThinkers(NULL, archiveThinker, &localSavePlayers);

    // Add a terminating marker.
    SV_WriteByte(TC_END);
}

static boolean restoreMobjLinks(thinker_t* th, void* context)
{
    mobj_t*             mo = (mobj_t *) th;

    mo->target = SV_GetArchiveThing((int) mo->target, &mo->target);

#if __JHEXEN__
    switch(mo->type)
    {
    // Just tracer
    case MT_BISH_FX:
    case MT_HOLY_FX:
    case MT_DRAGON:
    case MT_THRUSTFLOOR_UP:
    case MT_THRUSTFLOOR_DOWN:
    case MT_MINOTAUR:
    case MT_SORCFX1:
        if(saveVersion >= 3)
        {
            mo->tracer = SV_GetArchiveThing((int) mo->tracer, &mo->tracer);
        }
        else
        {
            mo->tracer = SV_GetArchiveThing(mo->special1, &mo->tracer);
            mo->special1 = 0;
        }
        break;

    // Just special2
    case MT_LIGHTNING_FLOOR:
    case MT_LIGHTNING_ZAP:
        mo->special2 = (int) SV_GetArchiveThing(mo->special2, &mo->special2);
        break;

    // Both tracer and special2
    case MT_HOLY_TAIL:
    case MT_LIGHTNING_CEILING:
        if(saveVersion >= 3)
        {
            mo->tracer = SV_GetArchiveThing((int) mo->tracer, &mo->tracer);
        }
        else
        {
            mo->tracer = SV_GetArchiveThing(mo->special1, &mo->tracer);
            mo->special1 = 0;
        }
        mo->special2 = (int) SV_GetArchiveThing(mo->special2, &mo->special2);
        break;

    default:
        break;
    }
#else
    mo->onMobj = SV_GetArchiveThing((int) mo->onMobj, &mo->onMobj);
# if __JDOOM__ || __JDOOM64__
    mo->tracer = SV_GetArchiveThing((int) mo->tracer, &mo->tracer);
# endif
# if __JHERETIC__
    mo->generator = SV_GetArchiveThing((int) mo->generator, &mo->generator);
# endif
#endif

    return true; // Continue iteration.
}

/**
 * Un-Archives thinkers for both client and server.
 */
static void P_UnArchiveThinkers(void)
{
    uint        i;
    byte        tClass;
    thinker_t  *th = 0;
    thinkerinfo_t *thInfo = 0;
    boolean     found, knownThinker;
    boolean     inStasis;
#if __JHEXEN__
    boolean     doSpecials = (saveVersion >= 4);
#else
    boolean     doSpecials = (hdr.version >= 5);
#endif

#if !__JHEXEN__
    if(IS_SERVER)
#endif
    {
        P_IterateThinkers(NULL, removeThinker, NULL);
        P_InitThinkers();
    }

#if __JHEXEN__
    if(saveVersion < 4)
        AssertSegment(ASEG_MOBJS);
    else
#endif
        AssertSegment(ASEG_THINKERS);

#if __JHEXEN__
    targetPlayerAddrs = NULL;
    SV_InitThingArchive(true, savingPlayers);
#endif

    // Read in saved thinkers.
#if __JHEXEN__
    i = 0;
#endif
    for(;;)
    {
#if __JHEXEN__
        if(doSpecials)
#endif
            tClass = SV_ReadByte();

#if __JHEXEN__
        if(saveVersion < 4)
        {
            if(doSpecials) // Have we started on the specials yet?
            {
                // Versions prior to 4 used a different value to mark
                // the end of the specials data and the thinker class ids
                // are differrent, so we need to manipulate the thinker
                // class identifier value.
                if(tClass != TC_END)
                    tClass += 2;
            }
            else
                tClass = TC_MOBJ;

            if(tClass == TC_MOBJ && i == thing_archiveSize)
            {
                AssertSegment(ASEG_THINKERS);
                // We have reached the begining of the "specials" block.
                doSpecials = true;
                continue;
            }
        }
#else
        if(hdr.version < 5)
        {
            if(doSpecials) // Have we started on the specials yet?
            {
                // Versions prior to 5 used a different value to mark
                // the end of the specials data so we need to manipulate
                // the thinker class identifier value.
                if(tClass == PRE_VER5_END_SPECIALS)
                    tClass = TC_END;
                else
                    tClass += 3;
            }
            else if(tClass == TC_END)
            {
                // We have reached the begining of the "specials" block.
                doSpecials = true;
                continue;
            }
        }
#endif
        if(tClass == TC_END)
            break; // End of the list.

        found = knownThinker = inStasis = false;
        thInfo = thinkerInfo;
        while(thInfo->thinkclass != TC_NULL && !found)
        {
            if(thInfo->thinkclass == tClass)
            {
                found = true;

                // Not for us? (it shouldn't be here anyway!).
                if(!((thInfo->flags & TSF_SERVERONLY) && IS_CLIENT))
                {
                    // Mobjs use a special engine-side allocator.
                    if(thInfo->thinkclass == TC_MOBJ)
                    {
                        th = (thinker_t*)
                            P_MobjCreate(P_MobjThinker, 0, 0, 0, 0, 64, 64, 0);
                    }
                    else
                    {
                        th = Z_Calloc(thInfo->size,
                                      ((thInfo->flags & TSF_SPECIAL)? PU_MAPSPEC : PU_MAP),
                                      0);
                    }

                    // Is there a thinker header block?
#if __JHEXEN__
                    if(saveVersion >= 6)
#else
                    if(hdr.version >= 6)
#endif
                    {
                        inStasis = (boolean) SV_ReadByte();
                    }

                    knownThinker = thInfo->Read(th);
                }
            }
            if(!found)
                thInfo++;
        }
#if __JHEXEN__
        if(tClass == TC_MOBJ)
            i++;
#endif
        if(!found)
            Con_Error("P_UnarchiveThinkers: Unknown tClass %i in savegame",
                      tClass);

        if(knownThinker)
            P_ThinkerAdd(th);
        if(inStasis)
            P_ThinkerSetStasis(th, true);
    }

    // Update references to things.
#if __JHEXEN__
    P_IterateThinkers(P_MobjThinker, restoreMobjLinks, NULL);
#else
    if(IS_SERVER)
    {
        P_IterateThinkers(P_MobjThinker, restoreMobjLinks, NULL);

        for(i = 0; i < numlines; ++i)
        {
            xline_t *xline = P_ToXLine(P_ToPtr(DMU_LINEDEF, i));
            if(xline->xg)
                xline->xg->activator =
                    SV_GetArchiveThing((int) xline->xg->activator,
                                       &xline->xg->activator);
        }
    }
#endif

#if __JHEXEN__
    P_CreateTIDList();
    P_InitCreatureCorpseQueue(true);    // true = scan for corpses
#endif
}

#if __JDOOM__
static void P_ArchiveBrain(void)
{
    int                 i;

    SV_WriteByte(numBrainTargets);
    SV_WriteByte(brain.targetOn);
    // Write the mobj references using the mobj archive.
    for(i = 0; i < numBrainTargets; ++i)
        SV_WriteShort(SV_ThingArchiveNum(brainTargets[i]));
}

static void P_UnArchiveBrain(void)
{
    int                 i;

    if(hdr.version < 3)
        return; // No brain data before version 3.

    numBrainTargets = SV_ReadByte();
    brain.targetOn = SV_ReadByte();
    for(i = 0; i < numBrainTargets; ++i)
    {
        brainTargets[i] = (mobj_t*) (int) SV_ReadShort();
        brainTargets[i] = SV_GetArchiveThing((int) brainTargets[i], NULL);
    }

    if(gameMode == commercial)
        P_SpawnBrainTargets();
}
#endif

#if !__JHEXEN__
static void P_ArchiveSoundTargets(void)
{
    uint                i;
    xsector_t*          xsec;

    // Write the total number.
    SV_WriteLong(numSoundTargets);

    // Write the mobj references using the mobj archive.
    for(i = 0; i < numsectors; ++i)
    {
        xsec = P_ToXSector(P_ToPtr(DMU_SECTOR, i));

        if(xsec->soundTarget)
        {
            SV_WriteLong(i);
            SV_WriteShort(SV_ThingArchiveNum(xsec->soundTarget));
        }
    }
}

static void P_UnArchiveSoundTargets(void)
{
    uint                i;
    uint                secid;
    uint                numsoundtargets;
    xsector_t*          xsec;

    // Sound Target data was introduced in ver 5
    if(hdr.version < 5)
        return;

    // Read the number of targets
    numsoundtargets = SV_ReadLong();

    // Read in the sound targets.
    for(i = 0; i < numsoundtargets; ++i)
    {
        secid = SV_ReadLong();

        if(secid > numsectors)
            Con_Error("P_UnArchiveSoundTargets: bad sector number\n");

        xsec = P_ToXSector(P_ToPtr(DMU_SECTOR, secid));
        xsec->soundTarget = (mobj_t*) (int) SV_ReadShort();
        xsec->soundTarget =
            SV_GetArchiveThing((int) xsec->soundTarget, &xsec->soundTarget);
    }
}
#endif

#if __JHEXEN__
static void P_ArchiveSounds(void)
{
    uint                i;
    int                 difference;
    seqnode_t*          node;
    sector_t*           sec;

    // Save the sound sequences.
    SV_BeginSegment(ASEG_SOUNDS);
    SV_WriteLong(ActiveSequences);
    for(node = SequenceListHead; node; node = node->next)
    {
        SV_WriteByte(1); // Write a version byte.

        SV_WriteLong(node->sequence);
        SV_WriteLong(node->delayTics);
        SV_WriteLong(node->volume);
        SV_WriteLong(SN_GetSequenceOffset(node->sequence, node->sequencePtr));
        SV_WriteLong(node->currentSoundID);
        if(node->mobj)
        {
            for(i = 0; i < numpolyobjs; ++i)
            {
                if(node->mobj == (mobj_t*) P_GetPolyobj(i | 0x80000000))
                {
                    break;
                }
            }
        }

        if(i == numpolyobjs)
        {   // Sound is attached to a sector, not a polyobj.
            sec = P_GetPtrp(R_PointInSubsector(node->mobj->pos[VX], node->mobj->pos[VY]),
                            DMU_SECTOR);
            difference = P_ToIndex(sec);
            SV_WriteLong(0); // 0 -- sector sound origin.
        }
        else
        {
            SV_WriteLong(1); // 1 -- polyobj sound origin
            difference = i;
        }
        SV_WriteLong(difference);
    }
}

static void P_UnArchiveSounds(void)
{
    int             i;
    int             numSequences, sequence, seqOffset;
    int             delayTics, soundID, volume;
    int             polySnd, secNum, ver;
    mobj_t*         sndMobj;

    AssertSegment(ASEG_SOUNDS);

    // Reload and restart all sound sequences
    numSequences = SV_ReadLong();
    i = 0;
    while(i < numSequences)
    {
        if(saveVersion >= 3)
            ver = SV_ReadByte();

        sequence = SV_ReadLong();
        delayTics = SV_ReadLong();
        volume = SV_ReadLong();
        seqOffset = SV_ReadLong();

        soundID = SV_ReadLong();
        polySnd = SV_ReadLong();
        secNum = SV_ReadLong();
        if(!polySnd)
        {
            sndMobj = P_GetPtr(DMU_SECTOR, secNum, DMU_SOUND_ORIGIN);
        }
        else
        {
            polyobj_t*          po;

            if((po = P_GetPolyobj(secNum | 0x80000000)))
                sndMobj = (mobj_t*) po;
        }

        SN_StartSequence(sndMobj, sequence);
        SN_ChangeNodeData(i, seqOffset, delayTics, volume, soundID);
        i++;
    }
}

static void P_ArchiveScripts(void)
{
    int         i;

    SV_BeginSegment(ASEG_SCRIPTS);
    for(i = 0; i < ACScriptCount; ++i)
    {
        SV_WriteShort(ACSInfo[i].state);
        SV_WriteShort(ACSInfo[i].waitValue);
    }
    SV_Write(MapVars, sizeof(MapVars));
}

static void P_UnArchiveScripts(void)
{
    int         i;

    AssertSegment(ASEG_SCRIPTS);
    for(i = 0; i < ACScriptCount; ++i)
    {
        ACSInfo[i].state = SV_ReadShort();
        ACSInfo[i].waitValue = SV_ReadShort();
    }
    memcpy(MapVars, saveptr.b, sizeof(MapVars));
    saveptr.b += sizeof(MapVars);
}

static void P_ArchiveMisc(void)
{
    int         ix;

    SV_BeginSegment(ASEG_MISC);
    for(ix = 0; ix < MAXPLAYERS; ++ix)
    {
        SV_WriteLong(localQuakeHappening[ix]);
    }
}

static void P_UnArchiveMisc(void)
{
    int         ix;

    AssertSegment(ASEG_MISC);
    for(ix = 0; ix < MAXPLAYERS; ++ix)
    {
        localQuakeHappening[ix] = SV_ReadLong();
    }
}
#endif

static void P_ArchiveMap(boolean savePlayers)
{
    // Place a header marker
    SV_BeginSegment(ASEG_MAP_HEADER2);

#if __JHEXEN__
    savingPlayers = savePlayers;

    // Write a version byte
    SV_WriteByte(MY_SAVE_VERSION);

    // Write the map timer
    SV_WriteLong(mapTime);
#else
    // Clear the sound target count (determined while saving sectors).
    numSoundTargets = 0;
#endif

    SV_InitMaterialArchives();

    P_ArchiveWorld();
    P_ArchiveThinkers(savePlayers);

#if __JHEXEN__
    P_ArchiveScripts();
    P_ArchiveSounds();
    P_ArchiveMisc();
#else
    if(IS_SERVER)
    {
# if __JDOOM__
        // Doom saves the brain data, too. (It's a part of the world.)
        P_ArchiveBrain();
# endif
        // Save the sound target data (prevents bug where monsters who have
        // been alerted go back to sleep when loading a save game).
        P_ArchiveSoundTargets();
    }
#endif

    // Place a termination marker
    SV_BeginSegment(ASEG_END);
}

static void P_UnArchiveMap(void)
{
#if __JHEXEN__
    int                 segType = SV_ReadLong();

    // Determine the map version.
    if(segType == ASEG_MAP_HEADER2)
    {
        saveVersion = SV_ReadByte();
    }
    else if(segType == ASEG_MAP_HEADER)
    {
        saveVersion = 2;
    }
    else
    {
        Con_Error("Corrupt save game: Segment [%d] failed alignment check",
                  ASEG_MAP_HEADER);
    }
#else
    AssertSegment(ASEG_MAP_HEADER2);
#endif

#if __JHEXEN__
    // Read the map timer
    mapTime = SV_ReadLong();
#endif

    P_UnArchiveWorld();
    P_UnArchiveThinkers();

#if __JHEXEN__
    P_UnArchiveScripts();
    P_UnArchiveSounds();
    P_UnArchiveMisc();
#else
    if(IS_SERVER)
    {
#if __JDOOM__
        // Doom saves the brain data, too. (It's a part of the world.)
        P_UnArchiveBrain();
#endif

        // Read the sound target data (prevents bug where monsters who have
        // been alerted go back to sleep when loading a save game).
        P_UnArchiveSoundTargets();
    }
#endif

    AssertSegment(ASEG_END);
}

int SV_GetSaveDescription(char *filename, char *str)
{
#if __JHEXEN__
    LZFILE     *fp;
    char        name[256];
    char        versionText[HXS_VERSION_TEXT_LENGTH];
    boolean     found = false;

    strncpy(name, filename, sizeof(name));
    M_TranslatePath(name, name);
    fp = lzOpen(name, "rp");
    if(fp)
    {
        lzRead(str, SAVESTRINGSIZE, fp);
        lzRead(versionText, HXS_VERSION_TEXT_LENGTH, fp);
        lzClose(fp);
        if(!strncmp(versionText, HXS_VERSION_TEXT, 8))
        {
            saveVersion = atoi(&versionText[8]);
            if(saveVersion <= MY_SAVE_VERSION)
                found = true;
        }
    }
    return found;
#else
    savefile = lzOpen(filename, "rp");
    if(!savefile)
    {
# if __JDOOM64__
        // We don't support the original game's save format (for obvious
        // reasons).
        return false;
# else
        // It might still be a v19 savegame.
        savefile = lzOpen(filename, "r");
        if(!savefile)
            return false;       // It just doesn't exist.
        lzRead(str, SAVESTRINGSIZE, savefile);
        str[SAVESTRINGSIZE - 1] = 0;
        lzClose(savefile);
        return true;
# endif
    }

    // Read the header.
    lzRead(&hdr, sizeof(hdr), savefile);
    lzClose(savefile);
    // Check the magic.
    if(hdr.magic != MY_SAVE_MAGIC)
        // This isn't a proper savegame file.
        return false;
    strcpy(str, hdr.description);
    return true;
#endif
}

/**
 * Initialize the savegame directories.
 * If the directories do not exist, they are created.
 */
void SV_Init(void)
{
    if(ArgCheckWith("-savedir", 1))
    {
        strcpy(savePath, ArgNext());
        // Add a trailing backslash is necessary.
        if(savePath[strlen(savePath) - 1] != '\\')
            strcat(savePath, "\\");
    }
    else
    {
        // Use the default path.
#if __JHEXEN__
        sprintf(savePath, "hexndata\\%s\\", (char *) G_GetVariable(DD_GAME_MODE));
#else
        sprintf(savePath, "savegame\\%s\\", (char *) G_GetVariable(DD_GAME_MODE));
#endif
    }

    // Build the client save path.
    strcpy(clientSavePath, savePath);
    strcat(clientSavePath, "client\\");

    // Check that the save paths exist.
    M_CheckPath(savePath);
    M_CheckPath(clientSavePath);
#if !__JHEXEN__
    M_TranslatePath(savePath, savePath);
    M_TranslatePath(clientSavePath, clientSavePath);
#endif
}

void SV_GetSaveGameFileName(int slot, char *str)
{
    sprintf(str, "%s" SAVEGAMENAME "%i." SAVEGAMEEXTENSION, savePath, slot);
}

void SV_GetClientSaveGameFileName(unsigned int game_id, char *str)
{
    sprintf(str, "%s" CLIENTSAVEGAMENAME "%08X." SAVEGAMEEXTENSION,
            clientSavePath, game_id);
}

enum {
    SV_OK = 0,
    SV_INVALIDFILENAME,
};

typedef struct savegameparam_s {
#if __JHEXEN__
    int         slot;
#endif
    char       *filename;
    char       *description;
} savegameparam_t;

int SV_SaveGameWorker(void *ptr)
{
    savegameparam_t *param = ptr;
#if __JHEXEN__
    char        versionText[HXS_VERSION_TEXT_LENGTH];
#else
    int     i;
#endif

    playerHeaderOK = false; // Uninitialized.

    // Open the output file
    savefile = lzOpen(param->filename, "wp");
    if(!savefile)
    {
        Con_BusyWorkerEnd();
        return SV_INVALIDFILENAME;           // No success.
    }

#if __JHEXEN__
    // Write game save description
    SV_Write(param->description, SAVESTRINGSIZE);

    // Write version info
    memset(versionText, 0, HXS_VERSION_TEXT_LENGTH);
    sprintf(versionText, HXS_VERSION_TEXT"%i", MY_SAVE_VERSION);
    SV_Write(versionText, HXS_VERSION_TEXT_LENGTH);

    // Place a header marker
    SV_BeginSegment(ASEG_GAME_HEADER);

    // Write current map and difficulty
    SV_WriteByte(gameMap);
    SV_WriteByte(gameSkill);
    SV_WriteByte(deathmatch);
    SV_WriteByte(noMonstersParm);
    SV_WriteByte(randomClassParm);

    // Write global script info
    SV_Write(WorldVars, sizeof(WorldVars));
    SV_Write(ACSStore, sizeof(ACSStore));
#else
    // Write the header.
    hdr.magic = MY_SAVE_MAGIC;
    hdr.version = MY_SAVE_VERSION;
# if __JDOOM__ || __JDOOM64__
    hdr.gameMode = gameMode;
# elif __JHERETIC__
    hdr.gameMode = 0;
# endif

    strncpy(hdr.description, param->description, SAVESTRINGSIZE);
    hdr.description[SAVESTRINGSIZE - 1] = 0;
    hdr.skill = gameSkill;
    if(fastParm)
        hdr.skill |= 0x80;      // Set high byte.
    hdr.episode = gameEpisode;
    hdr.map = gameMap;
    hdr.deathmatch = deathmatch;
    hdr.noMonsters = noMonstersParm;
    hdr.respawnMonsters = respawnMonsters;
    hdr.mapTime = mapTime;
    hdr.gameID = SV_GameID();
    for(i = 0; i < MAXPLAYERS; i++)
        hdr.players[i] = players[i].plr->inGame;
    lzWrite(&hdr, sizeof(hdr), savefile);

    // In netgames the server tells the clients to save their games.
    NetSv_SaveGame(hdr.gameID);
#endif

    // Set the mobj archive numbers
    SV_InitThingArchive(false, true);
#if !__JHEXEN__
    SV_WriteLong(thing_archiveSize);
#endif

    P_ArchivePlayerHeader();
    P_ArchivePlayers();

    // Place a termination marker
    SV_BeginSegment(ASEG_END);

#if __JHEXEN__
    // Close the output file (maps are saved into a seperate file).
    CloseStreamOut();
#endif

    // Save out the current map
#if __JHEXEN__
    {
        char        fileName[100];

        // Open the output file
        sprintf(fileName, "%shex6%02d.hxs", savePath, gameMap);
        M_TranslatePath(fileName, fileName);
        OpenStreamOut(fileName);

        P_ArchiveMap(true);         // true = save player info

        // Close the output file
        CloseStreamOut();
    }
#else
    P_ArchiveMap(true);
#endif

#if!__JHEXEN__
    // To be absolutely sure...
    SV_WriteByte(CONSISTENCY);

    SV_FreeThingArchive();
    lzClose(savefile);
#endif

#if __JHEXEN__
    // Clear all save files at destination slot.
    ClearSaveSlot(param->slot);

    // Copy base slot to destination slot
    CopySaveSlot(BASE_SLOT, param->slot);
#endif

    Con_BusyWorkerEnd();
    return SV_OK; // Success!
}

#if __JHEXEN__
boolean SV_SaveGame(int slot, char *description)
#else
boolean SV_SaveGame(char *filename, char *description)
#endif
{
    int         result;
#if __JHEXEN__
    char        filename[256];
#endif
    savegameparam_t param;

#if __JHEXEN__
    param.slot = slot;
#endif
#if __JHEXEN__
    sprintf(filename, "%shex6.hxs", savePath);
    M_TranslatePath(filename, filename);
#endif
    param.filename = filename;
    param.description = description;

    // \todo Use progress bar mode and update progress during the setup.
    result = Con_Busy(BUSYF_ACTIVITY | /*BUSYF_PROGRESS_BAR |*/ (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                      "Saving game...", SV_SaveGameWorker, &param);

    if(result == SV_INVALIDFILENAME)
    {
        Con_Message("P_SaveGame: Couldn't open \"%s\" for writing.\n",
                    filename);
    }

    return result;
}

#if __JHEXEN__
static boolean readSaveHeader(void)
#else
static boolean readSaveHeader(saveheader_t *hdr, LZFILE *savefile)
#endif
{
#if __JHEXEN__
    // Set the save pointer and skip the description field
    saveptr.b = saveBuffer + SAVESTRINGSIZE;

    if(strncmp(saveptr.b, HXS_VERSION_TEXT, 8))
    {
        Con_Message("SV_LoadGame: Bad magic.\n");
        return false;
    }

    saveVersion = atoi(saveptr.b + 8);

    // Check for unsupported versions.
    if(saveVersion > MY_SAVE_VERSION)
    {
        return false; // A future version
    }
    // We are incompatible with ver3 saves. Due to an invalid test
    // used to determine present sidedefs, the ver3 format's sides
    // included chunks of junk data.
    if(saveVersion == 3)
        return false;

    saveptr.b += HXS_VERSION_TEXT_LENGTH;

    AssertSegment(ASEG_GAME_HEADER);

    gameEpisode = 1;
    gameMap = SV_ReadByte();
    gameSkill = SV_ReadByte();
    deathmatch = SV_ReadByte();
    noMonstersParm = SV_ReadByte();
    randomClassParm = SV_ReadByte();

#else
    lzRead(hdr, sizeof(*hdr), savefile);

    if(hdr->magic != MY_SAVE_MAGIC)
    {
        Con_Message("SV_LoadGame: Bad magic.\n");
        return false;
    }

    // Check for unsupported versions.
    if(hdr->version > MY_SAVE_VERSION)
    {
        return false; // A future version.
    }

# if __JDOOM__ || __JDOOM64__
    if(hdr->gameMode != gameMode && !ArgExists("-nosavecheck"))
    {
        Con_Message("SV_LoadGame: savegame not from gameMode %i.\n",
                    gameMode);
        return false;
    }
# endif

    gameSkill = hdr->skill & 0x7f;
    fastParm = (hdr->skill & 0x80) != 0;
    gameEpisode = hdr->episode;
    gameMap = hdr->map;
    deathmatch = hdr->deathmatch;
    noMonstersParm = hdr->noMonsters;
    respawnMonsters = hdr->respawnMonsters;
#endif

    return true; // Read was OK.
}

static boolean SV_LoadGame2(void)
{
    int         i;
    char        buf[80];
    boolean     loaded[MAXPLAYERS], infile[MAXPLAYERS];
#if __JHEXEN__
    int         k;
    player_t    playerBackup[MAXPLAYERS];
#endif

    // Read the header.
#if __JHEXEN__
    if(!readSaveHeader())
#else
    if(!readSaveHeader(&hdr, savefile))
#endif
        return false; // Something went wrong.

    // Read global save data not part of the game metadata.
#if __JHEXEN__
    // Read global script info
    memcpy(WorldVars, saveptr.b, sizeof(WorldVars));
    saveptr.b += sizeof(WorldVars);
    memcpy(ACSStore, saveptr.b, sizeof(ACSStore));
    saveptr.b += sizeof(ACSStore);
#endif

    // Allocate a small junk buffer.
    // (Data from old save versions is read into here).
    junkbuffer = malloc(sizeof(byte) * 64);

#if !__JHEXEN__
    // Load the map.
    G_InitNew(gameSkill, gameEpisode, gameMap);

    // Set the time.
    mapTime = hdr.mapTime;

    SV_InitThingArchive(true, true);
#endif

    P_UnArchivePlayerHeader();
    // Read the player structures
    AssertSegment(ASEG_PLAYERS);

    // We don't have the right to say which players are in the game. The
    // players that already are will continue to be. If the data for a given
    // player is not in the savegame file, he will be notified. The data for
    // players who were saved but are not currently in the game will be
    // discarded.
#if !__JHEXEN__
    for(i = 0; i < MAXPLAYERS; ++i)
        infile[i] = hdr.players[i];
#else
    for(i = 0; i < MAXPLAYERS; ++i)
        infile[i] = SV_ReadByte();
#endif

    memset(loaded, 0, sizeof(loaded));
    P_UnArchivePlayers(infile, loaded);
    AssertSegment(ASEG_END);

#if __JHEXEN__
    Z_Free(saveBuffer);

    // Save player structs
    memcpy(playerBackup, players, sizeof(playerBackup));
#endif

    // Load the current map
#if __JHEXEN__
    SV_HxLoadMap();
#else
    // Load the current map
    SV_DMLoadMap();
#endif

#if !__JHEXEN__
    // Check consistency.
    if(SV_ReadByte() != CONSISTENCY)
        Con_Error("SV_LoadGame: Bad savegame (consistency test failed!)\n");

    // We're done.
    SV_FreeThingArchive();
    lzClose(savefile);
#endif

#if __JHEXEN__
    // Don't need the player mobj relocation info for load game
    SV_FreeTargetPlayerList();

    // Restore player structs
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        mobj_t *mo = players[i].plr->mo;

        memcpy(&players[i], &playerBackup[i], sizeof(player_t));
        players[i].plr->mo = mo;
        players[i].readyArtifact = players[i].inventory[players[i].invPtr].type;

        P_InventoryResetCursor(&players[i]);
    }
#endif

    // Notify the players that weren't in the savegame.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        boolean notLoaded = false;

#if __JHEXEN__
        if(players[i].plr->inGame)
        {
            // Try to find a saved player that corresponds this one.
            for(k = 0; k < MAXPLAYERS; ++k)
                if(saveToRealPlayerNum[k] == i)
                    break;
            if(k < MAXPLAYERS)
                continue;           // Found; don't bother this player.

            players[i].playerState = PST_REBORN;

            if(!i)
            {
                // If the CONSOLEPLAYER isn't in the save, it must be some
                // other player's file?
                P_SetMessage(players, GET_TXT(TXT_LOADMISSING), false);
            }
            else
            {
                NetSv_SendMessage(i, GET_TXT(TXT_LOADMISSING));
                notLoaded = true;
            }
        }
#else
        if(!loaded[i] && players[i].plr->inGame)
        {
            if(!i)
            {
                P_SetMessage(players, GET_TXT(TXT_LOADMISSING), false);
            }
            else
            {
                NetSv_SendMessage(i, GET_TXT(TXT_LOADMISSING));
            }
            notLoaded = true;
        }
#endif

        if(notLoaded)
        {
            // Kick this player out, he doesn't belong here.
            sprintf(buf, "kick %i", i);
            DD_Execute(false, buf);
        }
    }

#if !__JHEXEN__
    // In netgames, the server tells the clients about this.
    NetSv_LoadGame(hdr.gameID);
#endif

    return true; // Success!
}

#if __JHEXEN__
boolean SV_LoadGame(int slot)
#else
boolean SV_LoadGame(char *filename)
#endif
{
#if __JHEXEN__
    char        fileName[200];

    // Copy all needed save files to the base slot
    if(slot != BASE_SLOT)
    {
        ClearSaveSlot(BASE_SLOT);
        CopySaveSlot(slot, BASE_SLOT);
    }

    // Create the name
    sprintf(fileName, "%shex6.hxs", savePath);
    M_TranslatePath(fileName, fileName);

    // Load the file
    M_ReadFile(fileName, &saveBuffer);
#else
    // Make sure an opening briefing is not shown.
    // (G_InitNew --> G_DoLoadMap)
    briefDisabled = true;

    savefile = lzOpen(filename, "rp");
    if(!savefile)
    {
# if __JDOOM64__
        // We don't support the original game's save format (for obvious
        // reasons).
        return false;
# else
#  if __JDOOM__
        // It might still be a v19 savegame.
        SV_v19_LoadGame(filename);
#  elif __JHERETIC__
        SV_v13_LoadGame(filename);
#  endif
# endif
        return true;
    }
#endif

    playerHeaderOK = false; // Uninitialized.

    return SV_LoadGame2();
}

/**
 * Saves a snapshot of the world, a still image.
 * No data of movement is included (server sends it).
 */
void SV_SaveClient(unsigned int gameID)
{
#if !__JHEXEN__ // unsupported in jHexen
    char                name[200];
    player_t           *pl = &players[CONSOLEPLAYER];
    mobj_t             *mo = pl->plr->mo;

    if(!IS_CLIENT || !mo)
        return;

    playerHeaderOK = false; // Uninitialized.

    SV_GetClientSaveGameFileName(gameID, name);
    // Open the file.
    savefile = lzOpen(name, "wp");
    if(!savefile)
    {
        Con_Message("SV_SaveClient: Couldn't open \"%s\" for writing.\n",
                    name);
        return;
    }
    // Prepare the header.
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = MY_CLIENT_SAVE_MAGIC;
    hdr.version = MY_SAVE_VERSION;
    hdr.skill = gameSkill;
    hdr.episode = gameEpisode;
    hdr.map = gameMap;
    hdr.deathmatch = deathmatch;
    hdr.noMonsters = noMonstersParm;
    hdr.respawnMonsters = respawnMonsters;
    hdr.mapTime = mapTime;
    hdr.gameID = gameID;
    SV_Write(&hdr, sizeof(hdr));

    // Some important information.
    // Our position and look angles.
    SV_WriteLong(FLT2FIX(mo->pos[VX]));
    SV_WriteLong(FLT2FIX(mo->pos[VY]));
    SV_WriteLong(FLT2FIX(mo->pos[VZ]));
    SV_WriteLong(FLT2FIX(mo->floorZ));
    SV_WriteLong(FLT2FIX(mo->ceilingZ));
    SV_WriteLong(mo->angle); /* $unifiedangles */
    SV_WriteFloat(pl->plr->lookDir); /* $unifiedangles */
    P_ArchivePlayerHeader();
    SV_WritePlayer(CONSOLEPLAYER);

    P_ArchiveMap(true);

    lzClose(savefile);
    free(junkbuffer);
#endif
}

void SV_LoadClient(unsigned int gameid)
{
#if !__JHEXEN__ // unsupported in jHexen
    char    name[200];
    player_t *cpl = players + CONSOLEPLAYER;
    mobj_t *mo = cpl->plr->mo;

    if(!IS_CLIENT || !mo)
        return;

    playerHeaderOK = false; // Uninitialized.

    SV_GetClientSaveGameFileName(gameid, name);
    // Try to open the file.
    savefile = lzOpen(name, "rp");
    if(!savefile)
        return;

    SV_Read(&hdr, sizeof(hdr));
    if(hdr.magic != MY_CLIENT_SAVE_MAGIC)
    {
        lzClose(savefile);
        Con_Message("SV_LoadClient: Bad magic!\n");
        return;
    }

    // Allocate a small junk buffer.
    // (Data from old save versions is read into here)
    junkbuffer = malloc(sizeof(byte) * 64);

    gameSkill = hdr.skill;
    deathmatch = hdr.deathmatch;
    noMonstersParm = hdr.noMonsters;
    respawnMonsters = hdr.respawnMonsters;
    // Do we need to change the map?
    if(gameMap != hdr.map || gameEpisode != hdr.episode)
    {
        gameMap = hdr.map;
        gameEpisode = hdr.episode;
        G_InitNew(gameSkill, gameEpisode, gameMap);
    }
    mapTime = hdr.mapTime;

    P_MobjUnsetPosition(mo);
    mo->pos[VX] = FIX2FLT(SV_ReadLong());
    mo->pos[VY] = FIX2FLT(SV_ReadLong());
    mo->pos[VZ] = FIX2FLT(SV_ReadLong());
    P_MobjSetPosition(mo);
    mo->floorZ = FIX2FLT(SV_ReadLong());
    mo->ceilingZ = FIX2FLT(SV_ReadLong());
    mo->angle = SV_ReadLong(); /* $unifiedangles */
    cpl->plr->lookDir = SV_ReadFloat(); /* $unifiedangles */
    P_UnArchivePlayerHeader();
    SV_ReadPlayer(cpl);

    P_UnArchiveMap();

    lzClose(savefile);
    free(junkbuffer);
#endif
}

#if !__JHEXEN__
static void SV_DMLoadMap(void)
{
    P_UnArchiveMap();

    // Spawn particle generators, fix HOMS etc, etc...
    R_SetupMap(DDSMM_AFTER_LOADING, 0);
}
#endif

#if __JHEXEN__
static void SV_HxLoadMap(void)
{
    char        fileName[100];

#ifdef _DEBUG
    Con_Printf("SV_HxLoadMap: Begin, G_InitNew...\n");
#endif

    // We don't want to see a briefing if we're loading a save game.
    briefDisabled = true;

    // Load a base map
    G_InitNew(gameSkill, gameEpisode, gameMap);

    // Create the name
    sprintf(fileName, "%shex6%02d.hxs", savePath, gameMap);
    M_TranslatePath(fileName, fileName);

#ifdef _DEBUG
    Con_Printf("SV_HxLoadMap: Reading %s\n", fileName);
#endif

    // Load the file
    M_ReadFile(fileName, &saveBuffer);
    saveptr.b = saveBuffer;

    P_UnArchiveMap();

    // Free mobj list and save buffer
    SV_FreeThingArchive();
    Z_Free(saveBuffer);

    // Spawn particle generators, fix HOMS etc, etc...
    R_SetupMap(DDSMM_AFTER_LOADING, 0);
}

void SV_MapTeleport(int map, int position)
{
    int         i;
    int         j;
    char        fileName[100];
    player_t    playerBackup[MAXPLAYERS];
    mobj_t     *targetPlayerMobj;
    boolean     rClass;
    boolean     playerWasReborn;
    boolean     oldWeaponOwned[NUM_WEAPON_TYPES];
    int         oldKeys = 0;
    int         oldPieces = 0;
    int         bestWeapon;

    playerHeaderOK = false; // Uninitialized.

    if(!deathmatch)
    {
        if(P_GetMapCluster(gameMap) == P_GetMapCluster(map))
        {   // Same cluster - save map without saving player mobjs
            char        fileName[100];

            // Set the mobj archive numbers
            SV_InitThingArchive(false, false);

            // Open the output file
            sprintf(fileName, "%shex6%02d.hxs", savePath, gameMap);
            M_TranslatePath(fileName, fileName);
            OpenStreamOut(fileName);

            P_ArchiveMap(false);

            // Close the output file
            CloseStreamOut();
        }
        else
        {   // Entering new cluster - clear base slot
            ClearSaveSlot(BASE_SLOT);
        }
    }

    // Store player structs for later
    rClass = randomClassParm;
    randomClassParm = false;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        memcpy(&playerBackup[i], &players[i], sizeof(player_t));
    }

    // Only SV_HxLoadMap() uses targetPlayerAddrs, so it's NULLed here
    // for the following check (player mobj redirection)
    targetPlayerAddrs = NULL;

    gameMap = map;
    sprintf(fileName, "%shex6%02d.hxs", savePath, gameMap);
    M_TranslatePath(fileName, fileName);
    if(!deathmatch && ExistingFile(fileName))
    {   // Unarchive map.
        SV_HxLoadMap();
        briefDisabled = true;
    }
    else
    {   // New map.
        G_InitNew(gameSkill, gameEpisode, gameMap);

        // Destroy all freshly spawned players
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
            {
                P_MobjRemove(players[i].plr->mo, true);
            }
        }
    }

    // Restore player structs.
    targetPlayerMobj = NULL;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
        {
            continue;
        }
        memcpy(&players[i], &playerBackup[i], sizeof(player_t));
        HUMsg_ClearMessages(i);
        players[i].attacker = NULL;
        players[i].poisoner = NULL;

        if(IS_NETGAME || deathmatch)
        {
            if(players[i].playerState == PST_DEAD)
            {   // In a network game, force all players to be alive
                players[i].playerState = PST_REBORN;
            }
            if(!deathmatch)
            {   // Cooperative net-play, retain keys and weapons
                oldKeys = players[i].keys;
                oldPieces = players[i].pieces;
                for(j = 0; j < NUM_WEAPON_TYPES; j++)
                {
                    oldWeaponOwned[j] = players[i].weapons[j].owned;
                }
            }
        }
        playerWasReborn = (players[i].playerState == PST_REBORN);
        if(deathmatch)
        {
            memset(players[i].frags, 0, sizeof(players[i].frags));
            players[i].plr->mo = NULL;
            G_DeathMatchSpawnPlayer(i);
        }
        else
        {
            P_SpawnPlayer(P_GetPlayerStart(position, i), i);
        }

        if(playerWasReborn && IS_NETGAME && !deathmatch)
        {   // Restore keys and weapons when reborn in co-op
            players[i].keys = oldKeys;
            players[i].pieces = oldPieces;
            for(bestWeapon = 0, j = 0; j < NUM_WEAPON_TYPES; ++j)
            {
                if(oldWeaponOwned[j])
                {
                    bestWeapon = j;
                    players[i].weapons[j].owned = true;
                }
            }
            players[i].ammo[AT_BLUEMANA].owned = 25; //// \fixme values.ded
            players[i].ammo[AT_GREENMANA].owned = 25; //// \fixme values.ded
            if(bestWeapon)
            {   // Bring up the best weapon
                players[i].pendingWeapon = bestWeapon;
            }
        }

        if(targetPlayerMobj == NULL)
        {   // The poor sap.
            targetPlayerMobj = players[i].plr->mo;
        }
    }
    randomClassParm = rClass;

    //// \fixme Redirect anything targeting a player mobj
    //// FIXME! This only supports single player games!!
    if(targetPlayerAddrs)
    {
        targetplraddress_t *p;

        p = targetPlayerAddrs;
        while(p != NULL)
        {
            *(p->address) = targetPlayerMobj;
            p = p->next;
        }
        SV_FreeTargetPlayerList();

        /* DJS - When XG is available in jHexen, call this after updating
        target player references (after a load).
        // The activator mobjs must be set.
        XL_UpdateActivators();
        */
    }

    // Destroy all things touching players
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
        {
            P_TeleportMove(players[i].plr->mo, players[i].plr->mo->pos[VX],
                           players[i].plr->mo->pos[VY], true);
        }
    }

    // Launch waiting scripts
    if(!deathmatch)
    {
        P_CheckACSStore();
    }

    // For single play, save immediately into the reborn slot
    if(!IS_NETGAME && !deathmatch)
    {
        SV_SaveGame(REBORN_SLOT, REBORN_DESCRIPTION);
    }
}
#endif
