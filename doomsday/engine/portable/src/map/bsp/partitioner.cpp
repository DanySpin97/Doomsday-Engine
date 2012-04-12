/**
 * @file partitioner.cpp
 * BSP Partitioner. Recursive node creation and sorting. @ingroup map
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
 * @authors Copyright © 1997-1998 Raphael.Quinet <raphael.quinet@eed.ericsson.se>
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
#include <vector>
#include <list>
#include <algorithm>

#include <de/Log>

#include "de_base.h"
#include "de_console.h"
#include "p_mapdata.h"
#include "p_maputil.h"
#include "m_misc.h"

#include "bspleaf.h"
#include "bspnode.h"
#include "hedge.h"
#include "map/bsp/hedgeinfo.h"
#include "map/bsp/hedgeintercept.h"
#include "map/bsp/hedgetip.h"
#include "map/bsp/hplane.h"
#include "map/bsp/linedefinfo.h"
#include "map/bsp/partitioncost.h"
#include "map/bsp/superblockmap.h"
#include "map/bsp/vertexinfo.h"

#include "map/bsp/partitioner.h"

using namespace de::bsp;

/// Minimum length of a half-edge post partitioning. Used in cost evaluation.
static const double SHORT_HEDGE_EPSILON = 4.0;

/// Smallest distance between two points before being considered equal.
static const double DIST_EPSILON = (1.0 / 128.0);

/// Smallest difference between two angles before being considered equal (in degrees).
static const double ANG_EPSILON = (1.0 / 1024.0);

static bool getAveragedCoords(BspLeaf* leaf, pvec2d_t midPoint);
static void logUnclosed(const BspLeaf* leaf);
static void initAABoxFromEditableLineDefVertexes(AABoxf* aaBox, const LineDef* line);
static Sector* findFirstSectorInHEdgeList(const BspLeaf* leaf);

DENG_DEBUG_ONLY(static int printSuperBlockHEdgesWorker(SuperBlock* block, void* /*parameters*/));

struct Partitioner::Instance
{
    // Used when sorting BSP leaf half-edges by angle around midpoint.
    typedef std::vector<HEdge*>HEdgeSortBuffer;

    Instance(GameMap* _map, uint* _numEditableVertexes,
        Vertex*** _editableVertexes, int _splitCostFactor) :
        splitCostFactor(_splitCostFactor),
        map(_map),
        numEditableVertexes(_numEditableVertexes), editableVertexes(_editableVertexes),
        numNodes(0), numLeafs(0), numHEdges(0), numVertexes(0),
        rootNode(0), partition(0), partitionLineDef(0),
        unclosedSectors(), migrantHEdges(),
        builtOK(false)
    {
        initPartitionInfo();
    }

    ~Instance()
    {
        for(uint i = 0; i < *numEditableVertexes; ++i)
        {
            clearHEdgeTipsByVertex((*editableVertexes)[i]);
        }

        for(Vertexes::iterator it = vertexes.begin(); it != vertexes.end(); ++it)
        {
            Vertex* vtx = *it;
            // Has ownership of this vertex been claimed?
            if(!vtx) continue;

            clearHEdgeTipsByVertex(vtx);
            free(vtx);
        }

        // We are finished with the BSP data.
        if(rootNode)
        {
            // If ownership of the BSP data has been claimed this should be a no-op.
            clearAllBspObjects();

            // Destroy our private BSP tree.
            delete rootNode;
        }
    }

    /**
     * Retrieve the extended build info for the specified @a lineDef.
     * @return  Extended info for that LineDef.
     */
    LineDefInfo& lineDefInfo(const LineDef& lineDef) {
        return lineDefInfos[lineDef.buildData.index - 1];
    }
    const LineDefInfo& lineDefInfo(const LineDef& lineDef) const {
        return lineDefInfos[lineDef.buildData.index - 1];
    }

    /**
     * Retrieve the extended build info for the specified @a hedge.
     * @return  Extended info for that HEdge.
     */
    HEdgeInfo& hedgeInfo(const HEdge& hedge) {
        return hedgeInfos[hedge.buildData.index - 1];
    }
    const HEdgeInfo& hedgeInfo(const HEdge& hedge) const {
        return hedgeInfos[hedge.buildData.index - 1];
    }

    /**
     * Retrieve the extended build info for the specified @a vertex.
     * @return  Extended info for that Vertex.
     */
    VertexInfo& vertexInfo(const Vertex& vertex) {
        return vertexInfos[vertex.buildData.index - 1];
    }
    const VertexInfo& vertexInfo(const Vertex& vertex) const {
        return vertexInfos[vertex.buildData.index - 1];
    }

    void initForMap()
    {
        uint numLineDefs = GameMap_LineDefCount(map);
        lineDefInfos.resize(numLineDefs);

        for(uint i = 0; i < numLineDefs; ++i)
        {
            LineDef* l = GameMap_LineDef(map, i);
            LineDefInfo& info = lineDefInfo(*l);
            const Vertex* start = l->v[0];
            const Vertex* end   = l->v[1];

            // Check for zero-length line.
            if((fabs(start->buildData.pos[VX] - end->buildData.pos[VX]) < DIST_EPSILON) &&
               (fabs(start->buildData.pos[VY] - end->buildData.pos[VY]) < DIST_EPSILON))
                info.flags |= LineDefInfo::ZEROLENGTH;

            if(l->sideDefs[BACK] && l->sideDefs[FRONT])
            {
                info.flags |= LineDefInfo::TWOSIDED;

                if(l->sideDefs[BACK]->sector == l->sideDefs[FRONT]->sector)
                    info.flags |= LineDefInfo::SELFREF;
            }
        }

        vertexInfos.resize(*numEditableVertexes);
    }

    void findMapBounds(AABoxf* aaBox) const
    {
        Q_ASSERT(aaBox);

        AABoxf bounds;
        boolean initialized = false;

        for(uint i = 0; i < GameMap_LineDefCount(map); ++i)
        {
            LineDef* line = GameMap_LineDef(map, i);

            // Do not consider zero-length LineDefs.
            if(lineDefInfo(*line).flags.testFlag(LineDefInfo::ZEROLENGTH)) continue;

            AABoxf lineAABox;
            initAABoxFromEditableLineDefVertexes(&lineAABox, line);

            if(initialized)
            {
                V2f_AddToBox(bounds.arvec2, lineAABox.min);
            }
            else
            {
                V2f_InitBox(bounds.arvec2, lineAABox.min);
                initialized = true;
            }

            V2f_AddToBox(bounds.arvec2, lineAABox.max);
        }

        if(initialized)
        {
            V2f_CopyBox(aaBox->arvec2, bounds.arvec2);
            return;
        }

        // Clear.
        V2f_Set(aaBox->min, DDMAXFLOAT, DDMAXFLOAT);
        V2f_Set(aaBox->max, DDMINFLOAT, DDMINFLOAT);
    }

    HEdge* linkHEdgeInSuperBlockmap(SuperBlock& block, HEdge* hedge)
    {
        if(hedge)
        {
            HEdgeInfo& hInfo = hedgeInfo(*hedge);
            SuperBlock* subblock = &block.push(*hedge);

            // Associate this half-edge with the final subblock.
            hInfo.bmapBlock = subblock;
        }
        return hedge;
    }

    /**
     * Initially create all half-edges and add them to specified SuperBlock.
     */
    void createInitialHEdges(SuperBlock& hedgeList)
    {
        Q_ASSERT(map);

        for(uint i = 0; i < map->numLineDefs; ++i)
        {
            LineDef* line = GameMap_LineDef(map, i);
            HEdge* front = NULL;
            HEdge* back = NULL;

            // Polyobj lines are completely ignored.
            if(line->inFlags & LF_POLYOBJ) continue;

            // Ignore zero-length and polyobj lines.
            if(!lineDefInfo(*line).flags.testFlag(LineDefInfo::ZEROLENGTH)
               /*&& !lineDefInfo(*line).overlap*/)
            {
                // Check for Humungously long lines.
                if(ABS(line->v[0]->buildData.pos[VX] - line->v[1]->buildData.pos[VX]) >= 10000 ||
                   ABS(line->v[0]->buildData.pos[VY] - line->v[1]->buildData.pos[VY]) >= 10000)
                {
                    if(3000 >=
                       M_Length(line->v[0]->buildData.pos[VX] - line->v[1]->buildData.pos[VX],
                                line->v[0]->buildData.pos[VY] - line->v[1]->buildData.pos[VY]))
                    {
                        LOG_WARNING("LineDef #%d is very long, it may cause problems.") << line->buildData.index;
                    }
                }

                if(line->sideDefs[FRONT])
                {
                    SideDef* side = line->sideDefs[FRONT];

                    if(!side->sector)
                        LOG_INFO("Bad SideDef on LineDef #%d.") << line->buildData.index;

                    front = newHEdge(line, line, line->v[0], line->v[1], side->sector, false);
                    linkHEdgeInSuperBlockmap(hedgeList, front);
                }
                else
                {
                    LOG_INFO("LineDef #%d has no front SideDef!") << line->buildData.index;
                }

                if(line->sideDefs[BACK])
                {
                    SideDef* side = line->sideDefs[BACK];

                    if(!side->sector)
                        LOG_INFO("Bad SideDef on LineDef #%d.") << line->buildData.index;

                    back = newHEdge(line, line, line->v[1], line->v[0], side->sector, true);
                    linkHEdgeInSuperBlockmap(hedgeList, back);

                    if(front)
                    {
                        // Half-edges always maintain a one-to-one relationship with their
                        // twins, so if one gets split, the other must be split also.
                        back->twin = front;
                        front->twin = back;
                    }
                }
                else
                {
                    if(lineDefInfo(*line).flags.testFlag(LineDefInfo::TWOSIDED))
                    {
                        LOG_INFO("LineDef #%d is two-sided but has no back SideDef.") << line->buildData.index;
                        lineDefInfo(*line).flags &= ~LineDefInfo::TWOSIDED;
                    }

                    // Handle the 'One-Sided Window' trick.
                    if(line->buildData.windowEffect && front)
                    {
                        HEdge* other = newHEdge(front->lineDef, line,
                                                line->v[1], line->v[0], line->buildData.windowEffect, true);

                        linkHEdgeInSuperBlockmap(hedgeList, other);

                        // Setup the twin-ing (it's very strange to have a mini
                        // and a normal partnered together).
                        other->twin = front;
                        front->twin = other;
                    }
                }
            }

            // @todo edge tips should be created when half-edges are created.
            double x1 = line->v[0]->buildData.pos[VX];
            double y1 = line->v[0]->buildData.pos[VY];
            double x2 = line->v[1]->buildData.pos[VX];
            double y2 = line->v[1]->buildData.pos[VY];

            addHEdgeTip(line->v[0], M_SlopeToAngle(x2 - x1, y2 - y1), back, front);
            addHEdgeTip(line->v[1], M_SlopeToAngle(x1 - x2, y1 - y2), front, back);
        }
    }

