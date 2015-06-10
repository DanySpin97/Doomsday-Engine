/** @file drawlist.h  Drawable primitive list.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_RENDER_DRAWLIST_H
#define DENG_CLIENT_RENDER_DRAWLIST_H

#include <array>
#include <QFlags>
#include <de/GLBuffer>
#include <de/Vector>
#include "api_gl.h" // blendmode_e
#include "gl/gltextureunit.h"

struct Store;

/// Semantic geometry group identifiers.
enum GeomGroup
{
    UnlitGeom = 0,  ///< Normal, unlit geometries.
    LitGeom,        ///< Normal, lit goemetries.
    SkyMaskGeom,    ///< Sky mask geometries.
    LightGeom,      ///< Dynamic light geometries.
    ShadowGeom,     ///< Map object and/or Fake Radio shadow geometries.
    ShineGeom       ///< Surface reflection geometries.
};

/// Logical drawing modes.
enum DrawMode
{
    DM_SKYMASK,
    DM_ALL,
    DM_LIGHT_MOD_TEXTURE,
    DM_FIRST_LIGHT,
    DM_TEXTURE_PLUS_LIGHT,
    DM_UNBLENDED_TEXTURE_AND_DETAIL,
    DM_BLENDED,
    DM_BLENDED_FIRST_LIGHT,
    DM_NO_LIGHTS,
    DM_WITHOUT_TEXTURE,
    DM_LIGHTS,
    DM_MOD_TEXTURE,
    DM_MOD_TEXTURE_MANY_LIGHTS,
    DM_UNBLENDED_MOD_TEXTURE_AND_DETAIL,
    DM_BLENDED_MOD_TEXTURE,
    DM_ALL_DETAILS,
    DM_BLENDED_DETAILS,
    DM_SHADOW,
    DM_SHINY,
    DM_MASKED_SHINY,
    DM_ALL_SHINY
};

/// Virtual/logical texture unit indices. These map to real GL texture units.
enum texunitid_t
{
    TU_PRIMARY = 0,
    TU_PRIMARY_DETAIL,
    TU_INTER,
    TU_INTER_DETAIL,
    NUM_TEXTURE_UNITS
};

struct AttributeSpec
{
    enum Semantic
    {
        TexCoord0,
        TexCoord1,
        ModTexCoord,

        NUM_SEMANTICS
    };
};
typedef std::array<de::dint, MAX_TEX_UNITS> TexUnitMap;

/**
 * A buffered list of drawable GL primitives and/or GL commands.
 */
class DrawList
{
public:
    struct Spec
    {
        GeomGroup group;
        de::GLTextureUnit texunits[NUM_TEXTURE_UNITS];

        Spec(GeomGroup group = UnlitGeom) : group(group)
        {}

        inline de::GLTextureUnit &unit(int index) {
            DENG2_ASSERT(index >= 0 && index < NUM_TEXTURE_UNITS);
            return texunits[index];
        }

        inline de::GLTextureUnit const &unit(int index) const {
            DENG2_ASSERT(index >= 0 && index < NUM_TEXTURE_UNITS);
            return texunits[index];
        }
    };

    typedef QVector<de::duint> Indices;

public:
    /**
     * Construct a new draw list.
     *
     * @param spec  List specification. A copy is made.
     */
    DrawList(Spec const &spec);

    /**
     * Write indices for a geometry primitive to the list.
     *
     * @param primitive        Type identifier for the GL primitive being written.
     * @param indices          Indices for the vertex elements in the backing store.
     * @param blendMode
     * @param oneLight
     * @param manyLights
     * @param texScale
     * @param texOffset
     * @param detailTexScale
     * @param detailTexOffset
     * @param modTexture       GL-name of the modulation texture; otherwise @c 0.
     * @param modColor         Modulation color.
     */
    DrawList &write(de::gl::Primitive primitive, Indices const &indices,
        blendmode_t blendMode, bool oneLight, bool manyLights,
        de::Vector2f const &texScale,       de::Vector2f const &texOffset,
        de::Vector2f const &detailTexScale, de::Vector2f const &detailTexOffset,
        GLuint modTexture = 0, de::Vector3f const &modColor = de::Vector3f());

    void draw(DrawMode mode, TexUnitMap const &texUnitMap) const;

    /**
     * Returns @c true iff there are no commands/geometries in the list.
     */
    bool isEmpty() const;

    /**
     * Clear the list of all buffered GL commands, returning it to the default,
     * empty state.
     */
    void clear();

    /**
     * Return the read/write cursor to the beginning of the list, retaining all
     * allocated storage for buffered GL commands so that it can be reused.
     *
     * To be called at the beginning of a new render frame before any geometry
     * is written to the list.
     */
    void rewind();

    /**
     * Provides mutable access to the list's specification. Note that any changes
     * to this configuration will affect @em all geometry in the list.
     */
    Spec &spec();

    /**
     * Provides immutable access to the list's specification.
     */
    Spec const &spec() const;

private:
    DENG2_PRIVATE(d)
};

typedef DrawList::Spec DrawListSpec;

#endif // DENG_CLIENT_RENDER_DRAWLIST_H
