/** @file lumobj.h Luminous object.
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

#ifndef DENG_CLIENT_RENDER_LUMOBJ_H
#define DENG_CLIENT_RENDER_LUMOBJ_H

#include <de/Error>
#include <de/Vector>

#include "Texture"

class BspLeaf;

namespace de {
class Map;
}

class Lumobj
{
public:
    /// No map is attributed. @ingroup errors
    DENG2_ERROR(MissingMapError);

    /// Special identifier used to mark an invalid index.
    enum { NoIndex = -1 };

    /// Identifiers for attributing lightmaps for projection (relative surface directions).
    enum LightmapSemantic {
        Side,
        Down,
        Up
    };

public:
    /**
     * Construct a new luminous object.
     */
    Lumobj();
    Lumobj(Lumobj const &other);

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

    static int radiusMax();
    static float radiusFactor();

    /**
     * Returns @c true iff a map is attributed to the lumobj.
     *
     * @see map(), setMap()
     */
    bool hasMap() const;

    /**
     * Returns the map attributed to the lumobj.
     *
     * @see hasMap(), setMap()
     */
    de::Map &map() const;

    /**
     * Change the map attributed to the lumobj.
     *
     * @param newMap
     *
     * @see hasMap(), map()
     */
    void setMap(de::Map *newMap);

    /**
     * Returns the "in-map" index attributed to the lumobj.
     *
     * @see setIndexInMap()
     */
    int indexInMap() const;

    /**
     * Change the "in-map" index attributed to the lumobj.
     *
     * @param newIndex  New index to attribute to the lumobj. Use @c NoIndex to
     *                  clear the attribution (not a valid index).
     *
     * @see indexInMap()
     */
    void setIndexInMap(int newIndex = NoIndex);

    /**
     * Translate the origin of the lumobj in map space.
     *
     * @param delta  Movement delta on the XY plane.
     *
     * @see setOrigin(), origin()
     */
    void move(de::Vector3d const &delta);

    /**
     * Returns the origin of the lumobj in map space.
     *
     * @see move(), setOrigin(), bspLeafAtOrigin()
     */
    de::Vector3d const &origin() const;

    /**
     * Change the origin of the lumobj in map space.
     *
     * @param newOrigin  New absolute map space origin to apply, in map units.
     *
     * @see move(), origin()
     */
    Lumobj &setOrigin(de::Vector3d const &newOrigin);

    /**
     * Returns the map BSP leaf at the origin of the lumobj (result cached).
     */
    BspLeaf &bspLeafAtOrigin() const;

    /**
     * Returns the light color/intensity of the lumobj.
     *
     * @see setColor()
     */
    de::Vector3f const &color() const;

    /**
     * Change the light color/intensity of the lumobj.
     *
     * @param newColor  New color to apply.
     *
     * @see color()
     */
    Lumobj &setColor(de::Vector3f const &newColor);

    /**
     * Returns the radius of the lumobj in map space units.
     *
     * @see setRadius()
     */
    double radius() const;

    /**
     * Change the radius of the lumobj in map space units.
     *
     * @param newRadius  New radius to apply.
     *
     * @see radius()
     */
    Lumobj &setRadius(double newRadius);

    /**
     * Returns the z-offset of the lumobj.
     *
     * @see setZOffset()
     */
    double zOffset() const;

    /**
     * Change the z-offset of the lumobj.
     *
     * @param newZOffset  New z-offset to apply.
     *
     * @see zOffset()
     */
    Lumobj &setZOffset(double newZOffset);

    /**
     * Returns the maximum distance at which the lumobj will be drawn. If no
     * maximum is configured then @c 0 is returned (default).
     *
     * @see setMaxDistance()
     */
    double maxDistance() const;

    /**
     * Change the maximum distance at which the lumobj will drawn. For use with
     * surface decorations, which, should only de visible within a fairly small
     * radius around the viewer).
     *
     * @param newMaxDistance  New maximum distance to apply.
     *
     * @see maxDistance()
     */
    Lumobj &setMaxDistance(double newMaxDistance);

    /**
     * Returns the identified custom lightmap (if any).
     *
     * @param semantic  Identifier of the lightmap.
     *
     * @see setLightmap()
     */
    de::Texture *lightmap(LightmapSemantic semantic) const;

    /**
     * Change an attributed lightmap to the texture specified.
     *
     * @param semantic    Identifier of the lightmap to change.
     * @param newTexture  Lightmap texture to apply. Use @c 0 to clear.
     *
     * @see lightmap()
     */
    Lumobj &setLightmap(LightmapSemantic semantic, de::Texture *newTexture);

    /**
     * Calculate a distance attentuation factor for the lumobj.
     *
     * @param distance  Distance between the lumobj and the viewer.
     *
     * @return  Attentuation factor [0..1].
     */
    float attenuation(double distance) const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_RENDER_LUMOBJ_H