    void initHEdgesAndBuildBsp(SuperBlockmap& blockmap)
    {
        Q_ASSERT(map);
        // It begins...
        rootNode = NULL;

        createInitialHEdges(blockmap.root());
        builtOK = buildNodes(blockmap.root(), &rootNode);

        if(rootNode)
        {
            windLeafs();

            // We're done with the build info.
            //clearAllHEdgeInfo();
        }
    }

    /// @c true  Iff @a hedge has been added to a BSP leaf (i.e., it is no longer
    ///          linked in the hedge blockmap).
    bool hedgeIsInLeaf(const HEdge& hedge) const
    {
        /// @todo Are we now able to determine this by testing hedge->leaf ?
        return !hedgeInfo(hedge).bmapBlock;
    }

    const HPlaneIntercept* makePartitionIntersection(HEdge* hedge, int leftSide)
    {
        Q_ASSERT(hedge);

        // Already present on this edge?
        Vertex* vertex = hedge->v[leftSide?1:0];
        const HPlaneIntercept* inter = partitionInterceptByVertex(vertex);
        if(inter) return inter;

        LineDef* line = hedge->lineDef;
        HEdgeIntercept* intercept = newHEdgeIntercept(vertex, line && lineDefInfo(*line).flags.testFlag(LineDefInfo::SELFREF));

        return partition->newIntercept(vertexDistanceFromPartition(vertex), intercept);
    }

    /**
     * @return  Same as @a final for caller convenience.
     */
    HEdgeIntercept& mergeHEdgeIntercepts(HEdgeIntercept& final, HEdgeIntercept& other)
    {
        /*
        LOG_TRACE("Merging intersections:");
        HEdgeIntercept::DebugPrint(final);
        HEdgeIntercept::DebugPrint(other);
        */

        if(final.selfRef && !other.selfRef)
        {
            if(final.before && other.before)
                final.before = other.before;

            if(final.after && other.after)
                final.after = other.after;

            final.selfRef = false;
        }

        if(!final.before && other.before)
            final.before = other.before;

        if(!final.after && other.after)
            final.after = other.after;

        /*
        LOG_TRACE("Result:");
        HEdgeIntercept::DebugPrint(final);
        */

        // Destroy the redundant other.
        deleteHEdgeIntercept(other);
        return final;
    }

    void mergeIntersections()
    {
        HPlane::Intercepts::iterator node = partition->begin();
        while(node != partition->end())
        {
            HPlane::Intercepts::iterator np = node; np++;
            if(np == partition->end()) break;

            double len = *np - *node;
            if(len < -0.1)
            {
                Con_Error("Partitioner::MergeIntersections: Invalid intercept order - %1.3f > %1.3f\n",
                          node->distance(), np->distance());
            }
            else if(len > 0.2)
            {
                node++;
                continue;
            }

            HEdgeIntercept* cur  = reinterpret_cast<HEdgeIntercept*>(node->userData());
            HEdgeIntercept* next = reinterpret_cast<HEdgeIntercept*>(np->userData());

            /*if(len > DIST_EPSILON)
            {
                LOG_DEBUG("Skipping very short half-edge (len: %1.3f) near [%1.1f, %1.1f]")
                        << len << cur->vertex->V_pos[VX] << cur->vertex->V_pos[VY];
            }*/

            // Merge info for the two intersections into one (next is destroyed).
            mergeHEdgeIntercepts(*cur, *next);

            // Unlink this intercept.
            partition->deleteIntercept(np);
        }
    }

    void buildHEdgesAtIntersectionGaps(SuperBlock& rightList, SuperBlock& leftList)
    {
        HPlane::Intercepts::const_iterator node = partition->begin();
        while(node != partition->end())
        {
            HPlane::Intercepts::const_iterator np = node; np++;
            if(np == partition->end()) break;

            HEdgeIntercept* cur = reinterpret_cast<HEdgeIntercept*>((*node).userData());
            HEdgeIntercept* next = reinterpret_cast<HEdgeIntercept*>((*np).userData());

            if(!(!cur->after && !next->before))
            {
                // Check for some nasty open/closed or close/open cases.
                if(cur->after && !next->before)
                {
                    if(!cur->selfRef)
                    {
                        double pos[2];

                        pos[VX] = cur->vertex->buildData.pos[VX] + next->vertex->buildData.pos[VX];
                        pos[VY] = cur->vertex->buildData.pos[VY] + next->vertex->buildData.pos[VY];

                        pos[VX] /= 2;
                        pos[VY] /= 2;

                        registerUnclosedSector(cur->after, pos[VX], pos[VY]);
                    }
                }
                else if(!cur->after && next->before)
                {
                    if(!next->selfRef)
                    {
                        double pos[2];

                        pos[VX] = cur->vertex->buildData.pos[VX] + next->vertex->buildData.pos[VX];
                        pos[VY] = cur->vertex->buildData.pos[VY] + next->vertex->buildData.pos[VY];
                        pos[VX] /= 2;
                        pos[VY] /= 2;

                        registerUnclosedSector(next->before, pos[VX], pos[VY]);
                    }
                }
                else
                {
                    // This is definitetly open space.
                    HEdge* right, *left;

                    // Do a sanity check on the sectors (just for good measure).
                    if(cur->after != next->before)
                    {
                        if(!cur->selfRef && !next->selfRef)
                        {
                            LOG_DEBUG("Sector mismatch (#%d [%1.1f, %1.1f] != #%d [%1.1f, %1.1f]).")
                                    << cur->after->buildData.index << cur->vertex->buildData.pos[VX]
                                    << cur->vertex->buildData.pos[VY] << next->before->buildData.index
                                    << next->vertex->buildData.pos[VX] << next->vertex->buildData.pos[VY];
                        }

                        // Choose the non-self-referencing sector when we can.
                        if(cur->selfRef && !next->selfRef)
                        {
                            cur->after = next->before;
                        }
                    }

                    addHEdgesBetweenIntercepts(cur, next, &right, &left);

                    // Add the new half-edges to the appropriate lists.
                    linkHEdgeInSuperBlockmap(rightList, right);
                    linkHEdgeInSuperBlockmap(leftList,  left);
                }
            }

            node++;
        }
    }

    /**
     * Splits the given half-edge at the point (x,y). The new half-edge is returned.
     * The old half-edge is shortened (the original start vertex is unchanged), the
     * new half-edge becomes the cut-off tail (keeping the original end vertex).
     *
     * @note If the half-edge has a twin it is also split.
     */
    HEdge* splitHEdge(HEdge* oldHEdge, const_pvec2d_t point)
    {
        Q_ASSERT(oldHEdge);

        //LOG_DEBUG("Splitting hedge %p at [%1.1f, %1.1f].")
        //        << oldHEdge << x << y;

        Vertex* newVert = newVertex(point);
        HEdgeInfo& oldInfo = hedgeInfo(*oldHEdge);
        addHEdgeTip(newVert, M_SlopeToAngle(-oldInfo.pDX, -oldInfo.pDY), oldHEdge, oldHEdge->twin);
        addHEdgeTip(newVert, M_SlopeToAngle( oldInfo.pDX,  oldInfo.pDY), oldHEdge->twin, oldHEdge);

        HEdge* newHEdge = cloneHEdge(*oldHEdge);
        //oldInfo = hedgeInfo(*oldHEdge);
        HEdgeInfo& newInfo = hedgeInfo(*newHEdge);

        newInfo.prevOnSide = oldHEdge;
        oldInfo.nextOnSide = newHEdge;

        oldHEdge->v[1] = newVert;
        oldInfo.initFromHEdge(*oldHEdge);

        newHEdge->v[0] = newVert;
        newInfo.initFromHEdge(*newHEdge);

        // Handle the twin.
        if(oldHEdge->twin)
        {
            //LOG_DEBUG("Splitting hedge twin %p.") << oldHEdge->twin;

            // Copy the old hedge info.
            newHEdge->twin = cloneHEdge(*oldHEdge->twin);
            newHEdge->twin->twin = newHEdge;

            hedgeInfo(*newHEdge->twin).nextOnSide = oldHEdge->twin;
            hedgeInfo(*oldHEdge->twin).prevOnSide = newHEdge->twin;

            oldHEdge->twin->v[0] = newVert;
            hedgeInfo(*oldHEdge->twin).initFromHEdge(*oldHEdge->twin);

            newHEdge->twin->v[1] = newVert;
            hedgeInfo(*newHEdge->twin).initFromHEdge(*newHEdge->twin);

            // Has this already been added to a leaf?
            if(hedgeIsInLeaf(*oldHEdge->twin))
            {
                // Update the in-leaf references.
                oldHEdge->twin->next = newHEdge->twin;
            }
        }

        return newHEdge;
    }

