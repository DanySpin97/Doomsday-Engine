/** @file map.cpp World map.
 *
 * @todo This file has grown far too large. It should be split up through the
 * introduction of new abstractions / collections.
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

#include <cmath>

#include <de/aabox.h>
#include <de/binangle.h>
#include <de/memory.h>
#include <de/memoryzone.h>
#include <de/vector1.h>

#include <de/Rectangle>

#include "de_platform.h"
#include "de_base.h"
#include "de_console.h" // Con_GetInteger
#include "de_defs.h"
#include "m_nodepile.h"

#include "Face"

#include "BspLeaf"
#include "BspNode"
#include "Line"
#include "Plane"
#include "Polyobj"
#include "Sector"
#include "Segment"
#include "Vertex"

#include "world/bsp/partitioner.h"

#include "world/blockmap.h"
#include "world/entitydatabase.h"
#include "world/generators.h"
#include "world/lineowner.h"
#include "world/maputil.h"
#include "world/p_intercept.h"
#include "world/p_object.h"
#include "world/p_objlink.h"
#include "world/thinkers.h"
#include "world/world.h" // ddMapSetup

#include "render/r_main.h" // validCount
#ifdef __CLIENT__
#  include "BiasDigest"
#  include "render/lumobj.h"
#  include "render/rend_decor.h"
#  include "render/rend_main.h"
#  include "render/sky.h"
#endif

#include "world/map.h"

/// Size of Blockmap blocks in map units. Must be an integer power of two.
#define MAPBLOCKUNITS               (128)

// Linkstore is list of pointers gathered when iterating stuff.
// This is pretty much the only way to avoid *all* potential problems
// caused by callback routines behaving badly (moving or destroying
// mobjs). The idea is to get a snapshot of all the objects being
// iterated before any callbacks are called. The hardcoded limit is
// a drag, but I'd like to see you iterating 2048 mobjs/lines in one block.

#define MAXLINKED           2048
#define DO_LINKS(it, end, _Type)   { \
    for(it = linkStore; it < end; it++) \
    { \
        result = callback(reinterpret_cast<_Type>(*it), parameters); \
        if(result) break; \
    } \
}

static int bspSplitFactor = 7; // cvar

namespace de {

struct EditableElements
{
    Map::Lines lines;
    Map::Sectors sectors;
    Map::Polyobjs polyobjs;

    EditableElements()
    {}

    ~EditableElements()
    {
        clearAll();
    }

    void clearAll();
};

DENG2_PIMPL(Map),
DENG2_OBSERVES(bsp::Partitioner, UnclosedSectorFound)
{
    /// @c true= editing is currently enabled.
    bool editingEnabled;

    /// Editable map element LUTs:
    EditableElements editable;

    /// Universal resource identifier for the map.
    Uri uri;

    /// Old unique identifier for the map (used with some legacy definitions).
    char oldUniqueId[256];

    /// Boundary points which encompass the entire map.
    AABoxd bounds;

    /// Map thinkers.
    QScopedPointer<Thinkers> thinkers;

    /// Mesh from which we'll assign geometries.
    Mesh mesh;

    /// Element LUTs:
    Sectors sectors;
    Lines lines;
    Polyobjs polyobjs;

    /// BSP root element.
    MapElement *bspRoot;

    /// BSP element LUTs:
    BspNodes bspNodes;
    BspLeafs bspLeafs;

    /// Map entities and element properties (things, line specials, etc...).
    EntityDatabase entityDatabase;

    /// Blockmaps:
    QScopedPointer<Blockmap> mobjBlockmap;
    QScopedPointer<Blockmap> polyobjBlockmap;
    QScopedPointer<Blockmap> lineBlockmap;
    QScopedPointer<Blockmap> bspLeafBlockmap;

    nodepile_t mobjNodes;
    nodepile_t lineNodes;
    nodeindex_t *lineLinks; // Indices to roots.

#ifdef __CLIENT__
    PlaneSet trackedPlanes;
    SurfaceSet scrollingSurfaces;

    SurfaceSet decoratedSurfaces;
    SurfaceSet glowingSurfaces;

    QScopedPointer<Generators> generators;
    QScopedPointer<LightGrid> lightGrid;

    /// Shadow Bias data for the map.
    struct Bias
    {
        /// Time in milliseconds of the "current" frame.
        uint currentTime;

        /// Frame number of the last update.
        uint lastChangeOnFrame;

        /// The set of light sources.
        BiasSources sources;

        Bias() : currentTime(0), lastChangeOnFrame(0)
        {}
    } bias;

    coord_t skyFloorHeight;
    coord_t skyCeilingHeight;
#endif

    // Current LOS trace state.
    /// @todo Does not belong here.
    TraceOpening traceOpening;
    divline_t traceLine;

    Instance(Public *i, Uri const &uri)
        : Base            (i),
          editingEnabled  (true),
          uri             (uri),
          bspRoot         (0),
          lineLinks       (0)
#ifdef __CLIENT__
          , skyFloorHeight(DDMAXFLOAT),
          skyCeilingHeight(DDMINFLOAT)
#endif
    {
        zap(oldUniqueId);
        zap(traceOpening);
        zap(traceLine);
    }

    ~Instance()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(Deletion, i) i->mapBeingDeleted(self);

#ifdef __CLIENT__
        self.removeAllBiasSources();

        // The light grid observes changes to sector lighting and so
        // must be destroyed first.
        lightGrid.reset();
#endif

        /// @todo fixme: What about Segments?

        qDeleteAll(bspNodes);
        qDeleteAll(bspLeafs);
        qDeleteAll(sectors);
        foreach(Polyobj *polyobj, polyobjs)
        {
            polyobj->~Polyobj();
            M_Free(polyobj);
        }
        qDeleteAll(lines);

        /// @todo fixme: Free all memory we have ownership of.
        // Client only data:
        // mobjHash/activePlanes/activePolyobjs
        // End client only data.
        // mobjNodes/lineNodes/lineLinks
    }

    /**
     * @pre Axis-aligned bounding boxes of all Sectors must be initialized.
     */
    void updateBounds()
    {
        bool isFirst = true;
        foreach(Line *line, lines)
        {
            // Polyobj lines don't count.
            if(line->definesPolyobj()) continue;

            if(isFirst)
            {
                // The first line's bounds are used as is.
                V2d_CopyBox(bounds.arvec2, line->aaBox().arvec2);
                isFirst = false;
            }
            else
            {
                // Expand the bounding box.
                V2d_UniteBox(bounds.arvec2, line->aaBox().arvec2);
            }
        }
    }

    // Observes bsp::Partitioner UnclosedSectorFound.
    void unclosedSectorFound(Sector &sector, Vector2d const &nearPoint)
    {
        // Notify interested parties that an unclosed sector was found.
        DENG2_FOR_PUBLIC_AUDIENCE(UnclosedSectorFound, i)
        {
            i->unclosedSectorFound(sector, nearPoint);
        }
    }

    /**
     * Notify interested parties of a "one-way window" in the map.
     *
     * @param line    The window line.
     * @param backFacingSector  Sector that the back of the line is facing.
     */
    void notifyOneWayWindowFound(Line &line, Sector &backFacingSector)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(OneWayWindowFound, i)
        {
            i->oneWayWindowFound(line, backFacingSector);
        }
    }

    struct testForWindowEffectParams
    {
        double frontDist, backDist;
        Sector *frontOpen, *backOpen;
        Line *frontLine, *backLine;
        Line *testLine;
        Vector2d testLineCenter;
        bool castHorizontal;

        testForWindowEffectParams()
            : frontDist(0), backDist(0), frontOpen(0), backOpen(0),
              frontLine(0), backLine(0), testLine(0), castHorizontal(false)
        {}
    };

    static void testForWindowEffect2(Line &line, testForWindowEffectParams &p)
    {
        if(&line == p.testLine) return;
        if(line.isSelfReferencing()) return;
        if(line.hasZeroLength()) return;

        double dist = 0;
        Sector *hitSector = 0;
        bool isFront = false;
        if(p.castHorizontal)
        {
            if(de::abs(line.direction().y) < bsp::DIST_EPSILON)
                return;

            if((line.aaBox().maxY < p.testLineCenter.y - bsp::DIST_EPSILON) ||
               (line.aaBox().minY > p.testLineCenter.y + bsp::DIST_EPSILON))
                return;

            dist = (line.fromOrigin().x +
                    (p.testLineCenter.y - line.fromOrigin().y) * line.direction().x / line.direction().y)
                   - p.testLineCenter.x;

            isFront = ((p.testLine->direction().y > 0) != (dist > 0));
            dist = de::abs(dist);

            // Too close? (overlapping lines?)
            if(dist < bsp::DIST_EPSILON)
                return;

            bool dir = (p.testLine->direction().y > 0) ^ (line.direction().y > 0);
            hitSector = line.side(dir ^ !isFront).sectorPtr();
        }
        else // Cast vertically.
        {
            if(de::abs(line.direction().x) < bsp::DIST_EPSILON)
                return;

            if((line.aaBox().maxX < p.testLineCenter.x - bsp::DIST_EPSILON) ||
               (line.aaBox().minX > p.testLineCenter.x + bsp::DIST_EPSILON))
                return;

            dist = (line.fromOrigin().y +
                    (p.testLineCenter.x - line.fromOrigin().x) * line.direction().y / line.direction().x)
                   - p.testLineCenter.y;

            isFront = ((p.testLine->direction().x > 0) == (dist > 0));
            dist = de::abs(dist);

            bool dir = (p.testLine->direction().x > 0) ^ (line.direction().x > 0);
            hitSector = line.side(dir ^ !isFront).sectorPtr();
        }

        // Too close? (overlapping lines?)
        if(dist < bsp::DIST_EPSILON)
            return;

        if(isFront)
        {
            if(dist < p.frontDist)
            {
                p.frontDist = dist;
                p.frontOpen = hitSector;
                p.frontLine = &line;
            }
        }
        else
        {
            if(dist < p.backDist)
            {
                p.backDist = dist;
                p.backOpen = hitSector;
                p.backLine = &line;
            }
        }
    }

    static int testForWindowEffectWorker(Line *line, void *parms)
    {
        testForWindowEffect2(*line, *reinterpret_cast<testForWindowEffectParams *>(parms));
        return false; // Continue iteration.
    }

    bool lineMightHaveWindowEffect(Line const &line)
    {
        if(line.definesPolyobj()) return false;
        if(line.hasFrontSector() && line.hasBackSector()) return false;
        if(!line.hasFrontSector()) return false;
        if(line.hasZeroLength()) return false;

        // Look for window effects by checking for an odd number of one-sided
        // line owners for a single vertex. Idea courtesy of Graham Jackson.
        if((line.from()._onesOwnerCount % 2) == 1 &&
           (line.from()._onesOwnerCount + line.from()._twosOwnerCount) > 1)
            return true;

        if((line.to()._onesOwnerCount % 2) == 1 &&
           (line.to()._onesOwnerCount + line.to()._twosOwnerCount) > 1)
            return true;

        return false;
    }

    void findOneWayWindows()
    {
        foreach(Vertex *vertex, mesh.vertexes())
        {
            // Count the total number of one and two-sided line owners for each
            // vertex. (Used in the process of locating window effect lines.)
            vertex->countLineOwners();
        }

        // Search for "one-way window" effects.
        foreach(Line *line, lines)
        {
            if(!lineMightHaveWindowEffect(*line))
                continue;

            testForWindowEffectParams p;
            p.frontDist      = p.backDist = DDMAXFLOAT;
            p.testLine       = line;
            p.testLineCenter = line->center();
            p.castHorizontal = (de::abs(line->direction().x) < de::abs(line->direction().y)? true : false);

            AABoxd scanRegion = bounds;
            if(p.castHorizontal)
            {
                scanRegion.minY = line->aaBox().minY - bsp::DIST_EPSILON;
                scanRegion.maxY = line->aaBox().maxY + bsp::DIST_EPSILON;
            }
            else
            {
                scanRegion.minX = line->aaBox().minX - bsp::DIST_EPSILON;
                scanRegion.maxX = line->aaBox().maxX + bsp::DIST_EPSILON;
            }
            validCount++;
            self.linesBoxIterator(scanRegion, testForWindowEffectWorker, &p);

            if(p.backOpen && p.frontOpen && line->frontSectorPtr() == p.backOpen)
            {
                notifyOneWayWindowFound(*line, *p.frontOpen);

                line->_bspWindowSector = p.frontOpen; /// @todo Refactor away.
            }
        }
    }

    static void assignMapToSegmentsForFace(Face const &face, Map *map)
    {
        HEdge *base = face.hedge();
        HEdge *hedge = base;
        do
        {
            DENG_ASSERT(hedge->mapElement() != 0);
            Segment *seg = hedge->mapElement()->as<Segment>();
            seg->setMap(map);
        } while((hedge = &hedge->next()) != base);
    }

    void collateBspElements(bsp::Partitioner &partitioner, BspTreeNode &tree)
    {
        if(tree.isLeaf())
        {
            // Take ownership of the BspLeaf.
            DENG2_ASSERT(tree.userData() != 0);
            BspLeaf *leaf = tree.userData()->as<BspLeaf>();
            partitioner.take(leaf);

            // Add this BspLeaf to the LUT.
            leaf->setMap(thisPublic);
            leaf->setIndexInMap(bspLeafs.count());
            bspLeafs.append(leaf);

            if(!leaf->isDegenerate())
            {
                assignMapToSegmentsForFace(leaf->poly(), thisPublic);

                foreach(Mesh *mesh, leaf->extraMeshes())
                foreach(Face *face, mesh->faces())
                {
                    assignMapToSegmentsForFace(*face, thisPublic);
                }

#ifdef DENG_DEBUG
                // See if we received a partial geometry...
                int discontinuities = 0;
                HEdge *hedge = leaf->poly().hedge();
                do
                {
                    if(hedge->next().origin() != hedge->twin().origin())
                    {
                        discontinuities++;
                    }
                } while((hedge = &hedge->next()) != leaf->poly().hedge());

                if(discontinuities)
                {
                    LOG_WARNING("Face geometry for BSP leaf [%p] at %s in sector %i "
                                "is not contiguous (%i gaps/overlaps).\n%s")
                        << de::dintptr(leaf)
                        << leaf->poly().center().asText()
                        << leaf->sector().indexInArchive()
                        << discontinuities
                        << leaf->poly().description();
                }
#endif
            }

            return;
        }
        // Else; a node.

        // Take ownership of this BspNode.
        DENG2_ASSERT(tree.userData() != 0);
        BspNode *node = tree.userData()->as<BspNode>();
        partitioner.take(node);

        // Add this BspNode to the LUT.
        node->setMap(thisPublic);
        node->setIndexInMap(bspNodes.count());
        bspNodes.append(node);
    }

    /**
     * Build a BSP tree for the map.
     *
     * @pre Map line bounds have been determined and a line blockmap constructed.
     */
    bool buildBsp()
    {
        DENG2_ASSERT(bspRoot == 0);
        DENG2_ASSERT(bspLeafs.isEmpty());
        DENG2_ASSERT(bspNodes.isEmpty());

        // It begins...
        Time begunAt;

        LOG_TRACE("Building BSP for \"%s\" with split cost factor %d...")
            << uri << bspSplitFactor;

        // First we'll scan for so-called "one-way window" constructs and mark
        // them so that the space partitioner can treat them specially.
        findOneWayWindows();

        // Remember the current next vertex ordinal as we'll need to index any
        // new vertexes produced during the build process.
        int nextVertexOrd = mesh.vertexCount();

        // Determine the set of lines for which we will build a BSP.
        QSet<Line *> linesToBuildBspFor = QSet<Line *>::fromList(lines);

        // Polyobj lines should be excluded.
        foreach(Polyobj *po, polyobjs)
        foreach(Line *line, po->lines())
        {
            linesToBuildBspFor.remove(line);
        }

        try
        {
            // Configure a space partitioner.
            bsp::Partitioner partitioner(bspSplitFactor);
            partitioner.audienceForUnclosedSectorFound += this;

            // Build a new BSP!
            BspTreeNode *rootNode = partitioner.buildBsp(linesToBuildBspFor, mesh);

            LOG_INFO("BSP built: %d Nodes, %d Leafs, %d Segments and %d Vertexes."
                     "\nTree balance is %d:%d.")
                    << partitioner.numNodes()    << partitioner.numLeafs()
                    << partitioner.numSegments() << partitioner.numVertexes()
                    << (rootNode->isLeaf()? 0 : rootNode->right().height())
                    << (rootNode->isLeaf()? 0 : rootNode->left().height());

            // Attribute an index to any new vertexes.
            for(int i = nextVertexOrd; i < mesh.vertexCount(); ++i)
            {
                Vertex *vtx = mesh.vertexes().at(i);
                vtx->setMap(thisPublic);
                vtx->setIndexInMap(i);
            }

            /*
             * Take ownership of all the built map data elements.
             */
            bspRoot = rootNode->userData(); // We'll formally take ownership shortly...

#ifdef DENG2_QT_4_7_OR_NEWER
            bspNodes.reserve(partitioner.numNodes());
            bspLeafs.reserve(partitioner.numLeafs());
#endif

            // Iterative pre-order traversal of the BspBuilder's map element tree.
            BspTreeNode *cur = rootNode;
            BspTreeNode *prev = 0;
            while(cur)
            {
                while(cur)
                {
                    if(cur->userData())
                    {
                        // Acquire ownership of and collate all map data elements at
                        // this node of the tree.
                        collateBspElements(partitioner, *cur);
                    }

                    if(prev == cur->parentPtr())
                    {
                        // Descending - right first, then left.
                        prev = cur;
                        if(cur->hasRight()) cur = cur->rightPtr();
                        else                cur = cur->leftPtr();
                    }
                    else if(prev == cur->rightPtr())
                    {
                        // Last moved up the right branch - descend the left.
                        prev = cur;
                        cur = cur->leftPtr();
                    }
                    else if(prev == cur->leftPtr())
                    {
                        // Last moved up the left branch - continue upward.
                        prev = cur;
                        cur = cur->parentPtr();
                    }
                }

                if(prev)
                {
                    // No left child - back up.
                    cur = prev->parentPtr();
                }
            }
        }
        catch(Error const &er)
        {
            LOG_WARNING("%s.") << er.asText();
        }

        // How much time did we spend?
        LOG_INFO(String("BSP built in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));

        return bspRoot != 0;
    }

    /**
     * Construct an initial (empty) line blockmap for "this" map.
     *
     * @pre Coordinate space bounds have already been determined.
     */
    void initLineBlockmap()
    {
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

        // Setup the blockmap area to enclose the whole map, plus a margin
        // (margin is needed for a map that fits entirely inside one blockmap cell).
        Vector2d min(bounds.minX - BLOCKMAP_MARGIN, bounds.minY - BLOCKMAP_MARGIN);
        Vector2d max(bounds.maxX + BLOCKMAP_MARGIN, bounds.maxY + BLOCKMAP_MARGIN);
        AABoxd expandedBounds(min.x, min.y, max.x, max.y);

        lineBlockmap.reset(new Blockmap(expandedBounds, Vector2ui(CELL_SIZE, CELL_SIZE)));

        LOG_INFO("Line blockmap dimensions:") << lineBlockmap->dimensions().asText();

        // Populate the blockmap.
        foreach(Line *line, lines)
        {
            linkLineInBlockmap(*line);
        }

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
    }

    void linkLineInBlockmap(Line &line)
    {
        // Lines of Polyobjs don't get into the blockmap (presently...).
        if(line.definesPolyobj()) return;

        Vector2d const &origin = lineBlockmap->origin();
        Vector2d const &cellDimensions = lineBlockmap->cellDimensions();

        // Determine the block of cells we'll be working within.
        Blockmap::CellBlock cellBlock = lineBlockmap->toCellBlock(line.aaBox());

        Blockmap::Cell cell;
        for(cell.y = cellBlock.min.y; cell.y <= cellBlock.max.y; ++cell.y)
        for(cell.x = cellBlock.min.x; cell.x <= cellBlock.max.x; ++cell.x)
        {
            if(line.slopeType() == ST_VERTICAL ||
               line.slopeType() == ST_HORIZONTAL)
            {
                lineBlockmap->link(cell, &line);
                continue;
            }

            Vector2d point = origin + cellDimensions * Vector2d(cell.x, cell.y);

            // Choose a cell diagonal to test.
            Vector2d from, to;
            if(line.slopeType() == ST_POSITIVE)
            {
                // Line slope / vs \ cell diagonal.
                from = Vector2d(point.x, point.y + cellDimensions.y);
                to   = Vector2d(point.x + cellDimensions.x, point.y);
            }
            else
            {
                // Line slope \ vs / cell diagonal.
                from = Vector2d(point.x + cellDimensions.x, point.y + cellDimensions.y);
                to   = Vector2d(point.x, point.y);
            }

            // Would Line intersect this?
            if((line.pointOnSide(from) < 0) != (line.pointOnSide(to) < 0))
            {
                lineBlockmap->link(cell, &line);
            }
        }
    }
    /**
     * Construct an initial (empty) mobj blockmap for "this" map.
     *
     * @pre Coordinate space bounds have already been determined.
     */
    void initMobjBlockmap()
    {
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

        // Setup the blockmap area to enclose the whole map, plus a margin
        // (margin is needed for a map that fits entirely inside one blockmap cell).
        Vector2d min(bounds.minX - BLOCKMAP_MARGIN, bounds.minY - BLOCKMAP_MARGIN);
        Vector2d max(bounds.maxX + BLOCKMAP_MARGIN, bounds.maxY + BLOCKMAP_MARGIN);
        AABoxd expandedBounds(min.x, min.y, max.x, max.y);

        mobjBlockmap.reset(new Blockmap(expandedBounds, Vector2ui(CELL_SIZE, CELL_SIZE)));

        LOG_INFO("Mobj blockmap dimensions:") << mobjBlockmap->dimensions().asText();

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
    }

    bool unlinkMobjInBlockmap(mobj_t &mo)
    {
        Blockmap::Cell cell = mobjBlockmap->toCell(mo.origin);
        return mobjBlockmap->unlink(cell, &mo);
    }

    void linkMobjInBlockmap(mobj_t &mo)
    {
        Blockmap::Cell cell = mobjBlockmap->toCell(mo.origin);
        mobjBlockmap->link(cell, &mo);
    }

    /**
     * Unlinks the mobj from all the lines it's been linked to. Can be called
     * without checking that the list does indeed contain lines.
     */
    bool unlinkMobjFromLines(mobj_t &mo)
    {
        // Try unlinking from lines.
        if(!mo.lineRoot)
            return false; // A zero index means it's not linked.

        // Unlink from each line.
        linknode_t *tn = mobjNodes.nodes;
        for(nodeindex_t nix = tn[mo.lineRoot].next; nix != mo.lineRoot;
            nix = tn[nix].next)
        {
            // Data is the linenode index that corresponds this mobj.
            NP_Unlink((&lineNodes), tn[nix].data);
            // We don't need these nodes any more, mark them as unused.
            // Dismissing is a macro.
            NP_Dismiss((&lineNodes), tn[nix].data);
            NP_Dismiss((&mobjNodes), nix);
        }

        // The mobj no longer has a line ring.
        NP_Dismiss((&mobjNodes), mo.lineRoot);
        mo.lineRoot = 0;

        return true;
    }

    /**
     * @note Caller must ensure a mobj is linked only once to any given line.
     *
     * @param mo    Mobj to be linked.
     * @param line  Line to link the mobj to.
     */
    void linkMobjToLine(mobj_t *mo, Line *line)
    {
        if(!mo || !line) return;

        // Add a node to the mobj's ring.
        nodeindex_t nodeIndex = NP_New(&mobjNodes, line);
        NP_Link(&mobjNodes, nodeIndex, mo->lineRoot);

        // Add a node to the line's ring. Also store the linenode's index
        // into the mobjring's node, so unlinking is easy.
        nodeIndex = mobjNodes.nodes[nodeIndex].data = NP_New(&lineNodes, mo);
        NP_Link(&lineNodes, nodeIndex, lineLinks[line->indexInMap()]);
    }

    struct LineLinkerParams
    {
        Map *map;
        mobj_t *mo;
        AABoxd box;
    };

    /**
     * The given line might cross the mobj. If necessary, link the mobj into
     * the line's mobj link ring.
     */
    static int lineLinkerWorker(Line *ld, void *parameters)
    {
        LineLinkerParams *p = reinterpret_cast<LineLinkerParams *>(parameters);
        DENG_ASSERT(p);

        // Do the bounding boxes intercept?
        if(p->box.minX >= ld->aaBox().maxX ||
           p->box.minY >= ld->aaBox().maxY ||
           p->box.maxX <= ld->aaBox().minX ||
           p->box.maxY <= ld->aaBox().minY) return false;

        // Line does not cross the mobj's bounding box?
        if(ld->boxOnSide(p->box)) return false;

        // Lines with only one sector will not be linked to because a mobj can't
        // legally cross one.
        if(!ld->hasFrontSector() || !ld->hasBackSector()) return false;

        p->map->d->linkMobjToLine(p->mo, ld);
        return false;
    }

    /**
     * @note Caller must ensure that the mobj is currently unlinked.
     */
    void linkMobjToLines(mobj_t &mo)
    {
        // Get a new root node.
        mo.lineRoot = NP_New(&mobjNodes, NP_ROOT_NODE);

        // Set up a line iterator for doing the linking.
        LineLinkerParams parm;
        parm.map = &self;
        parm.mo  = &mo;

        vec2d_t point;
        V2d_Set(point, mo.origin[VX] - mo.radius, mo.origin[VY] - mo.radius);
        V2d_InitBox(parm.box.arvec2, point);
        V2d_Set(point, mo.origin[VX] + mo.radius, mo.origin[VY] + mo.radius);
        V2d_AddToBox(parm.box.arvec2, point);

        validCount++;
        self.allLinesBoxIterator(parm.box, lineLinkerWorker, &parm);
    }

    /**
     * Construct an initial (empty) polyobj blockmap for "this" map.
     *
     * @pre Coordinate space bounds have already been determined.
     */
    void initPolyobjBlockmap()
    {
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

        // Setup the blockmap area to enclose the whole map, plus a margin
        // (margin is needed for a map that fits entirely inside one blockmap cell).
        Vector2d min(bounds.minX - BLOCKMAP_MARGIN, bounds.minY - BLOCKMAP_MARGIN);
        Vector2d max(bounds.maxX + BLOCKMAP_MARGIN, bounds.maxY + BLOCKMAP_MARGIN);
        AABoxd expandedBounds(min.x, min.y, max.x, max.y);

        polyobjBlockmap.reset(new Blockmap(expandedBounds, Vector2ui(CELL_SIZE, CELL_SIZE)));

        LOG_INFO("Polyobj blockmap dimensions:") << polyobjBlockmap->dimensions().asText();

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
    }

    void unlinkPolyobjInBlockmap(Polyobj &polyobj)
    {
        Blockmap::CellBlock cellBlock = polyobjBlockmap->toCellBlock(polyobj.aaBox);
        polyobjBlockmap->unlink(cellBlock, &polyobj);
    }

    void linkPolyobjInBlockmap(Polyobj &polyobj)
    {
        Blockmap::CellBlock cellBlock = polyobjBlockmap->toCellBlock(polyobj.aaBox);

        Blockmap::Cell cell;
        for(cell.y = cellBlock.min.y; cell.y <= cellBlock.max.y; ++cell.y)
        for(cell.x = cellBlock.min.x; cell.x <= cellBlock.max.x; ++cell.x)
        {
            polyobjBlockmap->link(cell, &polyobj);
        }
    }

    /**
     * Construct an initial (empty) BSP leaf blockmap for "this" map.
     *
     * @pre Coordinate space bounds have already been determined.
     */
    void initBspLeafBlockmap()
    {
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

        // Setup the blockmap area to enclose the whole map, plus a margin
        // (margin is needed for a map that fits entirely inside one blockmap cell).
        Vector2d min(bounds.minX - BLOCKMAP_MARGIN, bounds.minY - BLOCKMAP_MARGIN);
        Vector2d max(bounds.maxX + BLOCKMAP_MARGIN, bounds.maxY + BLOCKMAP_MARGIN);
        AABoxd expandedBounds(min.x, min.y, max.x, max.y);

        bspLeafBlockmap.reset(new Blockmap(expandedBounds, Vector2ui(CELL_SIZE, CELL_SIZE)));

        LOG_INFO("BSP leaf blockmap dimensions:") << bspLeafBlockmap->dimensions().asText();

        // Populate the blockmap.
        foreach(BspLeaf *bspLeaf, bspLeafs)
        {
            linkBspLeafInBlockmap(*bspLeaf);
        }

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
    }

    void linkBspLeafInBlockmap(BspLeaf &bspLeaf)
    {
        // Degenerate BspLeafs don't get in.
        if(bspLeaf.isDegenerate()) return;

        // BspLeafs without sectors don't get in.
        if(!bspLeaf.hasSector()) return;

        Blockmap::CellBlock cellBlock = bspLeafBlockmap->toCellBlock(bspLeaf.poly().aaBox());

        Blockmap::Cell cell;
        for(cell.y = cellBlock.min.y; cell.y <= cellBlock.max.y; ++cell.y)
        for(cell.x = cellBlock.min.x; cell.x <= cellBlock.max.x; ++cell.x)
        {
            bspLeafBlockmap->link(cell, &bspLeaf);
        }
    }

    /**
     * Locate a polyobj in the map by sound emitter.
     *
     * @param soundEmitter  ddmobj_base_t to search for.
     *
     * @return  Pointer to the referenced Polyobj instance; otherwise @c 0.
     */
    Polyobj *polyobjBySoundEmitter(ddmobj_base_t const &soundEmitter) const
    {
        foreach(Polyobj *polyobj, polyobjs)
        {
            if(&soundEmitter == &polyobj->soundEmitter())
                return polyobj;
        }
        return 0;
    }

    /**
     * Locate a sector in the map by sound emitter.
     *
     * @param soundEmitter  ddmobj_base_t to search for.
     *
     * @return  Pointer to the referenced Sector instance; otherwise @c 0.
     */
    Sector *sectorBySoundEmitter(ddmobj_base_t const &soundEmitter) const
    {
        foreach(Sector *sector, sectors)
        {
            if(&soundEmitter == &sector->soundEmitter())
                return sector;
        }
        return 0; // Not found.
    }

    /**
     * Locate a sector plane in the map by sound emitter.
     *
     * @param soundEmitter  ddmobj_base_t to search for.
     *
     * @return  Pointer to the referenced Plane instance; otherwise @c 0.
     */
    Plane *planeBySoundEmitter(ddmobj_base_t const &soundEmitter) const
    {
        foreach(Sector *sector, sectors)
        foreach(Plane *plane, sector->planes())
        {
            if(&soundEmitter == &plane->soundEmitter())
            {
                return plane;
            }
        }
        return 0; // Not found.
    }

    /**
     * Locate a surface in the map by sound emitter.
     *
     * @param soundEmitter  ddmobj_base_t to search for.
     *
     * @return  Pointer to the referenced Surface instance; otherwise @c 0.
     */
    Surface *surfaceBySoundEmitter(ddmobj_base_t const &soundEmitter) const
    {
        // Perhaps a wall surface?
        foreach(Line *line, lines)
        for(int i = 0; i < 2; ++i)
        {
            Line::Side &side = line->side(i);
            if(!side.hasSections()) continue;

            if(&soundEmitter == &side.middleSoundEmitter())
            {
                return &side.middle();
            }
            if(&soundEmitter == &side.bottomSoundEmitter())
            {
                return &side.bottom();
            }
            if(&soundEmitter == &side.topSoundEmitter())
            {
                return &side.top();
            }
        }

        return 0; // Not found.
    }

#ifdef __CLIENT__

    void updateMapSkyFixForSector(Sector const &sector)
    {
        if(!sector.sideCount()) return;

        bool const skyFloor = sector.floorSurface().hasSkyMaskedMaterial();
        bool const skyCeil  = sector.ceilingSurface().hasSkyMaskedMaterial();

        if(!skyFloor && !skyCeil) return;

        if(skyCeil)
        {
            // Adjust for the plane height.
            if(sector.ceiling().visHeight() > self.skyFixCeiling())
            {
                // Must raise the skyfix ceiling.
                self.setSkyFixCeiling(sector.ceiling().visHeight());
            }

            // Check that all the mobjs in the sector fit in.
            for(mobj_t *mo = sector.firstMobj(); mo; mo = mo->sNext)
            {
                coord_t extent = mo->origin[VZ] + mo->height;

                if(extent > self.skyFixCeiling())
                {
                    // Must raise the skyfix ceiling.
                    self.setSkyFixCeiling(extent);
                }
            }
        }

        if(skyFloor)
        {
            // Adjust for the plane height.
            if(sector.floor().visHeight() < self.skyFixFloor())
            {
                // Must lower the skyfix floor.
                self.setSkyFixFloor(sector.floor().visHeight());
            }
        }

        // Update for middle materials on lines which intersect the
        // floor and/or ceiling on the front (i.e., sector) side.
        foreach(Line::Side *side, sector.sides())
        {
            if(!side->hasSections()) continue;
            if(!side->middle().hasMaterial()) continue;

            // There must be a sector on both sides.
            if(!side->hasSector() || !side->back().hasSector()) continue;

            coord_t bottomZ, topZ;
            Vector2f materialOrigin;
            R_SideSectionCoords(*side, Line::Side::Middle, 0,
                                &bottomZ, &topZ, &materialOrigin);
            if(skyCeil && topZ + materialOrigin.y > self.skyFixCeiling())
            {
                // Must raise the skyfix ceiling.
                self.setSkyFixCeiling(topZ + materialOrigin.y);
            }

            if(skyFloor && bottomZ + materialOrigin.y < self.skyFixFloor())
            {
                // Must lower the skyfix floor.
                self.setSkyFixFloor(bottomZ + materialOrigin.y);
            }
        }
    }

    /**
     * $smoothplane: interpolate the visual offset of planes.
     */
    void lerpTrackedPlanes(bool resetNextViewer)
    {
        if(resetNextViewer)
        {
            // Reset the plane height trackers.
            foreach(Plane *plane, trackedPlanes)
            {
                plane->resetVisHeight();
            }

            // Tracked movement is now all done.
            trackedPlanes.clear();
        }
        // While the game is paused there is no need to calculate any
        // visual plane offsets $smoothplane.
        else //if(!clientPaused)
        {
            // Set the visible offsets.
            QMutableSetIterator<Plane *> iter(trackedPlanes);
            while(iter.hasNext())
            {
                Plane *plane = iter.next();

                plane->lerpVisHeight();

                // Has this plane reached its destination?
                if(de::fequal(plane->visHeight(), plane->height()))
                {
                    iter.remove();
                }
            }
        }
    }

    /**
     * $smoothmatoffset: interpolate the visual offset of surfaces.
     */
    void lerpScrollingSurfaces(bool resetNextViewer)
    {
        if(resetNextViewer)
        {
            // Reset the surface material origin trackers.
            foreach(Surface *surface, scrollingSurfaces)
            {
                surface->resetVisMaterialOrigin();
            }

            // Tracked movement is now all done.
            scrollingSurfaces.clear();
        }
        // While the game is paused there is no need to calculate any
        // visual material origin offsets $smoothmaterialorigin.
        else //if(!clientPaused)
        {
            // Set the visible origins.
            QMutableSetIterator<Surface *> iter(scrollingSurfaces);
            while(iter.hasNext())
            {
                Surface *surface = iter.next();

                surface->lerpVisMaterialOrigin();

                // Has this material reached its destination?
                if(surface->visMaterialOrigin() == surface->materialOrigin())
                {
                    iter.remove();
                }
            }
        }
    }

    /**
     * Perform preprocessing which must be done before rendering a frame.
     */
    void biasBeginFrame()
    {
        if(!useBias) return;

        // The time that applies on this frame.
        bias.currentTime = Timer_RealMilliseconds();

        // Check which sources have changed and update the tracker bits for
        // any affected surfaces.
        BiasDigest allChanges;
        bool needUpdateSurfaces = false;

        for(int i = 0; i < bias.sources.count(); ++i)
        {
            BiasSource *bsrc = bias.sources.at(i);

            if(bsrc->trackChanges(allChanges, i, bias.currentTime))
            {
                // We'll need to redetermine source => surface affection.
                needUpdateSurfaces = true;
            }
        }

        if(!needUpdateSurfaces) return;

        /*
         * Apply changes to all surfaces:
         */
        bias.lastChangeOnFrame = frameCount;
        foreach(BspLeaf *bspLeaf, bspLeafs)
        {
            bspLeaf->applyBiasDigest(allChanges);
        }
    }

    /**
     * Create new objlinks for mobjs (contact spreading).
     */
    void createMobjLinks()
    {
        foreach(Sector *sector, sectors)
        for(mobj_t *iter = sector->firstMobj(); iter; iter = iter->sNext)
        {
            R_ObjlinkCreate(*iter); // For spreading purposes.
        }
    }

#endif // __CLIENT__
};

Map::Map(Uri const &uri) : d(new Instance(this, uri))
{
#ifdef __CLIENT__
    zap(clMobjHash);
    zap(clActivePlanes);
    zap(clActivePolyobjs);
#endif
    _globalGravity = 0;
    _effectiveGravity = 0;
    _ambientLightLevel = 0;
}

void Map::consoleRegister() // static
{
    C_VAR_INT("bsp-factor", &bspSplitFactor, CVF_NO_MAX, 0, 0);
}

Mesh const &Map::mesh() const
{
    return d->mesh;
}

MapElement &Map::bspRoot() const
{
    if(d->bspRoot)
    {
        return *d->bspRoot;
    }
    /// @throw MissingBspError  No BSP data is available.
    throw MissingBspError("Map::bspRoot", "No BSP data available");
}

Map::BspNodes const &Map::bspNodes() const
{
    return d->bspNodes;
}

Map::BspLeafs const &Map::bspLeafs() const
{
    return d->bspLeafs;
}

#ifdef __CLIENT__

bool Map::hasLightGrid()
{
    return !d->lightGrid.isNull();
}

LightGrid &Map::lightGrid()
{
    if(!d->lightGrid.isNull())
    {
        return *d->lightGrid;
    }
    /// @throw MissingLightGrid Attempted with no LightGrid initialized.
    throw MissingLightGridError("Map::lightGrid", "No light grid is initialized");
}

void Map::initLightGrid()
{
    // Disabled?
    if(!Con_GetInteger("rend-bias-grid"))
        return;

    // Time to initialize the LightGrid?
    if(d->lightGrid.isNull())
    {
        d->lightGrid.reset(new LightGrid(*this));
    }
    // Perform a full update right away.
    d->lightGrid->update();
}

void Map::initBias()
{
    Time begunAt;

    LOG_AS("Map::initBias");

    // Start with no sources whatsoever.
    d->bias.sources.clear();

    // Load light sources from Light definitions.
    for(int i = 0; i < defs.count.lights.num; ++i)
    {
        ded_light_t *def = defs.lights + i;

        if(def->state[0]) continue;
        if(qstricmp(d->oldUniqueId, def->uniqueMapID)) continue;

        // Already at maximum capacity?
        if(biasSourceCount() == MAX_BIAS_SOURCES)
            break;

        addBiasSource(BiasSource::fromDef(*def));
    }

    LOG_INFO(String("Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

void Map::unlinkInMaterialLists(Surface *surface)
{
    if(!surface) return;

    d->decoratedSurfaces.remove(surface);
    d->glowingSurfaces.remove(surface);
}

void Map::linkInMaterialLists(Surface *surface)
{
    if(!surface) return;

    // Only surfaces with a material will be linked.
    if(!surface->hasMaterial()) return;

    // Ignore surfaces not currently attributed to the map.
    if(&surface->map() != this)
    {
        qDebug() << "Ignoring alien surface" << de::dintptr(surface) << "in Map::unlinkInMaterialLists";
        return;
    }

    Material &material = surface->material();
    if(material.hasGlow())
    {
        d->glowingSurfaces.insert(surface);
    }
    if(material.isDecorated())
    {
        d->decoratedSurfaces.insert(surface);
    }
}

void Map::buildMaterialLists()
{
    d->decoratedSurfaces.clear();
    d->glowingSurfaces.clear();

    foreach(Line *line, d->lines)
    for(int i = 0; i < 2; ++i)
    {
        Line::Side &side = line->side(i);
        if(!side.hasSections()) continue;

        linkInMaterialLists(&side.middle());
        linkInMaterialLists(&side.top());
        linkInMaterialLists(&side.bottom());
    }

    foreach(Sector *sector, d->sectors)
    {
        // Skip sectors with no lines as their planes will never be drawn.
        if(!sector->sideCount()) continue;

        foreach(Plane *plane, sector->planes())
        {
            linkInMaterialLists(&plane->surface());
        }
    }
}

Map::SurfaceSet const &Map::decoratedSurfaces()
{
    return d->decoratedSurfaces;
}

Map::SurfaceSet const &Map::glowingSurfaces()
{
    return d->glowingSurfaces;
}
#endif // __CLIENT__

void Map::updateSurfacesOnMaterialChange(Material &material)
{
    if(ddMapSetup) return;

#ifdef __CLIENT__
    if(material.isDecorated())
    {
        foreach(Surface *surface, d->decoratedSurfaces)
        {
            if(&material == surface->materialPtr())
            {
                surface->markAsNeedingDecorationUpdate();
            }
        }
    }
#endif
}

Uri const &Map::uri() const
{
    return d->uri;
}

char const *Map::oldUniqueId() const
{
    return d->oldUniqueId;
}

void Map::setOldUniqueId(char const *newUniqueId)
{
    qstrncpy(d->oldUniqueId, newUniqueId, sizeof(d->oldUniqueId));
}

AABoxd const &Map::bounds() const
{
    return d->bounds;
}

coord_t Map::gravity() const
{
    return _effectiveGravity;
}

void Map::setGravity(coord_t newGravity)
{
    _effectiveGravity = newGravity;
}

Thinkers &Map::thinkers() const
{
    if(!d->thinkers.isNull())
    {
        return *d->thinkers;
    }
    /// @throw MissingThinkersError  The thinker lists are not yet initialized.
    throw MissingThinkersError("Map::thinkers", "Thinkers not initialized");
}

Map::Vertexes const &Map::vertexes() const
{
    return d->mesh.vertexes();
}

Map::Lines const &Map::lines() const
{
    return d->lines;
}

Map::Sectors const &Map::sectors() const
{
    return d->sectors;
}

Map::Polyobjs const &Map::polyobjs() const
{
    return d->polyobjs;
}

divline_t const &Map::traceLine() const
{
    return d->traceLine;
}

TraceOpening const &Map::traceOpening() const
{
    return d->traceOpening;
}

void Map::setTraceOpening(Line &line)
{
    if(!line.hasBackSector())
    {
        d->traceOpening.range = 0;
        return;
    }

    coord_t bottom, top;
    d->traceOpening.range  = float( R_OpenRange(line.front(), &bottom, &top) );
    d->traceOpening.bottom = float( bottom );
    d->traceOpening.top    = float( top );

    // Determine the "low floor".
    Sector const &frontSector = line.frontSector();
    Sector const &backSector  = line.backSector();

    if(frontSector.floor().height() > backSector.floor().height())
    {
        d->traceOpening.lowFloor = float( backSector.floor().height() );
    }
    else
    {
        d->traceOpening.lowFloor = float( frontSector.floor().height() );
    }
}

int Map::ambientLightLevel() const
{
    return _ambientLightLevel;
}

int Map::toSideIndex(int lineIndex, int backSide) // static
{
    DENG_ASSERT(lineIndex >= 0);
    return lineIndex * 2 + (backSide? 1 : 0);
}

Line::Side *Map::sideByIndex(int index) const
{
    if(index < 0) return 0;
    return &d->lines.at(index / 2)->side(index % 2);
}

bool Map::identifySoundEmitter(ddmobj_base_t const &emitter, Sector **sector,
    Polyobj **poly, Plane **plane, Surface **surface) const
{
    *sector  = 0;
    *poly    = 0;
    *plane   = 0;
    *surface = 0;

    /// @todo Optimize: All sound emitters in a sector are linked together forming
    /// a chain. Make use of the chains instead.

    *poly = d->polyobjBySoundEmitter(emitter);
    if(!*poly)
    {
        // Not a polyobj. Try the sectors next.
        *sector = d->sectorBySoundEmitter(emitter);
        if(!*sector)
        {
            // Not a sector. Try the planes next.
            *plane = d->planeBySoundEmitter(emitter);
            if(!*plane)
            {
                // Not a plane. Try the surfaces next.
                *surface = d->surfaceBySoundEmitter(emitter);
            }
        }
    }

    return (*sector != 0 || *poly != 0|| *plane != 0|| *surface != 0);
}

Polyobj *Map::polyobjByTag(int tag) const
{
    foreach(Polyobj *polyobj, d->polyobjs)
    {
        if(polyobj->tag == tag)
            return polyobj;
    }
    return 0;
}

void Map::initPolyobjs()
{
    LOG_AS("Map::initPolyobjs");

    foreach(Polyobj *po, d->polyobjs)
    {
        /// @todo Is this still necessary? -ds
        /// (This data is updated automatically when moving/rotating).
        po->updateAABox();
        po->updateSurfaceTangents();

        po->unlink();
        po->link();
    }
}

EntityDatabase &Map::entityDatabase() const
{
    return d->entityDatabase;
}

void Map::initNodePiles()
{
    Time begunAt;

    LOG_AS("Map::initNodePiles");
    LOG_TRACE("Initializing...");

    // Initialize node piles and line rings.
    NP_Init(&d->mobjNodes, 256);  // Allocate a small pile.
    NP_Init(&d->lineNodes, lineCount() + 1000);

    // Allocate the rings.
    DENG_ASSERT(d->lineLinks == 0);
    d->lineLinks = (nodeindex_t *) Z_Malloc(sizeof(*d->lineLinks) * lineCount(), PU_MAPSTATIC, 0);

    for(int i = 0; i < lineCount(); ++i)
    {
        d->lineLinks[i] = NP_New(&d->lineNodes, NP_ROOT_NODE);
    }

    // How much time did we spend?
    LOG_INFO(String("Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

Blockmap const &Map::mobjBlockmap() const
{
    if(!d->mobjBlockmap.isNull())
    {
        return *d->mobjBlockmap;
    }
    /// @throw MissingBlockmapError  The mobj blockmap is not yet initialized.
    throw MissingBlockmapError("Map::mobjBlockmap", "Mobj blockmap is not initialized");
}

Blockmap const &Map::polyobjBlockmap() const
{
    if(!d->polyobjBlockmap.isNull())
    {
        return *d->polyobjBlockmap;
    }
    /// @throw MissingBlockmapError  The polyobj blockmap is not yet initialized.
    throw MissingBlockmapError("Map::polyobjBlockmap", "Polyobj blockmap is not initialized");
}

Blockmap const &Map::lineBlockmap() const
{
    if(!d->lineBlockmap.isNull())
    {
        return *d->lineBlockmap;
    }
    /// @throw MissingBlockmapError  The line blockmap is not yet initialized.
    throw MissingBlockmapError("Map::lineBlockmap", "Line blockmap is not initialized");
}

Blockmap const &Map::bspLeafBlockmap() const
{
    if(!d->bspLeafBlockmap.isNull())
    {
        return *d->bspLeafBlockmap;
    }
    /// @throw MissingBlockmapError  The BSP leaf blockmap is not yet initialized.
    throw MissingBlockmapError("Map::bspLeafBlockmap", "BSP leaf blockmap is not initialized");
}

struct bmapmoiterparams_t
{
    int localValidCount;
    int (*func) (mobj_t *, void *);
    void *parms;
};

static int blockmapCellMobjsIterator(void *object, void *context)
{
    mobj_t *mobj = (mobj_t *)object;
    bmapmoiterparams_t *args = (bmapmoiterparams_t *) context;
    if(mobj->validCount != args->localValidCount)
    {
        int result;

        // This mobj has now been processed for the current iteration.
        mobj->validCount = args->localValidCount;

        // Action the callback.
        result = args->func(mobj, args->parms);
        if(result) return result; // Stop iteration.
    }
    return false; // Continue iteration.
}

static int iterateCellMobjs(Blockmap &mobjBlockmap, Blockmap::Cell const &cell,
    int (*callback) (mobj_t *, void *), void *context = 0)
{
    bmapmoiterparams_t args;
    args.localValidCount = validCount;
    args.func            = callback;
    args.parms           = context;

    return mobjBlockmap.iterate(cell, blockmapCellMobjsIterator, (void *) &args);
}

static int iterateCellBlockMobjs(Blockmap &mobjBlockmap, Blockmap::CellBlock const &cellBlock,
    int (*callback) (mobj_t *, void *), void *context = 0)
{
    bmapmoiterparams_t args;
    args.localValidCount = validCount;
    args.func            = callback;
    args.parms           = context;

    return mobjBlockmap.iterate(cellBlock, blockmapCellMobjsIterator, (void *) &args);
}

int Map::mobjsBoxIterator(AABoxd const &box, int (*callback) (mobj_t *, void *),
                          void *parameters) const
{
    if(!d->mobjBlockmap.isNull())
    {
        Blockmap::CellBlock cellBlock = d->mobjBlockmap->toCellBlock(box);
        return iterateCellBlockMobjs(*d->mobjBlockmap, cellBlock, callback, parameters);
    }
    /// @throw MissingBlockmapError  The mobj blockmap is not yet initialized.
    throw MissingBlockmapError("Map::mobjsBoxIterator", "Mobj blockmap is not initialized");
}

struct bmapiterparams_t
{
    int localValidCount;
    int (*callback) (Line *, void *);
    void *parms;
};

static int blockmapCellLinesIterator(void *mapElement, void *context)
{
    Line *line = static_cast<Line *>(mapElement);
    bmapiterparams_t *parms = static_cast<bmapiterparams_t *>(context);

    if(line->validCount() != parms->localValidCount)
    {
        int result;

        // This line has now been processed for the current iteration.
        line->setValidCount(parms->localValidCount);

        // Action the callback.
        result = parms->callback(line, parms->parms);
        if(result) return result; // Stop iteration.
    }

    return false; // Continue iteration.
}

static int iterateCellLines(Blockmap &lineBlockmap, Blockmap::Cell const &cell,
    int (*callback) (Line *, void *), void *context = 0)
{
    bmapiterparams_t parms;
    parms.localValidCount = validCount;
    parms.callback        = callback;
    parms.parms           = context;

    return lineBlockmap.iterate(cell, blockmapCellLinesIterator, (void *)&parms);
}

static int iterateCellBlockLines(Blockmap &lineBlockmap, Blockmap::CellBlock const &cellBlock,
    int (*callback) (Line *, void *), void *context = 0)
{
    bmapiterparams_t parms;
    parms.localValidCount = validCount;
    parms.callback        = callback;
    parms.parms           = context;

    return lineBlockmap.iterate(cellBlock, blockmapCellLinesIterator, (void *) &parms);
}

struct bmapbspleafiterateparams_t
{
    AABoxd const *box;
    Sector *sector;
    int localValidCount;
    int (*func) (BspLeaf *, void *);
    void *parms;
};

static int blockmapCellBspLeafsIterator(void *object, void *context)
{
    BspLeaf *bspLeaf = (BspLeaf *)object;
    bmapbspleafiterateparams_t *args = (bmapbspleafiterateparams_t *) context;
    if(bspLeaf->validCount() != args->localValidCount)
    {
        bool ok = true;

        // This BspLeaf has now been processed for the current iteration.
        bspLeaf->setValidCount(args->localValidCount);

        // Check the sector restriction.
        if(args->sector && bspLeaf->sectorPtr() != args->sector)
            ok = false;

        // Check the bounds.
        AABoxd const &leafAABox = bspLeaf->poly().aaBox();

        if(args->box &&
           (leafAABox.maxX < args->box->minX ||
            leafAABox.minX > args->box->maxX ||
            leafAABox.minY > args->box->maxY ||
            leafAABox.maxY < args->box->minY))
            ok = false;

        if(ok)
        {
            // Action the callback.
            int result = args->func(bspLeaf, args->parms);
            if(result) return result; // Stop iteration.
        }
    }
    return false; // Continue iteration.
}

/*
static int iterateCellBspLeafs(Blockmap &bspLeafBlockmap, Blockmap::const_Cell cell,
    Sector *sector, AABoxd const &box, int localValidCount,
    int (*callback) (BspLeaf *, void *), void *context = 0)
{
    bmapbspleafiterateparams_t args;
    args.localValidCount = localValidCount;
    args.func            = callback;
    args.param           = context;
    args.sector          = sector;
    args.box             = &box;

    return bspLeafBlockmap.iterateCellObjects(cell, blockmapCellBspLeafsIterator, (void*)&args);
}
*/

static int iterateBlockBspLeafs(Blockmap &bspLeafBlockmap,
    Blockmap::CellBlock const &cellBlock, Sector *sector,  AABoxd const &box,
    int localValidCount,
    int (*callback) (BspLeaf *, void *), void *context = 0)
{
    bmapbspleafiterateparams_t args;
    args.localValidCount = localValidCount;
    args.func            = callback;
    args.parms           = context;
    args.sector          = sector;
    args.box             = &box;

    return bspLeafBlockmap.iterate(cellBlock, blockmapCellBspLeafsIterator, (void *) &args);
}

int Map::bspLeafsBoxIterator(AABoxd const &box, Sector *sector,
    int (*callback) (BspLeaf *, void *), void *parameters) const
{
    if(!d->bspLeafBlockmap.isNull())
    {
        static int localValidCount = 0;
        // This is only used here.
        localValidCount++;

        Blockmap::CellBlock cellBlock = d->bspLeafBlockmap->toCellBlock(box);
        return iterateBlockBspLeafs(*d->bspLeafBlockmap, cellBlock, sector, box,
                                    localValidCount, callback, parameters);
    }
    /// @throw MissingBlockmapError  The BSP leaf blockmap is not yet initialized.
    throw MissingBlockmapError("Map::bspLeafsBoxIterator", "BSP leaf blockmap is not initialized");
}

int Map::mobjLinesIterator(mobj_t *mo, int (*callback) (Line *, void *),
                           void *parameters) const
{
    void *linkStore[MAXLINKED];
    void **end = linkStore, **it;
    int result = false;

    linknode_t *tn = d->mobjNodes.nodes;
    if(mo->lineRoot)
    {
        nodeindex_t nix;
        for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
            nix = tn[nix].next)
            *end++ = tn[nix].ptr;

        DO_LINKS(it, end, Line*);
    }
    return result;
}

int Map::mobjSectorsIterator(mobj_t *mo, int (*callback) (Sector *, void *),
                             void *parameters) const
{
    int result = false;
    void *linkStore[MAXLINKED];
    void **end = linkStore, **it;

    nodeindex_t nix;
    linknode_t *tn = d->mobjNodes.nodes;

    // Always process the mobj's own sector first.
    Sector &ownSec = mo->bspLeaf->sector();
    *end++ = &ownSec;
    ownSec.setValidCount(validCount);

    // Any good lines around here?
    if(mo->lineRoot)
    {
        for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
            nix = tn[nix].next)
        {
            Line *ld = (Line *) tn[nix].ptr;

            // All these lines have sectors on both sides.
            // First, try the front.
            Sector &frontSec = ld->frontSector();
            if(frontSec.validCount() != validCount)
            {
                *end++ = &frontSec;
                frontSec.setValidCount(validCount);
            }

            // And then the back.
            if(ld->hasBackSector())
            {
                Sector &backSec = ld->backSector();
                if(backSec.validCount() != validCount)
                {
                    *end++ = &backSec;
                    backSec.setValidCount(validCount);
                }
            }
        }
    }

    DO_LINKS(it, end, Sector *);
    return result;
}

int Map::lineMobjsIterator(Line *line, int (*callback) (mobj_t *, void *),
                           void *parameters) const
{
    void *linkStore[MAXLINKED];
    void **end = linkStore;

    nodeindex_t root = d->lineLinks[line->indexInMap()];
    linknode_t *ln = d->lineNodes.nodes;

    for(nodeindex_t nix = ln[root].next; nix != root; nix = ln[nix].next)
    {
        DENG_ASSERT(end < &linkStore[MAXLINKED]);
        *end++ = ln[nix].ptr;
    }

    int result = false;
    void **it;
    DO_LINKS(it, end, mobj_t *);

    return result;
}

int Map::sectorTouchingMobjsIterator(Sector *sector,
    int (*callback) (mobj_t *, void *), void *parameters) const
{
    /// @todo Fixme: Remove fixed limit (use QVarLengthArray if necessary).
    void *linkStore[MAXLINKED];
    void **end = linkStore;

    // Collate mobjs that obviously are in the sector.
    for(mobj_t *mo = sector->firstMobj(); mo; mo = mo->sNext)
    {
        if(mo->validCount == validCount) continue;
        mo->validCount = validCount;

        DENG_ASSERT(end < &linkStore[MAXLINKED]);
        *end++ = mo;
    }

    // Collate mobjs linked to the sector's lines.
    linknode_t const *ln = d->lineNodes.nodes;
    foreach(Line::Side *side, sector->sides())
    {
        nodeindex_t root = d->lineLinks[side->line().indexInMap()];

        for(nodeindex_t nix = ln[root].next; nix != root; nix = ln[nix].next)
        {
            mobj_t *mo = (mobj_t *) ln[nix].ptr;

            if(mo->validCount == validCount) continue;
            mo->validCount = validCount;

            DENG_ASSERT(end < &linkStore[MAXLINKED]);
            *end++ = mo;
        }
    }

    // Process all collected mobjs.
    int result = false;
    void **it;
    DO_LINKS(it, end, mobj_t *);

    return result;
}

int Map::unlink(mobj_t &mo)
{
    int links = 0;

    if(Mobj_UnlinkFromSector(&mo))
        links |= DDLINK_SECTOR;
    if(d->unlinkMobjInBlockmap(mo))
        links |= DDLINK_BLOCKMAP;
    if(!d->unlinkMobjFromLines(mo))
        links |= DDLINK_NOLINE;

    return links;
}

void Map::link(mobj_t &mo, byte flags)
{
    // Link into the sector.
    mo.bspLeaf = &bspLeafAt_FixedPrecision(mo.origin);

    if(flags & DDLINK_SECTOR)
    {
        // Unlink from the current sector, if any.
        Sector &sec = mo.bspLeaf->sector();

        if(mo.sPrev)
            Mobj_UnlinkFromSector(&mo);

        // Link the new mobj to the head of the list.
        // Prev pointers point to the pointer that points back to us.
        // (Which practically disallows traversing the list backwards.)

        if((mo.sNext = sec.firstMobj()))
            mo.sNext->sPrev = &mo.sNext;

        *(mo.sPrev = &sec._mobjList) = &mo;
    }

    // Link into blockmap?
    if(flags & DDLINK_BLOCKMAP)
    {
        // Unlink from the old block, if any.
        d->unlinkMobjInBlockmap(mo);
        d->linkMobjInBlockmap(mo);
    }

    // Link into lines.
    if(!(flags & DDLINK_NOLINE))
    {
        // Unlink from any existing lines.
        d->unlinkMobjFromLines(mo);

        // Link to all contacted lines.
        d->linkMobjToLines(mo);
    }

    // If this is a player - perform additional tests to see if they have
    // entered or exited the void.
    if(mo.dPlayer && mo.dPlayer->mo)
    {
        ddplayer_t *player = mo.dPlayer;
        Sector &sector = player->mo->bspLeaf->sector();

        player->inVoid = true;
        if(sector.pointInside(player->mo->origin))
        {
#ifdef __CLIENT__
            if(player->mo->origin[VZ] <  sector.ceiling().visHeight() + 4 &&
               player->mo->origin[VZ] >= sector.floor().visHeight())
#else
            if(player->mo->origin[VZ] <  sector.ceiling().height() + 4 &&
               player->mo->origin[VZ] >= sector.floor().height())
#endif
            {
                player->inVoid = false;
            }
        }
    }
}

void Map::unlink(Polyobj &polyobj)
{
    d->unlinkPolyobjInBlockmap(polyobj);
}

void Map::link(Polyobj &polyobj)
{
    d->linkPolyobjInBlockmap(polyobj);
}

struct bmappoiterparams_t
{
    int localValidCount;
    int (*func) (Polyobj *, void *);
    void *parms;
};

static int blockmapCellPolyobjsIterator(void *object, void *context)
{
    Polyobj *polyobj = (Polyobj *)object;
    bmappoiterparams_t *args = (bmappoiterparams_t *) context;
    if(polyobj->validCount != args->localValidCount)
    {
        int result;

        // This polyobj has now been processed for the current iteration.
        polyobj->validCount = args->localValidCount;

        // Action the callback.
        result = args->func(polyobj, args->parms);
        if(result) return result; // Stop iteration.
    }
    return false; // Continue iteration.
}

static int iterateCellPolyobjs(Blockmap &polyobjBlockmap, Blockmap::Cell const &cell,
    int (*callback) (Polyobj *, void *), void *context = 0)
{
    bmappoiterparams_t args;
    args.localValidCount = validCount;
    args.func            = callback;
    args.parms           = context;

    return polyobjBlockmap.iterate(cell, blockmapCellPolyobjsIterator, (void *)&args);
}

static int iterateCellBlockPolyobjs(Blockmap &polyobjBlockmap, Blockmap::CellBlock const &cellBlock,
    int (*callback) (Polyobj *, void *), void *context = 0)
{
    bmappoiterparams_t args;
    args.localValidCount = validCount;
    args.func            = callback;
    args.parms           = context;

    return polyobjBlockmap.iterate(cellBlock, blockmapCellPolyobjsIterator, (void*) &args);
}

int Map::polyobjsBoxIterator(AABoxd const &box,
    int (*callback) (struct polyobj_s *, void *), void *parameters) const
{
    if(!d->polyobjBlockmap.isNull())
    {
        Blockmap::CellBlock cellBlock = d->polyobjBlockmap->toCellBlock(box);
        return iterateCellBlockPolyobjs(*d->polyobjBlockmap, cellBlock, callback, parameters);
    }
    /// @throw MissingBlockmapError  The polyobj blockmap is not yet initialized.
    throw MissingBlockmapError("Map::polyobjsBoxIterator", "Polyobj blockmap is not initialized");
}

struct poiterparams_t
{
    int (*func) (Line *, void *);
    void *parms;
};

static int polyobjLineIterator(Polyobj *po, void* context)
{
    poiterparams_t *args = (poiterparams_t *) context;
    foreach(Line *line, po->lines())
    {
        if(line->validCount() == validCount)
            continue;

        line->setValidCount(validCount);
        int result = args->func(line, args->parms);
        if(result) return result;
    }
    return false; // Continue iteration.
}

/*
static int iterateCellPolyobjLinesIterator(Blockmap &polyobjBlockmap, const_BlockmapCell cell,
    int (*callback) (Line *, void *), void *context = 0)
{
    poiterparams_t poargs;
    poargs.func  = callback;
    poargs.param = context;

    bmappoiterparams_t args;
    args.localValidCount = validCount;
    args.func            = PTR_PolyobjLines;
    args.param           = &poargs;

    return Blockmap_IterateCellObjects(&polyobjBlockmap, cell,
                                       blockmapCellPolyobjsIterator, &args);
}
*/

static int iterateBlockPolyobjLines(Blockmap &polyobjBlockmap,
    Blockmap::CellBlock const &cellBlock,
    int (*callback) (Line *, void *), void *context = 0)
{
    poiterparams_t poargs;
    poargs.func  = callback;
    poargs.parms = context;

    bmappoiterparams_t args;
    args.localValidCount = validCount;
    args.func            = polyobjLineIterator;
    args.parms           = &poargs;

    return polyobjBlockmap.iterate(cellBlock, blockmapCellPolyobjsIterator, (void*) &args);
}

int Map::linesBoxIterator(AABoxd const &box,
    int (*callback) (Line *, void *), void *parameters) const
{
    if(!d->lineBlockmap.isNull())
    {
        Blockmap::CellBlock cellBlock = d->lineBlockmap->toCellBlock(box);
        return iterateCellBlockLines(*d->lineBlockmap, cellBlock, callback, parameters);
    }
    /// @throw MissingBlockmapError  The line blockmap is not yet initialized.
    throw MissingBlockmapError("Map::linesBoxIterator", "Line blockmap is not initialized");
}

int Map::polyobjLinesBoxIterator(AABoxd const &box,
    int (*callback) (Line *, void *), void *parameters) const
{
    if(!d->polyobjBlockmap.isNull())
    {
        Blockmap::CellBlock cellBlock = d->polyobjBlockmap->toCellBlock(box);
        return iterateBlockPolyobjLines(*d->polyobjBlockmap, cellBlock, callback, parameters);
    }
    /// @throw MissingBlockmapError  The polyobj blockmap is not yet initialized.
    throw MissingBlockmapError("Map::polyobjBlockmap", "Polyobj blockmap is not initialized");
}

int Map::allLinesBoxIterator(AABoxd const &box,
    int (*callback) (Line *, void *), void *parameters) const
{
    if(polyobjCount())
    {
        int result = polyobjLinesBoxIterator(box, callback, parameters);
        if(result) return result;
    }
    return linesBoxIterator(box, callback, parameters);
}

static int traverseCellPath2(Blockmap &bmap, Blockmap::Cell const &fromCell,
    Blockmap::Cell const &toCell, Vector2d const &from, Vector2d const &to,
    int (*callback) (Blockmap::Cell const &cell, void *parameters), void *parameters)
{
    Vector2i stepDir;
    coord_t frac;
    Vector2d delta, intercept;

    if(toCell.x > fromCell.x)
    {
        stepDir.x = 1;
        frac = from.x / bmap.cellWidth();
        frac = 1 - (frac - int( frac ));
        delta.y = (to.y - from.y) / de::abs(to.x - from.x);
    }
    else if(toCell.x < fromCell.x)
    {
        stepDir.x = -1;
        frac = from.x / bmap.cellWidth();
        frac = (frac - int( frac ));
        delta.y = (to.y - from.y) / de::abs(to.x - from.x);
    }
    else
    {
        stepDir.x = 0;
        frac = 1;
        delta.y = 256;
    }
    intercept.y = from.y / bmap.cellHeight() + frac * delta.y;

    if(toCell.y > fromCell.y)
    {
        stepDir.y = 1;
        frac = from.y / bmap.cellHeight();
        frac = 1 - (frac - int( frac ));
        delta.x = (to.x - from.x) / de::abs(to.y - from.y);
    }
    else if(toCell.y < fromCell.y)
    {
        stepDir.y = -1;
        frac = from.y / bmap.cellHeight();
        frac = frac - int( frac );
        delta.x = (to.x - from.x) / de::abs(to.y - from.y);
    }
    else
    {
        stepDir.y = 0;
        frac = 1;
        delta.x = 256;
    }
    intercept.x = from.x / bmap.cellWidth() + frac * delta.x;

    /*
     * Step through map cells.
     */
    Blockmap::Cell cell = fromCell;
    for(int pass = 0; pass < 64; ++pass) // Prevent a round off error leading us into
                                         // an infinite loop...
    {
        int result = callback(cell, parameters);
        if(result) return result; // Early out.

        if(cell.x == toCell.x && cell.y == toCell.y)
            break;

        /// @todo Replace incremental translation?
        if(cell.y == uint( intercept.y ))
        {
            cell.x += stepDir.x;
            intercept.y += delta.y;
        }
        else if(cell.x == uint( intercept.x ))
        {
            cell.y += stepDir.y;
            intercept.x += delta.x;
        }
    }

    return false; // Continue iteration.
}

static int traversePath(divline_t &traceLine, Blockmap &bmap,
    const_pvec2d_t from_, const_pvec2d_t to_,
    int (*callback) (Blockmap::Cell const &cell, void *parameters), void *parameters = 0)
{
    // Constant terms implicitly defined by DOOM's original version of this
    // algorithm (we must honor these fudge factors for compatibility).
    coord_t const epsilon    = FIX2FLT(FRACUNIT);
    coord_t const unitOffset = FIX2FLT(FRACUNIT);

    vec2d_t min; V2d_Copy(min, bmap.bounds().min);
    vec2d_t max; V2d_Copy(max, bmap.bounds().max);

    // We may need to clip and/or fudge these points.
    vec2d_t from; V2d_Copy(from, from_);
    vec2d_t to;   V2d_Copy(to, to_);

    if(!(from[VX] >= min[VX] && from[VX] <= max[VX] &&
         from[VY] >= min[VY] && from[VY] <= max[VY]))
    {
        // 'From' is outside the blockmap (really? very unusual...)
        return true;
    }

    // Check the easy case of a path that lies completely outside the bmap.
    if((from[VX] < min[VX] && to[VX] < min[VX]) ||
       (from[VX] > max[VX] && to[VX] > max[VX]) ||
       (from[VY] < min[VY] && to[VY] < min[VY]) ||
       (from[VY] > max[VY] && to[VY] > max[VY]))
    {
        // Nothing intercepts outside the blockmap!
        return true;
    }

    // Lines should not be perfectly parallel to a blockmap axis.
    // We honor these so-called fudge factors for compatible behavior
    // with DOOM's algorithm.
    coord_t dX = (from[VX] - bmap.origin().x) / bmap.cellWidth();
    coord_t dY = (from[VY] - bmap.origin().y) / bmap.cellHeight();
    if(INRANGE_OF(dX, 0, epsilon)) from[VX] += unitOffset;
    if(INRANGE_OF(dY, 0, epsilon)) from[VY] += unitOffset;

    traceLine.origin[VX] = FLT2FIX(from[VX]);
    traceLine.origin[VY] = FLT2FIX(from[VY]);
    traceLine.direction[VX] = FLT2FIX(to[VX] - from[VX]);
    traceLine.direction[VY] = FLT2FIX(to[VY] - from[VY]);

    /*
     * It is possible that one or both points are outside the blockmap.
     * Clip path so that 'to' is within the AABB of the blockmap (note we
     * would have already abandoned if 'from' lay outside..
     */
    if(!(to[VX] >= min[VX] && to[VX] <= max[VX] &&
         to[VY] >= min[VY] && to[VY] <= max[VY]))
    {
        // 'to' is outside the blockmap.
        vec2d_t bounds[4], point;
        coord_t ab;

        V2d_Set(bounds[0], min[VX], min[VY]);
        V2d_Set(bounds[1], min[VX], max[VY]);
        V2d_Set(bounds[2], max[VX], max[VY]);
        V2d_Set(bounds[3], max[VX], min[VY]);

        ab = V2d_Intercept(from, to, bounds[0], bounds[1], point);
        if(ab >= 0 && ab <= 1)
            V2d_Copy(to, point);

        ab = V2d_Intercept(from, to, bounds[1], bounds[2], point);
        if(ab >= 0 && ab <= 1)
            V2d_Copy(to, point);

        ab = V2d_Intercept(from, to, bounds[2], bounds[3], point);
        if(ab >= 0 && ab <= 1)
            V2d_Copy(to, point);

        ab = V2d_Intercept(from, to, bounds[3], bounds[0], point);
        if(ab >= 0 && ab <= 1)
            V2d_Copy(to, point);
    }

    // Clipping already applied above, so we don't need to check it again...
    Blockmap::Cell fromCell = bmap.toCell(from);
    Blockmap::Cell toCell   = bmap.toCell(to);

    V2d_Subtract(from, from, min);
    V2d_Subtract(to, to, min);
    return traverseCellPath2(bmap, fromCell, toCell, from, to, callback, parameters);
}

struct iteratepolyobjlines_params_t
{
    int (*callback) (Line *, void *);
    void *parms;
};

static int iteratePolyobjLines(Polyobj *po, void *parameters = 0)
{
    iteratepolyobjlines_params_t const *p = (iteratepolyobjlines_params_t *)parameters;
    foreach(Line *line, po->lines())
    {
        if(line->validCount() == validCount)
            continue;

        line->setValidCount(validCount);
        int result = p->callback(line, p->parms);
        if(result) return result;
    }
    return false; // Continue iteration.
}

/**
 * Looks for lines in the given block that intercept the given trace to add
 * to the intercepts list.
 * A line is crossed if its endpoints are on opposite sides of the trace.
 *
 * @return  Non-zero if current iteration should stop.
 */
static int interceptLinesWorker(Line *line, void * /*parameters*/)
{
    divline_t const &traceLos = line->map().traceLine();
    int s1, s2;

    fixed_t lineFromX[2] = { DBL2FIX(line->fromOrigin().x), DBL2FIX(line->fromOrigin().y) };
    fixed_t lineToX[2]   = { DBL2FIX(  line->toOrigin().x), DBL2FIX(  line->toOrigin().y) };

    // Is this line crossed?
    // Avoid precision problems with two routines.
    if(traceLos.direction[VX] >  FRACUNIT * 16 || traceLos.direction[VY] >  FRACUNIT * 16 ||
       traceLos.direction[VX] < -FRACUNIT * 16 || traceLos.direction[VY] < -FRACUNIT * 16)
    {
        s1 = V2x_PointOnLineSide(lineFromX, traceLos.origin, traceLos.direction);
        s2 = V2x_PointOnLineSide(lineToX,   traceLos.origin, traceLos.direction);
    }
    else
    {
        s1 = line->pointOnSide(FIX2FLT(traceLos.origin[VX]), FIX2FLT(traceLos.origin[VY])) < 0;
        s2 = line->pointOnSide(FIX2FLT(traceLos.origin[VX] + traceLos.direction[VX]),
                               FIX2FLT(traceLos.origin[VY] + traceLos.direction[VY])) < 0;
    }
    if(s1 == s2) return false;

    fixed_t lineDirectionX[2] = { DBL2FIX(line->direction().x), DBL2FIX(line->direction().y) };

    // On the correct side of the trace origin?
    float distance = FIX2FLT(V2x_Intersection(lineFromX, lineDirectionX,
                                              traceLos.origin, traceLos.direction));
    if(!(distance < 0))
    {
        P_AddIntercept(ICPT_LINE, distance, line);
    }

    // Continue iteration.
    return false;
}

static int collectPolyobjLineIntercepts(Blockmap::Cell const &cell, void *parameters)
{
    Blockmap *polyobjBlockmap = (Blockmap *)parameters;
    iteratepolyobjlines_params_t iplParams;
    iplParams.callback = interceptLinesWorker;
    iplParams.parms    = 0;
    return iterateCellPolyobjs(*polyobjBlockmap, cell, iteratePolyobjLines, (void *)&iplParams);
}

static int collectLineIntercepts(Blockmap::Cell const &cell, void *parameters)
{
    Blockmap *lineBlockmap = (Blockmap *)parameters;
    return iterateCellLines(*lineBlockmap, cell, interceptLinesWorker);
}

static int interceptMobjsWorker(mobj_t *mo, void * /*parameters*/)
{
    if(mo->dPlayer && (mo->dPlayer->flags & DDPF_CAMERA))
        return false; // $democam: ssshh, keep going, we're not here...

    DENG_ASSERT(mo->bspLeaf);

    // Check a corner to corner crossection for hit.
    divline_t const &traceLos = mo->bspLeaf->map().traceLine();
    vec2d_t from, to;
    if((traceLos.direction[VX] ^ traceLos.direction[VY]) > 0)
    {
        // \ Slope
        V2d_Set(from, mo->origin[VX] - mo->radius,
                      mo->origin[VY] + mo->radius);
        V2d_Set(to,   mo->origin[VX] + mo->radius,
                      mo->origin[VY] - mo->radius);
    }
    else
    {
        // / Slope
        V2d_Set(from, mo->origin[VX] - mo->radius,
                      mo->origin[VY] - mo->radius);
        V2d_Set(to,   mo->origin[VX] + mo->radius,
                      mo->origin[VY] + mo->radius);
    }

    // Is this line crossed?
    if(Divline_PointOnSide(&traceLos, from) == Divline_PointOnSide(&traceLos, to))
        return false;

    // Calculate interception point.
    divline_t dl;
    dl.origin[VX] = DBL2FIX(from[VX]);
    dl.origin[VY] = DBL2FIX(from[VY]);
    dl.direction[VX] = DBL2FIX(to[VX] - from[VX]);
    dl.direction[VY] = DBL2FIX(to[VY] - from[VY]);
    coord_t distance = FIX2FLT(Divline_Intersection(&dl, &traceLos));

    // On the correct side of the trace origin?
    if(!(distance < 0))
    {
        P_AddIntercept(ICPT_MOBJ, distance, mo);
    }

    // Continue iteration.
    return false;
}

static int collectMobjIntercepts(Blockmap::Cell const &cell, void *parameters)
{
    Blockmap *mobjBlockmap = (Blockmap *)parameters;
    return iterateCellMobjs(*mobjBlockmap, cell, interceptMobjsWorker);
}

int Map::pathTraverse(const_pvec2d_t from, const_pvec2d_t to, int flags,
    traverser_t callback, void *parameters)
{
    // A new intercept trace begins...
    P_ClearIntercepts();
    validCount++;

    // Step #1: Collect intercepts.
    if(flags & PT_ADDLINES)
    {
        if(!d->polyobjs.isEmpty())
        {
            traversePath(d->traceLine, *d->polyobjBlockmap, from, to,
                         collectPolyobjLineIntercepts,
                         (void *)d->polyobjBlockmap.data());
        }

        traversePath(d->traceLine, *d->lineBlockmap, from, to, collectLineIntercepts,
                     (void *)d->lineBlockmap.data());
    }
    if(flags & PT_ADDMOBJS)
    {
        traversePath(d->traceLine, *d->mobjBlockmap, from, to, collectMobjIntercepts,
                     (void *)d->mobjBlockmap.data());
    }

    // Step #2: Process sorted intercepts.
    return P_TraverseIntercepts(callback, parameters);
}

BspLeaf &Map::bspLeafAt(Vector2d const &point) const
{
    if(!d->bspRoot)
        /// @throw MissingBspError  No BSP data is available.
        throw MissingBspError("Map::bspLeafAt", "No BSP data available");

    MapElement *bspElement = d->bspRoot;
    while(bspElement->type() != DMU_BSPLEAF)
    {
        BspNode const *bspNode = bspElement->as<BspNode>();

        int side = bspNode->partition().pointOnSide(point) < 0;

        // Decend to the child subspace on "this" side.
        bspElement = bspNode->childPtr(side);
    }

    // We've arrived at a leaf.
    return *bspElement->as<BspLeaf>();
}

BspLeaf &Map::bspLeafAt_FixedPrecision(Vector2d const &point) const
{
    if(!d->bspRoot)
        /// @throw MissingBspError  No BSP data is available.
        throw MissingBspError("Map::bspLeafAt_FixedPrecision", "No BSP data available");

    fixed_t pointX[2] = { DBL2FIX(point.x), DBL2FIX(point.y) };

    MapElement *bspElement = d->bspRoot;
    while(bspElement->type() != DMU_BSPLEAF)
    {
        BspNode const *bspNode = bspElement->as<BspNode>();
        Partition const &partition = bspNode->partition();

        fixed_t lineOriginX[2]    = { DBL2FIX(partition.origin.x),    DBL2FIX(partition.origin.y) };
        fixed_t lineDirectionX[2] = { DBL2FIX(partition.direction.x), DBL2FIX(partition.direction.y) };

        int side = V2x_PointOnLineSide(pointX, lineOriginX, lineDirectionX);

        // Decend to the child subspace on "this" side.
        bspElement = bspNode->childPtr(side);
    }

    // We've arrived at a leaf.
    return *bspElement->as<BspLeaf>();
}

#ifdef __CLIENT__

void Map::updateScrollingSurfaces()
{
    foreach(Surface *surface, d->scrollingSurfaces)
    {
        surface->updateMaterialOriginTracking();
    }
}

Map::SurfaceSet &Map::scrollingSurfaces()
{
    return d->scrollingSurfaces;
}

void Map::updateTrackedPlanes()
{
    foreach(Plane *plane, d->trackedPlanes)
    {
        plane->updateHeightTracking();
    }
}

Map::PlaneSet &Map::trackedPlanes()
{
    return d->trackedPlanes;
}

void Map::initSkyFix()
{
    Time begunAt;

    LOG_AS("Map::initSkyFix");

    d->skyFloorHeight   = DDMAXFLOAT;
    d->skyCeilingHeight = DDMINFLOAT;

    // Update for sector plane heights and mobjs which intersect the ceiling.
    foreach(Sector *sector, d->sectors)
    {
        d->updateMapSkyFixForSector(*sector);
    }

    LOG_INFO(String("Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

coord_t Map::skyFix(bool ceiling) const
{
    return ceiling? d->skyCeilingHeight : d->skyFloorHeight;
}

void Map::setSkyFix(bool ceiling, coord_t newHeight)
{
    if(ceiling) d->skyCeilingHeight = newHeight;
    else        d->skyFloorHeight   = newHeight;
}

Generators &Map::generators()
{
    // Time to initialize a new collection?
    if(d->generators.isNull())
    {
        d->generators.reset(new Generators(sectorCount()));
    }
    return *d->generators;
}

BiasSource &Map::addBiasSource(BiasSource const &biasSource)
{
    if(biasSourceCount() < MAX_BIAS_SOURCES)
    {
        d->bias.sources.append(new BiasSource(biasSource));
        return *d->bias.sources.last();
    }
    /// @throw FullError  Attempt to add a new bias source when already at capcity.
    throw FullError("Map::addBiasSource", QString("Already at maximum capacity (%1)").arg(MAX_BIAS_SOURCES));
}

void Map::removeBiasSource(int which)
{
    if(which >= 0 && which < biasSourceCount())
    {
        delete d->bias.sources.takeAt(which);
    }
}

void Map::removeAllBiasSources()
{
    qDeleteAll(d->bias.sources);
    d->bias.sources.clear();
}

Map::BiasSources const &Map::biasSources() const
{
    return d->bias.sources;
}

BiasSource *Map::biasSource(int index) const
{
   if(index >= 0 && index < biasSourceCount())
   {
       return biasSources().at(index);
   }
   return 0;
}

/**
 * @todo Implement a blockmap for these?
 * @todo Cache this result (MRU?).
 */
BiasSource *Map::biasSourceNear(Vector3d const &point) const
{
    BiasSource *nearest = 0;
    coord_t minDist = 0;
    foreach(BiasSource *src, d->bias.sources)
    {
        coord_t dist = (src->origin() - point).length();
        if(!nearest || dist < minDist)
        {
            minDist = dist;
            nearest = src;
        }
    }
    return nearest;
}

int Map::toIndex(BiasSource const &source) const
{
    return d->bias.sources.indexOf(const_cast<BiasSource *>(&source));
}

uint Map::biasCurrentTime() const
{
    return d->bias.currentTime;
}

uint Map::biasLastChangeOnFrame() const
{
    return d->bias.lastChangeOnFrame;
}

/**
 * Given a side section, look at the neighbouring surfaces and pick the
 * best choice of material used on those surfaces to be applied to "this"
 * surface.
 *
 * Material on back neighbour plane has priority.
 * Non-animated materials are preferred.
 * Sky materials are ignored.
 */
static Material *chooseFixMaterial(Line::Side &side, int section)
{
    Material *choice1 = 0, *choice2 = 0;

    Sector *frontSec = side.sectorPtr();
    Sector *backSec  = side.back().sectorPtr();

    if(backSec)
    {
        // Our first choice is a material in the other sector.
        if(section == Line::Side::Bottom)
        {
            if(frontSec->floor().height() < backSec->floor().height())
            {
                choice1 = backSec->floorSurface().materialPtr();
            }
        }
        else if(section == Line::Side::Top)
        {
            if(frontSec->ceiling().height()  > backSec->ceiling().height())
            {
                choice1 = backSec->ceilingSurface().materialPtr();
            }
        }

        // In the special case of sky mask on the back plane, our best
        // choice is always this material.
        if(choice1 && choice1->isSkyMasked())
        {
            return choice1;
        }
    }
    else
    {
        // Our first choice is a material on an adjacent wall section.
        // Try the left neighbor first.
        Line *other = R_FindLineNeighbor(frontSec, &side.line(), side.line().vertexOwner(side.sideId()),
                                         false /*next clockwise*/);
        if(!other)
            // Try the right neighbor.
            other = R_FindLineNeighbor(frontSec, &side.line(), side.line().vertexOwner(side.sideId()^1),
                                       true /*next anti-clockwise*/);

        if(other)
        {
            if(!other->hasBackSector())
            {
                // Our choice is clear - the middle material.
                choice1 = other->front().middle().materialPtr();
            }
            else
            {
                // Compare the relative heights to decide.
                Line::Side &otherSide = other->side(&other->frontSector() == frontSec? Line::Front : Line::Back);
                Sector &otherSec = other->side(&other->frontSector() == frontSec? Line::Back : Line::Front).sector();

                if(otherSec.ceiling().height() <= frontSec->floor().height())
                    choice1 = otherSide.top().materialPtr();
                else if(otherSec.floor().height() >= frontSec->ceiling().height())
                    choice1 = otherSide.bottom().materialPtr();
                else if(otherSec.ceiling().height() < frontSec->ceiling().height())
                    choice1 = otherSide.top().materialPtr();
                else if(otherSec.floor().height() > frontSec->floor().height())
                    choice1 = otherSide.bottom().materialPtr();
                // else we'll settle for a plane material.
            }
        }
    }

    // Our second choice is a material from this sector.
    choice2 = frontSec->planeSurface(section == Line::Side::Bottom? Sector::Floor : Sector::Ceiling).materialPtr();

    // Prefer a non-animated, non-masked material.
    if(choice1 && !choice1->isAnimated() && !choice1->isSkyMasked())
        return choice1;
    if(choice2 && !choice2->isAnimated() && !choice2->isSkyMasked())
        return choice2;

    // Prefer a non-masked material.
    if(choice1 && !choice1->isSkyMasked())
        return choice1;
    if(choice2 && !choice2->isSkyMasked())
        return choice2;

    // At this point we'll accept anything if it means avoiding HOM.
    if(choice1) return choice1;
    if(choice2) return choice2;

    // We'll assign the special "missing" material...
    return &App_Materials().find(de::Uri("System", Path("missing"))).material();
}

static void addMissingMaterial(Line::Side &side, int section)
{
    // Sides without sections need no fixing.
    if(!side.hasSections()) return;
    // ...nor those of self-referencing lines.
    if(side.line().isSelfReferencing()) return;
    // ...nor those of "one-way window" lines.
    if(!side.back().hasSections() && side.back().hasSector()) return;

    // A material must actually be missing to qualify for fixing.
    Surface &surface = side.surface(section);
    if(surface.hasMaterial()) return;

    // Look for and apply a suitable replacement if found.
    surface.setMaterial(chooseFixMaterial(side, section), true/* is missing fix */);

    // During map setup we log missing materials.
    if(ddMapSetup && verbose)
    {
        String path = surface.hasMaterial()? surface.material().manifest().composeUri().asText() : "<null>";

        LOG_WARNING("%s of Line #%d is missing a material for the %s section.\n"
                    "  %s was chosen to complete the definition.")
            << (side.isBack()? "Back" : "Front") << side.line().indexInMap()
            << (section == Line::Side::Middle? "middle" : section == Line::Side::Top? "top" : "bottom")
            << path;
    }
}

void Map::updateMissingMaterialsForLinesOfSector(Sector const &sec)
{
    foreach(Line::Side *side, sec.sides())
    {
        /**
         * Do as in the original Doom if the texture has not been defined -
         * extend the floor/ceiling to fill the space (unless it is skymasked),
         * or if there is a midtexture use that instead.
         */
        if(side->hasSector() && side->back().hasSector())
        {
            Sector const &frontSec = side->sector();
            Sector const &backSec  = side->back().sector();

            // A potential bottom section fix?
            if(!(frontSec.floorSurface().hasSkyMaskedMaterial() &&
                  backSec.floorSurface().hasSkyMaskedMaterial()))
            {
                if(frontSec.floor().height() < backSec.floor().height())
                {
                    addMissingMaterial(*side, Line::Side::Bottom);
                }
                else if(frontSec.floor().height() > backSec.floor().height())
                {
                    addMissingMaterial(side->back(), Line::Side::Bottom);
                }
            }

            // A potential top section fix?
            if(!(frontSec.ceilingSurface().hasSkyMaskedMaterial() &&
                  backSec.ceilingSurface().hasSkyMaskedMaterial()))
            {
                if(backSec.ceiling().height() < frontSec.ceiling().height())
                {
                    addMissingMaterial(*side, Line::Side::Top);
                }
                else if(backSec.ceiling().height() > frontSec.ceiling().height())
                {
                    addMissingMaterial(side->back(), Line::Side::Top);
                }
            }
        }
        else
        {
            // A potential middle section fix.
            addMissingMaterial(*side, Line::Side::Middle);
        }
    }
}

#endif // __CLIENT__

void Map::update()
{
#ifdef __CLIENT__
    // Update all surfaces.
    foreach(Sector *sector, d->sectors)
    foreach(Plane *plane, sector->planes())
    {
        plane->surface().markAsNeedingDecorationUpdate();
    }

    foreach(Line *line, d->lines)
    for(int i = 0; i < 2; ++i)
    {
        Line::Side &side = line->side(i);
        if(!side.hasSections()) continue;

        side.top().markAsNeedingDecorationUpdate();
        side.middle().markAsNeedingDecorationUpdate();
        side.bottom().markAsNeedingDecorationUpdate();
    }

    /// @todo Is this even necessary?
    foreach(Polyobj *polyobj, d->polyobjs)
    foreach(Line *line, polyobj->lines())
    {
        line->front().middle().markAsNeedingDecorationUpdate();
    }

    // Rebuild the surface material lists.
    buildMaterialLists();

#endif // __CLIENT__

    // Reapply values defined in MapInfo (they may have changed).
    ded_mapinfo_t *mapInfo = Def_GetMapInfo(reinterpret_cast<uri_s *>(&d->uri));
    if(!mapInfo)
    {
        // Use the default def instead.
        Uri defaultDefUri(Path("*"));
        mapInfo = Def_GetMapInfo(reinterpret_cast<uri_s *>(&defaultDefUri));
    }

    if(mapInfo)
    {
        _globalGravity     = mapInfo->gravity;
        _ambientLightLevel = mapInfo->ambient * 255;
    }
    else
    {
        // No map info found -- apply defaults.
        _globalGravity = 1.0f;
        _ambientLightLevel = 0;
    }

    _effectiveGravity = _globalGravity;

#ifdef __CLIENT__
    // Reconfigure the sky.
    /// @todo Sky needs breaking up into multiple components. There should be
    /// a representation on server side and a logical entity which the renderer
    /// visualizes. We also need multiple concurrent skies for BOOM support.
    ded_sky_t *skyDef = 0;
    if(mapInfo)
    {
        skyDef = Def_GetSky(mapInfo->skyID);
        if(!skyDef) skyDef = &mapInfo->sky;
    }
    Sky_Configure(skyDef);
#endif
}

#ifdef __CLIENT__
void Map::worldFrameBegins(World &world, bool resetNextViewer)
{
    DENG2_ASSERT(&world.map() == this); // Sanity check.

    // Clear all flags that can be cleared before each frame.
    foreach(Sector *sector, d->sectors)
    {
        sector->_frameFlags &= ~SIF_FRAME_CLEAR;
    }

    // Interpolate the map ready for drawing view(s) of it.
    d->lerpTrackedPlanes(resetNextViewer);
    d->lerpScrollingSurfaces(resetNextViewer);

    if(!freezeRLs)
    {
        // Initialize and/or update the LightGrid.
        initLightGrid();

        d->biasBeginFrame();

        LO_BeginWorldFrame();
        R_ClearObjlinksForFrame(); // Zeroes the links.

        // Clear the objlinks.
        R_InitForNewFrame();

        // Generate surface decorations for the frame.
        Rend_DecorBeginFrame();

        // Spawn omnilights for decorations.
        Rend_DecorAddLuminous();

        // Spawn omnilights for mobjs.
        LO_AddLuminousMobjs();

        // Create objlinks for mobjs.
        d->createMobjLinks();

        // Link all active particle generators into the world.
        P_CreatePtcGenLinks();

        // Link objs to all contacted surfaces.
        R_LinkObjs();
    }
}

#endif // __CLIENT__

/// Runtime map editing -----------------------------------------------------

/// Used when sorting vertex line owners.
static Vertex *rootVtx;

/**
 * Compares the angles of two lines that share a common vertex.
 *
 * pre: rootVtx must point to the vertex common between a and b
 *      which are (lineowner_t*) ptrs.
 */
static int lineAngleSorter(void const *a, void const *b)
{
    binangle_t angles[2];

    LineOwner *own[2] = { (LineOwner *)a, (LineOwner *)b };
    for(uint i = 0; i < 2; ++i)
    {
        if(own[i]->_link[Anticlockwise]) // We have a cached result.
        {
            angles[i] = own[i]->angle();
        }
        else
        {
            Line *line = &own[i]->line();
            Vertex const &otherVtx = line->vertex(&line->from() == rootVtx? 1:0);

            fixed_t dx = otherVtx.origin().x - rootVtx->origin().x;
            fixed_t dy = otherVtx.origin().y - rootVtx->origin().y;

            own[i]->_angle = angles[i] = bamsAtan2(-100 *dx, 100 * dy);

            // Mark as having a cached angle.
            own[i]->_link[Anticlockwise] = (LineOwner *) 1;
        }
    }

    return (angles[1] - angles[0]);
}

/**
 * Merge left and right line owner lists into a new list.
 *
 * @return  The newly merged list.
 */
static LineOwner *mergeLineOwners(LineOwner *left, LineOwner *right,
    int (*compare) (void const *a, void const *b))
{
    LineOwner tmp;
    LineOwner *np = &tmp;

    tmp._link[Clockwise] = np;
    while(left && right)
    {
        if(compare(left, right) <= 0)
        {
            np->_link[Clockwise] = left;
            np = left;

            left = &left->next();
        }
        else
        {
            np->_link[Clockwise] = right;
            np = right;

            right = &right->next();
        }
    }

    // At least one of these lists is now empty.
    if(left)
    {
        np->_link[Clockwise] = left;
    }
    if(right)
    {
        np->_link[Clockwise] = right;
    }

    // Is the list empty?
    if(!tmp.hasNext())
        return NULL;

    return &tmp.next();
}

static LineOwner *splitLineOwners(LineOwner *list)
{
    if(!list) return NULL;

    LineOwner *lista = list;
    LineOwner *listb = list;
    LineOwner *listc = list;

    do
    {
        listc = listb;
        listb = &listb->next();
        lista = &lista->next();
        if(lista)
        {
            lista = &lista->next();
        }
    } while(lista);

    listc->_link[Clockwise] = NULL;
    return listb;
}

/**
 * This routine uses a recursive mergesort algorithm; O(NlogN)
 */
static LineOwner *sortLineOwners(LineOwner *list,
    int (*compare) (void const *a, void const *b))
{
    if(list && list->_link[Clockwise])
    {
        LineOwner *p = splitLineOwners(list);

        // Sort both halves and merge them back.
        list = mergeLineOwners(sortLineOwners(list, compare),
                               sortLineOwners(p, compare), compare);
    }
    return list;
}

static void setVertexLineOwner(Vertex *vtx, Line *lineptr, LineOwner **storage)
{
    if(!lineptr) return;

    // Has this line already been registered with this vertex?
    LineOwner const *own = vtx->firstLineOwner();
    while(own)
    {
        if(&own->line() == lineptr)
            return; // Yes, we can exit.

        own = &own->next();
    }

    // Add a new owner.
    vtx->_numLineOwners++;
    LineOwner *newOwner = (*storage)++;

    newOwner->_line = lineptr;
    newOwner->_link[Anticlockwise] = NULL;

    // Link it in.
    // NOTE: We don't bother linking everything at this stage since we'll
    // be sorting the lists anyway. After which we'll finish the job by
    // setting the prev and circular links.
    // So, for now this is only linked singlely, forward.
    newOwner->_link[Clockwise] = vtx->_lineOwners;
    vtx->_lineOwners = newOwner;

    // Link the line to its respective owner node.
    if(vtx == &lineptr->from())
        lineptr->_vo1 = newOwner;
    else
        lineptr->_vo2 = newOwner;
}

#ifdef DENG2_DEBUG
/**
 * Determines whether the specified vertex @a v has a correctly formed line
 * owner ring.
 */
static bool vertexHasValidLineOwnerRing(Vertex &v)
{
    LineOwner const *base = v.firstLineOwner();
    LineOwner const *cur = base;
    do
    {
        if(&cur->prev().next() != cur) return false;
        if(&cur->next().prev() != cur) return false;

    } while((cur = &cur->next()) != base);
    return true;
}
#endif

/**
 * Generates the line owner rings for each vertex. Each ring includes all
 * the lines which the vertex belongs to sorted by angle, (the rings are
 * arranged in clockwise order, east = 0).
 */
void buildVertexLineOwnerRings(Map::Vertexes const &vertexes, Map::Lines &editableLines)
{
    LOG_AS("buildVertexLineOwnerRings");

    /*
     * Step 1: Find and link up all line owners.
     */
    // We know how many vertex line owners we need (numLines * 2).
    LineOwner *lineOwners = (LineOwner *) Z_Malloc(sizeof(LineOwner) * editableLines.count() * 2, PU_MAPSTATIC, 0);
    LineOwner *allocator = lineOwners;

    foreach(Line *line, editableLines)
    for(uint p = 0; p < 2; ++p)
    {
        setVertexLineOwner(&line->vertex(p), line, &allocator);
    }

    /*
     * Step 2: Sort line owners of each vertex and finalize the rings.
     */
    foreach(Vertex *v, vertexes)
    {
        if(!v->_numLineOwners) continue;

        // Sort them; ordered clockwise by angle.
        rootVtx = v;
        v->_lineOwners = sortLineOwners(v->_lineOwners, lineAngleSorter);

        // Finish the linking job and convert to relative angles.
        // They are only singly linked atm, we need them to be doubly
        // and circularly linked.
        binangle_t firstAngle = v->_lineOwners->angle();
        LineOwner *last = v->_lineOwners;
        LineOwner *p = &last->next();
        while(p)
        {
            p->_link[Anticlockwise] = last;

            // Convert to a relative angle between last and this.
            last->_angle = last->angle() - p->angle();

            last = p;
            p = &p->next();
        }
        last->_link[Clockwise] = v->_lineOwners;
        v->_lineOwners->_link[Anticlockwise] = last;

        // Set the angle of the last owner.
        last->_angle = last->angle() - firstAngle;

/*#ifdef DENG2_DEBUG
        LOG_VERBOSE("Vertex #%i: line owners #%i")
            << editmap.vertexes.indexOf(v) << v->lineOwnerCount();

        LineOwner const *base = v->firstLineOwner();
        LineOwner const *cur = base;
        uint idx = 0;
        do
        {
            LOG_VERBOSE("  %i: p= #%05i this= #%05i n= #%05i, dANG= %-3.f")
                << idx << cur->prev().line().indexInMap() << cur->line().indexInMap()
                << cur->next().line().indexInMap() << BANG2DEG(cur->angle());

            idx++;
        } while((cur = &cur->next()) != base);
#endif*/

        // Sanity check.
        DENG2_ASSERT(vertexHasValidLineOwnerRing(*v));
    }
}

bool Map::isEditable() const
{
    return d->editingEnabled;
}

struct VertexInfo
{
    /// Vertex for this info.
    Vertex *vertex;

    /// Determined equivalent vertex.
    Vertex *equiv;

    /// Line -> Vertex reference count.
    uint refCount;

    VertexInfo() : vertex(0), equiv(0), refCount(0)
    {}

    /// @todo Math here is not correct (rounding directionality). -ds
    int compareVertexOrigins(VertexInfo const &other) const
    {
        DENG_ASSERT(vertex != 0 && other.vertex != 0);

        if(this == &other) return 0;
        if(vertex == other.vertex) return 0;

        // Order is firstly X axis major.
        if(int(vertex->origin().x) != int(other.vertex->origin().x))
            return int(vertex->origin().x) - int(other.vertex->origin().x);

        // Order is secondly Y axis major.
        return int(vertex->origin().y) - int(other.vertex->origin().y);
    }

    bool operator < (VertexInfo const &other) const {
        return compareVertexOrigins(other) < 0;
    }
};

void pruneVertexes(Mesh &mesh, Map::Lines const &lines)
{
    /*
     * Step 1 - Find equivalent vertexes:
     */

    // Populate the vertex info.
    QVector<VertexInfo> vertexInfo(mesh.vertexCount());
    int ord = 0;
    foreach(Vertex *vertex, mesh.vertexes())
        vertexInfo[ord++].vertex = vertex;

    {
        // Sort a copy to place near vertexes adjacently.
        QVector<VertexInfo> sortedInfo(vertexInfo);
        qSort(sortedInfo.begin(), sortedInfo.end());

        // Locate equivalent vertexes in the sorted info.
        for(int i = 0; i < sortedInfo.count() - 1; ++i)
        {
            VertexInfo &a = sortedInfo[i];
            VertexInfo &b = sortedInfo[i + 1];

            // Are these equivalent?
            /// @todo fixme: What about polyobjs? They need unique vertexes! -ds
            if(a.compareVertexOrigins(b) == 0)
            {
                b.equiv = (a.equiv? a.equiv : a.vertex);
            }
        }
    }

    /*
     * Step 2 - Replace line references to equivalent vertexes:
     */

    // Count line -> vertex references.
    foreach(Line *line, lines)
    {
        vertexInfo[line->from().indexInMap()].refCount++;
        vertexInfo[  line->to().indexInMap()].refCount++;
    }

    // Perform the replacement.
    foreach(Line *line, lines)
    {
        while(vertexInfo[line->from().indexInMap()].equiv)
        {
            VertexInfo &info = vertexInfo[line->from().indexInMap()];

            info.refCount--;
            line->replaceFrom(*info.equiv);

            vertexInfo[line->from().indexInMap()].refCount++;
        }

        while(vertexInfo[line->to().indexInMap()].equiv)
        {
            VertexInfo &info = vertexInfo[line->to().indexInMap()];

            info.refCount--;
            line->replaceTo(*info.equiv);

            vertexInfo[line->to().indexInMap()].refCount++;
        }
    }

    /*
     * Step 3 - Prune vertexes:
     */
    int prunedCount = 0, numUnused = 0;
    foreach(VertexInfo const &info, vertexInfo)
    {
        Vertex *vertex = info.vertex;

        if(info.refCount)
            continue;

        mesh.removeVertex(*vertex);

        prunedCount += 1;
        if(!info.equiv) numUnused += 1;
    }

    if(prunedCount)
    {
        // Re-index with a contiguous range of indices.
        int ord = 0;
        foreach(Vertex *vertex, mesh.vertexes())
            vertex->setIndexInMap(ord++);

        /// Update lines. @todo Line should handle this itself.
        foreach(Line *line, lines)
        {
            line->updateSlopeType();
            line->updateAABox();
        }

        LOG_INFO("Pruned %d vertexes (%d equivalents, %d unused).")
            << prunedCount << (prunedCount - numUnused) << numUnused;
    }
}

bool Map::endEditing()
{
    if(!d->editingEnabled)
        return true; // Huh?

    d->editingEnabled = false;

    LOG_AS("Map");
    LOG_VERBOSE("Editing ended.");
    LOG_DEBUG("New elements: %d Vertexes, %d Lines, %d Polyobjs and %d Sectors.")
        << d->mesh.vertexCount()        << d->editable.lines.count()
        << d->editable.polyobjs.count() << d->editable.sectors.count();

    /*
     * Perform cleanup on the new map elements.
     */
    pruneVertexes(d->mesh, d->editable.lines);

    // Ensure lines with only one sector are flagged as blocking.
    foreach(Line *line, d->editable.lines)
    {
        if(!line->hasFrontSector() || !line->hasBackSector())
            line->setFlags(DDLF_BLOCKING);
    }

    buildVertexLineOwnerRings(d->mesh.vertexes(), d->editable.lines);

    /*
     * Move the editable elements to the "static" element lists.
     */

    // Collate sectors:
    DENG2_ASSERT(d->sectors.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    d->sectors.reserve(d->editable.sectors.count());
#endif
    d->sectors.append(d->editable.sectors);
    d->editable.sectors.clear();

    // Collate lines:
    DENG2_ASSERT(d->lines.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    d->lines.reserve(d->editable.lines.count());
#endif
    d->lines.append(d->editable.lines);
    d->editable.lines.clear();

    // Collate polyobjs:
    DENG2_ASSERT(d->polyobjs.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    d->polyobjs.reserve(d->editable.polyobjs.count());
#endif
    while(!d->editable.polyobjs.isEmpty())
    {
        d->polyobjs.append(d->editable.polyobjs.takeFirst());
        Polyobj *polyobj = d->polyobjs.back();

        // Create a segment for each line of this polyobj.
        foreach(Line *line, polyobj->lines())
        {
            HEdge *hedge = polyobj->mesh().newHEdge(line->from());

            hedge->setTwin(polyobj->mesh().newHEdge(line->to()));
            hedge->twin().setTwin(hedge);

            // Polyobj has ownership of the line segments.
            Segment *segment = new Segment(&line->front(), hedge);

            segment->setMap(this);
            segment->setLength(line->length());

            line->front().setLeftHEdge(hedge);
            line->front().setRightHEdge(hedge);
        }

        polyobj->buildUniqueVertexes();
        polyobj->updateOriginalVertexCoords();
    }

    // Determine the map bounds.
    d->updateBounds();
    LOG_INFO("Geometry bounds:") << Rectangled(d->bounds.min, d->bounds.max).asText();

    // Build a line blockmap.
    d->initLineBlockmap();

    // Build a BSP.
    if(!d->buildBsp())
        return false;

    // The mobj and polyobj blockmaps are maintained dynamically.
    d->initMobjBlockmap();
    d->initPolyobjBlockmap();

    // Finish lines.
    foreach(Line *line, d->lines)
    for(int i = 0; i < 2; ++i)
    {
        line->side(i).updateSurfaceNormals();
        line->side(i).updateAllSoundEmitterOrigins();
    }

    // Finish sectors.
    foreach(Sector *sector, d->sectors)
    {
        sector->buildBspLeafs(*this);
        sector->buildSides(*this);
        sector->updateAABox();
        sector->updateRoughArea();

        /*
         * Chain sound emitters (ddmobj_base_t) owned by all Surfaces in the
         * sector-> These chains are used for efficiently traversing all of the
         * sound emitters in a sector (e.g., when stopping all sounds emitted
         * in the sector).
         */
        ddmobj_base_t &emitter = sector->soundEmitter();

        // Clear the head of the emitter chain.
        emitter.thinker.next = emitter.thinker.prev = 0;

        // Link plane surface emitters:
        foreach(Plane *plane, sector->planes())
        {
            sector->linkSoundEmitter(plane->soundEmitter());
        }

        // Link wall surface emitters:
        foreach(Line::Side *side, sector->sides())
        {
            if(side->hasSections())
            {
                sector->linkSoundEmitter(side->middleSoundEmitter());
                sector->linkSoundEmitter(side->bottomSoundEmitter());
                sector->linkSoundEmitter(side->topSoundEmitter());
            }
            if(side->line().isSelfReferencing() && side->back().hasSections())
            {
                Line::Side &back = side->back();
                sector->linkSoundEmitter(back.middleSoundEmitter());
                sector->linkSoundEmitter(back.bottomSoundEmitter());
                sector->linkSoundEmitter(back.topSoundEmitter());
            }
        }

        sector->updateSoundEmitterOrigin();
    }

    // Finish planes.
    foreach(Sector *sector, d->sectors)
    foreach(Plane *plane, sector->planes())
    {
        plane->updateSoundEmitterOrigin();
    }

    // We can now initialize the BSP leaf blockmap.
    d->initBspLeafBlockmap();

#ifdef __CLIENT__
    S_DetermineBspLeafsAffectingSectorReverb(this);
#endif

    // Prepare the thinker lists.
    d->thinkers.reset(new Thinkers);

    return true;
}

Vertex *Map::createVertex(Vector2d const &origin, int archiveIndex)
{
    if(!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::createVertex", "Editing is not enabled");

    Vertex *vtx = d->mesh.newVertex(origin);

    vtx->setMap(this);
    vtx->setIndexInArchive(archiveIndex);

    /// @todo Don't do this here.
    vtx->setIndexInMap(d->mesh.vertexCount() - 1);

    return vtx;
}

Line *Map::createLine(Vertex &v1, Vertex &v2, int flags, Sector *frontSector,
    Sector *backSector, int archiveIndex)
{
    if(!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::createLine", "Editing is not enabled");

    Line *line = new Line(v1, v2, flags, frontSector, backSector);
    d->editable.lines.append(line);

    line->setMap(this);
    line->setIndexInArchive(archiveIndex);

    /// @todo Don't do this here.
    line->setIndexInMap(d->editable.lines.count() - 1);
    line->front().setIndexInMap(Map::toSideIndex(line->indexInMap(), Line::Front));
    line->back().setIndexInMap(Map::toSideIndex(line->indexInMap(), Line::Back));

    return line;
}

Sector *Map::createSector(float lightLevel, Vector3f const &lightColor,
    int archiveIndex)
{
    if(!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::createSector", "Editing is not enabled");

    Sector *sector = new Sector(lightLevel, lightColor);
    d->editable.sectors.append(sector);

    sector->setMap(this);
    sector->setIndexInArchive(archiveIndex);

    /// @todo Don't do this here.
    sector->setIndexInMap(d->editable.sectors.count() - 1);

    return sector;
}

Polyobj *Map::createPolyobj(Vector2d const &origin)
{
    if(!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::createPolyobj", "Editing is not enabled");

    void *region = M_Calloc(POLYOBJ_SIZE);
    Polyobj *po = new (region) Polyobj(origin);
    d->editable.polyobjs.append(po);

    /// @todo Don't do this here.
    po->setIndexInMap(d->editable.polyobjs.count() - 1);

    return po;
}

Map::Lines const &Map::editableLines() const
{
    if(!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::editableLines", "Editing is not enabled");
    return d->editable.lines;
}

Map::Sectors const &Map::editableSectors() const
{
    if(!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::editableSectors", "Editing is not enabled");
    return d->editable.sectors;
}

Map::Polyobjs const &Map::editablePolyobjs() const
{
    if(!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::editablePolyobjs", "Editing is not enabled");
    return d->editable.polyobjs;
}

void EditableElements::clearAll()
{
    qDeleteAll(lines);
    lines.clear();

    qDeleteAll(sectors);
    sectors.clear();

    foreach(Polyobj *po, polyobjs)
    {
        po->~Polyobj();
        M_Free(po);
    }
    polyobjs.clear();
}

} // namespace de
