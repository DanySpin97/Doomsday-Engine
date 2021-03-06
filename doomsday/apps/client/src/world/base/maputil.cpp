/** @file maputil.cpp  World map utilities.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "world/maputil.h"

#include "Line"
#include "Plane"
#include "Sector"

#ifdef __CLIENT__
#  include "Surface"
#  include "world/lineowner.h"

#  include "MaterialVariantSpec"

#  include "render/rend_main.h" // Rend_MapSurfacematerialSpec
#  include "MaterialAnimator"
#  include "WallEdge"
#endif

using namespace de;

lineopening_s::lineopening_s(Line const &line)
{
    if(!line.back().hasSector())
    {
        top = bottom = range = lowFloor = 0;
        return;
    }

    Sector const *frontSector = line.front().sectorPtr();
    Sector const *backSector  = line.back().sectorPtr();
    DENG_ASSERT(frontSector != 0);

    if(backSector && backSector->ceiling().height() < frontSector->ceiling().height())
    {
        top = backSector->ceiling().height();
    }
    else
    {
        top = frontSector->ceiling().height();
    }

    if(backSector && backSector->floor().height() > frontSector->floor().height())
    {
        bottom = backSector->floor().height();
    }
    else
    {
        bottom = frontSector->floor().height();
    }

    range = top - bottom;

    // Determine the "low floor".
    if(backSector && frontSector->floor().height() > backSector->floor().height())
    {
        lowFloor = float( backSector->floor().height() );
    }
    else
    {
        lowFloor = float( frontSector->floor().height() );
    }
}

lineopening_s &lineopening_s::operator = (lineopening_s const &other)
{
    top      = other.top;
    bottom   = other.bottom;
    range    = other.range;
    lowFloor = other.lowFloor;
    return *this;
}

#ifdef __CLIENT__

/**
 * Same as @ref R_OpenRange() but works with the "visual" (i.e., smoothed) plane
 * height coordinates rather than the "sharp" coordinates.
 *
 * @param side      Line side to find the open range for.
 *
 * Return values:
 * @param bottom    Bottom Z height is written here. Can be @c nullptr.
 * @param top       Top Z height is written here. Can be @c nullptr.
 *
 * @return Height of the open range.
 *
 * @todo fixme: Should use the visual plane heights of subsectors.
 */
static coord_t visOpenRange(LineSide const &side, coord_t *retBottom = nullptr, coord_t *retTop = nullptr)
{
    Sector const *frontSec = side.sectorPtr();
    Sector const *backSec  = side.back().sectorPtr();

    coord_t bottom;
    if(backSec && backSec->floor().heightSmoothed() > frontSec->floor().heightSmoothed())
    {
        bottom = backSec->floor().heightSmoothed();
    }
    else
    {
        bottom = frontSec->floor().heightSmoothed();
    }

    coord_t top;
    if(backSec && backSec->ceiling().heightSmoothed() < frontSec->ceiling().heightSmoothed())
    {
        top = backSec->ceiling().heightSmoothed();
    }
    else
    {
        top = frontSec->ceiling().heightSmoothed();
    }

    if(retBottom) *retBottom = bottom;
    if(retTop)    *retTop    = top;

    return top - bottom;
}

/// @todo fixme: Should use the visual plane heights of subsectors.
bool R_SideBackClosed(LineSide const &side, bool ignoreOpacity)
{
    if(!side.hasSections()) return false;
    if(!side.hasSector()) return false;
    if(side.line().isSelfReferencing()) return false; // Never.

    if(side.considerOneSided()) return true;

    Sector const &frontSec = side.sector();
    Sector const &backSec  = side.back().sector();

    if(backSec.floor().heightSmoothed()   >= backSec.ceiling().heightSmoothed())  return true;
    if(backSec.ceiling().heightSmoothed() <= frontSec.floor().heightSmoothed())   return true;
    if(backSec.floor().heightSmoothed()   >= frontSec.ceiling().heightSmoothed()) return true;

    // Perhaps a middle material completely covers the opening?
    //if(side.middle().hasMaterial())

    if (MaterialAnimator *matAnimator = side.middle().materialAnimator())
    {
        //MaterialAnimator &matAnimator = static_cast<ClientMaterial &>(side.middle().material())
        //        .getAnimator(Rend_MapSurfaceMaterialSpec());

        // Ensure we have up to date info about the material.
        matAnimator->prepare();

        if(ignoreOpacity || (matAnimator->isOpaque() && !side.middle().blendMode() && side.middle().opacity() >= 1))
        {
            // Stretched middles always cover the opening.
            if(side.isFlagged(SDF_MIDDLE_STRETCH))
                return true;

            if(side.leftHEdge()) // possibility of degenerate BSP leaf
            {
                coord_t openRange, openBottom, openTop;
                openRange = visOpenRange(side, &openBottom, &openTop);
                if(matAnimator->dimensions().y >= openRange)
                {
                    // Possibly; check the placement.
                    WallEdge edge(WallSpec::fromMapSide(side, LineSide::Middle),
                                  *side.leftHEdge(), Line::From);

                    return (edge.isValid() && edge.top().z() > edge.bottom().z()
                            && edge.top().z() >= openTop && edge.bottom().z() <= openBottom);
                }
            }
        }
    }

    return false;
}