    /**
     * Partition the given edge and perform any further necessary action (moving it
     * into either the left list, right list, or splitting it).
     *
     * Take the given half-edge 'cur', compare it with the partition line and determine
     * it's fate: moving it into either the left or right lists (perhaps both, when
     * splitting it in two). Handles the twin as well. Updates the intersection list
     * if the half-edge lies on or crosses the partition line.
     *
     * @note AJA: I have rewritten this routine based on evalPartition() (which I've
     *       also reworked, heavily). I think it is important that both these routines
     *       follow the exact same logic.
     */
    void divideHEdge(HEdge* hedge, SuperBlock& rightList, SuperBlock& leftList)
    {
    #define RIGHT 0
    #define LEFT  1

        // Determine the relationship between this half-edge and the partition plane.
        double a = hedgeDistanceFromPartition(hedge, false/*start vertex*/);
        double b = hedgeDistanceFromPartition(hedge, true/*end vertex*/);

        /// @kludge Half-edges produced from the same source linedef must always
        ///         be treated as collinear.
        /// @todo   Why is this override necessary?
        HEdgeInfo& hInfo = hedgeInfo(*hedge);
        if(hInfo.sourceLineDef == partitionInfo.sourceLineDef)
            a = b = 0;
        // kludge end

        // Collinear with the partition plane?
        if(fabs(a) <= DIST_EPSILON && fabs(b) <= DIST_EPSILON)
        {
            makePartitionIntersection(hedge, RIGHT);
            makePartitionIntersection(hedge, LEFT);

            // Direction (vs that of the partition plane) determines in which subset
            // this half-edge belongs.
            if(hInfo.pDX * partitionInfo.pDX +
               hInfo.pDY * partitionInfo.pDY < 0)
            {
                linkHEdgeInSuperBlockmap(leftList, hedge);
            }
            else
            {
                linkHEdgeInSuperBlockmap(rightList, hedge);
            }
            return;
        }

        // Right of the partition plane?.
        if(a > -DIST_EPSILON && b > -DIST_EPSILON)
        {
            // Close enough to intersect?
            if(a < DIST_EPSILON)
                makePartitionIntersection(hedge, RIGHT);
            else if(b < DIST_EPSILON)
                makePartitionIntersection(hedge, LEFT);

            linkHEdgeInSuperBlockmap(rightList, hedge);
            return;
        }

        // Left of the partition plane?
        if(a < DIST_EPSILON && b < DIST_EPSILON)
        {
            // Close enough to intersect?
            if(a > -DIST_EPSILON)
                makePartitionIntersection(hedge, RIGHT);
            else if(b > -DIST_EPSILON)
                makePartitionIntersection(hedge, LEFT);

            linkHEdgeInSuperBlockmap(leftList, hedge);
            return;
        }

        /**
         * Straddles the partition plane and must therefore be split.
         */
        vec2d_t point;
        interceptHEdgePartition(hedge, a, b, point);

        HEdge* newHEdge = splitHEdge(hedge, point);

        // Ensure the new twin is inserted into the same block as the old twin.
        if(hedge->twin && !hedgeIsInLeaf(*hedge->twin))
        {
            SuperBlock* bmapBlock = hedgeInfo(*hedge->twin).bmapBlock;
            Q_ASSERT(bmapBlock);
            linkHEdgeInSuperBlockmap(*bmapBlock, newHEdge->twin);
        }

        makePartitionIntersection(hedge, LEFT);

        if(a < 0)
        {
            linkHEdgeInSuperBlockmap(rightList, newHEdge);
            linkHEdgeInSuperBlockmap(leftList,  hedge);
        }
        else
        {
            linkHEdgeInSuperBlockmap(rightList, hedge);
            linkHEdgeInSuperBlockmap(leftList,  newHEdge);
        }

    #undef LEFT
    #undef RIGHT
    }

    /**
     * Remove all the half-edges from the list, partitioning them into the left or
     * right lists based on the given partition line. Adds any intersections onto the
     * intersection list as it goes.
     */
    void partitionHEdges(SuperBlock& hedgeList, SuperBlock& rights, SuperBlock& lefts)
    {
        // Iterative pre-order traversal of SuperBlock.
        SuperBlock* cur = &hedgeList;
        SuperBlock* prev = 0;
        while(cur)
        {
            while(cur)
            {
                HEdge* hedge;
                while((hedge = cur->pop()))
                {
                    // Disassociate the half-edge from the blockmap.
                    hedgeInfo(*hedge).bmapBlock = 0;

                    divideHEdge(hedge, rights, lefts);
                }

                if(prev == cur->parent())
                {
                    // Descending - right first, then left.
                    prev = cur;
                    if(cur->hasRight()) cur = cur->right();
                    else                cur = cur->left();
                }
                else if(prev == cur->right())
                {
                    // Last moved up the right branch - descend the left.
                    prev = cur;
                    cur = cur->left();
                }
                else if(prev == cur->left())
                {
                    // Last moved up the left branch - continue upward.
                    prev = cur;
                    cur = cur->parent();
                }
            }

            if(prev)
            {
                // No left child - back up.
                cur = prev->parent();
            }
        }

        // Sanity checks...
        if(!rights.totalHEdgeCount())
            Con_Error("Partitioner::partitionhedges: Separated half-edge has no right side.");

        if(!lefts.totalHEdgeCount())
            Con_Error("Partitioner::partitionhedges: Separated half-edge has no left side.");
    }

    void evalPartitionCostForHEdge(const HEdgeInfo& partInfo, int costFactorMultiplier,
        const HEdge* hedge, PartitionCost& cost)
    {
#define ADD_LEFT()  \
        if (hedge->lineDef) cost.realLeft += 1;  \
        else                cost.miniLeft += 1;  \

#define ADD_RIGHT()  \
        if (hedge->lineDef) cost.realRight += 1;  \
        else                cost.miniRight += 1;  \

        double qnty, a, b, fa, fb;
        Q_ASSERT(hedge);

        // Get state of lines' relation to each other.
        const HEdgeInfo& hInfo = hedgeInfo(*hedge);
        if(hInfo.sourceLineDef == partInfo.sourceLineDef)
        {
            a = b = fa = fb = 0;
        }
        else
        {
            a = M_PerpDist(partInfo.pDX, partInfo.pDY, partInfo.pPerp, partInfo.pLength,
                           hInfo.pSX, hInfo.pSY);
            b = M_PerpDist(partInfo.pDX, partInfo.pDY, partInfo.pPerp, partInfo.pLength,
                           hInfo.pEX, hInfo.pEY);

            fa = fabs(a);
            fb = fabs(b);
        }

        // Collinear?
        if(fa <= DIST_EPSILON && fb <= DIST_EPSILON)
        {
            // This half-edge runs along the same line as the partition.
            // hedge whether it goes in the same direction or the opposite.
            if(hInfo.pDX * partInfo.pDX + hInfo.pDY * partInfo.pDY < 0)
            {
                ADD_LEFT();
            }
            else
            {
                ADD_RIGHT();
            }

            return;
        }

        // Off to the right?
        if(a > -DIST_EPSILON && b > -DIST_EPSILON)
        {
            ADD_RIGHT();

            // Near miss?
            if((a >= SHORT_HEDGE_EPSILON && b >= SHORT_HEDGE_EPSILON) ||
               (a <= DIST_EPSILON && b >= SHORT_HEDGE_EPSILON) ||
               (b <= DIST_EPSILON && a >= SHORT_HEDGE_EPSILON))
            {
                // No.
                return;
            }

            cost.nearMiss++;

            /**
             * Near misses are bad, since they have the potential to cause really short
             * minihedges to be created in future processing. Thus the closer the near
             * miss, the higher the cost.
             */

            if(a <= DIST_EPSILON || b <= DIST_EPSILON)
                qnty = SHORT_HEDGE_EPSILON / MAX_OF(a, b);
            else
                qnty = SHORT_HEDGE_EPSILON / MIN_OF(a, b);

            cost.total += (int) (100 * costFactorMultiplier * (qnty * qnty - 1.0));
            return;
        }

        // Off to the left?
        if(a < DIST_EPSILON && b < DIST_EPSILON)
        {
            ADD_LEFT();

            // Near miss?
            if((a <= -SHORT_HEDGE_EPSILON && b <= -SHORT_HEDGE_EPSILON) ||
               (a >= -DIST_EPSILON && b <= -SHORT_HEDGE_EPSILON) ||
               (b >= -DIST_EPSILON && a <= -SHORT_HEDGE_EPSILON))
            {
                // No.
                return;
            }

            cost.nearMiss++;

            // The closer the miss, the higher the cost (see note above).
            if(a >= -DIST_EPSILON || b >= -DIST_EPSILON)
                qnty = SHORT_HEDGE_EPSILON / -MIN_OF(a, b);
            else
                qnty = SHORT_HEDGE_EPSILON / -MAX_OF(a, b);

            cost.total += (int) (70 * costFactorMultiplier * (qnty * qnty - 1.0));
            return;
        }

        /**
         * When we reach here, we have a and b non-zero and opposite sign,
         * hence this half-edge will be split by the partition line.
         */

        cost.splits++;
        cost.total += 100 * costFactorMultiplier;

        /**
         * If the split point is very close to one end, which is quite an undesirable
         * situation (producing really short edges), thus a rather hefty surcharge.
         */
        if(fa < SHORT_HEDGE_EPSILON || fb < SHORT_HEDGE_EPSILON)
        {
            cost.iffy++;

            // The closer to the end, the higher the cost.
            qnty = SHORT_HEDGE_EPSILON / MIN_OF(fa, fb);
            cost.total += (int) (140 * costFactorMultiplier * (qnty * qnty - 1.0));
        }

    #undef ADD_RIGHT
    #undef ADD_LEFT
    }

