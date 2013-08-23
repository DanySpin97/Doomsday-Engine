/** @file maputil.cpp World map utilities.
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

#include "Face"

#include "BspLeaf"
#include "Plane"
#include "Sector"
#include "world/lineowner.h"
#include "world/p_players.h"

#ifdef __CLIENT__
#  include "MaterialSnapshot"
#  include "MaterialVariantSpec"

#  include "render/rend_main.h"
#endif

#include "world/maputil.h"

using namespace de;

void R_SetRelativeHeights(Sector const *front, Sector const *back, int planeIndex,
    coord_t *fz, coord_t *bz, coord_t *bhz)
{
    if(fz)
    {
        if(front)
        {
#ifdef __CLIENT__
            *fz = front->plane(planeIndex).visHeight();
#else
            *fz = front->plane(planeIndex).height();
#endif
            if(planeIndex != Sector::Floor)
                *fz = -(*fz);
        }
        else
        {
            *fz = 0;
        }
    }
    if(bz)
    {
        if(back)
        {
#ifdef __CLIENT__
            *bz = back->plane(planeIndex).visHeight();
#else
            *bz = back->plane(planeIndex).height();
#endif
            if(planeIndex != Sector::Floor)
                *bz = -(*bz);
        }
        else
        {
            *bz = 0;
        }
    }
    if(bhz)
    {
        if(back)
        {
            int otherPlaneIndex = planeIndex == Sector::Floor? Sector::Ceiling : Sector::Floor;
#ifdef __CLIENT__
            *bhz = back->plane(otherPlaneIndex).visHeight();
#else
            *bhz = back->plane(otherPlaneIndex).height();
#endif
            if(planeIndex != Sector::Floor)
                *bhz = -(*bhz);
        }
        else
        {
            *bhz = 0;
        }
    }
}

#ifdef __CLIENT__

void R_SideSectionCoords(Line::Side const &side, int section, bool skyClip,
    coord_t *retBottom, coord_t *retTop, Vector2f *retMaterialOrigin)
{
    DENG_ASSERT(side.hasSector());
    DENG_ASSERT(side.hasSections());

    Line const &line       = side.line();

    Sector const *frontSec = line.definesPolyobj()? line.polyobj().sectorPtr() : side.sectorPtr();
    Sector const *backSec  = side.back().sectorPtr();

    bool const unpegBottom = (line.flags() & DDLF_DONTPEGBOTTOM) != 0;
    bool const unpegTop    = (line.flags() & DDLF_DONTPEGTOP) != 0;

    coord_t bottom = 0, top = 0; // Shutup compiler.

    if(side.considerOneSided())
    {
        if(section == Line::Side::Middle)
        {
            bottom = frontSec->floor().visHeight();
            top    = frontSec->ceiling().visHeight();
        }
        else
        {
            bottom = top = frontSec->floor().visHeight();
        }

        if(retMaterialOrigin)
        {
            *retMaterialOrigin = side.middle().visMaterialOrigin();
            if(unpegBottom)
            {
                retMaterialOrigin->y -= top - bottom;
            }
        }
    }
    else
    {
        // Two sided.
        bool const stretchMiddle = side.isFlagged(SDF_MIDDLE_STRETCH);
        Surface const *surface = &side.surface(section);
        Plane const *ffloor = &frontSec->floor();
        Plane const *fceil  = &frontSec->ceiling();
        Plane const *bfloor = &backSec->floor();
        Plane const *bceil  = &backSec->ceiling();

        switch(section)
        {
        case Line::Side::Top:
            // Self-referencing lines only ever get a middle.
            if(!side.line().isSelfReferencing())
            {
                // Can't go over front ceiling (would induce geometry flaws).
                if(bceil->visHeight() < ffloor->visHeight())
                    bottom = ffloor->visHeight();
                else
                    bottom = bceil->visHeight();
                top = fceil->visHeight();

                if(skyClip && fceil->surface().hasSkyMaskedMaterial() && bceil->surface().hasSkyMaskedMaterial())
                {
                    top = bottom;
                }

                if(retMaterialOrigin)
                {
                    *retMaterialOrigin = surface->visMaterialOrigin();
                    if(!unpegTop)
                    {
                        // Align with normal middle texture.
                        retMaterialOrigin->y -= fceil->visHeight() - bceil->visHeight();
                    }
                }
            }
            break;

        case Line::Side::Bottom:
            // Self-referencing lines only ever get a middle.
            if(!side.line().isSelfReferencing())
            {
                bool const raiseToBackFloor = (fceil->surface().hasSkyMaskedMaterial() && bceil->surface().hasSkyMaskedMaterial() &&
                                               fceil->visHeight() < bceil->visHeight() &&
                                               bfloor->visHeight() > fceil->visHeight());
                coord_t t = bfloor->visHeight();

                bottom = ffloor->visHeight();
                // Can't go over the back ceiling, would induce polygon flaws.
                if(bfloor->visHeight() > bceil->visHeight())
                    t = bceil->visHeight();

                // Can't go over front ceiling, would induce polygon flaws.
                // In the special case of a sky masked upper we must extend the bottom
                // section up to the height of the back floor.
                if(t > fceil->visHeight() && !raiseToBackFloor)
                    t = fceil->visHeight();
                top = t;

                if(skyClip && ffloor->surface().hasSkyMaskedMaterial() && bfloor->surface().hasSkyMaskedMaterial())
                {
                    bottom = top;
                }

                if(retMaterialOrigin)
                {
                    *retMaterialOrigin = surface->visMaterialOrigin();
                    if(bfloor->visHeight() > fceil->visHeight())
                    {
                        retMaterialOrigin->y -= (raiseToBackFloor? t : fceil->visHeight()) - bfloor->visHeight();
                    }

                    if(unpegBottom)
                    {
                        // Align with normal middle texture.
                        retMaterialOrigin->y += (raiseToBackFloor? t : fceil->visHeight()) - bfloor->visHeight();
                    }
                }
            }
            break;

        case Line::Side::Middle:
            if(!side.line().isSelfReferencing())
            {
                bottom = de::max(bfloor->visHeight(), ffloor->visHeight());
                top    = de::min(bceil->visHeight(),  fceil->visHeight());
            }
            else
            {
                bottom = ffloor->visHeight();
                top    = bceil->visHeight();
            }

            if(retMaterialOrigin)
            {
                retMaterialOrigin->x = surface->visMaterialOrigin().x;
                retMaterialOrigin->y = 0;
            }

            // Perform clipping.
            if(surface->hasMaterial() && !stretchMiddle)
            {
#ifdef __CLIENT__
                bool const clipBottom = !(!(devRendSkyMode || P_IsInVoid(viewPlayer)) && ffloor->surface().hasSkyMaskedMaterial() && bfloor->surface().hasSkyMaskedMaterial());
                bool const clipTop    = !(!(devRendSkyMode || P_IsInVoid(viewPlayer)) && fceil->surface().hasSkyMaskedMaterial()  && bceil->surface().hasSkyMaskedMaterial());
#else
                bool const clipBottom = !(ffloor->surface().hasSkyMaskedMaterial() && bfloor->surface().hasSkyMaskedMaterial());
                bool const clipTop    = !(fceil->surface().hasSkyMaskedMaterial()  && bceil->surface().hasSkyMaskedMaterial());
#endif

                coord_t openBottom, openTop;
                if(!side.line().isSelfReferencing())
                {
                    openBottom = bottom;
                    openTop    = top;
                }
                else
                {
                    BspLeaf const &bspLeaf = side.leftHEdge()->face().mapElement()->as<BspLeaf>();
                    openBottom = bspLeaf.visFloorHeight();
                    openTop    = bspLeaf.visCeilingHeight();
                }

                int const matHeight      = surface->material().height();
                coord_t const matYOffset = surface->visMaterialOrigin().y;

                if(openTop > openBottom)
                {
                    if(unpegBottom)
                    {
                        bottom += matYOffset;
                        top = bottom + matHeight;
                    }
                    else
                    {
                        top += matYOffset;
                        bottom = top - matHeight;
                    }

                    if(retMaterialOrigin && top > openTop)
                    {
                        retMaterialOrigin->y = top - openTop;
                    }

                    // Clip it?
                    if(clipTop || clipBottom)
                    {
                        if(clipBottom && bottom < openBottom)
                            bottom = openBottom;

                        if(clipTop && top > openTop)
                            top = openTop;
                    }

                    if(retMaterialOrigin && !clipTop)
                    {
                        retMaterialOrigin->y = 0;
                    }
                }
            }
            break;
        }
    }

    if(retBottom) *retBottom = bottom;
    if(retTop)    *retTop    = top;
}

#endif // __CLIENT__

coord_t R_OpenRange(Line::Side const &side, Sector const *frontSec,
    Sector const *backSec, coord_t *retBottom, coord_t *retTop)
{
    DENG_UNUSED(side); // Don't remove (present for API symmetry) -ds
    DENG_ASSERT(frontSec != 0);

    coord_t top;
    if(backSec && backSec->ceiling().height() < frontSec->ceiling().height())
    {
        top = backSec->ceiling().height();
    }
    else
    {
        top = frontSec->ceiling().height();
    }

    coord_t bottom;
    if(backSec && backSec->floor().height() > frontSec->floor().height())
    {
        bottom = backSec->floor().height();
    }
    else
    {
        bottom = frontSec->floor().height();
    }

    if(retBottom) *retBottom = bottom;
    if(retTop)    *retTop    = top;

    return top - bottom;
}

#ifdef __CLIENT__

coord_t R_VisOpenRange(Line::Side const &side, Sector const *frontSec,
    Sector const *backSec, coord_t *retBottom, coord_t *retTop)
{
    DENG_UNUSED(side); // Don't remove (present for API symmetry) -ds
    DENG_ASSERT(frontSec != 0);

    coord_t top;
    if(backSec && backSec->ceiling().visHeight() < frontSec->ceiling().visHeight())
    {
        top = backSec->ceiling().visHeight();
    }
    else
    {
        top = frontSec->ceiling().visHeight();
    }

    coord_t bottom;
    if(backSec && backSec->floor().visHeight() > frontSec->floor().visHeight())
    {
        bottom = backSec->floor().visHeight();
    }
    else
    {
        bottom = frontSec->floor().visHeight();
    }

    if(retBottom) *retBottom = bottom;
    if(retTop)    *retTop    = top;

    return top - bottom;
}

bool R_SideBackClosed(Line::Side const &side, bool ignoreOpacity)
{
    if(!side.hasSections()) return false;
    if(!side.hasSector()) return false;
    if(side.line().isSelfReferencing()) return false; // Never.

    if(side.considerOneSided()) return true;

    Sector const &frontSec = side.sector();
    Sector const &backSec  = side.back().sector();

    if(backSec.floor().visHeight()   >= backSec.ceiling().visHeight())  return true;
    if(backSec.ceiling().visHeight() <= frontSec.floor().visHeight())   return true;
    if(backSec.floor().visHeight()   >= frontSec.ceiling().visHeight()) return true;

    // Perhaps a middle material completely covers the opening?
    if(side.middle().hasMaterial())
    {
        // Ensure we have up to date info about the material.
        MaterialSnapshot const &ms = side.middle().material().prepare(Rend_MapSurfaceMaterialSpec());

        if(ignoreOpacity || (ms.isOpaque() && !side.middle().blendMode() && side.middle().opacity() >= 1))
        {
            // Stretched middles always cover the opening.
            if(side.isFlagged(SDF_MIDDLE_STRETCH))
                return true;

            coord_t openRange, openBottom, openTop;
            openRange = R_VisOpenRange(side, &openBottom, &openTop);
            if(ms.height() >= openRange)
            {
                // Possibly; check the placement.
                coord_t bottom, top;
                R_SideSectionCoords(side, Line::Side::Middle, 0, &bottom, &top);
                return (top > bottom && top >= openTop && bottom <= openBottom);
            }
        }
    }

    return false;
}

Line *R_FindLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff)
{
    LineOwner const *cown = antiClockwise? &own->prev() : &own->next();
    Line *other = &cown->line();

    if(other == line)
        return 0;

    if(diff) *diff += (antiClockwise? cown->angle() : own->angle());

    if(!other->hasBackSector() || !other->isSelfReferencing())
    {
        if(sector) // Must one of the sectors match?
        {
            if(other->frontSectorPtr() == sector ||
               (other->hasBackSector() && other->backSectorPtr() == sector))
                return other;
        }
        else
        {
            return other;
        }
    }

    // Not suitable, try the next.
    return R_FindLineNeighbor(sector, line, cown, antiClockwise, diff);
}

/**
 * @param side  Line side for which to determine covered opening status.
 *
 * @return  @c true iff there is a "middle" material on @a side which
 * completely covers the open range.
 */