Line *R_FindLineNeighbor(Line const &line, LineOwner const &own, ClockDirection direction,
    Sector const *sector, binangle_t *diff)
{
    LineOwner const *cown = (direction == Anticlockwise ? own.prev() : own.next());
    Line *other = &cown->line();

    if(other == &line)
        return nullptr;

    if(diff)
    {
        *diff += (direction == Anticlockwise ? cown->angle() : own.angle());
    }

    if(!other->back().hasSector() || !other->isSelfReferencing())
    {
        if(sector)  // Must one of the sectors match?
        {
            if(other->front().sectorPtr() == sector ||
               (other->back().hasSector() && other->back().sectorPtr() == sector))
                return other;
        }
        else
        {
            return other;
        }
    }

    // Not suitable, try the next.
    DENG2_ASSERT(cown);
    return R_FindLineNeighbor(line, *cown, direction, sector, diff);
}

/**
 * @param side  Line side for which to determine covered opening status.
 *
 * @return  @c true iff there is a "middle" material on @a side which
 * completely covers the open range.
 *
 * @todo fixme: Should use the visual plane heights of subsectors.
 */
static bool middleMaterialCoversOpening(LineSide const &side)
{
    if(!side.hasSector()) return false; // Never.

    if(!side.hasSections()) return false;
    //if(!side.middle().hasMaterial()) return false;

    // Stretched middles always cover the opening.
    if(side.isFlagged(SDF_MIDDLE_STRETCH))
        return true;

    MaterialAnimator *matAnimator = side.middle().materialAnimator();
            //.as<ClientMaterial>().getAnimator(Rend_MapSurfaceMaterialSpec());
    if (!matAnimator) return false;

    // Ensure we have up to date info about the material.
    matAnimator->prepare();

    if(matAnimator->isOpaque() && !side.middle().blendMode() && side.middle().opacity() >= 1)
    {
        if(side.leftHEdge())  // possibility of degenerate BSP leaf.
        {
            coord_t openRange, openBottom, openTop;
            openRange = visOpenRange(side, &openBottom, &openTop);
            if(matAnimator->dimensions().y >= openRange)
            {
                // Possibly; check the placement.
                WallEdge edge(WallSpec::fromMapSide(side, LineSide::Middle), *side.leftHEdge(), Line::From);

                return (edge.isValid()
                        && edge.top   ().z() > edge.bottom().z()
                        && edge.top   ().z() >= openTop
                        && edge.bottom().z() <= openBottom);
            }
        }
    }

    return false;
}

/// @todo fixme: Should use the visual plane heights of subsectors.
Line *R_FindSolidLineNeighbor(Line const &line, LineOwner const &own, ClockDirection direction,
    Sector const *sector, binangle_t *diff)
{
    DENG2_ASSERT(sector);

    LineOwner const *cown = (direction == Anticlockwise ? own.prev() : own.next());
    Line *other = &cown->line();

    if (other == &line) return nullptr;

    if (diff)
    {
        *diff += (direction == Anticlockwise ? cown->angle() : own.angle());
    }

    if (!((other->isBspWindow()) && other->front().sectorPtr() != sector)
        && !other->isSelfReferencing())
    {
        if (!other->front().hasSector() || !other->back().hasSector())
            return other;

        if (   other->front().sector().floor  ().heightSmoothed() >= sector->ceiling().heightSmoothed()
            || other->front().sector().ceiling().heightSmoothed() <= sector->floor  ().heightSmoothed()
            || other->back().sector().floor  ().heightSmoothed() >= sector->ceiling().heightSmoothed()
            || other->back().sector().ceiling().heightSmoothed() <= sector->floor  ().heightSmoothed()
            || other->back().sector().ceiling().heightSmoothed() <= other->back().sector().floor().heightSmoothed())
            return other;

        // Both front and back MUST be open by this point.

        // Perhaps a middle material completely covers the opening?
        // We should not give away the location of false walls (secrets).
        LineSide &otherSide = other->side(other->front().sectorPtr() == sector ? Line::Front : Line::Back);
        if (middleMaterialCoversOpening(otherSide))
            return other;
    }

    // Not suitable, try the next.
    DENG2_ASSERT(cown);
    return R_FindSolidLineNeighbor(line, *cown, direction, sector, diff);
}

#endif  // __CLIENT__
