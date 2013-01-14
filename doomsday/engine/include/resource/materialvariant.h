/** @file materialvariant.h Logical Material Variant.
 *
 * @author Copyright &copy; 2011-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_MATERIALVARIANT_H
#define LIBDENG_RESOURCE_MATERIALVARIANT_H

#include "def_data.h"
#include "resource/materialsnapshot.h"
#include "resource/material.h"
#include "resource/texture.h"

struct texturevariantspecification_s;

/// Identifiers for material usage contexts.
typedef enum {
    MC_UNKNOWN = -1,
    MATERIALCONTEXT_FIRST = 0,
    MC_UI = MATERIALCONTEXT_FIRST,
    MC_MAPSURFACE,
    MC_SPRITE,
    MC_MODELSKIN,
    MC_PSPRITE,
    MC_SKYSPHERE,
    MATERIALCONTEXT_LAST = MC_SKYSPHERE
} materialcontext_t;

#define MATERIALCONTEXT_COUNT (MATERIALCONTEXT_LAST + 1 - MATERIALCONTEXT_FIRST )

/// @c true= val can be interpreted as a valid material context identifier.
#define VALID_MATERIALCONTEXT(val) ((val) >= MATERIALCONTEXT_FIRST && (val) <= MATERIALCONTEXT_LAST)

namespace de {

/**
 * Specialization specification for a variant material. Property values are
 * public for user convenience.
 *
 * @see MaterialVariant, material_t
 * @ingroup resource
 */
struct MaterialVariantSpec
{
public:
    /// Usage context identifier.
    materialcontext_t context;

    /// Specification for the primary texture.
    struct texturevariantspecification_s *primarySpec;

public:
    /**
     * Construct a zeroed MaterialVariantSpec instance.
     */
    MaterialVariantSpec() : context(MC_UNKNOWN), primarySpec(0)
    {}

    /**
     * Construct a MaterialVariantSpec instance by duplicating @a other.
     */
    MaterialVariantSpec(MaterialVariantSpec const &other)
        : context(other.context), primarySpec(other.primarySpec)
    {}

    /**
     * Determines whether specification @a other is equal to this specification.
     *
     * @param other  The other specification.
     * @return  @c true if specifications are equal; otherwise @c false.
     *
     * Same as operator ==
     */
    bool compare(MaterialVariantSpec const &other) const;

    /**
     * Determines whether specification @a other is equal to this specification.
     * @see compare()
     */
    bool operator == (MaterialVariantSpec const &other) const {
        return compare(other);
    }

    /**
     * Determines whether specification @a other is NOT equal to this specification.
     * @see compare()
     */
    bool operator != (MaterialVariantSpec const &other) const {
        return !(*this == other);
    }
};

/**
 * Context-specialized material variant. Encapsulates all context variant values
 * and logics pertaining to a specialized version of a @em superior material.
 *
 * MaterialVariant instances are usually only created by the superior material
 * when asked to prepare for render (@ref Material_ChooseVariant()) using a context
 * specialization specification which it cannot fulfill/match.
 *
 * @see material_t, MaterialVariantSpec
 * @ingroup resource
 */
class MaterialVariant
{
public:
    /// Maximum number of layers a material supports.
    static int const max_layers = DED_MAX_MATERIAL_LAYERS;

    /// Current state of a material layer.
    struct LayerState
    {
        /// Animation stage else @c -1 => layer not in use.
        int stage;

        /// Remaining (sharp) tics in the current stage.
        short tics;

        /// Intermark from the current stage to the next [0..1].
        float inter;
    };

    /// Maximum number of (light) decorations a material supports.
    static int const max_decorations = DED_MAX_MATERIAL_DECORATIONS;

    /// Current state of a material (light) decoration.
    struct DecorationState
    {
        /// Animation stage else @c -1 => decoration not in use.
        int stage;

        /// Remaining (sharp) tics in the current stage.
        short tics;

        /// Intermark from the current stage to the next [0..1].
        float inter;
    };

public:
    /// The requested layer does not exist. @ingroup errors
    DENG2_ERROR(InvalidLayerError);

    /// The requested decoration does not exist. @ingroup errors
    DENG2_ERROR(InvalidDecorationError);

    /// Required snapshot data is missing. @ingroup errors
    DENG2_ERROR(MissingSnapshotError);

public:
    MaterialVariant(material_t &generalCase, MaterialVariantSpec const &spec);
    ~MaterialVariant();

    /**
     * Retrieve the general case for this variant. Allows for a variant
     * reference to be used in place of a material (implicit indirection).
     *
     * @see generalCase()
     */
    inline operator material_t &() const {
        return generalCase();
    }

    /// @return  Superior material from which the variant was derived.
    material_t &generalCase() const;

    /// @return  Material variant specification for the variant.
    MaterialVariantSpec const &spec() const;

    /**
     * Returns @c true if animation of the variant is currently paused (e.g.,
     * the variant is for use with an in-game render context and the client has
     * paused the game).
     */
    bool isPaused() const;

    /**
     * Process a system tick event. If not currently paused and the material is
     * valid; layer stages are animated and state property values are updated
     * accordingly.
     *
     * @param ticLength  Length of the tick in seconds.
     *
     * @see isPaused()
     */
    void ticker(timespan_t ticLength);

    /**
     * Reset the staged animation point for this Material.
     */
    void resetAnim();

    /**
     * Returns the current state of @a layerNum for the variant.
     */
    LayerState const &layer(int layerNum);

    /**
     * Returns the current state of @a decorNum for the variant.
     */
    DecorationState const &decoration(int decorNum);

    /**
     * Attach new MaterialSnapshot data to the variant. If an existing snapshot
     * is already present it will be replaced. Ownership of @a materialSnapshot
     * is given to the variant.
     *
     * @return  Same as @a materialSnapshot for caller convenience.
     *
     * @see detachSnapshot(), snapshot()
     */
    MaterialSnapshot &attachSnapshot(MaterialSnapshot &snapshot);

    /**
     * Detach the MaterialSnapshot data from the variant, relinquishing ownership
     * to the caller.
     *
     * @see attachSnapshot(), detachSnapshot()
     */
    MaterialSnapshot *detachSnapshot();

    /**
     * Returns the MaterialSnapshot data from the variant or otherwise @c 0.
     * Ownership is unaffected.
     *
     * @see attachSnapshot(), detachSnapshot()
     */
    MaterialSnapshot *snapshot() const;

    /**
     * Returns the frame number when the variant's associated snapshot was last
     * prepared/updated.
     */
    int snapshotPrepareFrame() const;

    /**
     * Change the frame number when the variant's snapshot was last prepared/updated.
     * @param frameNum  Frame number to mark the snapshot with.
     */
    void setSnapshotPrepareFrame(int frameNum);

private:
    struct Instance;
    Instance *d;
};

} // namespace de

struct materialvariant_s; // The materialvariant instance (opaque).

#endif /* LIBDENG_RESOURCE_MATERIALVARIANT_H */
