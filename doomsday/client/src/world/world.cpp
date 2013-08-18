/** @file world.cpp World.
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

#include <map>
#include <utility>

#include <QMap>
#include <QtAlgorithms>

#include <de/memoryzone.h>

#include <de/Error>
#include <de/Log>
#include <de/Time>

#include "de_base.h"
#include "de_console.h"
#include "de_defs.h"
#include "de_play.h"
#include "de_filesys.h"

#include "Materials"

#include "BspLeaf"
#include "Plane"
#include "Sector"

#include "audio/s_main.h"
#include "edit_map.h"
#include "network/net_main.h"

#include "render/r_main.h" // R_ResetViewer

#ifdef __CLIENT__
#  include "client/cl_def.h"
#  include "client/cl_frame.h"
#  include "client/cl_player.h"
#  include "edit_bias.h"
#  include "Hand"
#  include "HueCircle"

#  include "render/lumobj.h"
#  include "render/r_shadow.h"
#  include "render/rend_decor.h"
#  include "render/rend_fakeradio.h"
#  include "render/rend_list.h"
#  include "render/rend_main.h"
#  include "render/sky.h"
#  include "render/vlight.h"
#endif

#ifdef __SERVER__
#  include "server/sv_pool.h"
#endif

#include "world/thinkers.h"

#include "world/world.h"

using namespace de;

#ifdef __CLIENT__
static float handDistance = 300; //cvar
#endif

/**
 * Observes the progress of a map conversion and records any issues/problems that
 * are encountered in the process. When asked, compiles a human-readable report
 * intended to assist mod authors in debugging their maps.
 *
 * @todo Consolidate with the missing material reporting done elsewhere -ds
 */
class MapConversionReporter :
DENG2_OBSERVES(Map, UnclosedSectorFound),
DENG2_OBSERVES(Map, OneWayWindowFound)
{
    /// Record "unclosed sectors".
    /// Sector index => world point relatively near to the problem area.
    typedef std::map<int, Vector2i> UnclosedSectorMap;

    /// Record "one-way window lines".
    /// Line index => Sector index the back side faces.
    typedef std::map<int, int> OneWayWindowMap;

    /// Maximum number of warnings to output (of each type) about any problems
    /// encountered during the build process.
    static int const maxWarningsPerType;

public:
    MapConversionReporter() {}

    inline int unclosedSectorCount() const { return int( _unclosedSectors.size() ); }
    inline int oneWayWindowCount() const   { return int( _oneWayWindows.size() ); }

    void writeLog()
    {
        if(int numToLog = maxWarnings(unclosedSectorCount()))
        {
            String str;

            UnclosedSectorMap::const_iterator it = _unclosedSectors.begin();
            for(int i = 0; i < numToLog; ++i, ++it)
            {
                if(i != 0) str += "\n";
                str += String("Sector #%1 is unclosed near %2.")
                           .arg(it->first).arg(it->second.asText());
            }

            if(numToLog < unclosedSectorCount())
                str += String("\n(%1 more like this)").arg(unclosedSectorCount() - numToLog);

            LOG_WARNING("%s") << str;
        }

        if(int numToLog = maxWarnings(oneWayWindowCount()))
        {
            String str;

            OneWayWindowMap::const_iterator it = _oneWayWindows.begin();
            for(int i = 0; i < numToLog; ++i, ++it)
            {
                if(i != 0) str += "\n";
                str += String("Line #%1 seems to be a One-Way Window (back faces sector #%2).")
                           .arg(it->first).arg(it->second);
            }

            if(numToLog < oneWayWindowCount())
                str += String("\n(%1 more like this)").arg(oneWayWindowCount() - numToLog);

            LOG_VERBOSE("%s") << str;
        }
    }

protected:
    // Observes Partitioner UnclosedSectorFound.
    void unclosedSectorFound(Sector &sector, Vector2d const &nearPoint)
    {
        _unclosedSectors.insert(std::make_pair(sector.indexInArchive(), nearPoint.toVector2i()));
    }

    // Observes Partitioner OneWayWindowFound.
    void oneWayWindowFound(Line &line, Sector &backFacingSector)
    {
        _oneWayWindows.insert(std::make_pair(line.indexInArchive(), backFacingSector.indexInArchive()));
    }

private:
    static inline int maxWarnings(int issueCount)
    {
#ifdef DENG_DEBUG
        return issueCount; // No limit.
#else
        return de::min(issueCount, maxWarningsPerType);
#endif
    }

    UnclosedSectorMap _unclosedSectors;
    OneWayWindowMap   _oneWayWindows;
};

