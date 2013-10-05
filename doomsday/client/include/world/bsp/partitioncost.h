/** @file world/bsp/partitioncost.h World BSP Partition Cost.
 *
 * Originally based on glBSP 2.24 (in turn, based on BSP 2.3)
 * @see http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#ifndef DENG_WORLD_BSP_PARTITIONCOST_H
#define DENG_WORLD_BSP_PARTITIONCOST_H

#include "world/bsp/linesegment.h"

namespace de {
namespace bsp {

/**
 * @ingroup bsp
 */
struct PartitionCost
{
    enum Side
    {
        Right,
        Left
    };

    int total;
    int splits;
    int iffy;
    int nearMiss;
    int mapRight;
    int mapLeft;
    int partRight;
    int partLeft;

    PartitionCost() :
        total(0), splits(0), iffy(0), nearMiss(0), mapRight(0),
        mapLeft(0), partRight(0), partLeft(0)
    {}

    inline PartitionCost &addSegmentRight(LineSegmentSide const &seg)
    {
        if(seg.hasMapSide()) mapRight += 1;
        else                 partRight += 1;
        return *this;
    }

    inline PartitionCost &addSegmentLeft(LineSegmentSide const &seg)
    {
        if(seg.hasMapSide()) mapLeft += 1;
        else                 partLeft += 1;
        return *this;
    }

    PartitionCost &operator += (PartitionCost const &other)
    {
        total     += other.total;
        splits    += other.splits;
        iffy      += other.iffy;
        nearMiss  += other.nearMiss;
        mapLeft   += other.mapLeft;
        mapRight  += other.mapRight;
        partLeft  += other.partLeft;
        partRight += other.partRight;
        return *this;
    }

    PartitionCost &operator = (PartitionCost const &other)
    {
        total     = other.total;
        splits    = other.splits;
        iffy      = other.iffy;
        nearMiss  = other.nearMiss;
        mapLeft   = other.mapLeft;
        mapRight  = other.mapRight;
        partLeft  = other.partLeft;
        partRight = other.partRight;
        return *this;
    }

    bool operator < (PartitionCost const &rhs) const
    {
        return total < rhs.total;
    }
};

} // namespace bsp
} // namespace de

#endif // DENG_WORLD_BSP_PARTITIONCOST_H
