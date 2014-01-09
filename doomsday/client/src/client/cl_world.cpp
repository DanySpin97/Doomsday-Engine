/** @file cl_world.cpp Clientside world management.
 * @ingroup client
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_filesys.h"
#include "de_defs.h"
#include "de_misc.h"
#include "api_map.h"
#include "api_materialarchive.h"

#include "world/map.h"
#include "world/thinkers.h"
#include "client/cl_world.h"
#include "r_util.h"

using namespace de;

#define MAX_TRANSLATIONS    16384

#define MVF_CEILING         0x1 // Move ceiling.
#define MVF_SET_FLOORPIC    0x2 // Set floor texture when move done.

typedef struct clplane_s {
    thinker_t   thinker;
    int         sectorIndex;
    clplanetype_t type;
    int         property; // floor or ceiling
    int         dmuPlane;
    coord_t     destination;
    float       speed;
} clplane_t;

typedef struct clpolyobj_s {
    thinker_t   thinker;
    int         number;
    Polyobj    *polyobj;
    boolean     move;
    boolean     rotate;
} clpolyobj_t;

typedef struct {
    int size;
    int *serverToLocal;
} indextranstable_t;

/**
 * Plane mover. Makes changes in planes using DMU.
 */
void Cl_MoverThinker(clplane_t *mover);

void Cl_PolyMoverThinker(clpolyobj_t *mover);

static MaterialArchive* serverMaterials;
static indextranstable_t xlatMobjType;
static indextranstable_t xlatMobjState;

void Cl_ReadServerMaterials(void)
{
    LOG_AS("Cl_ReadServerMaterials");

    if(!serverMaterials)
    {
        serverMaterials = MaterialArchive_NewEmpty(false /*no segment check*/);
    }
    MaterialArchive_Read(serverMaterials, msgReader, -1 /*no forced version*/);

    LOGDEV_NET_VERBOSE("Received %i materials") << MaterialArchive_Count(serverMaterials);
}

static void setTableSize(indextranstable_t* table, int size)
{
    if(size > 0)
    {
        table->serverToLocal = (int*) M_Realloc(table->serverToLocal,
                                                sizeof(*table->serverToLocal) * size);
    }
    else
    {
        M_Free(table->serverToLocal);
        table->serverToLocal = 0;
    }
    table->size = size;
}

void Cl_ReadServerMobjTypeIDs(void)
{
    int i;
    StringArray* ar = StringArray_New();
    StringArray_Read(ar, msgReader);

    LOG_AS("Cl_ReadServerMobjTypeIDs");
    LOGDEV_NET_VERBOSE("Received %i mobj type IDs") << StringArray_Size(ar);

    setTableSize(&xlatMobjType, StringArray_Size(ar));

    // Translate the type IDs to local.
    for(i = 0; i < StringArray_Size(ar); ++i)
    {
        xlatMobjType.serverToLocal[i] = Def_GetMobjNum(StringArray_At(ar, i));
        if(xlatMobjType.serverToLocal[i] < 0)
        {
            LOG_NET_WARNING("Could not find '%s' in local thing definitions")
                    << StringArray_At(ar, i);
        }
    }

    StringArray_Delete(ar);
}

void Cl_ReadServerMobjStateIDs(void)
{
    int i;
    StringArray* ar = StringArray_New();
    StringArray_Read(ar, msgReader);

    LOG_AS("Cl_ReadServerMobjStateIDs");
    LOGDEV_NET_VERBOSE("Received %i mobj state IDs") << StringArray_Size(ar);

    setTableSize(&xlatMobjState, StringArray_Size(ar));

    // Translate the type IDs to local.
    for(i = 0; i < StringArray_Size(ar); ++i)
    {
        xlatMobjState.serverToLocal[i] = Def_GetStateNum(StringArray_At(ar, i));
        if(xlatMobjState.serverToLocal[i] < 0)
        {
            LOG_NET_WARNING("Could not find '%s' in local state definitions")
                    << StringArray_At(ar, i);
        }
    }

    StringArray_Delete(ar);
}

static Material *Cl_FindLocalMaterial(materialarchive_serialid_t archId)
{    
    if(!serverMaterials)
    {
        // Can't do it.
        LOGDEV_NET_WARNING("Cannot translate serial id %i, server has not sent its materials!") << archId;
        return 0;
    }
    return (Material *) MaterialArchive_Find(serverMaterials, archId, 0);
}