    /**
     * @param best  Best half-edge found thus far.
     * @param bestCost  Running cost total result for the best half-edge found thus far.
     * @param hedge  The candidate half-edge to be evaluated.
     * @param cost  PartitionCost analysis to be completed for this candidate. Must have
     *              been initialized prior to calling this.
     * @return  @c true iff this half-edge is suitable for use as a partition.
     */
    int evalPartitionCostForSuperBlock(const SuperBlock& block, int splitCostFactor,
        HEdge* best, const PartitionCost& bestCost, HEdge* hedge, PartitionCost& cost)
    {
        /**
         * Test the whole block against the partition line to quickly handle all the
         * half-edges within it at once. Only when the partition line intercepts the
         * box do we need to go deeper into it.
         */
        const HEdgeInfo& hInfo = hedgeInfo(*hedge);
        int side = P_BoxOnLineSide3(&block.bounds(), hInfo.pSX, hInfo.pSY,
                                    hInfo.pDX, hInfo.pDY, hInfo.pPerp,
                                    hInfo.pLength, DIST_EPSILON);

        if(side < 0)
        {
            // Left.
            cost.realLeft += block.realHEdgeCount();
            cost.miniLeft += block.miniHEdgeCount();
            return true;
        }
        else if(side > 0)
        {
            // Right.
            cost.realRight += block.realHEdgeCount();
            cost.miniRight += block.miniHEdgeCount();
            return true;
        }

        // Check partition against all half-edges.
        for(SuperBlock::HEdges::const_iterator it = block.hedgesBegin();
            it != block.hedgesEnd(); ++it)
        {
            // Do we already have a better choice?
            if(best && !(cost < bestCost)) return false; // Stop iteration.

            // Evaluate the cost delta for this hedge.
            PartitionCost costDelta;
            evalPartitionCostForHEdge(hInfo, splitCostFactor, *it, costDelta);

            // Merge cost result into the cummulative total.
            cost += costDelta;
        }

        // Handle sub-blocks recursively.
        if(block.hasRight())
        {
            bool unsuitable = !evalPartitionCostForSuperBlock(*block.right(), splitCostFactor,
                                                              best, bestCost, hedge, cost);
            if(unsuitable) return false;
        }

        if(block.hasLeft())
        {
            bool unsuitable = !evalPartitionCostForSuperBlock(*block.left(), splitCostFactor,
                                                              best, bestCost, hedge, cost);
            if(unsuitable) return false;
        }

        // This is a suitable candidate.
        return true;
    }

    /**
     * Evaluate a partition and determine the cost, taking into account the
     * number of splits and the difference between left and right.
     *
     * To be able to divide the nodes down, evalPartition must decide which
     * is the best half-edge to use as a nodeline. It does this by selecting
     * the line with least splits and has least difference of hald-edges on
     * either side of it.
     *
     * @return  @c true iff this half-edge is suitable for use as a partition.
     */
    bool evalPartition(const SuperBlock& block, int splitCostFactor,
        HEdge* best, const PartitionCost& bestCost, HEdge* hedge, PartitionCost& cost)
    {
        if(!hedge) return false;

        // "Mini-hedges" are never potential candidates.
        LineDef* lineDef = hedge->lineDef;
        if(!lineDef) return false;

        if(!evalPartitionCostForSuperBlock(block, splitCostFactor, best, bestCost, hedge, cost))
        {
            // Unsuitable or we already have a better choice.
            return false;
        }

        // Make sure there is at least one real half-edge on each side.
        if(!cost.realLeft || !cost.realRight)
        {
            //LOG_DEBUG("evalPartition: No real half-edges on %s%sside")
            //    << (cost.realLeft? "" : "left ") << (cost.realRight? "" : "right ");
            return false;
        }

        // Increase cost by the difference between left and right.
        cost.total += 100 * abs(cost.realLeft - cost.realRight);

        // Allow minihedge counts to affect the outcome.
        cost.total += 50 * abs(cost.miniLeft - cost.miniRight);

        // Another little twist, here we show a slight preference for partition
        // lines that lie either purely horizontally or purely vertically.
        const HEdgeInfo& hInfo = hedgeInfo(*hedge);
        if(!FEQUAL(hInfo.pDX, 0) && !FEQUAL(hInfo.pDY, 0))
            cost.total += 25;

        //LOG_DEBUG("evalPartition: %p: splits=%d iffy=%d near=%d left=%d+%d right=%d+%d cost=%d.%02d")
        //    << &hInfo << cost.splits << cost.iffy << cost.nearMiss
        //    << cost.realLeft << cost.miniLeft << cost.realRight << cost.miniRight
        //    << cost.total / 100 << cost.total % 100;

        return true;
    }

    void chooseNextPartitionFromSuperBlock(const SuperBlock& partList, const SuperBlock& hedgeList,
        HEdge** best, PartitionCost& bestCost)
    {
        Q_ASSERT(best);

        // Test each half-edge as a potential partition.
        for(SuperBlock::HEdges::const_iterator it = partList.hedgesBegin();
            it != partList.hedgesEnd(); ++it)
        {
            HEdge* hedge = *it;

            //LOG_DEBUG("chooseNextPartitionFromSuperBlock: %shedge %p sector:%d [%1.1f, %1.1f] -> [%1.1f, %1.1f]")
            //    << (lineDef? "" : "mini-") << hedge
            //    << (hedge->bspBuildInfo->sector? hedge->bspBuildInfo->sector->index : -1)
            //    << hedge->v[0]->V_pos[VX] << hedge->v[0]->V_pos[VY]
            //    << hedge->v[1]->V_pos[VX] << hedge->v[1]->V_pos[VY];

            // Only test half-edges from the same linedef once per round of
            // partition picking (they are collinear).
            if(hedge->lineDef)
            {
                LineDefInfo& lInfo = lineDefInfo(*hedge->lineDef);
                if(lInfo.validCount == validCount) continue;
                lInfo.validCount = validCount;
            }

            // Calculate the cost metrics for this half-edge.
            PartitionCost cost;
            if(evalPartition(hedgeList, splitCostFactor, *best, bestCost, hedge, cost))
            {
                // Suitable for use as a partition.
                if(!*best || cost < bestCost)
                {
                    // We have a new better choice.
                    bestCost = cost;

                    // Remember which half-edge.
                    *best = hedge;
                }
            }
        }
    }

    /**
     * Find the best half-edge in the list to use as the next partition.
     *
     * @param hedgeList     List of half-edges to choose from.
     * @return  The chosen half-edge.
     */
    HEdge* chooseNextPartition(const SuperBlock& hedgeList)
    {
        PartitionCost bestCost;
        HEdge* best = 0;

        // Increment valid count so we can avoid testing the half edges produced
        // from a single linedef more than once per round of partition selection.
        validCount++;

        // Iterative pre-order traversal of SuperBlock.
        const SuperBlock* cur = &hedgeList;
        const SuperBlock* prev = 0;
        while(cur)
        {
            while(cur)
            {
                chooseNextPartitionFromSuperBlock(*cur, hedgeList, &best, bestCost);

                if(prev == cur->parent())
                {
                    // Descending - right first, then left.
                    prev = cur;
                    if(cur->hasRight()) cur = cur->right();
                    else                cur = cur->left();
                }
                else if(prev == cur->right())
                {
                    // Last moved up the right branch - descend the left.
                    prev = cur;
                    cur = cur->left();
                }
                else if(prev == cur->left())
                {
                    // Last moved up the left branch - continue upward.
                    prev = cur;
                    cur = cur->parent();
                }
            }

            if(prev)
            {
                // No left child - back up.
                cur = prev->parent();
            }
        }

        /*if(best)
        {
            LOG_DEBUG("Partitioner::choosePartition: best %p score: %d.%02d.")
                << best << bestCost.total / 100 << bestCost.total % 100;
        }*/

        return best;
    }

    /**
     * Takes the half-edge list and determines if it is convex, possibly converting
     * it into a BSP leaf. Otherwise, the list is divided into two halves and recursion
     * will continue on the new sub list.
     *
     * This is done by scanning all of the half-edges and finding the one that does
     * the least splitting and has the least difference in numbers of half-edges on
     * either side.
     *
     * If the ones on the left side make a BspLeaf, then create another BspLeaf
     * else put the half-edges into the left list.
     *
     * If the ones on the right side make a BspLeaf, then create another BspLeaf
     * else put the half-edges into the right list.
     *
     * @param superblock    The list of half edges at the current node.
     * @param subtree       Ptr to write back the address of any newly created subtree.
     * @return  @c true iff successfull.
     */
    bool buildNodes(SuperBlock& hedgeList, BspTreeNode** subtree)
    {
        *subtree = NULL;

        //hedgeList.traverse(printSuperBlockHEdgesWorker);

        // Pick a half-edge to use as the next partition plane.
        HEdge* hedge = chooseNextPartition(hedgeList);
        if(!hedge)
        {
            // No partition required, already convex.
            //LOG_TRACE("Partitioner::buildNodes: Convex.");

            BspLeaf* leaf = newBspLeaf(hedgeList);
            *subtree = new BspTreeNode(reinterpret_cast<runtime_mapdata_header_t*>(leaf));
            return true;
        }

        //LOG_TRACE("Partitioner::buildNodes: Partition %p [%1.0f, %1.0f] -> [%1.0f, %1.0f].")
        //      << hedge << hedge->v[0]->V_pos[VX] << hedge->v[0]->V_pos[VY]
        //      << hedge->v[1]->V_pos[VX] << hedge->v[1]->V_pos[VY];

        // Reconfigure the half plane for the next round of hedge sorting.
        configurePartition(hedge);

        // Create left and right super blockmaps.
        /// @todo There should be no need to construct entirely independent
        ///       data structures to contain these hedge subsets.
        // Copy the bounding box of the edge list to the superblocks.
        SuperBlockmap* rightHEdges = new SuperBlockmap(hedgeList.bounds());
        SuperBlockmap* leftHEdges  = new SuperBlockmap(hedgeList.bounds());

        // Divide the half-edges into two lists: left & right.
        partitionHEdges(hedgeList, rightHEdges->root(), leftHEdges->root());

        addMiniHEdges(rightHEdges->root(), leftHEdges->root());

        clearPartitionIntercepts();

        AABoxf rightHEdgesBounds, leftHEdgesBounds;
        rightHEdges->findHEdgeBounds(rightHEdgesBounds);
        leftHEdges->findHEdgeBounds(leftHEdgesBounds);

        BspNode* node = newBspNode(partition->origin(), partition->angle(),
                                   &rightHEdgesBounds, &leftHEdgesBounds);

        *subtree = new BspTreeNode(reinterpret_cast<runtime_mapdata_header_t*>(node));

        // Recurse on the right subset.
        BspTreeNode* child;
        bool builtOK = buildNodes(rightHEdges->root(), &child);
        (*subtree)->setRight(child);
        if((*subtree)->hasRight())
        {
            (*subtree)->right()->setParent(*subtree);

            // Link the BSP object too.
            BspNode_SetRight(node, (*subtree)->right()->userData());
        }
        delete rightHEdges;

        if(builtOK)
        {
            // Recurse on the left subset.
            builtOK = buildNodes(leftHEdges->root(), &child);
            (*subtree)->setLeft(child);
            if((*subtree)->hasLeft())
            {
                (*subtree)->left()->setParent(*subtree);

                // Link the BSP object too.
                BspNode_SetLeft(node, (*subtree)->left()->userData());
            }
        }

        delete leftHEdges;

        return builtOK;
    }