int const MapConversionReporter::maxWarningsPerType = 10;

boolean ddMapSetup;

// Should we be caching successfully loaded maps?
//static byte mapCache = true; // cvar

static char const *mapCacheDir = "mapcache/";

static inline lumpnum_t markerLumpNumForPath(String path)
{
    return App_FileSystem().lumpNumForName(path);
}

static String composeUniqueMapId(de::File1 &markerLump)
{
    return String("%1|%2|%3|%4")
              .arg(markerLump.name().fileNameWithoutExtension())
              .arg(markerLump.container().name().fileNameWithoutExtension())
              .arg(markerLump.container().hasCustom()? "pwad" : "iwad")
              .arg(Str_Text(App_CurrentGame().identityKey()))
              .toLower();
}

/// Determine the identity key for maps loaded from the specified @a sourcePath.
static String cacheIdForMap(String const &sourcePath)
{
    DENG_ASSERT(!sourcePath.isEmpty());

    ushort id = 0;
    for(int i = 0; i < sourcePath.size(); ++i)
    {
        id ^= sourcePath.at(i).unicode() << ((i * 3) % 11);
    }

    return String("%1").arg(id, 4, 16);
}

namespace de {

/**
 * Data for a map load task.
 */
struct MapLoadTaskData
{
    Uri uri;
    QScopedPointer<Map> map;
    QScopedPointer<MapConversionReporter> reporter;
};

DENG2_PIMPL(World)
{
    /**
     * Information about a map in the cache.
     */
    struct MapCacheRecord
    {
        Uri mapUri;                 ///< Unique identifier for the map.
        //String path;                ///< Path to the cached map data.
        //bool dataAvailable;
        //bool lastLoadAttemptFailed;
    };

    typedef QMap<String, MapCacheRecord> MapRecords;

    /// Map cache records.
    MapRecords records;

    /// Current map.
    Map *map;

    /// World-wide time.
    timespan_t time;

#ifdef __CLIENT__
    /// Hand for runtime map manipulation/editing.
    QScopedPointer<Hand> hand;
#endif

    Instance(Public *i) : Base(i), map(0), time(0)
    {}

    void notifyMapChange()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(MapChange, i)
        {
            i->worldMapChanged(self);
        }
    }

    /**
     * Compose the relative path (relative to the runtime directory) to the
     * directory of the cache where maps from this source (e.g., the add-on
     * which contains the map) will reside.
     *
     * @param sourcePath  Path to the primary resource file (the source) for
     *                    the original map data.
     *
     * @return  The composed path.
     */
    static Path cachePath(String sourcePath)
    {
        if(sourcePath.isEmpty()) return String();

        // Compose the final path.
        return mapCacheDir + String(Str_Text(App_CurrentGame().identityKey()))
               / sourcePath.fileNameWithoutExtension() + '-' + cacheIdForMap(sourcePath);
    }

    /**
     * Try to locate a cache record for a map by URI.
     *
     * @param uri  Map identifier.
     *
     * @return  Pointer to the found MapCacheRecord; otherwise @c 0.
     */
    MapCacheRecord *findCacheRecord(Uri const &uri) const
    {
        MapRecords::const_iterator found = records.find(uri.resolved());
        if(found != records.end())
        {
            return const_cast<MapCacheRecord *>(&found.value());
        }
        return 0; // Not found.
    }

    /**
     * Create a new MapCacheRecord for the map. If an existing record is found it
     * will be returned instead (becomes a no-op).
     *
     * @param uri  Map identifier.
     *
     * @return  Possibly newly-created MapCacheRecord.
     */
    MapCacheRecord &createCacheRecord(Uri const &uri)
    {
        // Do we have an existing record for this?
        if(MapCacheRecord *record = findCacheRecord(uri))
            return *record;

        // Prepare a new record.
        MapCacheRecord rec;
        rec.mapUri = uri;

        // Compose the cache directory path.
        /*lumpnum_t markerLumpNum = App_FileSystem().lumpNumForName(uri.path().toString().toLatin1().constData());
        if(markerLumpNum >= 0)
        {
            File1 &lump = App_FileSystem().nameIndex().lump(markerLumpNum);
            String cacheDir = cachePath(lump.container().composePath());

            rec.path = cacheDir + lump.name() + ".dcm";
        }*/

        return records.insert(uri.resolved(), rec).value();
    }