int Cl_LocalMobjType(int serverMobjType)
{
    if(serverMobjType < 0 || serverMobjType >= xlatMobjType.size)
        return 0; // Invalid type.
    return xlatMobjType.serverToLocal[serverMobjType];
}

int Cl_LocalMobjState(int serverMobjState)
{
    if(serverMobjState < 0 || serverMobjState >= xlatMobjState.size)
        return 0; // Invalid state.
    return xlatMobjState.serverToLocal[serverMobjState];
}

bool Map::isValidClPlane(int i)
{
    if(!clActivePlanes[i]) return false;
    return (clActivePlanes[i]->thinker.function == reinterpret_cast<thinkfunc_t>(Cl_MoverThinker));
}

bool Map::isValidClPolyobj(int i)
{
    if(!clActivePolyobjs[i]) return false;
    return (clActivePolyobjs[i]->thinker.function == reinterpret_cast<thinkfunc_t>(Cl_PolyMoverThinker));
}

void Map::initClMovers()
{
    zap(clActivePlanes);
    zap(clActivePolyobjs);
}

void Map::resetClMovers()
{
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(isValidClPlane(i))
        {
            thinkers().remove(clActivePlanes[i]->thinker);
        }
        if(isValidClPolyobj(i))
        {
            thinkers().remove(clActivePolyobjs[i]->thinker);
        }
    }
}

void Cl_WorldInit()
{
    if(App_World().hasMap())
    {
        App_World().map().initClMovers();
    }

    serverMaterials = 0;
    std::memset(&xlatMobjType, 0, sizeof(xlatMobjType));
    std::memset(&xlatMobjState, 0, sizeof(xlatMobjState));
}

void Cl_WorldReset()
{
    if(serverMaterials)
    {
        MaterialArchive_Delete(serverMaterials);
        serverMaterials = 0;
    }

    setTableSize(&xlatMobjType, 0);
    setTableSize(&xlatMobjState, 0);

    if(App_World().hasMap())
    {
        App_World().map().resetClMovers();
    }
}

int Map::clPlaneIndex(clplane_t *mover)
{
    if(!clActivePlanes) return -1;

    /// @todo Optimize lookup.
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(clActivePlanes[i] == mover)
            return i;
    }
    return -1;
}

int Map::clPolyobjIndex(clpolyobj_t *mover)
{
    if(!clActivePolyobjs) return -1;

    /// @todo Optimize lookup.
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(clActivePolyobjs[i] == mover)
            return i;
    }
    return -1;
}

void Map::deleteClPlane(clplane_t *mover)
{
    LOG_AS("Map::deleteClPlane");

    int index = clPlaneIndex(mover);
    if(index < 0)
    {
        LOG_MAP_VERBOSE("Mover in sector #%i not removed!") << mover->sectorIndex;
        return;
    }

    LOG_MAP_XVERBOSE("Removing mover [%i] (sector: #%i)") << index << mover->sectorIndex;
    thinkers().remove(mover->thinker);
}

void Map::deleteClPolyobj(clpolyobj_t *mover)
{
    LOG_AS("Map::deleteClPolyobj");

    int index = clPolyobjIndex(mover);
    if(index < 0)
    {
        LOG_MAP_VERBOSE("Mover not removed!");
        return;
    }

    LOG_MAP_XVERBOSE("Removing mover [%i]") << index;
    thinkers().remove(mover->thinker);
}