    bool build()
    {
        if(!map) return false;

        initForMap();

        // Find maximal vertexes.
        AABoxf mapBounds;
        findMapBounds(&mapBounds);

        LOG_VERBOSE("Map bounds:")
                << " min[x:" << mapBounds.minX << ", y:" << mapBounds.minY << "]"
                << " max[x:" << mapBounds.maxX << ", y:" << mapBounds.maxY << "]";

        AABox mapBoundsi;
        mapBoundsi.minX = (int) floor(mapBounds.minX);
        mapBoundsi.minY = (int) floor(mapBounds.minY);
        mapBoundsi.maxX = (int)  ceil(mapBounds.maxX);
        mapBoundsi.maxY = (int)  ceil(mapBounds.maxY);

        AABox blockBounds;
        blockBounds.minX = mapBoundsi.minX - (mapBoundsi.minX & 0x7);
        blockBounds.minY = mapBoundsi.minY - (mapBoundsi.minY & 0x7);
        int bw = ((mapBoundsi.maxX - blockBounds.minX) / 128) + 1;
        int bh = ((mapBoundsi.maxY - blockBounds.minY) / 128) + 1;

        blockBounds.maxX = blockBounds.minX + 128 * M_CeilPow2(bw);
        blockBounds.maxY = blockBounds.minY + 128 * M_CeilPow2(bh);

        SuperBlockmap* blockmap = new SuperBlockmap(blockBounds);
        partition = new HPlane();

        initHEdgesAndBuildBsp(*blockmap);

        delete partition;
        delete blockmap;

        return builtOK;
    }

    /**
     * Determine the distance (euclidean) from @a vertex to the current partition plane.
     *
     * @param vertex  Vertex to test.
     */
    inline double vertexDistanceFromPartition(const Vertex* vertex) const
    {
        Q_ASSERT(vertex);
        const HEdgeInfo& info = partitionInfo;
        return M_ParallelDist(info.pDX, info.pDY, info.pPara, info.pLength,
                              vertex->buildData.pos[VX], vertex->buildData.pos[VY]);
    }

    /**
     * Determine the distance (euclidean) from @a hedge to the current partition plane.
     *
     * @param hedge  Half-edge to test.
     * @param end    @c true= use the point defined by the end (else start) vertex.
     */
    inline double hedgeDistanceFromPartition(const HEdge* hedge, bool end) const
    {
        Q_ASSERT(hedge);
        const HEdgeInfo& pInfo = partitionInfo;
        const HEdgeInfo& hInfo = hedgeInfo(*hedge);
        return M_PerpDist(pInfo.pDX, pInfo.pDY, pInfo.pPerp, pInfo.pLength,
                          end? hInfo.pEX : hInfo.pSX, end? hInfo.pEY : hInfo.pSY);
    }

    /**
     * Calculate the intersection point between a half-edge and the current partition
     * plane. Takes advantage of some common situations like horizontal and vertical
     * lines to choose a 'nicer' intersection point.
     */
    void interceptHEdgePartition(const HEdge* hedge, double perpC, double perpD,
        pvec2d_t point) const
    {
        if(!hedge || !point) return;

        const HEdgeInfo& hInfo = hedgeInfo(*hedge);

        // Horizontal partition against vertical half-edge.
        if(partitionInfo.pDY == 0 && hInfo.pDX == 0)
        {
            V2d_Set(point, hInfo.pSX, partitionInfo.pSY);
            return;
        }

        // Vertical partition against horizontal half-edge.
        if(partitionInfo.pDX == 0 && hInfo.pDY == 0)
        {
            V2d_Set(point, partitionInfo.pSX, hInfo.pSY);
            return;
        }

        // 0 = start, 1 = end.
        double ds = perpC / (perpC - perpD);

        if(hInfo.pDX == 0)
            point[VX] = hInfo.pSX;
        else
            point[VX] = hInfo.pSX + (hInfo.pDX * ds);

        if(hInfo.pDY == 0)
            point[VY] = hInfo.pSY;
        else
            point[VY] = hInfo.pSY + (hInfo.pDY * ds);
    }

    void clearPartitionIntercepts()
    {
        for(HPlane::Intercepts::iterator it = partition->begin(); it != partition->end(); ++it)
        {
            HEdgeIntercept* intercept = static_cast<HEdgeIntercept*>((*it).userData());
            deleteHEdgeIntercept(*intercept);
        }
        partition->clear();
    }

    bool configurePartition(const HEdge* hedge)
    {
        if(!hedge) return false;

        LineDef* lineDef = hedge->lineDef;
        if(!lineDef) return false; // A "mini hedge" is not suitable.

        // Clear the HEdge intercept data associated with points in the half-plane.
        clearPartitionIntercepts();

        setPartitionInfo(hedgeInfo(*hedge), lineDef);

        // We can now reconfire the half-plane itself.

        const Vertex* from = lineDef->L_v(hedge->side);
        const Vertex* to   = lineDef->L_v(hedge->side^1);
        partition->setOrigin(from->buildData.pos);

        vec2d_t angle; V2d_Subtract(angle, to->buildData.pos, from->buildData.pos);
        partition->setAngle(angle);

        //LOG_DEBUG("Partitioner::configureHPlane: hedge %p [%1.1f, %1.1f] -> [%1.1f, %1.1f].")
        //    << best << from->buildData.pos[VX] << from->buildData.pos[VY]
        //    << angle[VX] << angle[VY];

        return true;
    }

    /**
     * Sort half-edges by angle (from the middle point to the start vertex).
     * The desired order (clockwise) means descending angles.
     */
    static void sortHEdgesByAngleAroundPoint(HEdgeSortBuffer::iterator begin,
        HEdgeSortBuffer::iterator end, pvec2d_t point)
    {
        Q_ASSERT(point);

        /// @algorithm "double bubble"
        bool done = false;
        while(begin != end && !done)
        {
            done = true;
            HEdgeSortBuffer::iterator it(begin);
            HEdgeSortBuffer::iterator next(begin);
            ++next;
            while(next != end)
            {
                HEdge* a = *it;
                HEdge* b = *next;
                double angle1 = M_SlopeToAngle(a->v[0]->buildData.pos[VX] - point[VX],
                                               a->v[0]->buildData.pos[VY] - point[VY]);
                double angle2 = M_SlopeToAngle(b->v[0]->buildData.pos[VX] - point[VX],
                                               b->v[0]->buildData.pos[VY] - point[VY]);

                if(angle1 + ANG_EPSILON < angle2)
                {
                    // Swap them.
                    std::swap(*next, *it);
                    done = false;
                }

                // Bubble down.
                ++it;
                ++next;
            }

            // Bubble up.
            --end;
        }
    }

    /**
     * Sort the given list of half-edges into clockwise order based on their
     * position/orientation compared to the specified point.
     *
     * @param headPtr       Ptr to the address of the headPtr to the list
     *                      of hedges to be sorted.
     * @param num           Number of half edges in the list.
     * @param point         The point to order around.
     */
    static void clockwiseOrder(HEdgeSortBuffer& sortBuffer, HEdge** headPtr,
        uint num, pvec2d_t point)
    {
        // Ensure the sort buffer is large enough.
        if(num > sortBuffer.size())
        {
            sortBuffer.resize(num);
        }

        // Insert the hedges into the sort buffer.
        uint i = 0;
        for(HEdge* hedge = *headPtr; hedge; hedge = hedge->next, ++i)
        {
            sortBuffer[i] = hedge;
        }

        HEdgeSortBuffer::iterator begin = sortBuffer.begin();
        HEdgeSortBuffer::iterator end = begin + num;
        sortHEdgesByAngleAroundPoint(begin, end, point);

        // Re-link the half-edge list in the order of the sorted array.
        *headPtr = NULL;
        for(uint i = 0; i < num; ++i)
        {
            uint idx = (num - 1) - i;
            uint j = idx % num;

            sortBuffer[j]->next = *headPtr;
            *headPtr = sortBuffer[j];
        }

        /*
        LOG_DEBUG("Sorted half-edges around [%1.1f, %1.1f]" << point[VX] << point[VY];
        for(hedge = *headPtr; hedge; hedge = hedge->next)
        {
            double angle = M_SlopeToAngle(hedge->v[0]->V_pos[VX] - point[VX],
                                          hedge->v[0]->V_pos[VY] - point[VY]);

            LOG_DEBUG("  half-edge %p: Angle %1.6f [%1.1f, %1.1f] -> [%1.1f, %1.1f]")
                << hedge << angle
                << hedge->v[0]->V_pos[VX] << hedge->v[0]->V_pos[VY]
                << hedge->v[1]->V_pos[VX] << hedge->v[1]->V_pos[VY];
        }
        */
    }

