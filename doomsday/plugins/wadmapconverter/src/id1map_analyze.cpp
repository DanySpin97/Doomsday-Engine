/** @file id1map_analyze.cpp  id Tech 1 map analyses.
 *
 * @ingroup wadmapconverter
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

#include "wadmapconverter.h"
#include <de/Log>
#include <de/memory.h>
#include <de/timer.h>

static uint validCount = 0; // Used for Polyobj LineDef collection.

void Id1Map::collectPolyobjLinesWorker(LineList &lineList, coord_t x, coord_t y)
{
    DENG2_FOR_EACH(Lines, i, lines)
    {
        // Already belongs to another polyobj?
        if(i->aFlags & LAF_POLYOBJ) continue;

        // Have we already encounterd this?
        if(i->validCount == validCount) continue;

        coord_t v1[2] = { vertexes[i->v[0] * 2],
                          vertexes[i->v[0] * 2 + 1] };

        coord_t v2[2] = { vertexes[i->v[1] * 2],
                          vertexes[i->v[1] * 2 + 1] };

        if(de::fequal(v1[VX], x) && de::fequal(v1[VY], y))
        {
            i->validCount = validCount;
            lineList.push_back( i - lines.begin() );
            collectPolyobjLinesWorker(lineList, v2[VX], v2[VY]);
        }
    }
}

/**
 * @todo This terribly inefficent (naive) algorithm may need replacing
 *       (it is far outside an acceptable polynomial range!).
 */
void Id1Map::collectPolyobjLines(LineList &lineList, Lines::iterator lineIt)
{
    mline_t &line = *lineIt;
    line.xType    = 0;
    line.xArgs[0] = 0;

    coord_t v2[2] = { vertexes[line.v[1] * 2],
                      vertexes[line.v[1] * 2 + 1] };

    validCount++;
    // Insert the first line.
    lineList.push_back(lineIt - lines.begin());
    line.validCount = validCount;
    collectPolyobjLinesWorker(lineList, v2[VX], v2[VY]);
}

mpolyobj_t *Id1Map::createPolyobj(LineList &lineList, int tag, int sequenceType,
    int16_t anchorX, int16_t anchorY)
{
    // Allocate the new polyobj.
    polyobjs.push_back(mpolyobj_t());
    mpolyobj_t *po = &polyobjs.back();

    po->index = polyobjs.size()-1;
    po->tag = tag;
    po->seqType = sequenceType;
    po->anchor[VX] = anchorX;
    po->anchor[VY] = anchorY;

    // Construct the line indices array we'll pass to the MPE interface.
    po->lineCount = lineList.size();
    po->lineIndices = (int *)M_Malloc(sizeof(int) * po->lineCount);
    int n = 0;
    for(LineList::iterator i = lineList.begin(); i != lineList.end(); ++i, ++n)
    {
        int lineIdx = *i;
        mline_t *line = &lines[lineIdx];

        // This line now belongs to a polyobj.
        line->aFlags |= LAF_POLYOBJ;

        /*
         * Due a logic error in hexen.exe, when the column drawer is presented
         * with polyobj segs built from two-sided linedefs; clipping is always
         * calculated using the pegging logic for single-sided linedefs.
         *
         * Here we emulate this behavior by automatically applying bottom unpegging
         * for two-sided linedefs.
         */
        if(line->sides[LEFT] >= 0)
            line->ddFlags |= DDLF_DONTPEGBOTTOM;

        po->lineIndices[n] = lineIdx;
    }

    return po;
}

bool Id1Map::findAndCreatePolyobj(int16_t tag, int16_t anchorX, int16_t anchorY)
{
    LineList polyLines;

    // First look for a PO_LINE_START linedef set with this tag.
    DENG2_FOR_EACH(Lines, i, lines)
    {
        // Already belongs to another polyobj?
        if(i->aFlags & LAF_POLYOBJ) continue;

        if(!(i->xType == PO_LINE_START && i->xArgs[0] == tag)) continue;

        collectPolyobjLines(polyLines, i);
        if(!polyLines.empty())
        {
            int8_t sequenceType = i->xArgs[2];
            if(sequenceType >= SEQTYPE_NUMSEQ) sequenceType = 0;

            createPolyobj(polyLines, tag, sequenceType, anchorX, anchorY);
            return true;
        }
        return false;
    }

    // Perhaps a PO_LINE_EXPLICIT linedef set with this tag?
    for(int n = 0; ; ++n)
    {
        bool foundAnotherLine = false;

        DENG2_FOR_EACH(Lines, i, lines)
        {
            // Already belongs to another polyobj?
            if(i->aFlags & LAF_POLYOBJ) continue;

            if(i->xType == PO_LINE_EXPLICIT && i->xArgs[0] == tag)
            {
                if(i->xArgs[1] <= 0)
                {
                    LOG_WARNING("Linedef missing (probably #%d) in explicit polyobj (tag:%d).") << n + 1 << tag;
                    return false;
                }

                if(i->xArgs[1] == n + 1)
                {
                    // Add this line to the list.
                    polyLines.push_back( i - lines.begin() );
                    foundAnotherLine = true;

                    // Clear any special.
                    i->xType = 0;
                    i->xArgs[0] = 0;
                }
            }
        }

        if(foundAnotherLine)
        {
            // Check if an explicit line order has been skipped.
            // A line has been skipped if there are any more explicit lines with
            // the current tag value.
            DENG2_FOR_EACH(Lines, i, lines)
            {
                if(i->xType == PO_LINE_EXPLICIT && i->xArgs[0] == tag)
                {
                    LOG_WARNING("Linedef missing (#%d) in explicit polyobj (tag:%d).") << n << tag;
                    return false;
                }
            }
        }
        else
        {
            // All lines have now been found.
            break;
        }
    }

    if(polyLines.empty())
    {
        LOG_WARNING("Failed to locate a single line for polyobj (tag:%d).") << tag;
        return false;
    }

    mline_t *line = &lines[ polyLines.front() ];
    int8_t const sequenceType = line->xArgs[3];

    // Setup the mirror if it exists.
    line->xArgs[1] = line->xArgs[2];

    createPolyobj(polyLines, tag, sequenceType, anchorX, anchorY);
    return true;
}

void Id1Map::findPolyobjs()
{
    LOG_TRACE("Locating polyobjs...");
    DENG2_FOR_EACH(Things, i, things)
    {
        // A polyobj anchor?
        if(i->doomEdNum == PO_ANCHOR_DOOMEDNUM)
        {
            int const tag = i->angle;
            findAndCreatePolyobj(tag, i->origin[VX], i->origin[VY]);
        }
    }
}

void Id1Map::analyze()
{
    uint startTime = Timer_RealMilliseconds();

    LOG_AS("Id1Map");
    if(mapFormat == MF_HEXEN)
    {
        findPolyobjs();
    }

    LOG_VERBOSE("Analyses completed in %.2f seconds.") << ((Timer_RealMilliseconds() - startTime) / 1000.0f);
}