    /**
     * Attempt JIT conversion of the map data with the help of a plugin.
     * Note that the map is left in an editable state in case the caller
     * wishes to perform any further changes.
     *
     * return @c true if conversion was completed successfully.
     */
    bool convertMap(MapLoadTaskData &task)
    {
        // Record this map if we haven't already.
        /*MapCacheRecord &record =*/ createCacheRecord(task.uri);

        // We require a map converter for this.
        if(!Plug_CheckForHook(HOOK_MAP_CONVERT))
            return false;

        //LOG_DEBUG("Attempting \"%s\"...") << uri;

        lumpnum_t markerLumpNum = markerLumpNumForPath(task.uri.path());
        if(markerLumpNum < 0)
            return false;

        // Initiate the conversion process.
        MPE_Begin(reinterpret_cast<uri_s const *>(&task.uri));
        Map *newMap = MPE_Map();

        // Configure a reporter to observe the conversion process.
        task.reporter.reset(new MapConversionReporter);

        // Connect the reporter to the relevant audiences.
        /// @todo MapConversionReporter should handle this.
        newMap->audienceForOneWayWindowFound   += *task.reporter;
        newMap->audienceForUnclosedSectorFound += *task.reporter;

        // Generate and attribute the old unique map id.
        File1 &markerLump       = App_FileSystem().nameIndex().lump(markerLumpNum);
        String uniqueId         = composeUniqueMapId(markerLump);
        QByteArray uniqueIdUtf8 = uniqueId.toUtf8();
        newMap->setOldUniqueId(uniqueIdUtf8.constData());

        // Ask each converter in turn whether the map format is recognizable
        // and if so to interpret and transfer it to us via the runtime map
        // editing interface.
        if(!DD_CallHooks(HOOK_MAP_CONVERT, 0, (void *) reinterpret_cast<uri_s const *>(&task.uri)))
            return false;

        // A converter signalled success.

        // End the conversion process (if not already).
        MPE_End();

        // Take ownership of the map.
        task.map.reset(MPE_TakeMap());
        return true;
    }

#if 0
    /**
     * Returns @c true iff data for the map is available in the cache.
     */
    bool isCacheDataAvailable(MapCacheRecord &rec)
    {
        if(DAM_MapIsValid(Str_Text(&rec.path), markerLumpNum()))
        {
            rec.dataAvailable = true;
        }
        return rec.dataAvailable;
    }

    /**
     * Attempt to load data for the map from the cache.
     *
     * @see isCachedDataAvailable()
     *
     * @return @c true if loading completed successfully.
     */
    bool loadMapFromCache(Uri const &uri)
    {
        // Record this map if we haven't already.
        MapCacheRecord &rec = createCacheRecord(uri);

        Map *map = DAM_MapRead(Str_Text(&rec.path));
        if(!map)
            /// Failed to load the map specified from the data cache.
            throw Error("loadMapFromCache", QString("Failed loading map \"%1\" from cache.").arg(uri.asText()));

        map->_uri = rec.mapUri;
        return map;
    }
#endif

    /**
     * Attempt to load the associated map data.
     *
     * @return @c true if loading completed successfully.
     */
    bool loadMap(MapLoadTaskData &task)
    {
        LOG_AS("World::loadMap");

        // Record this map if we haven't already.
        /*MapCacheRecord &rec =*/ createCacheRecord(task.uri);

        /*if(rec.lastLoadAttemptFailed && !forceRetry)
            return 0;

        // Load from cache?
        if(mapCache && rec.dataAvailable)
        {
            return loadMapFromCache(task);
        }*/

        // Try a JIT conversion with the help of a plugin.
        if(convertMap(task))
            return true;

        LOG_WARNING("Failed conversion of \"%s\".") << task.uri;
        //rec.lastLoadAttemptFailed = true;
        return false;
    }

