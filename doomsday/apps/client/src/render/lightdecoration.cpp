/** @file lightdecoration.cpp  World surface light decoration.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "render/lightdecoration.h"

#include "world/convexsubspace.h"
#include "world/map.h"
#include "Surface"
#include "client/clientsubsector.h"

#include "render/rend_main.h" // Rend_ApplyLightAdaptation

#include "def_main.h"

#include <doomsday/console/var.h>

using namespace de;
using namespace world;

static dfloat angleFadeFactor = .1f; ///< cvar
static dfloat brightFactor    = 1;   ///< cvar

LightDecoration::LightDecoration(MaterialAnimator::Decoration const &source, Vector3d const &origin)
    : Decoration(source, origin)
    , Source()
{}

String LightDecoration::description() const
{
    String desc;
#ifdef DENG2_DEBUG
    desc.prepend(String(_E(b) "LightDecoration " _E(.) "[0x%1]\n").arg(de::dintptr(this), 0, 16));
#endif
    return Decoration::description() + "\n" + desc;
}

dfloat LightDecoration::occlusion(Vector3d const &eye) const
{
    // Halo brightness drops as the angle gets too big.
    if (source().elevation() < 2 && ::angleFadeFactor > 0) // Close the surface?
    {
        Vector3d const vecFromOriginToEye = (origin() - eye).normalize();

        auto dot = dfloat( -surface().normal().dot(vecFromOriginToEye) );
        if (dot < ::angleFadeFactor / 2)
        {
            return 0;
        }
        else if (dot < 3 * ::angleFadeFactor)
        {
            return (dot - ::angleFadeFactor / 2) / (2.5f * ::angleFadeFactor);
        }
    }
    return 1;
}

/**
 * @return  @c > 0 if @a lightlevel passes the min max limit condition.
 */
static dfloat checkLightLevel(dfloat lightlevel, dfloat min, dfloat max)
{
    // Has a limit been set?
    if (de::fequal(min, max)) return 1;
    return de::clamp(0.f, (lightlevel - min) / dfloat(max - min), 1.f);
}

Lumobj *LightDecoration::generateLumobj() const
{
    // Decorations with zero color intensity produce no light.
    if (source().color() == Vector3f(0, 0, 0))
        return nullptr;

    ConvexSubspace *subspace = bspLeafAtOrigin().subspacePtr();
    if (!subspace) return nullptr;

    // Does it pass the ambient light limitation?
    dfloat intensity = subspace->subsector().as<ClientSubsector>().lightSourceIntensity();
    Rend_ApplyLightAdaptation(intensity);

    dfloat lightLevels[2];
    source().lightLevels(lightLevels[0], lightLevels[1]);

    intensity = checkLightLevel(intensity, lightLevels[0], lightLevels[1]);
    if (intensity < .0001f)
        return nullptr;

    // Apply the brightness factor (was calculated using sector lightlevel).
    dfloat fadeMul = intensity * ::brightFactor;
    if (fadeMul <= 0)
        return nullptr;

    Lumobj *lum = new Lumobj(origin(), source().radius(), source().color() * fadeMul);

    lum->setSource(this);

    lum->setMaxDistance (MAX_DECOR_DISTANCE)
        .setLightmap    (Lumobj::Side, source().tex())
        .setLightmap    (Lumobj::Down, source().floorTex())
        .setLightmap    (Lumobj::Up,   source().ceilTex())
        .setFlareSize   (source().flareSize())
        .setFlareTexture(source().flareTex());

    return lum;
}

void LightDecoration::consoleRegister() // static
{
    C_VAR_FLOAT("rend-light-decor-angle",  &::angleFadeFactor, 0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-bright", &::brightFactor,    0, 0, 10);
}

dfloat LightDecoration::angleFadeFactor() // static
{
    return ::angleFadeFactor;
}

dfloat LightDecoration::brightFactor() // static
{
    return ::brightFactor;
}