static bool middleMaterialCoversOpening(Line::Side const &side)
{
    if(!side.hasSector()) return false; // Never.

    if(!side.hasSections()) return false;
    if(!side.middle().hasMaterial()) return false;

    // Stretched middles always cover the opening.
    if(side.isFlagged(SDF_MIDDLE_STRETCH))
        return true;

    // Ensure we have up to date info about the material.
    MaterialSnapshot const &ms = side.middle().material().prepare(Rend_MapSurfaceMaterialSpec());

    if(ms.isOpaque() && !side.middle().blendMode() && side.middle().opacity() >= 1)
    {
        coord_t openRange, openBottom, openTop;
        openRange = R_VisOpenRange(side, &openBottom, &openTop);
        if(ms.height() >= openRange)
        {
            // Possibly; check the placement.
            coord_t bottom, top;
            R_SideSectionCoords(side, Line::Side::Middle, 0, &bottom, &top);
            return (top > bottom && top >= openTop && bottom <= openBottom);
        }
    }

    return false;
}

Line *R_FindSolidLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff)
{
    LineOwner const *cown = antiClockwise? &own->prev() : &own->next();
    Line *other = &cown->line();

    if(other == line) return 0;

    if(diff) *diff += (antiClockwise? cown->angle() : own->angle());

    if(!((other->isBspWindow()) && other->frontSectorPtr() != sector) &&
       !other->isSelfReferencing())
    {
        if(!other->hasFrontSector()) return other;
        if(!other->hasBackSector()) return other;

        if(other->frontSector().floor().visHeight() >= sector->ceiling().visHeight() ||
           other->frontSector().ceiling().visHeight() <= sector->floor().visHeight() ||
           other->backSector().floor().visHeight() >= sector->ceiling().visHeight() ||
           other->backSector().ceiling().visHeight() <= sector->floor().visHeight() ||
           other->backSector().ceiling().visHeight() <= other->backSector().floor().visHeight())
            return other;

        // Both front and back MUST be open by this point.

        // Perhaps a middle material completely covers the opening?
        // We should not give away the location of false walls (secrets).
        Line::Side &otherSide = other->side(other->frontSectorPtr() == sector? Line::Front : Line::Back);
        if(middleMaterialCoversOpening(otherSide))
            return other;
    }

    // Not suitable, try the next.
    return R_FindSolidLineNeighbor(sector, line, cown, antiClockwise, diff);
}

#endif // __CLIENT__
