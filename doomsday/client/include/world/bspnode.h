/** @file bspnode.h  World map BSP node.
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

#ifndef DENG_WORLD_BSPNODE_H
#define DENG_WORLD_BSPNODE_H

#include "MapElement"
#include "partition.h"
#include <de/Error>
#include <de/aabox.h>

/**
 * Represents a node in the map's binary space partition (BSP) tree. Each node
 * defines a partition line which divides the subspace in two, a left child and
 * a right child subspace. Each child may be associated with either another node,
 * or a @ref BspLeaf.
 *
 * @todo There is a missing abstraction here. All BSP tree elements should be
 * derived from a common base class, thereby enforcing which objects can be
 * linked into the tree. -ds
 *
 * @see http://en.wikipedia.org/wiki/Binary_space_partitioning
 *
 * @ingroup world
 */
class BspNode : public de::MapElement
{
    DENG2_NO_COPY  (BspNode)
    DENG2_NO_ASSIGN(BspNode)

public:
    /// An invalid child element was specified. @ingroup errors
    DENG2_ERROR(InvalidChildError);

    /// Required child element is missing. @ingroup errors
    DENG2_ERROR(MissingChildError);

    /// Child element identifiers:
    enum { Right, Left };

public:
    /**
     * Construct a new BSP node.
     *
     * @param partition  Partition line in the map coordinate space which divides
     *                   the space into two 'child' subspaces.
     */
    BspNode(de::Partition const &partition = de::Partition());

    /**
     * Returns the space partition line at the node.
     */
    de::Partition const &partition() const;

    /**
     * Calculates the height of this BSP subtree (note result is not cached).
     */
    size_t height() const;

    /**
     * Returns @c true iff the specified child is configured.
     */
    bool hasChild(int left) const;

    /**
     * Returns @c true iff a @em right child is configured.
     */
    inline bool hasRight() const { return hasChild(Right); }

    /**
     * Returns @c true iff a @em left child is configured.
     */
    inline bool hasLeft() const { return hasChild(Left); }

    /**
     * Returns the specified child of the node.
     *
     * @param left  If not @c 0 return the Left child; otherwise the Right child.
     *
     * @see hasChild()
     */
    de::MapElement &child(int left);
    de::MapElement const &child(int left) const;

    inline de::MapElement &right() { return child(Right); }
    inline de::MapElement const &right() const { return child(Right); }

    inline de::MapElement &left() { return child(Left); }
    inline de::MapElement const &left() const { return child(Left); }

    /**
     * Returns a pointer to the specified child of the BSP node, which may be
     * @c 0 if no child is configured.
     *
     * @param left  If not @c 0 return the Left child; otherwise the Right child.
     *
     * @see hasChild()
     */
    inline de::MapElement *childPtr(int left) {
        return hasChild(left)? &child(left) : 0;
    }
    inline de::MapElement const *childPtr(int left) const {
        return hasChild(left)? &child(left) : 0;
    }

    void setChild(int left, de::MapElement *newChild);

    inline void setRight(de::MapElement *newChild) { setChild(Right, newChild); }

    inline void setLeft(de::MapElement *newChild) { setChild(Left, newChild); }

    /**
     * Returns the axis-aligned bounding box for the specified child, which,
     * encompases all the vertexes which define the geometry of that subspace
     * of the BSP, in map coordinate space units.
     */
    AABoxd const &childAABox(int left) const;

    /**
     * Returns the axis-aligned bounding box for the Right child, which,
     * encompases all the vertexes which define the geometry of that subspace
     * of the BSP, in map coordinate space units.
     */
    inline AABoxd const &rightAABox() const { return childAABox(Right); }

    /**
     * Returns the axis-aligned bounding box for the Left child, which,
     * encompases all the vertexes which define the geometry of that subspace
     * of the BSP, in map coordinate space units.
     */
    inline AABoxd const &leftAABox() const { return childAABox(Left); }

    void setChildAABox(int left, AABoxd const *newAABox);

    inline void setRightAABox(AABoxd const *newAABox) { setChildAABox(Right, newAABox); }

    inline void setLeftAABox(AABoxd const *newAABox) { setChildAABox(Left, newAABox); }

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_BSPNODE_H