    void clockwiseLeaf(BspTreeNode& tree, HEdgeSortBuffer& sortBuffer)
    {
        Q_ASSERT(tree.isLeaf());

        BspLeaf* leaf = reinterpret_cast<BspLeaf*>(tree.userData());
        vec2d_t midPoint;

        V2d_Set(midPoint, 0, 0);
        getAveragedCoords(leaf, midPoint);

        // Count half-edges.
        leaf->hedgeCount = 0;
        for(HEdge* hedge = leaf->hedge; hedge; hedge = hedge->next)
            leaf->hedgeCount++;

        clockwiseOrder(sortBuffer, &leaf->hedge, leaf->hedgeCount, midPoint);

        if(leaf->hedge)
        {
            /// @todo Construct the leaf's hedge ring as we go.
            HEdge* hedge;
            for(hedge = leaf->hedge; ;)
            {
                /// @kludge This should not be done here!
                if(hedge->lineDef)
                {
                    SideDef* side = HEDGE_SIDEDEF(hedge);
                    // Already processed?
                    if(side && !side->hedgeLeft)
                    {
                        side->hedgeLeft = hedge;
                        // Find the left-most hedge.
                        while(hedgeInfo(*side->hedgeLeft).prevOnSide)
                            side->hedgeLeft = hedgeInfo(*side->hedgeLeft).prevOnSide;

                        // Find the right-most hedge.
                        side->hedgeRight = hedge;
                        while(hedgeInfo(*side->hedgeRight).nextOnSide)
                            side->hedgeRight = hedgeInfo(*side->hedgeRight).nextOnSide;
                    }
                }
                /// kludge end

                if(hedge->next)
                {
                    // Reverse link.
                    hedge->next->prev = hedge;
                    hedge = hedge->next;
                }
                else
                {
                    // Circular link.
                    hedge->next = leaf->hedge;
                    hedge->next->prev = hedge;
                    break;
                }
            }

            // Determine which sector this BSP leaf belongs to.
            hedge = leaf->hedge;
            do
            {
                if(hedge->lineDef && hedge->lineDef->sideDefs[hedge->side])
                {
                    SideDef* side = hedge->lineDef->sideDefs[hedge->side];
                    leaf->sector = side->sector;
                }
            } while(!leaf->sector && (hedge = hedge->next) != leaf->hedge);
        }

        if(!leaf->sector)
        {
            LOG_WARNING("BspLeaf %p is orphan.") << leaf;
        }

        if(verbose)
        {
            registerMigrantHEdges(leaf);
            logUnclosed(leaf);
        }

        if(!sanityCheckHasRealHEdge(leaf))
        {
            Con_Error("BSP Leaf #%p has no linedef-linked half-edge!", leaf);
        }
    }

    /**
     * Traverse the BSP tree and put all the half-edges in each BSP leaf into clockwise
     * order, and renumber their indices.
     *
     * @important This cannot be done during BspBuilder::buildNodes() since splitting
     * a half-edge with a twin may insert another half-edge into that twin's list,
     * usually in the wrong place order-wise.
     */
    void windLeafs()
    {
        HEdgeSortBuffer sortBuffer;

        // Iterative pre-order traversal of the BSP tree.
        BspTreeNode* cur = rootNode;
        BspTreeNode* prev = 0;
        while(cur)
        {
            while(cur)
            {
                if(cur->isLeaf())
                {
                    clockwiseLeaf(*cur, sortBuffer);
                }

                if(prev == cur->parent())
                {
                    // Descending - right first, then left.
                    prev = cur;
                    if(cur->hasRight()) cur = cur->right();
                    else                cur = cur->left();
                }
                else if(prev == cur->right())
                {
                    // Last moved up the right branch - descend the left.
                    prev = cur;
                    cur = cur->left();
                }
                else if(prev == cur->left())
                {
                    // Last moved up the left branch - continue upward.
                    prev = cur;
                    cur = cur->parent();
                }
            }

            if(prev)
            {
                // No left child - back up.
                cur = prev->parent();
            }
        }
    }

    void addHEdgesBetweenIntercepts(HEdgeIntercept* start, HEdgeIntercept* end,
        HEdge** right, HEdge** left)
    {
        Q_ASSERT(start && end);

        // Create the half-edge pair.
        // Leave 'linedef' field as NULL as these are not linedef-linked.
        // Leave 'side' as zero too.
        (*right) = newHEdge(NULL, partitionLineDef, start->vertex, end->vertex, start->after, false);
        ( *left) = newHEdge(NULL, partitionLineDef, end->vertex, start->vertex, start->after, false);

        // Twin the half-edges together.
        (*right)->twin = *left;
        ( *left)->twin = *right;

        /*
        DEBUG_Message(("buildHEdgesBetweenIntersections: Capped intersection:\n"));
        DEBUG_Message(("  %p RIGHT sector %d (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                       (*right), ((*right)->sector? (*right)->sector->index : -1),
                       (*right)->v[0]->V_pos[VX], (*right)->v[0]->V_pos[VY],
                       (*right)->v[1]->V_pos[VX], (*right)->v[1]->V_pos[VY]));
        DEBUG_Message(("  %p LEFT  sector %d (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                       (*left), ((*left)->sector? (*left)->sector->index : -1),
                       (*left)->v[0]->V_pos[VX], (*left)->v[0]->V_pos[VY],
                       (*left)->v[1]->V_pos[VX], (*left)->v[1]->V_pos[VY]));
        */
    }

    /**
     * Analyze the intersection list, and add any needed minihedges to the given
     * half-edge lists (one minihedge on each side).
     */
    void addMiniHEdges(SuperBlock& rightList, SuperBlock& leftList)
    {
        //DEND_DEBUG_ONLY(HPlane::DebugPrint(*partition));

        // Fix any issues with the current intersections.
        mergeIntersections();

        LOG_TRACE("Building HEdges along partition [%1.1f, %1.1f] > [%1.1f, %1.1f]")
                << partitionInfo.pSX << partitionInfo.pSY << partitionInfo.pDX << partitionInfo.pDY;

        // Find connections in the intersections.
        buildHEdgesAtIntersectionGaps(rightList, leftList);
    }

    /**
     * Search the given list for an intercept, if found; return it.
     *
     * @param list  The list to be searched.
     * @param vert  Ptr to the vertex to look for.
     *
     * @return  Ptr to the found intercept, else @c NULL;
     */
    const HPlaneIntercept* partitionInterceptByVertex(Vertex* vertex)
    {
        if(!vertex) return NULL; // Hmm...

        for(HPlane::Intercepts::const_iterator it = partition->begin(); it != partition->end(); ++it)
        {
            const HPlaneIntercept* inter = &*it;
            if(reinterpret_cast<HEdgeIntercept*>(inter->userData())->vertex == vertex) return inter;
        }

        return NULL;
    }

    /**
     * Create a new intersection.
     */
    HEdgeIntercept* newHEdgeIntercept(Vertex* vertex, bool lineDefIsSelfReferencing)
    {
        Q_ASSERT(vertex);

        HEdgeIntercept* inter = new HEdgeIntercept();
        inter->vertex = vertex;
        inter->selfRef = lineDefIsSelfReferencing;

        inter->before = openSectorAtAngle(vertex, M_SlopeToAngle(-partitionInfo.pDX, -partitionInfo.pDY));
        inter->after  = openSectorAtAngle(vertex, M_SlopeToAngle( partitionInfo.pDX,  partitionInfo.pDY));

        return inter;
    }

    /**
     * Destroy the specified intersection.
     *
     * @param inter  Ptr to the intersection to be destroyed.
     */
    void deleteHEdgeIntercept(HEdgeIntercept& intercept)
    {
        delete &intercept;
    }

    HEdgeIntercept* hedgeInterceptByVertex(Vertex* vertex)
    {
        const HPlaneIntercept* hpi = partitionInterceptByVertex(vertex);
        if(!hpi) return NULL; // Not found.
        return reinterpret_cast<HEdgeIntercept*>(hpi->userData());
    }

    void clearBspObject(BspTreeNode& tree)
    {
        if(tree.isLeaf())
        {
            BspLeaf* leaf = reinterpret_cast<BspLeaf*>(tree.userData());
            if(leaf)
            {
                LOG_DEBUG("Partitioner: Clearing unclaimed leaf %p.") << leaf;
                BspLeaf_Delete(leaf);
                // There is now one less BspLeaf.
                numLeafs -= 1;
            }
        }
        else
        {
            BspNode* node = reinterpret_cast<BspNode*>(tree.userData());
            if(node)
            {
                LOG_DEBUG("Partitioner: Clearing unclaimed node %p.") << node;
                BspNode_Delete(node);
                // There is now one less BspNode.
                numNodes -= 1;
            }
        }
    }

    void clearAllBspObjects()
    {
        // Iterative pre-order traversal of the BSP tree.
        BspTreeNode* cur = rootNode;
        BspTreeNode* prev = 0;
        while(cur)
        {
            while(cur)
            {
                clearBspObject(*cur);

                if(prev == cur->parent())
                {
                    // Descending - right first, then left.
                    prev = cur;
                    if(cur->hasRight()) cur = cur->right();
                    else                cur = cur->left();
                }
                else if(prev == cur->right())
                {
                    // Last moved up the right branch - descend the left.
                    prev = cur;
                    cur = cur->left();
                }
                else if(prev == cur->left())
                {
                    // Last moved up the left branch - continue upward.
                    prev = cur;
                    cur = cur->parent();
                }
            }

            if(prev)
            {
                // No left child - back up.
                cur = prev->parent();
            }
        }
    }

    BspTreeNode* treeNodeForBspObject(runtime_mapdata_header_t const& ob)
    {
        if(DMU_GetType(&ob) == DMU_BSPLEAF || DMU_GetType(&ob) == DMU_BSPNODE)
        {
            // Iterative pre-order traversal of the BSP tree.
            BspTreeNode* cur = rootNode;
            BspTreeNode* prev = 0;
            while(cur)
            {
                while(cur)
                {
                    if(cur->isLeaf() && DMU_GetType(&ob) == DMU_BSPLEAF)
                    {
                        const BspLeaf* leaf = reinterpret_cast<BspLeaf const*>(&ob);
                        if(cur->userData() && leaf == reinterpret_cast<BspLeaf*>(cur->userData()))
                        {
                            return cur;
                        }
                    }
                    else if(DMU_GetType(&ob) == DMU_BSPNODE)
                    {
                        const BspNode* node = reinterpret_cast<BspNode const*>(&ob);
                        if(cur->userData() && node == reinterpret_cast<BspNode*>(cur->userData()))
                        {
                            return cur;
                        }
                    }

                    if(prev == cur->parent())
                    {
                        // Descending - right first, then left.
                        prev = cur;
                        if(cur->hasRight()) cur = cur->right();
                        else                cur = cur->left();
                    }
                    else if(prev == cur->right())
                    {
                        // Last moved up the right branch - descend the left.
                        prev = cur;
                        cur = cur->left();
                    }
                    else if(prev == cur->left())
                    {
                        // Last moved up the left branch - continue upward.
                        prev = cur;
                        cur = cur->parent();
                    }
                }

                if(prev)
                {
                    // No left child - back up.
                    cur = prev->parent();
                }
            }
        }

        LOG_DEBUG("Partitioner::treeNodeForBspObject: Attempted to locate using an unknown object %p.") << &ob;
        return 0;
    }