    /**
     * Replace the current map with @a map.
     */
    void makeCurrent(Map *newMap)
    {
        // This is now the current map (if any).
        map = newMap;
        if(!map) return;

        // We cannot make an editable map current.
        DENG_ASSERT(!map->isEditable());

        // Should we cache this map?
        /*MapCacheRecord &rec = createCacheRecord(map->uri());
        if(mapCache && !rec.dataAvailable)
        {
            // Ensure the destination directory exists.
            F_MakePath(rec.cachePath.toUtf8().constData());

            // Cache the map!
            DAM_MapWrite(map, rec.cachePath);
        }*/

#ifdef __CLIENT__
        // Connect the map to world audiences:
        /// @todo The map should instead be notified when it is made current
        /// so that it may perform the connection itself. Such notification
        /// would also afford the map the opportunity to prepare various data
        /// which is only needed when made current (e.g., caches for render).
        self.audienceForFrameBegin += map;
#endif

        // Print summary information about this map.
#define TABBED(count, label) String(_E(Ta) "  %1 " _E(Tb) "%2\n").arg(count).arg(label)

        LOG_MSG(_E(b) "Current map elements:");
        String str;
        QTextStream os(&str);
        os << TABBED(map->vertexCount(),  "Vertexes");
        os << TABBED(map->lineCount(),    "Lines");
        os << TABBED(map->polyobjCount(), "Polyobjs");
        os << TABBED(map->sectorCount(),  "Sectors");
        os << TABBED(map->bspNodeCount(), "BSP Nodes");
        os << TABBED(map->bspLeafCount(), "BSP Leafs");

        LOG_INFO("%s") << str.rightStrip();

#undef TABBED

        // See what MapInfo says about this map.
        ded_mapinfo_t *mapInfo = Def_GetMapInfo(reinterpret_cast<uri_s const *>(&map->uri()));
        if(!mapInfo)
        {
            // Use the default def instead.
            Uri defaultMapUri(Path("*"));
            mapInfo = Def_GetMapInfo(reinterpret_cast<uri_s *>(&defaultMapUri));
        }

        if(mapInfo)
        {
            map->_globalGravity = mapInfo->gravity;
            map->_ambientLightLevel = mapInfo->ambient * 255;
        }
        else
        {
            // No map info found -- apply defaults.
            map->_globalGravity = 1.0f;
            map->_ambientLightLevel = 0;
        }

        map->_effectiveGravity = map->_globalGravity;

#ifdef __CLIENT__
        // Reconfigure the sky.
        ded_sky_t *skyDef = 0;
        if(mapInfo)
        {
            skyDef = Def_GetSky(mapInfo->skyID);
            if(!skyDef) skyDef = &mapInfo->sky;
        }
        Sky_Configure(skyDef);
#endif

        // Init the thinker lists (public and private).
        map->thinkers().initLists(0x1 | 0x2);

        // Must be called before we go any further.
        P_InitUnusedMobjList();

        // Must be called before any mobjs are spawned.
        map->initNodePiles();

#ifdef __CLIENT__
        // Prepare the client-side data.
        if(isClient)
        {
            map->initClMobjs();
        }
        Cl_ResetFrame();
        map->reinitClMobjs();
        Cl_InitPlayers(); // Player data, too.

        // Spawn generators for the map.
        /// @todo Defer until after finalization.
        P_PtcInitForMap(*map);
#endif

        // The game may need to perform it's own finalization now that the
        // "current" map has changed.
        if(gx.FinalizeMapChange)
        {
            gx.FinalizeMapChange(reinterpret_cast<uri_s const *>(&map->uri()));
        }

        if(gameTime > 20000000 / TICSPERSEC)
        {
            // In very long-running games, gameTime will become so large that
            // it cannot be accurately converted to 35 Hz integer tics. Thus it
            // needs to be reset back to zero.
            gameTime = 0;
        }

        // Init player values.
        for(uint i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t *plr = &ddPlayers[i];
            ddplayer_t &ddpl = ddPlayers[i].shared;

            plr->extraLight = plr->targetExtraLight = 0;
            plr->extraLightCounter = 0;

            // Determine the "invoid" status.
            ddpl.inVoid = true;

            if(mobj_t *mo = ddpl.mo)
            {
                if(Sector *sector = map->bspLeafAt(mo->origin).sectorPtr())
                {
#ifdef __CLIENT__
                    if(mo->origin[VZ] >= sector->floor().visHeight() &&
                       mo->origin[VZ] <  sector->ceiling().visHeight() - 4)
#else
                    if(mo->origin[VZ] >= sector->floor().height() &&
                       mo->origin[VZ] <  sector->ceiling().height() - 4)
#endif
                    {
                        ddpl.inVoid = false;
                    }
                }
            }
        }

#ifdef __CLIENT__
        /// @todo Refactor away:
        foreach(Sector *sector, map->sectors())
        {
            sector->fixMissingMaterialsForSides();
            sector->markReverbDirty();
        }
#endif

        map->initPolyobjs();
        S_SetupForChangedMap();

#ifdef __SERVER__
        if(isServer)
        {
            // Init server data.
            Sv_InitPools();
        }
#endif

#ifdef __CLIENT__
        map->initLightGrid();
        map->initSkyFix();
        map->buildMaterialLists();
        P_MapSpawnPlaneParticleGens();

        Time begunPrecacheAt;
        Rend_CacheForMap();
        App_Materials().processCacheQueue();
        LOG_INFO(String("Precaching completed in %1 seconds.").arg(begunPrecacheAt.since(), 0, 'g', 2));

        RL_DeleteLists();
        R_InitRendPolyPools();
        Rend_UpdateLightModMatrix();

        Rend_DecorInitForMap(*map);
        Rend_RadioInitForMap(*map);

        R_InitObjlinkBlockmapForMap(*map);
        R_InitShadowProjectionListsForMap(*map); // Projected mobj shadows.
        LO_InitForMap(*map); // Lumobj management.
        VL_InitForMap(*map); // Converted vlights (from lumobjs).
        map->initBias(); // Shadow bias sources and surfaces.

        // Restart all material animations.
        App_Materials().restartAllAnimations();
#endif

        /*
         * Post-change map setup has now been fully completed.
         */

        // Run any commands specified in MapInfo.
        if(mapInfo && mapInfo->execute)
        {
            Con_Execute(CMDS_SCRIPT, mapInfo->execute, true, false);
        }

        // Run the special map setup command, which the user may alias to do
        // something useful.
        String cmd = "init-" + map->uri().resolved();
        if(Con_IsValidCommand(cmd.toUtf8().constData()))
        {
            Con_Executef(CMDS_SCRIPT, false, "%s", cmd.toUtf8().constData());
        }

        // Reset world time.
        time = 0;

        // Now that the setup is done, let's reset the timer so that it will
        // appear that no time has passed during the setup.
        DD_ResetTimer();

        // Make sure that the next frame doesn't use a filtered viewer.
        R_ResetViewer();

#ifdef __CLIENT__
        // Clear any input events that might have accumulated during setup.
        DD_ClearEvents();

        // Inform the timing system to suspend the starting of the clock.
        firstFrameAfterLoad = true;
#endif

        Z_PrintStatus();

        // Inform interested parties that the "current" map has changed.
        notifyMapChange();
    }

#ifdef __CLIENT__
    void updateHandOrigin()
    {
        DENG2_ASSERT(hand != 0 && map != 0);

        viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
        hand->setOrigin(Vector3d(viewData->current.origin[VX] + viewData->frontVec[VX] * handDistance,
                                 viewData->current.origin[VY] + viewData->frontVec[VZ] * handDistance,
                                 viewData->current.origin[VZ] + viewData->frontVec[VY] * handDistance));
    }
#endif
};