void Cl_MoverThinker(clplane_t *mover)
{
    Map &map = App_World().map(); /// @todo Do not assume mover is from the CURRENT map.
    coord_t original;
    boolean remove = false;
    boolean freeMove;
    float fspeed;

    // Can we think yet?
    if(!Cl_GameReady()) return;

    LOG_AS("Cl_MoverThinker");

#ifdef _DEBUG
    if(map.clPlaneIndex(mover) < 0)
    {
        LOG_MAP_WARNING("Running a mover that is not in activemovers!");
    }
#endif

    // The move is cancelled if the consolePlayer becomes obstructed.
    freeMove = ClPlayer_IsFreeToMove(consolePlayer);
    fspeed = mover->speed;

    // How's the gap?
    original = P_GetDouble(DMU_SECTOR, mover->sectorIndex, mover->property);
    if(fabs(fspeed) > 0 && fabs(mover->destination - original) > fabs(fspeed))
    {
        // Do the move.
        P_SetDouble(DMU_SECTOR, mover->sectorIndex, mover->property, original + fspeed);
    }
    else
    {
        // We have reached the destination.
        P_SetDouble(DMU_SECTOR, mover->sectorIndex, mover->property, mover->destination);

        // This thinker can now be removed.
        remove = true;
    }

    LOGDEV_MAP_XVERBOSE_DEBUGONLY("plane height %f in sector #%i",
            P_GetDouble(DMU_SECTOR, mover->sectorIndex, mover->property)
            << mover->sectorIndex);

    // Let the game know of this.
    if(gx.SectorHeightChangeNotification)
    {
        gx.SectorHeightChangeNotification(mover->sectorIndex);
    }

    // Make sure the client didn't get stuck as a result of this move.
    if(freeMove != ClPlayer_IsFreeToMove(consolePlayer))
    {
        LOG_MAP_VERBOSE("move blocked in sector #%i, undoing move") << mover->sectorIndex;

        // Something was blocking the way! Go back to original height.
        P_SetDouble(DMU_SECTOR, mover->sectorIndex, mover->property, original);

        if(gx.SectorHeightChangeNotification)
        {
            gx.SectorHeightChangeNotification(mover->sectorIndex);
        }
    }
    else
    {
        // Can we remove this thinker?
        if(remove)
        {
            LOG_MAP_VERBOSE("finished in sector #%i") << mover->sectorIndex;

            // It stops.
            P_SetDouble(DMU_SECTOR, mover->sectorIndex, mover->dmuPlane | DMU_SPEED, 0);

            map.deleteClPlane(mover);
        }
    }
}

clplane_t *Map::newClPlane(int sectorIndex, clplanetype_t type, coord_t dest, float speed)
{
    LOG_AS("Map::newClPlane");

    int dmuPlane = (type == CPT_FLOOR ? DMU_FLOOR_OF_SECTOR : DMU_CEILING_OF_SECTOR);

    LOG_MAP_XVERBOSE("Sector #%i, type:%s, dest:%f, speed:%f")
            << sectorIndex << (type == CPT_FLOOR? "floor" : "ceiling")
            << dest << speed;

    if(sectorIndex < 0 || sectorIndex >= sectorCount())
    {
        DENG_ASSERT(false); // Invalid Sector index.
        return 0;
    }

    // Remove any existing movers for the same plane.
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(isValidClPlane(i) &&
           clActivePlanes[i]->sectorIndex == sectorIndex &&
           clActivePlanes[i]->type == type)
        {
            LOG_MAP_XVERBOSE("Removing existing mover #%i in sector #%i, type %s")
                    << i << sectorIndex << (type == CPT_FLOOR? "floor" : "ceiling");

            deleteClPlane(clActivePlanes[i]);
        }
    }

    // Add a new mover.
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(clActivePlanes[i]) continue;

        LOG_MAP_XVERBOSE("New mover #%i") << i;

        // Allocate a new clplane_t thinker.
        clplane_t *mov = clActivePlanes[i] = (clplane_t *) Z_Calloc(sizeof(clplane_t), PU_MAP, &clActivePlanes[i]);
        mov->thinker.function = reinterpret_cast<thinkfunc_t>(Cl_MoverThinker);
        mov->type = type;
        mov->sectorIndex = sectorIndex;
        mov->destination = dest;
        mov->speed = speed;
        mov->property = dmuPlane | DMU_HEIGHT;
        mov->dmuPlane = dmuPlane;

        // Set the right sign for speed.
        if(mov->destination < P_GetDouble(DMU_SECTOR, sectorIndex, mov->property))
            mov->speed = -mov->speed;

        // Update speed and target height.
        P_SetDouble(DMU_SECTOR, sectorIndex, dmuPlane | DMU_TARGET_HEIGHT, dest);
        P_SetFloat(DMU_SECTOR, sectorIndex, dmuPlane | DMU_SPEED, speed);

        thinkers().add(mov->thinker, false /*not public*/);

        // Immediate move?
        if(FEQUAL(speed, 0))
        {
            // This will remove the thinker immediately if the move is ok.
            Cl_MoverThinker(mov);
        }
        return mov;
    }

    throw Error("Map::newClPlane", "Exhausted activemovers");
}