    HEdgeTip* newHEdgeTip()
    {
        HEdgeTip* tip = new HEdgeTip();
        return tip;
    }

    void deleteHEdgeTip(HEdgeTip* tip)
    {
        Q_ASSERT(tip);
        delete tip;
    }

    void clearHEdgeTipsByVertex(Vertex* vtx)
    {
        if(!vtx) return;

        HEdgeTip* tip = vertexInfo(*vtx).tipSet;
        while(tip)
        {
            HEdgeTip* next = tip->ET_next;
            deleteHEdgeTip(tip);
            tip = next;
        }
        vertexInfo(*vtx).tipSet = 0;
    }

    /**
     * Allocate another Vertex.
     *
     * @param point  Origin of the vertex in the map coordinate space.
     * @return  Newly created Vertex.
     */
    Vertex* newVertex(const_pvec2d_t point)
    {
        Vertex* vtx;

        // Allocate with calloc for uniformity with the editable vertexes.
        vtx = static_cast<Vertex*>(calloc(1, sizeof *vtx));
        vtx->header.type = DMU_VERTEX;
        vtx->buildData.index = *numEditableVertexes + uint(vertexes.size() + 1); // 1-based index, 0 = NIL.
        vertexes.push_back(vtx);

        // There is now one more Vertex.
        numVertexes += 1;
        vertexInfos.push_back(VertexInfo());

        if(point)
        {
            V2d_Copy(vtx->buildData.pos, point);
        }
        return vtx;
    }

    void addHEdgeTip(Vertex* vtx, double angle, HEdge* back, HEdge* front)
    {
        Q_ASSERT(vtx);

        HEdgeTip* tip = newHEdgeTip();
        tip->angle = angle;
        tip->ET_back  = back;
        tip->ET_front = front;

        // Find the correct place (order is increasing angle).
        VertexInfo& vtxInfo = vertexInfo(*vtx);
        HEdgeTip* after;
        for(after = vtxInfo.tipSet; after && after->ET_next;
            after = after->ET_next) {}

        while(after && tip->angle + ANG_EPSILON < after->angle)
            after = after->ET_prev;

        // Link it in.
        if(after)
            tip->ET_next = after->ET_next;
        else
            tip->ET_next = vtxInfo.tipSet;
        tip->ET_prev = after;

        if(after)
        {
            if(after->ET_next)
                after->ET_next->ET_prev = tip;

            after->ET_next = tip;
        }
        else
        {
            if(vtxInfo.tipSet)
                vtxInfo.tipSet->ET_prev = tip;

            vtxInfo.tipSet = tip;
        }
    }

    /**
     * Create a new half-edge.
     */
    HEdge* newHEdge(LineDef* lineDef, LineDef* sourceLineDef, Vertex* start, Vertex* end,
        Sector* sec, bool back)
    {
        HEdge* hedge = HEdge_New();

        hedge->v[0] = start;
        hedge->v[1] = end;
        hedge->sector = sec;
        Q_ASSERT(sec == NULL || GameMap_SectorIndex(map, sec) >= 0);
        hedge->side = (back? 1 : 0);
        hedge->lineDef = lineDef;

        // There is now one more HEdge.
        hedge->buildData.index = numHEdges += 1;
        hedgeInfos.push_back(HEdgeInfo());

        HEdgeInfo& info = hedgeInfo(*hedge);
        info.sourceLineDef = sourceLineDef;
        info.initFromHEdge(*hedge);

        return hedge;
    }

    /**
     * Create a clone of an existing half-edge.
     */
    HEdge* cloneHEdge(const HEdge& other)
    {
        HEdge* hedge = HEdge_NewCopy(&other);

        // There is now one more HEdge.
        hedge->buildData.index = numHEdges += 1;
        hedgeInfos.push_back(HEdgeInfo());

        memcpy(&hedgeInfo(*hedge), &hedgeInfo(other), sizeof(HEdgeInfo));

        return hedge;
    }

    /**
     * Allocate another BspLeaf and populate it with half-edges from the supplied list.
     *
     * @param hedgeList  SuperBlock containing the list of half-edges with which
     *                   to build the leaf using.
     * @return  Newly created BspLeaf.
     */
    BspLeaf* newBspLeaf(SuperBlock& hedgeList)
    {
        BspLeaf* leaf = BspLeaf_New();

        // Iterative pre-order traversal of SuperBlock.
        SuperBlock* cur = &hedgeList;
        SuperBlock* prev = 0;
        while(cur)
        {
            while(cur)
            {
                HEdge* hedge;
                while((hedge = cur->pop()))
                {
                    // Disassociate the half-edge from the blockmap.
                    hedgeInfo(*hedge).bmapBlock = 0;

                    // Link it into head of the leaf's list.
                    hedge->next = leaf->hedge;
                    leaf->hedge = hedge;

                    // Link hedge to this leaf.
                    hedge->bspLeaf = leaf;
                }

                if(prev == cur->parent())
                {
                    // Descending - right first, then left.
                    prev = cur;
                    if(cur->hasRight()) cur = cur->right();
                    else                cur = cur->left();
                }
                else if(prev == cur->right())
                {
                    // Last moved up the right branch - descend the left.
                    prev = cur;
                    cur = cur->left();
                }
                else if(prev == cur->left())
                {
                    // Last moved up the left branch - continue upward.
                    prev = cur;
                    cur = cur->parent();
                }
            }

            if(prev)
            {
                // No left child - back up.
                cur = prev->parent();
            }
        }

        // There is now one more BspLeaf;
        numLeafs += 1;
        return leaf;
    }

    /**
     * Allocate another BspNode.
     *
     * @param origin  Origin of the half-plane in the map coordinate space.
     * @param angle  Angle of the half-plane in the map coordinate space.
     * @param rightBounds  Boundary of the right child map coordinate subspace. Can be @c NULL.
     * @param leftBoubds   Boundary of the left child map coordinate subspace. Can be @a NULL.
     *
     * @return  Newly created BspNode.
     */
    BspNode* newBspNode(double const origin[2], double const angle[2],
        AABoxf* rightBounds, AABoxf* leftBounds)
    {
        BspNode* node = BspNode_New(origin, angle);

        if(rightBounds)
        {
            BspNode_SetRightBounds(node, rightBounds);
        }

        if(leftBounds)
        {
            BspNode_SetLeftBounds(node, leftBounds);
        }

        // There is now one more BspNode.
        numNodes += 1;
        return node;
    }

    /**
     * Check whether a line with the given delta coordinates and beginning at this
     * vertex is open. Returns a sector reference if it's open, or NULL if closed
     * (void space or directly along a linedef).
     */
    Sector* openSectorAtAngle(Vertex* vtx, double angle)
    {
        Q_ASSERT(vtx);

        // First check whether there's a wall_tip that lies in the exact direction of
        // the given direction (which is relative to the vtxex).
        for(HEdgeTip* tip = vertexInfo(*vtx).tipSet; tip; tip = tip->ET_next)
        {
            double diff = fabs(tip->angle - angle);

            if(diff < ANG_EPSILON || diff > (360.0 - ANG_EPSILON))
            {
                // Yes, found one.
                return NULL;
            }
        }

        // OK, now just find the first wall_tip whose angle is greater than the angle
        // we're interested in. Therefore we'll be on the FRONT side of that tip edge.
        for(HEdgeTip* tip = vertexInfo(*vtx).tipSet; tip; tip = tip->ET_next)
        {
            if(angle + ANG_EPSILON < tip->angle)
            {
                // Found it.
                return (tip->ET_front? tip->ET_front->sector : NULL);
            }

            if(!tip->ET_next)
            {
                // No more tips, therefore this is the BACK of the tip with the largest angle.
                return (tip->ET_back? tip->ET_back->sector : NULL);
            }
        }

        Con_Error("Vertex %d has no hedge tips !", vtx->buildData.index);
        exit(1); // Unreachable.
    }

    /**
     * Initialize the extra info about the current partition plane.
     */
    void initPartitionInfo()
    {
        memset(&partitionInfo, 0, sizeof(partitionInfo));
        partitionLineDef = 0;
    }

    /**
     * Update the extra info about the current partition plane.
     */
    void setPartitionInfo(const HEdgeInfo& info, LineDef* lineDef)
    {
        memcpy(&partitionInfo, &info, sizeof(partitionInfo));
        partitionLineDef = lineDef;
    }

    /**
     * Register the specified sector in the list of unclosed sectors.
     *
     * @param sector  Sector to be registered.
     * @param x  X coordinate near the open point.
     * @param y  X coordinate near the open point.
     *
     * @return @c true iff the sector was newly registered.
     */
    bool registerUnclosedSector(Sector* sector, double x, double y)
    {
        if(!sector) return false;

        // Has this sector already been registered?
        for(UnclosedSectors::const_iterator it = unclosedSectors.begin();
            it != unclosedSectors.end(); ++it)
        {
            UnclosedSectorRecord const& record = *it;
            if(record.sector == sector)
                return false;
        }

        // Add a new record.
        unclosedSectors.push_back(UnclosedSectorRecord(sector, x, y));

        // In the absence of a better mechanism, simply log this right away.
        /// @todo Implement something better!
        LOG_WARNING("Sector %p #%d is unclosed near [%1.1f, %1.1f].")
                << sector << sector->buildData.index - 1 << x << y;

        return true;
    }