World::World() : d(new Instance(this))
{}

void World::consoleRegister() // static
{
    //C_VAR_BYTE ("map-cache", &mapCache, 0, 0, 1);
#ifdef __CLIENT__
    C_VAR_FLOAT("edit-bias-grab-distance", &handDistance, 0, 10, 1000);
#endif
    Map::consoleRegister();
}

bool World::hasMap() const
{
    return d->map != 0;
}

Map &World::map() const
{
    if(d->map)
    {
        return *d->map;
    }
    /// @throw MapError Attempted with no map loaded.
    throw MapError("World::map", "No map is currently loaded");
}

bool World::changeMap(de::Uri const &uri)
{
#ifdef __CLIENT__
    if(d->map)
    {
        // Remove the current map from our audiences.
        /// @todo Map should handle this.
        audienceForFrameBegin -= d->map;
    }
#endif

    // As the memory zone does not provide the mechanisms to prepare another
    // map in parallel we must free the current map first.
    /// @todo The memory zone would still be useful if the purge and tagging
    /// mechanisms allowed more fine grained control. It is no longer useful
    /// for allocating memory used elsewhere so it should be repurposed for
    /// this usage specifically.
    delete d->map; d->map = 0;
    Z_FreeTags(PU_MAP, PU_PURGELEVEL - 1);

    // Are we just unloading the current map?
    if(uri.isEmpty()) return true;

    LOG_MSG("Loading map \"%s\"...") << uri;

    // A new map is about to be setup.
    ddMapSetup = true;

    // Attempt to load in the new map.
    MapLoadTaskData task; task.uri = uri;
    if(d->loadMap(task))
    {
        // The map may still be in an editable state -- switch to playable.
        bool mapIsPlayable = task.map->endEditing();

        if(!task.reporter.isNull())
        {
            // We are no longer interested in reports about this map.
            task.map->audienceForOneWayWindowFound   -= *task.reporter;
            task.map->audienceForUnclosedSectorFound -= *task.reporter;
        }

        if(!mapIsPlayable)
        {
            // Darn. Discard the useless data.
            task.map.reset();
        }
    }

    // Take ownership of the map (if any) and make current.
    d->makeCurrent(task.map.data()? task.map.take() : 0);

    // We've finished setting up the map.
    ddMapSetup = false;

    // Output a human-readable log of any issues encountered in the conversion process.
    if(!task.reporter.isNull())
    {
        task.reporter->writeLog();
    }

    return d->map != 0;
}

