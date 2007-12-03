/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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
 * bsp_edge.h: Choose the best half-edge to use for a node line.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

#ifndef __BSP_EDGE_H__
#define __BSP_EDGE_H__

#define IFFY_LEN  4.0

// Smallest distance between two points before being considered equal.
#define DIST_EPSILON  (1.0 / 128.0)

// Smallest degrees between two angles before being considered equal.
#define ANG_EPSILON  (1.0 / 1024.0)

typedef struct hedge_s {
    vertex_t  *v[2]; // [Start, End] of the half-edge..

    // Half-edge on the other side, or NULL if one-sided. This relationship
    // is always one-to-one -- if one of the half-edges is split, the twin
    // must also be split.
    struct hedge_s *twin;

    struct hedge_s *next;
    struct hedge_s *nextOnSide;
    struct hedge_s *prevOnSide;

    // Index of the half-edge. Only valid once the half-edge has been added
    // to a polygon. A negative value means it is invalid -- there
    // shouldn't be any of these once the BSP tree has been built.
    int         index;

    // The superblock that contains this half-edge, or NULL if the half-edge
    // is no longer in any superblock (e.g. now in a subsector).
    struct superblock_s *block;

    // Precomputed data for faster calculations.
    double      pSX, pSY;
    double      pEX, pEY;
    double      pDX, pDY;

    double      pLength;
    double      pAngle;
    double      pPara;
    double      pPerp;

    // Linedef that this half-edge goes along, or NULL if miniseg.
    line_t *linedef;

    // Linedef that this half-edge initially comes from.
    // For "real" half-edges, this is just the same as the 'linedef' field
    // above. For "miniedges", this is the linedef of the partition line.
    line_t  *sourceLine;

    sector_t  *sector; // Adjacent sector, or NULL if invalid sidedef or minihedge.
    byte        side; // 0 for right, 1 for left.
} hedge_t;

hedge_t    *LookupHEdge(int index);

hedge_t    *BSP_CreateHEdge(line_t *line, line_t *sourceLine,
                            vertex_t *start, vertex_t *end,
                            sector_t *sec, boolean back);
hedge_t    *BSP_SplitHEdge(hedge_t *oldHEdge, double x, double y);

void        BSP_SortHEdgesByIndex(void);
void        BSP_FreeHEdges(void);
int         BSP_GetNumHEdges(void);

// Edge tip functions:
void        BSP_CreateVertexEdgeTip(vertex_t *vert, double dx, double dy,
                                    hedge_t *back, hedge_t *front);
void        BSP_CountEdgeTips(vertex_t *vert, uint *oneSided, uint *twoSided);
sector_t  *BSP_VertexCheckOpen(vertex_t *vert, double dx, double dy);
void        BSP_FreeEdgeTips(void);

// Misc util functions:
double      ParallelDist(hedge_t *part, double x, double y);
double      PerpDist(hedge_t *part, double x, double y);
#endif