    /**
     * Register the specified half-edge as as migrant edge of the specified sector.
     *
     * @param migrant  Migrant half-edge to register.
     * @param sector  Sector containing the @a migrant half-edge.
     *
     * @return @c true iff the half-edge was newly registered.
     */
    bool registerMigrantHEdge(HEdge* migrant, Sector* sector)
    {
        if(!migrant || !sector) return false;

        // Has this pair already been registered?
        for(MigrantHEdges::const_iterator it = migrantHEdges.begin();
            it != migrantHEdges.end(); ++it)
        {
            MigrantHEdgeRecord const& record = *it;
            if(record.facingSector == sector && record.hedge == migrant)
                return false;
        }

        // Add a new record.
        migrantHEdges.push_back(MigrantHEdgeRecord(migrant, sector));

        // In the absence of a better mechanism, simply log this right away.
        /// @todo Implement something better!
        if(migrant->lineDef)
            LOG_WARNING("Sector #%d has HEdge facing #%d (line #%d).")
                    << sector->buildData.index << migrant->sector->buildData.index
                    << migrant->lineDef->buildData.index;
        else
            LOG_WARNING("Sector #%d has HEdge facing #%d.")
                    << sector->buildData.index << migrant->sector->buildData.index;

        return true;
    }

    /**
     * Look for migrant half-edges in the specified leaf and register them to the
     * list of migrants.
     *
     * @param leaf  BSP leaf to be searched.
     */
    void registerMigrantHEdges(const BspLeaf* leaf)
    {
        if(!leaf) return;

        // Find a suitable half-edge for comparison.
        Sector* sector = findFirstSectorInHEdgeList(leaf);
        if(!sector) return;

        // Log migrants.
        HEdge* hedge = leaf->hedge;
        do
        {
            if(hedge->sector && hedge->sector != sector)
            {
                registerMigrantHEdge(hedge, sector);
            }
        } while((hedge = hedge->next) != leaf->hedge);
    }

    bool sanityCheckHasRealHEdge(const BspLeaf* leaf) const
    {
        Q_ASSERT(leaf);
        HEdge* hedge = leaf->hedge;
        do
        {
            if(hedge->lineDef) return true;
        } while((hedge = hedge->next) != leaf->hedge);
        return false;
    }

    /// HEdge split cost factor.
    int splitCostFactor;

    /// Current map which we are building BSP data for.
    GameMap* map;

    /// @todo Refactor me away:
    uint* numEditableVertexes;
    Vertex*** editableVertexes;

    /// Running totals of constructed BSP data objects.
    uint numNodes;
    uint numLeafs;
    uint numHEdges;
    uint numVertexes;

    /// Extended info about LineDefs in the current map.
    typedef std::vector<LineDefInfo> LineDefInfos;
    LineDefInfos lineDefInfos;

    /// Extended info about HEdges in the BSP object tree.
    typedef std::vector<HEdgeInfo> HEdgeInfos;
    HEdgeInfos hedgeInfos;

    /// Extended info about Vertexes in the current map (including extras).
    /// @note May be larger than Instance::numVertexes (deallocation is lazy).
    typedef std::vector<VertexInfo> VertexInfos;
    VertexInfos vertexInfos;

    /// Extra vertexes allocated for the current map.
    /// @note May be larger than Instance::numVertexes (deallocation is lazy).
    typedef std::vector<Vertex*> Vertexes;
    Vertexes vertexes;

    /// Root node of our internal binary tree around which the final BSP data
    /// objects are constructed.
    BspTreeNode* rootNode;

    /// HPlane used to model the current BSP partition and the list of intercepts.
    HPlane* partition;

    /// Extra info about the partition plane.
    HEdgeInfo partitionInfo;
    LineDef* partitionLineDef;

    /// Unclosed sectors are recorded here so we don't print too many warnings.
    struct UnclosedSectorRecord
    {
        Sector* sector;
        vec2d_t nearPoint;

        UnclosedSectorRecord(Sector* _sector, double x, double y)
            : sector(_sector)
        {
            V2d_Set(nearPoint, x, y);
        }
    };
    typedef std::list<UnclosedSectorRecord> UnclosedSectors;
    UnclosedSectors unclosedSectors;

    /// Migrant hedges are recorded here so we don't print too many warnings.
    struct MigrantHEdgeRecord
    {
        HEdge* hedge;
        Sector* facingSector;

        MigrantHEdgeRecord(HEdge* _hedge, Sector* _facingSector)
            : hedge(_hedge), facingSector(_facingSector)
        {}
    };
    typedef std::list<MigrantHEdgeRecord> MigrantHEdges;
    MigrantHEdges migrantHEdges;

    /// @c true = a BSP for the current map has been built successfully.
    bool builtOK;
};

Partitioner::Partitioner(GameMap* map, uint* numEditableVertexes,
    Vertex*** editableVertexes, int splitCostFactor)
{
    d = new Instance(map, numEditableVertexes, editableVertexes, splitCostFactor);
}

Partitioner::~Partitioner()
{
    delete d;
}

Partitioner& Partitioner::setSplitCostFactor(int factor)
{
    d->splitCostFactor = factor;
    return *this;
}

bool Partitioner::build()
{
    return d->build();
}

BspTreeNode* Partitioner::root() const
{
    return d->rootNode;
}

uint Partitioner::numNodes()
{
    return d->numNodes;
}

uint Partitioner::numLeafs()
{
    return d->numLeafs;
}

uint Partitioner::numHEdges()
{
    return d->numHEdges;
}

uint Partitioner::numVertexes()
{
    return d->numVertexes;
}

Vertex const& Partitioner::vertex(uint idx)
{
    Q_ASSERT(idx < d->vertexes.size());
    Q_ASSERT(d->vertexes[idx]);
    return *d->vertexes[idx];
}

Partitioner& Partitioner::releaseOwnership(runtime_mapdata_header_t const& ob)
{
    // Is this object owned?
    if(DMU_GetType(&ob) == DMU_VERTEX)
    {
        const Vertex* vtx = reinterpret_cast<Vertex const*>(&ob);
        if(vtx->buildData.index > 0 && uint(vtx->buildData.index) > *d->numEditableVertexes)
        {
            uint idx = uint(vtx->buildData.index) - 1 - *d->numEditableVertexes;
            if(idx < d->vertexes.size() && d->vertexes[idx])
            {
                d->vertexes[idx] = 0;
                // There is now one fewer Vertex.
                d->numVertexes -= 1;
                return *this;
            }
        }
    }
    else if(DMU_GetType(&ob) == DMU_HEDGE)
    {
        /// @todo Implement a mechanic for tracking HEdge ownership.
        return *this;
    }
    else if(BspTreeNode* treeNode = d->treeNodeForBspObject(ob))
    {
        treeNode->setUserData(0);
        switch(DMU_GetType(&ob))
        {
        case DMU_BSPLEAF:
            // There is now one fewer BspLeaf.
            d->numLeafs -= 1;
            break;
        case DMU_BSPNODE:
            // There is now one fewer BspNode.
            d->numNodes -= 1;
            break;

        default: Q_ASSERT(0);
        }
        return *this;
    }

    LOG_DEBUG("Partitioner::releaseOwnership: Attempted to release an unknown/unowned object %p.") << &ob;
    return *this;
}

static bool getAveragedCoords(BspLeaf* leaf, pvec2d_t midPoint)
{
    if(!leaf || !midPoint) return false;

    vec2d_t avg;
    V2d_Set(avg, 0, 0);
    size_t numPoints = 0;

    for(HEdge* hedge = leaf->hedge; hedge; hedge = hedge->next)
    {
        V2d_Sum(avg, avg, hedge->v[0]->buildData.pos);
        V2d_Sum(avg, avg, hedge->v[1]->buildData.pos);
        numPoints += 2;
    }

    if(numPoints)
    {
        V2d_Set(midPoint, avg[VX] / numPoints, avg[VY] / numPoints);
    }

    return true;
}

static void logUnclosed(const BspLeaf* leaf)
{
    uint total = 0, gaps = 0;
    const HEdge* hedge = leaf->hedge;
    do
    {
        HEdge* next = hedge->next;
        if(hedge->v[1]->buildData.pos[VX] != next->v[0]->buildData.pos[VX] ||
           hedge->v[1]->buildData.pos[VY] != next->v[0]->buildData.pos[VY])
        {
            gaps++;
        }
        total++;

    } while((hedge = hedge->next) != leaf->hedge);

    if(gaps > 0)
    {
        LOG_INFO("HEdge list for BspLeaf #%p is not closed (%u gaps, %u hedges).")
                << leaf << gaps << total;

        /*
        hedge = leaf->hedge;
        do
        {
            LOG_DEBUG("  half-edge %p [%1.1f, %1.1f] -> [%1.1f, %1.1f]")
                << hedge
                << hedge->v[0]->pos[VX] << hedge->v[0]->pos[VY],
                << hedge->v[1]->pos[VX] << hedge->v[1]->pos[VY];

        } while((hedge = hedge->next) != leaf->hedge);
        */
    }
}

DENG_DEBUG_ONLY(
static int printSuperBlockHEdgesWorker(SuperBlock* block, void* /*parameters*/)
{
    SuperBlock::DebugPrint(*block);
    return false; // Continue iteration.
})

static void initAABoxFromEditableLineDefVertexes(AABoxf* aaBox, const LineDef* line)
{
    const double* from = line->L_v1->buildData.pos;
    const double* to   = line->L_v2->buildData.pos;
    aaBox->minX = MIN_OF(from[VX], to[VX]);
    aaBox->minY = MIN_OF(from[VY], to[VY]);
    aaBox->maxX = MAX_OF(from[VX], to[VX]);
    aaBox->maxY = MAX_OF(from[VY], to[VY]);
}

static Sector* findFirstSectorInHEdgeList(const BspLeaf* leaf)
{
    Q_ASSERT(leaf);
    HEdge* hedge = leaf->hedge;
    do
    {
        if(hedge->sector)
        {
            return hedge->sector;
        }
    } while((hedge = hedge->next) != leaf->hedge);
    return NULL; // Nothing??
}
