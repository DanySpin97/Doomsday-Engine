/** @file materialdetailtexturelayer.h  Logical material, detail-texture layer.
 *
 * @authors Copyright © 2011-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_RESOURCE_MATERIALDETAILTEXTURELAYER_H
#define CLIENT_RESOURCE_MATERIALDETAILTEXTURELAYER_H

#include <de/String>
#include <doomsday/defs/dedtypes.h>
#include "Material"
#include "Texture"

/**
 * Specialized Material::Layer for describing an animated detail-texture layer.
 *
 * @ingroup resource
 */
class MaterialDetailTextureLayer : public Material::Layer
{
public:
    /**
     * Stages describe texture change animations.
     */
    class AnimationStage : public Stage
    {
    public:
        AnimationStage(de::Texture *texture, int tics,
                       float variance    = 0,
                       float scale       = 1,
                       float strength    = 1,
                       float maxDistance = 0);
        AnimationStage(AnimationStage const &other);
        virtual ~AnimationStage();

        /**
         * Construct a new AnimationStage from the given @a definition.
         */
        static AnimationStage *fromDef(ded_detail_stage_t const &definition);

        /**
         * Returns the detail Texture in effect for the animation stage.
         */
        de::Texture *texture() const;

        /**
         * Returns the detail texture scale factor in effect for the animation stage.
         */
        float scale() const;

        /**
         * Returns the detail texture blending factor in effect for the animation stage.
         */
        float strength() const;

        /**
         * Returns the maximum world distance in effect for the animation stage.
         */
        float maxDistance() const;

        de::String description() const;

    private:
        DENG2_PRIVATE(d)
    };

public:
    virtual ~MaterialDetailTextureLayer() {}

    de::String describe() const;

    /**
     * Construct a new DetailTextureLayer from the given @a definition.
     */
    static MaterialDetailTextureLayer *fromDef(ded_detailtexture_t const &definition);

    /**
     * Add a new animation stage to the detail texture layer.
     *
     * @param stage  New stage to add (a copy is made).
     *
     * @return  Index of the newly added stage (0 based).
     */
    int addStage(AnimationStage const &stage);

    /**
     * Lookup an AnimationStage by it's unique @a index.
     *
     * @param index  Index of the AnimationStage to lookup. Will be cycled into valid range.
     */
    AnimationStage &stage(int index) const;
};

#endif  // CLIENT_RESOURCE_MATERIALDETAILTEXTURELAYER_H