void Cl_PolyMoverThinker(clpolyobj_t *mover)
{
    DENG_ASSERT(mover != 0);

    LOG_AS("Cl_PolyMoverThinker");

    Polyobj *po = mover->polyobj;
    if(mover->move)
    {
        // How much to go?
        Vector2d delta = Vector2d(po->dest) - Vector2d(po->origin);

        ddouble dist = M_ApproxDistance(delta.x, delta.y);
        if(dist <= po->speed || de::fequal(po->speed, 0))
        {
            // We'll arrive at the destination.
            mover->move = false;
        }
        else
        {
            // Adjust deltas to fit speed.
            delta = (delta / dist) * po->speed;
        }

        // Do the move.
        po->move(delta);
    }

    if(mover->rotate)
    {
        // How much to go?
        int dist = po->destAngle - po->angle;
        int speed = po->angleSpeed;

        //dist = FIX2FLT(po->destAngle - po->angle);
        //if(!po->angleSpeed || dist > 0   /*(abs(FLT2FIX(dist) >> 4) <= abs(((signed) po->angleSpeed) >> 4)*/
        //    /* && po->destAngle != -1*/) || !po->angleSpeed)
        if(!po->angleSpeed || ABS(dist >> 2) <= ABS(speed >> 2))
        {
            LOG_MAP_XVERBOSE("Mover %i reached end of turn, destAngle=%i")
                    << mover->number << po->destAngle;

            // We'll arrive at the destination.
            mover->rotate = false;
        }
        else
        {
            // Adjust to speed.
            dist = /*FIX2FLT((int)*/ po->angleSpeed;
        }

        po->rotate(dist);
    }

    // Can we get rid of this mover?
    if(!mover->move && !mover->rotate)
    {
        /// @todo Do not assume the move is from the CURRENT map.
        App_World().map().deleteClPolyobj(mover);
    }
}

clpolyobj_t *Map::clPolyobjByPolyobjIndex(int index)
{
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(!isValidClPolyobj(i)) continue;

        if(clActivePolyobjs[i]->number == index)
            return clActivePolyobjs[i];
    }

    return 0;
}

clpolyobj_t *Map::newClPolyobj(int polyobjIndex)
{
    LOG_AS("Map::newClPolyobj");

    // Take the first unused slot.
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(clActivePolyobjs[i]) continue;

        LOG_MAP_XVERBOSE("New polymover [%i] for polyobj #%i.") << i << polyobjIndex;

        clpolyobj_t *mover = (clpolyobj_t *) Z_Calloc(sizeof(clpolyobj_t), PU_MAP, &clActivePolyobjs[i]);
        clActivePolyobjs[i] = mover;
        mover->thinker.function = reinterpret_cast<thinkfunc_t>(Cl_PolyMoverThinker);
        mover->polyobj = polyobjs().at(polyobjIndex);
        mover->number = polyobjIndex;
        thinkers().add(mover->thinker, false /*not public*/);
        return mover;
    }

    return 0; // Not successful.
}

clpolyobj_t *Cl_FindOrMakeActivePoly(uint polyobjIndex)
{
    clpolyobj_t *mover = App_World().map().clPolyobjByPolyobjIndex(polyobjIndex);
    if(mover) return mover;
    // Not found; make a new one.
    return App_World().map().newClPolyobj(polyobjIndex);
}

void Cl_SetPolyMover(uint number, int move, int rotate)
{
    clpolyobj_t *mover = Cl_FindOrMakeActivePoly(number);
    if(!mover)
    {
        LOGDEV_NET_WARNING("Out of polymovers");
        return;
    }

    // Flag for moving.
    if(move) mover->move = true;
    if(rotate) mover->rotate = true;
}

clplane_t *Map::clPlaneBySectorIndex(int sectorIndex, clplanetype_t type)
{
    for(int i = 0; i < CLIENT_MAX_MOVERS; ++i)
    {
        if(!isValidClPlane(i)) continue;
        if(clActivePlanes[i]->sectorIndex != sectorIndex) continue;
        if(clActivePlanes[i]->type != type) continue;

        // Found it!
        return clActivePlanes[i];
    }
    return 0;
}

