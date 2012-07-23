/**
 * @file id1map_analyse.cpp @ingroup wadmapconverter
 *
 * id tech 1 post load map analyses.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#define mapFormat               DENG_PLUGIN_GLOBAL(mapFormat)
#define map                     DENG_PLUGIN_GLOBAL(map)

static uint PolyLineCount;

static uint validCount = 0; // Used for Polyobj LineDef collection.

/**
 * Create a temporary polyobj (read from the original map data).
 */
static boolean createPolyobj(mline_t** lineList, uint num, uint* poIdx,
    int tag, int sequenceType, int16_t anchorX, int16_t anchorY)
{
    if(!lineList || num == 0)
        return false;

    // Allocate the new polyobj.
    mpolyobj_t* po = (mpolyobj_t*)calloc(1, sizeof(*po));

    /**
     * Link the new polyobj into the global list.
     */
    mpolyobj_t** newList = (mpolyobj_t**)malloc(((++map->numPolyobjs) + 1) * sizeof(mpolyobj_t*));
    // Copy the existing list.
    uint n = 0;
    for(uint i = 0; i < map->numPolyobjs - 1; ++i, n++)
    {
        newList[i] = map->polyobjs[i];
    }
    newList[n++] = po; // Add the new polyobj.
    newList[n] = NULL; // Terminate.

    if(map->numPolyobjs-1 > 0)
        free(map->polyobjs);
    map->polyobjs = newList;

    po->idx = map->numPolyobjs-1;
    po->tag = tag;
    po->seqType = sequenceType;
    po->anchor[VX] = anchorX;
    po->anchor[VY] = anchorY;
    po->lineCount = num;
    po->lineIndices = (uint*)malloc(sizeof(uint) * num);
    for(uint i = 0; i < num; ++i)
    {
        mline_t* line = lineList[i];
        line->aFlags |= LAF_POLYOBJ;
        /**
         * Due a logic error in hexen.exe, when the column drawer is
         * presented with polyobj segs built from two-sided linedefs;
         * clipping is always calculated using the pegging logic for
         * single-sided linedefs.
         *
         * Here we emulate this behavior by automatically applying
         * bottom unpegging for two-sided linedefs.
         */
        if(line->sides[LEFT] != 0)
            line->ddFlags |= DDLF_DONTPEGBOTTOM;
        po->lineIndices[i] = line - map->lines;
    }

    if(poIdx)
        *poIdx = po->idx;

    return true; // Success!
}

/**
 * @param lineList      @c NULL, will cause IterFindPolyLines to count
 *                      the number of lines in the polyobj.
 */
static void iterFindPolyLines(coord_t x, coord_t y, mline_t** lineList)
{
    uint i;

    for(i = 0; i < map->numLines; ++i)
    {
        mline_t* line = &map->lines[i];
        coord_t v1[2], v2[2];

        if(line->aFlags & LAF_POLYOBJ) continue;
        if(line->validCount == validCount) continue;

        v1[VX] = map->vertexes[(line->v[0] - 1) * 2];
        v1[VY] = map->vertexes[(line->v[0] - 1) * 2 + 1];
        v2[VX] = map->vertexes[(line->v[1] - 1) * 2];
        v2[VY] = map->vertexes[(line->v[1] - 1) * 2 + 1];

        if(FEQUAL(v1[VX], x) && FEQUAL(v1[VY], y))
        {
            line->validCount = validCount;

            if(!lineList)
                PolyLineCount++;
            else
                *lineList++ = line;

            iterFindPolyLines(v2[VX], v2[VY], lineList);
        }
    }
}

/**
 * @todo This terribly inefficent (naive) algorithm may need replacing
 * (it is far outside an exceptable polynominal range!).
 */
static mline_t** collectPolyobjLineDefs(mline_t* lineDef, uint* num)
{
    mline_t** lineList;
    coord_t v1[2], v2[2];

    lineDef->xType = 0;
    lineDef->xArgs[0] = 0;

    v1[VX] = map->vertexes[(lineDef->v[0]-1) * 2];
    v1[VY] = map->vertexes[(lineDef->v[0]-1) * 2 + 1];
    v2[VX] = map->vertexes[(lineDef->v[1]-1) * 2];
    v2[VY] = map->vertexes[(lineDef->v[1]-1) * 2 + 1];

    PolyLineCount = 1;
    validCount++;
    lineDef->validCount = validCount;
    iterFindPolyLines(v2[VX], v2[VY], NULL);

    lineList = (mline_t**)malloc((PolyLineCount+1) * sizeof(mline_t*));

    lineList[0] = lineDef; // Insert the first line.
    validCount++;
    lineDef->validCount = validCount;
    iterFindPolyLines(v2[VX], v2[VY], lineList + 1);
    lineList[PolyLineCount] = 0; // Terminate.

    *num = PolyLineCount;
    return lineList;
}