void World::reset()
{
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t *plr = &ddPlayers[i];
        ddplayer_t *ddpl = &plr->shared;

        // Mobjs go down with the map.
        ddpl->mo = 0;
        // States have changed, the state pointers are unknown.
        ddpl->pSprites[0].statePtr = ddpl->pSprites[1].statePtr = 0;

        //ddpl->inGame = false;
        ddpl->flags &= ~DDPF_CAMERA;

        ddpl->fixedColorMap = 0;
        ddpl->extraLight = 0;
    }

#ifdef __CLIENT__
    if(isClient)
    {
        Cl_ResetFrame();
        Cl_InitPlayers();
    }
#endif

    // If a map is currently loaded -- unload it.
    unloadMap();
}

void World::update()
{
#ifdef __CLIENT__
    P_UpdateParticleGens(); // Defs might've changed.
#endif

    // Reset the archived map cache (the available maps may have changed).
    d->records.clear();

    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t *plr = &ddPlayers[i];
        ddplayer_t *ddpl = &plr->shared;

        // States have changed, the state pointers are unknown.
        ddpl->pSprites[0].statePtr = ddpl->pSprites[1].statePtr = 0;
    }

    // Update the current map too.
    if(d->map)
    {
        d->map->update();
    }
}

void World::advanceTime(timespan_t delta)
{
#ifdef __CLIENT__
    if(clientPaused) return;
#endif
    d->time += delta;
}

timespan_t const World::time() const
{
    return d->time;
}

#ifdef __CLIENT__
Hand &World::hand(coord_t *distance) const
{
    // Time to create the hand?
    if(d->hand.isNull())
    {
        d->hand.reset(new Hand());
        const_cast<World *>(this)->audienceForFrameEnd += *d->hand;
        if(d->map)
        {
            d->updateHandOrigin();
        }
    }
    if(distance)
    {
        *distance = handDistance;
    }
    return *d->hand;
}

void World::beginFrame(bool resetNextViewer)
{
    // Notify interested parties that a new frame has begun.
    DENG2_FOR_AUDIENCE(FrameBegin, i) i->worldFrameBegins(*this, resetNextViewer);
}

void World::endFrame()
{
    if(d->map && !d->hand.isNull())
    {
        d->updateHandOrigin();

        // If the HueCircle is active update the current edit color.
        if(HueCircle *hueCircle = SBE_HueCircle())
        {
            viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
            d->hand->setEditColor(hueCircle->colorAt(viewData->frontVec));
        }
    }

    // Notify interested parties that the current frame has ended.
    DENG2_FOR_AUDIENCE(FrameEnd, i) i->worldFrameEnds(*this);
}

#endif // __CLIENT__

} // namespace de