/**
 * Reads a sector delta from the PSV_FRAME2 message buffer and applies it
 * to the world.
 */
void Cl_ReadSectorDelta2(int deltaType, boolean /*skip*/)
{
    DENG_UNUSED(deltaType);

    /// @todo Do not assume the CURRENT map.
    Map &map = App_World().map();

#define PLN_FLOOR   0
#define PLN_CEILING 1

    float height[2] = { 0, 0 };
    float target[2] = { 0, 0 };
    float speed[2]  = { 0, 0 };

    // Sector index number.
    ushort num = Reader_ReadUInt16(msgReader);

#ifdef _DEBUG
    DENG_ASSERT(num < map.sectorCount());
#endif
    Sector *sec = map.sectors().at(num);

    // Flags.
    int df = Reader_ReadPackedUInt32(msgReader);

    if(df & SDF_FLOOR_MATERIAL)
    {
        P_SetPtrp(sec, DMU_FLOOR_OF_SECTOR | DMU_MATERIAL,
                  Cl_FindLocalMaterial(Reader_ReadPackedUInt16(msgReader)));
    }
    if(df & SDF_CEILING_MATERIAL)
    {
        P_SetPtrp(sec, DMU_CEILING_OF_SECTOR | DMU_MATERIAL,
                  Cl_FindLocalMaterial(Reader_ReadPackedUInt16(msgReader)));
    }

    if(df & SDF_LIGHT)
        P_SetFloatp(sec, DMU_LIGHT_LEVEL, Reader_ReadByte(msgReader) / 255.0f);

    if(df & SDF_FLOOR_HEIGHT)
        height[PLN_FLOOR] = FIX2FLT(Reader_ReadInt16(msgReader) << 16);
    if(df & SDF_CEILING_HEIGHT)
        height[PLN_CEILING] = FIX2FLT(Reader_ReadInt16(msgReader) << 16);
    if(df & SDF_FLOOR_TARGET)
        target[PLN_FLOOR] = FIX2FLT(Reader_ReadInt16(msgReader) << 16);
    if(df & SDF_FLOOR_SPEED)
        speed[PLN_FLOOR] = FIX2FLT(Reader_ReadByte(msgReader) << (df & SDF_FLOOR_SPEED_44 ? 12 : 15));
    if(df & SDF_CEILING_TARGET)
        target[PLN_CEILING] = FIX2FLT(Reader_ReadInt16(msgReader) << 16);
    if(df & SDF_CEILING_SPEED)
        speed[PLN_CEILING] = FIX2FLT(Reader_ReadByte(msgReader) << (df & SDF_CEILING_SPEED_44 ? 12 : 15));

    if(df & (SDF_COLOR_RED | SDF_COLOR_GREEN | SDF_COLOR_BLUE))
    {
        Vector3f newColor = sec->lightColor();
        if(df & SDF_COLOR_RED)
            newColor.x = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_COLOR_GREEN)
            newColor.y = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_COLOR_BLUE)
            newColor.z = Reader_ReadByte(msgReader) / 255.f;
        sec->setLightColor(newColor);
    }

    if(df & (SDF_FLOOR_COLOR_RED | SDF_FLOOR_COLOR_GREEN | SDF_FLOOR_COLOR_BLUE))
    {
        Vector3f newColor = sec->floorSurface().tintColor();
        if(df & SDF_FLOOR_COLOR_RED)
            newColor.x = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_FLOOR_COLOR_GREEN)
            newColor.y = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_FLOOR_COLOR_BLUE)
            newColor.z = Reader_ReadByte(msgReader) / 255.f;
        sec->floorSurface().setTintColor(newColor);
    }

    if(df & (SDF_CEIL_COLOR_RED | SDF_CEIL_COLOR_GREEN | SDF_CEIL_COLOR_BLUE))
    {
        Vector3f newColor = sec->ceilingSurface().tintColor();
        if(df & SDF_CEIL_COLOR_RED)
            newColor.x = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_CEIL_COLOR_GREEN)
            newColor.y = Reader_ReadByte(msgReader) / 255.f;
        if(df & SDF_CEIL_COLOR_BLUE)
            newColor.z = Reader_ReadByte(msgReader) / 255.f;
        sec->ceilingSurface().setTintColor(newColor);
    }

    // The whole delta has now been read.

    // Do we need to start any moving planes?
    if(df & SDF_FLOOR_HEIGHT)
    {
        map.newClPlane(num, CPT_FLOOR, height[PLN_FLOOR], 0);
    }
    else if(df & (SDF_FLOOR_TARGET | SDF_FLOOR_SPEED))
    {
        map.newClPlane(num, CPT_FLOOR, target[PLN_FLOOR], speed[PLN_FLOOR]);
    }

    if(df & SDF_CEILING_HEIGHT)
    {
        map.newClPlane(num, CPT_CEILING, height[PLN_CEILING], 0);
    }
    else if(df & (SDF_CEILING_TARGET | SDF_CEILING_SPEED))
    {
        map.newClPlane(num, CPT_CEILING, target[PLN_CEILING], speed[PLN_CEILING]);
    }