/**
 * Find all linedefs marked as belonging to a polyobject with the given tag
 * and attempt to create a polyobject from them.
 *
 * @param tag           Line tag of linedefs to search for.
 *
 * @return              @c true = successfully created polyobj.
 */
static boolean findAndCreatePolyobj(int16_t tag, int16_t anchorX, int16_t anchorY)
{
#define MAXPOLYLINES         32

    uint i;

    for(i = 0; i < map->numLines; ++i)
    {
        mline_t* line = &map->lines[i];

        if(line->aFlags & LAF_POLYOBJ) continue;
        if(!(line->xType == PO_LINE_START && line->xArgs[0] == tag)) continue;

        uint num;
        mline_t** lineList = collectPolyobjLineDefs(line, &num);
        if(lineList)
        {
            uint poIdx;
            byte seqType;
            boolean result;

            seqType = line->xArgs[2];
            if(seqType >= SEQTYPE_NUMSEQ)
                seqType = 0;

            result = createPolyobj(lineList, num, &poIdx, tag, seqType, anchorX, anchorY);
            free(lineList);

            if(result) return true;
        }
    }

    /**
     * Didn't find a polyobj through PO_LINE_START.
     * We'll try another approach...
     */
    mline_t* polyLineList[MAXPOLYLINES];
    uint lineCount = 0;
    uint j, psIndex, psIndexOld;

    psIndex = 0;
    for(j = 1; j < MAXPOLYLINES; ++j)
    {
        psIndexOld = psIndex;
        for(i = 0; i < map->numLines; ++i)
        {
            mline_t* line = &map->lines[i];

            if(line->aFlags & LAF_POLYOBJ) continue;

            if(line->xType == PO_LINE_EXPLICIT &&
               line->xArgs[0] == tag)
            {
                if(!line->xArgs[1])
                {
                    Con_Error("WadMapConverter::findAndCreatePolyobj: Explicit line missing order number "
                              "(probably %d) in poly %d.\n", j + 1, tag);
                }

                if(line->xArgs[1] == j)
                {
                    // Add this line to the list.
                    polyLineList[psIndex] = line;
                    lineCount++;
                    psIndex++;
                    if(psIndex > MAXPOLYLINES)
                    {
                        Con_Error("WadMapConverter::findAndCreatePolyobj: psIndex > MAXPOLYLINES\n");
                    }

                    // Clear out any special.
                    line->xType = 0;
                    line->xArgs[0] = 0;
                    line->aFlags |= LAF_POLYOBJ;
                }
            }
        }

        if(psIndex == psIndexOld)
        {
            // Check if an explicit line order has been skipped
            // A line has been skipped if there are any more explicit
            // lines with the current tag value
            for(i = 0; i < map->numLines; ++i)
            {
                mline_t* line = &map->lines[i];

                if(line->xType == PO_LINE_EXPLICIT && line->xArgs[0] == tag)
                {
                    Con_Error("WadMapConverter::findAndCreatePolyobj: Missing explicit line %d for poly %d\n",
                              j, tag);
                }
            }
        }
    }

    if(lineCount)
    {
        const int seqType = polyLineList[0]->xArgs[3];
        uint poIdx;

        if(createPolyobj(polyLineList, lineCount, &poIdx, tag,
                         seqType, anchorX, anchorY))
        {
            mline_t* line = polyLineList[0];

            // Next, change the polyobjs first line to point to a mirror
            // if it exists.
            line->xArgs[1] = line->xArgs[2];

            return true;
        }
    }

    return false;

#undef MAXPOLYLINES
}

static void findPolyobjs(void)
{
    WADMAPCONVERTER_TRACE("Locating polyobjs...");

    for(uint i = 0; i < map->numThings; ++i)
    {
        mthing_t* thing = &map->things[i];
        if(thing->doomEdNum == PO_ANCHOR_DOOMEDNUM)
        {
            // A polyobj anchor.
            const int tag = thing->angle;
            findAndCreatePolyobj(tag, thing->origin[VX], thing->origin[VY]);
        }
    }
}

void AnalyzeMap(void)
{
    if(mapFormat == MF_HEXEN)
    {
        findPolyobjs();
    }
}