#undef PLN_CEILING
#undef PLN_FLOOR
}

/**
 * Reads a side delta from the message buffer and applies it to the world.
 */
void Cl_ReadSideDelta2(int deltaType, boolean skip)
{
    DENG_UNUSED(deltaType);

    ushort num;

    int df, topMat = 0, midMat = 0, botMat = 0;
    int blendmode = 0;
    byte lineFlags = 0, sideFlags = 0;
    float toprgb[3] = {0,0,0}, midrgba[4] = {0,0,0,0};
    float bottomrgb[3] = {0,0,0};

    // First read all the data.
    num = Reader_ReadUInt16(msgReader);

    // Flags.
    df = Reader_ReadPackedUInt32(msgReader);

    if(df & SIDF_TOP_MATERIAL)
        topMat = Reader_ReadPackedUInt16(msgReader);
    if(df & SIDF_MID_MATERIAL)
        midMat = Reader_ReadPackedUInt16(msgReader);
    if(df & SIDF_BOTTOM_MATERIAL)
        botMat = Reader_ReadPackedUInt16(msgReader);
    if(df & SIDF_LINE_FLAGS)
        lineFlags = Reader_ReadByte(msgReader);

    if(df & SIDF_TOP_COLOR_RED)
        toprgb[CR] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SIDF_TOP_COLOR_GREEN)
        toprgb[CG] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SIDF_TOP_COLOR_BLUE)
        toprgb[CB] = Reader_ReadByte(msgReader) / 255.f;

    if(df & SIDF_MID_COLOR_RED)
        midrgba[CR] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SIDF_MID_COLOR_GREEN)
        midrgba[CG] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SIDF_MID_COLOR_BLUE)
        midrgba[CB] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SIDF_MID_COLOR_ALPHA)
        midrgba[CA] = Reader_ReadByte(msgReader) / 255.f;

    if(df & SIDF_BOTTOM_COLOR_RED)
        bottomrgb[CR] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SIDF_BOTTOM_COLOR_GREEN)
        bottomrgb[CG] = Reader_ReadByte(msgReader) / 255.f;
    if(df & SIDF_BOTTOM_COLOR_BLUE)
        bottomrgb[CB] = Reader_ReadByte(msgReader) / 255.f;

    if(df & SIDF_MID_BLENDMODE)
        blendmode = Reader_ReadInt32(msgReader);

    if(df & SIDF_FLAGS)
        sideFlags = Reader_ReadByte(msgReader);

    // Must we skip this?
    if(skip)
        return;

    LineSide *side = App_World().map().sideByIndex(num);
    DENG_ASSERT(side != 0);

    if(df & SIDF_TOP_MATERIAL)
    {
        side->top().setMaterial(Cl_FindLocalMaterial(topMat));
    }
    if(df & SIDF_MID_MATERIAL)
    {
        side->middle().setMaterial(Cl_FindLocalMaterial(midMat));
    }
    if(df & SIDF_BOTTOM_MATERIAL)
    {
        side->bottom().setMaterial(Cl_FindLocalMaterial(botMat));
    }

    if(df & (SIDF_TOP_COLOR_RED | SIDF_TOP_COLOR_GREEN | SIDF_TOP_COLOR_BLUE))
    {
        Vector3f newColor = side->top().tintColor();
        if(df & SIDF_TOP_COLOR_RED)
            newColor.x = toprgb[CR];
        if(df & SIDF_TOP_COLOR_GREEN)
            newColor.y = toprgb[CG];
        if(df & SIDF_TOP_COLOR_BLUE)
            newColor.z = toprgb[CB];
        side->top().setTintColor(newColor);
    }

    if(df & (SIDF_MID_COLOR_RED | SIDF_MID_COLOR_GREEN | SIDF_MID_COLOR_BLUE))
    {
        Vector3f newColor = side->middle().tintColor();
        if(df & SIDF_MID_COLOR_RED)
            newColor.x = midrgba[CR];
        if(df & SIDF_MID_COLOR_GREEN)
            newColor.y = midrgba[CG];
        if(df & SIDF_MID_COLOR_BLUE)
            newColor.z = midrgba[CB];
        side->middle().setTintColor(newColor);
    }
    if(df & SIDF_MID_COLOR_ALPHA)
    {
        side->middle().setOpacity(midrgba[CA]);
    }

    if(df & (SIDF_BOTTOM_COLOR_RED | SIDF_BOTTOM_COLOR_GREEN | SIDF_BOTTOM_COLOR_BLUE))
    {
        Vector3f newColor = side->bottom().tintColor();
        if(df & SIDF_BOTTOM_COLOR_RED)
            newColor.x = bottomrgb[CR];
        if(df & SIDF_BOTTOM_COLOR_GREEN)
            newColor.y = bottomrgb[CG];
        if(df & SIDF_BOTTOM_COLOR_BLUE)
            newColor.z = bottomrgb[CB];
        side->bottom().setTintColor(newColor);
    }

    if(df & SIDF_MID_BLENDMODE)
    {
        side->middle().setBlendMode(blendmode_t(blendmode));
    }

    if(df & SIDF_FLAGS)
    {
        // The delta includes the entire lowest byte.
        side->setFlags((side->flags() & ~0xff) | int(sideFlags), de::ReplaceFlags);
    }

    if(df & SIDF_LINE_FLAGS)
    {
        Line &line = side->line();
        // The delta includes the entire lowest byte.
        line.setFlags((line.flags() & ~0xff) | int(lineFlags), de::ReplaceFlags);
    }
}

/**
 * Reads a poly delta from the message buffer and applies it to
 * the world.
 */
void Cl_ReadPolyDelta2(boolean skip)
{
    int                 df;
    unsigned short      num;
    Polyobj            *po;
    float               destX = 0, destY = 0;
    float               speed = 0;
    int                 destAngle = 0, angleSpeed = 0;

    num = Reader_ReadPackedUInt16(msgReader);

    // Flags.
    df = Reader_ReadByte(msgReader);

    if(df & PODF_DEST_X)
        destX = Reader_ReadFloat(msgReader);
    if(df & PODF_DEST_Y)
        destY = Reader_ReadFloat(msgReader);
    if(df & PODF_SPEED)
        speed = Reader_ReadFloat(msgReader);
    if(df & PODF_DEST_ANGLE)
        destAngle = ((angle_t)Reader_ReadInt16(msgReader)) << 16;
    if(df & PODF_ANGSPEED)
        angleSpeed = ((angle_t)Reader_ReadInt16(msgReader)) << 16;

    if(skip)
        return;

    po = App_World().map().polyobjs().at(num);

    if(df & PODF_DEST_X)
        po->dest[VX] = destX;
    if(df & PODF_DEST_Y)
        po->dest[VY] = destY;
    if(df & PODF_SPEED)
        po->speed = speed;
    if(df & PODF_DEST_ANGLE)
        po->destAngle = destAngle;
    if(df & PODF_ANGSPEED)
        po->angleSpeed = angleSpeed;
    if(df & PODF_PERPETUAL_ROTATE)
        po->destAngle = -1;

    // Update the polyobj's mover thinkers.
    Cl_SetPolyMover(num, df & (PODF_DEST_X | PODF_DEST_Y | PODF_SPEED),
                    df & (PODF_DEST_ANGLE | PODF_ANGSPEED |
                          PODF_PERPETUAL_ROTATE));
}
