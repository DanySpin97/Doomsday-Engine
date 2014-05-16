/** @file material.h  Logical material resource.
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_RESOURCE_MATERIAL_H
#define DENG_RESOURCE_MATERIAL_H

#include "MapElement"
#include "audio/s_environ.h"
#include "world/dmuargs.h"
#ifdef __CLIENT__
#  include "MaterialContext"
#endif
#include "Texture"
#include <doomsday/uri.h>
#include <doomsday/defs/ded.h>
#include <de/Error>
#include <de/Observers>
#include <de/Vector>
#include <QFlag>
#include <QList>
#include <QMap>

// Forward declarations:

namespace de {

class MaterialManifest;
#ifdef __CLIENT__
class MaterialSnapshot;
struct MaterialVariantSpec;
#endif

}

/**
 * Logical material resource.
 *
 * @ingroup resource
 */
class Material : public de::MapElement
{
public:
    /// Maximum number of layers a material supports.
    static int const max_layers = 1;

#ifdef __CLIENT__
    /// Maximum number of (light) decorations a material supports.
    static int const max_decorations = 16;
#endif

    /// Required animation is missing. @ingroup errors
    DENG2_ERROR(MissingAnimationError);

    /// The referenced layer does not exist. @ingroup errors
    DENG2_ERROR(UnknownLayerError);

#ifdef __CLIENT__
    /// The referenced decoration does not exist. @ingroup errors
    DENG2_ERROR(UnknownDecorationError);
#endif

    /**
     * Notified when the material is about to be deleted.
     */
    DENG2_DEFINE_AUDIENCE(Deletion, void materialBeingDeleted(Material const &material))

    /**
     * Notified when the logical dimensions change.
     */
    DENG2_DEFINE_AUDIENCE(DimensionsChange, void materialDimensionsChanged(Material &material))

    /// @todo Define these values here instead of at API level
    enum Flag
    {
        //Unused1 = MATF_UNUSED1,

        /// Map surfaces using the material should never be drawn.
        NoDraw = MATF_NO_DRAW,

        /// Apply sky masking for map surfaces using the material.
        SkyMask = MATF_SKYMASK
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    /**
     * Each material consitutes at least one layer. Layers are arranged in a
     * stack according to the order in which they should be drawn, from the
     * bottom-most to the top-most layer.
     */
    class Layer
    {
    public:
        struct Stage
        {
            de::Texture *texture;
            int tics;
            float variance; // Stage variance (time).
            float glowStrength;
            float glowStrengthVariance;
            de::Vector2f texOrigin;

            Stage(de::Texture *_texture, int _tics,
                  float _variance               = 0,
                  float _glowStrength           = 0,
                  float _glowStrengthVariance   = 0,
                  de::Vector2f const _texOrigin = de::Vector2f())
                : texture(_texture), tics(_tics), variance(_variance),
                  glowStrength(_glowStrength),
                  glowStrengthVariance(_glowStrengthVariance),
                  texOrigin(_texOrigin)
            {}

            Stage(Stage const &other)
                : texture(other.texture),
                  tics(other.tics),
                  variance(other.variance),
                  glowStrength(other.glowStrength),
                  glowStrengthVariance(other.glowStrengthVariance),
                  texOrigin(other.texOrigin)
            {}

            static Stage *fromDef(ded_material_layer_stage_t const &def);
        };

        /// A list of stages.
        typedef QList<Stage *> Stages;

    public:
        /**
         * Construct a new default layer.
         */
        Layer();
        ~Layer();

        /**
         * Construct a new layer from the specified definition.
         */
        static Layer *fromDef(ded_material_layer_t const &def);

        /// @return  @c true if the layer is animated; otherwise @c false.
        inline bool isAnimated() const { return stageCount() > 1; }

        /**
         * Returns the total number of animation stages for the layer.
         */
        int stageCount() const;

        /**
         * Add a new animation stage to the layer.
         *
         * @param stage New stage to add (a copy is made).
         * @return  Index of the newly added stage (0-based).
         */
        int addStage(Stage const &stage);

        /**
         * Provides access to the animation stages for efficient traversal.
         */
        Stages const &stages() const;

    private:
        /// Animation stages.
        Stages _stages;
    };

    /// A list of layers.
    typedef QList<Material::Layer *> Layers;

    /// @todo $revise-texture-animation Merge with Material::Layer
    class DetailLayer
    {
    public:
        struct Stage
        {
            de::Texture *texture; // The file/lump with the detail texture.
            int tics;
            float variance;
            float scale;
            float strength;
            float maxDistance;

            Stage(de::Texture *_texture, int _tics,
                  float _variance    = 0,
                  float _scale       = 1,
                  float _strength    = 1,
                  float _maxDistance = 0)
                : texture(_texture), tics(_tics), variance(_variance),
                  scale(_scale), strength(_strength), maxDistance(_maxDistance)
            {}

            Stage(Stage const &other)
                : texture(other.texture),
                  tics(other.tics),
                  variance(other.variance),
                  scale(other.scale),
                  strength(other.strength),
                  maxDistance(other.maxDistance)
            {}

            static Stage *fromDef(ded_detail_stage_t const &def);
        };

        /// A list of stages.
        typedef QList<Stage *> Stages;

    public:
        /**
         * Construct a new default layer.
         */
        DetailLayer();
        ~DetailLayer();

        /**
         * Construct a new layer from the specified definition.
         */
        static DetailLayer *fromDef(ded_detailtexture_t const &def);

        /// @return  @c true if the layer is animated; otherwise @c false.
        inline bool isAnimated() const { return stageCount() > 1; }

        /**
         * Returns the total number of animation stages for the layer.
         */
        int stageCount() const;

        /**
         * Add a new animation stage to the layer.
         *
         * @param stage New stage to add (a copy is made).
         * @return  Index of the newly added stage (0-based).
         */
        int addStage(Stage const &stage);

        /**
         * Provides access to the animation stages for efficient traversal.
         */
        Stages const &stages() const;

    private:
        /// Animation stages.
        Stages _stages;
    };

    /// @todo $revise-texture-animation Merge with Material::Layer
    class ShineLayer
    {
    public:
        struct Stage
        {
            de::Texture *texture;
            int tics;
            float variance;
            de::Texture *maskTexture;
            blendmode_t blendMode; // Blend mode flags (bm_*).
            float shininess;
            de::Vector3f minColor;
            de::Vector2f maskDimensions;

            Stage(de::Texture *_texture, int _tics, float _variance,
                  de::Texture *_maskTexture           = 0,
                  blendmode_t _blendMode              = BM_ADD,
                  float _shininess                    = 1,
                  de::Vector3f const &_minColor       = de::Vector3f(0, 0, 0),
                  de::Vector2f const &_maskDimensions = de::Vector2f(1, 1))
                : texture(_texture), tics(_tics), variance(_variance),
                  maskTexture(_maskTexture), blendMode(_blendMode),
                  shininess(_shininess), minColor(_minColor),
                  maskDimensions(_maskDimensions)
            {}

            Stage(Stage const &other)
                : texture(other.texture),
                  tics(other.tics),
                  variance(other.variance),
                  maskTexture(other.maskTexture),
                  blendMode(other.blendMode),
                  shininess(other.shininess),
                  minColor(other.minColor),
                  maskDimensions(other.maskDimensions)
            {}

            static Stage *fromDef(ded_shine_stage_t const &def);
        };

        /// A list of stages.
        typedef QList<Stage *> Stages;

    public:
        /**
         * Construct a new default layer.
         */
        ShineLayer();
        ~ShineLayer();

        /**
         * Construct a new layer from the specified definition.
         */
        static ShineLayer *fromDef(ded_reflection_t const &def);

        /// @return  @c true if the layer is animated; otherwise @c false.
        inline bool isAnimated() const { return stageCount() > 1; }

        /**
         * Returns the total number of animation stages for the layer.
         */
        int stageCount() const;

        /**
         * Add a new animation stage to the layer.
         *
         * @param stage New stage to add (a copy is made).
         * @return  Index of the newly added stage (0-based).
         */
        int addStage(Stage const &stage);

        /**
         * Provides access to the animation stages for efficient traversal.
         */
        Stages const &stages() const;

    private:
        /// Animation stages.
        Stages _stages;
    };

#ifdef __CLIENT__

#undef min
#undef max

    /**
     * (Light) decoration.
     */
    class Decoration
    {
    public:
        struct Stage
        {
            struct LightLevels
            {
                float min;
                float max;

                LightLevels(float _min = 0, float _max = 0)
                    : min(_min), max(_max)
                {}

                LightLevels(float const minMax[2])
                    : min(minMax[0]), max(minMax[1])
                {}

                LightLevels(LightLevels const &other)
                    : min(other.min), max(other.max)
                {}

                /// Returns a textual representation of the lightlevels.
                de::String asText() const;
            };

            int tics;
            float variance; // Stage variance (time).
            de::Vector2f pos; // Coordinates on the surface.
            float elevation; // Distance from the surface.
            de::Vector3f color; // Light color.
            float radius; // Dynamic light radius (-1 = no light).
            float haloRadius; // Halo radius (zero = no halo).
            LightLevels lightLevels; // Fade by sector lightlevel.

            de::Texture *up, *down, *sides;
            de::Texture *flare;
            int sysFlareIdx; /// @todo Remove me

            Stage(int _tics, float _variance, de::Vector2f const &_pos, float _elevation,
                  de::Vector3f const &_color, float _radius, float _haloRadius,
                  LightLevels const &_lightLevels,
                  de::Texture *upTexture, de::Texture *downTexture, de::Texture *sidesTexture,
                  de::Texture *flareTexture,
                  int _sysFlareIdx = -1 /** @todo Remove me */)
                : tics(_tics), variance(_variance), pos(_pos), elevation(_elevation), color(_color),
                  radius(_radius), haloRadius(_haloRadius), lightLevels(_lightLevels),
                  up(upTexture), down(downTexture), sides(sidesTexture), flare(flareTexture),
                  sysFlareIdx(_sysFlareIdx)
            {}

            static Stage *fromDef(ded_decorlight_stage_t const &def);
        };

        /// A list of stages.
        typedef QList<Stage *> Stages;

    public:
        /**
         * Construct a new default decoration.
         */
        Decoration(de::Vector2i const &_patternSkip   = de::Vector2i(),
                   de::Vector2i const &_patternOffset = de::Vector2i());
        ~Decoration();

        /**
         * Construct a new decoration from the specified definition.
         */
        static Decoration *fromDef(ded_material_decoration_t const &def);

        /**
         * Construct a new decoration from the specified definition.
         */
        static Decoration *fromDef(ded_decoration_t const &def);

        /**
         * Returns the attributed owner material for the decoration.
         */
        Material &material();

        /// @copydoc material()
        Material const &material() const;

        /**
         * Change the attributed 'owner' material for the decoration. Usually
         * it is unnecessary to call this manually as adding the decoration to
         * a material will setup the attribution automatically.
         *
         * @param newOwner  New material to attribute as 'owner'. Use @c 0 to
         *                  clear the attribution.
         */
        void setMaterial(Material *newOwner);

        /// @return  @c true if the decoration is animated; otherwise @c false.
        inline bool isAnimated() const { return stageCount() > 1; }

        /**
         * Retrieve the pattern skip for the decoration. Normally a decoration
         * is repeated on a surface as many times as the material does. A skip
         * pattern allows sparser repeats on the horizontal and vertical axes
         * respectively.
         *
         * @see patternOffset()
         */
        de::Vector2i const &patternSkip() const;

        /**
         * Retrieve the pattern offset for the decoration. Used with pattern
         * skip to offset the origin of the pattern.
         *
         * @see patternSkip()
         */
        de::Vector2i const &patternOffset() const;

        /**
         * Returns the total number of animation stages for the decoration.
         */
        int stageCount() const;

        /**
         * Provides access to the animation stages for efficient traversal.
         */
        Stages const &stages() const;

    private:
        Material *_material;         ///< Owning Material.
        de::Vector2i _patternSkip;   ///< Pattern skip intervals.
        de::Vector2i _patternOffset; ///< Pattern skip interval offsets.
        Stages _stages;              ///< Animation stages.
    };

    /// A list of decorations.
    typedef QList<Decoration *> Decorations;

    /**
     * Animation (state).
     */
    class Animation
    {
        Animation(Material &material, MaterialContextId context);

    public:
        /**
         * Notified when a decoration stage change occurs.
         */
        DENG2_DEFINE_AUDIENCE(DecorationStageChange,
            void materialAnimationDecorationStageChanged(Animation &anim, Decoration &decor))

        /// Current state of a layer animation.
        struct LayerState
        {
            /// Animation stage else @c -1 => layer not in use.
            int stage;

            /// Remaining (sharp) tics in the current stage.
            short tics;

            /// Intermark from the current stage to the next [0..1].
            float inter;
        };

        /// Current state of a (light) decoration animation.
        struct DecorationState
        {
            /// Animation stage else @c -1 => decoration not in use.
            int stage;

            /// Remaining (sharp) tics in the current stage.
            short tics;

            /// Intermark from the current stage to the next [0..1].
            float inter;
        };

        /**
         * Returns the logical usage context identifier for the animation.
         */
        MaterialContextId context() const;

        /**
         * Process a system tick event. If not currently paused and still valid;
         * the material's layers and decorations are animated.
         *
         * @param ticLength  Length of the tick in seconds.
         *
         * @see isPaused()
         */
        void animate(timespan_t ticLength);

        /**
         * Returns @c true if animation is currently paused (e.g., this state
         * is for use with a context driven by the game timer (and the client
         * has paused the game)).
         */
        bool isPaused() const;

        /**
         * Restart the animation, resetting the staged animation point. The
         * state of all layers and decorations will be rewound to the beginning.
         */
        void restart();

        /**
         * Returns the current state of layer @a layerNum for the animation.
         */
        LayerState const &layer(int layerNum) const;

        /**
         * Returns the current state of the detail layer for the animation.
         *
         * @see Material::isDetailed()
         */
        LayerState const &detailLayer() const;

        /**
         * Returns the current state of the shine layer for the animation.
         *
         * @see Material::isShiny()
         */
        LayerState const &shineLayer() const;

        /**
         * Returns the current state of (light) decoration @a decorNum for
         * the animation.
         */
        DecorationState const &decoration(int decorNum) const;

        friend class Material;

    private:
        DENG2_PRIVATE(d)
    };

    /// An animation state for each usage context.
    typedef QMap<MaterialContextId, Animation *> Animations;

    /**
     * Context-specialized variant. Encapsulates all context variant values
     * and logics pertaining to a specialized version of the @em superior
     * material instance.
     *
     * Variant instances are only created by the superior material when asked
     * to @ref Material::prepare() for render using a context specialization
     * specification which it cannot fulfill/match.
     *
     * @see MaterialVariantSpec
     */
    class Variant
    {
        /**
         * @param generalCase  Material from which the variant is to be derived.
         * @param spec         Specification used to derive the variant.
         */
        Variant(Material &generalCase, de::MaterialVariantSpec const &spec);

    public:
        /**
         * Retrieve the general case for this variant. Allows for a variant
         * reference to be used in place of a material (implicit indirection).
         *
         * @see generalCase()
         */
        inline operator Material &() const {
            return generalCase();
        }

        /// @return  Superior material from which the variant was derived.
        Material &generalCase() const;

        /// @return  Material variant specification for the variant.
        de::MaterialVariantSpec const &spec() const;

        /**
         * Returns the usage context for this variant (from the spec).
         *
         * Same as @c spec().context
         *
         * @see spec()
         */
        MaterialContextId context() const;

        /**
         * Prepare the context variant for render (if necessary, uploading
         * GL textures and updating the state snapshot).
         *
         * @param forceSnapshotUpdate  @c true= Force an update of the state
         *      snapshot. The snapshot is automatically updated when first
         *      prepared for a new render frame. Typically the only time force
         *      is needed is when the material variant has changed since.
         *
         * @return  Snapshot for the variant.
         *
         * @see Material::chooseVariant(), Material::prepare()
         */
        de::MaterialSnapshot const &prepare(bool forceSnapshotUpdate = false);

        friend class Material;

    private:
        DENG2_PRIVATE(d)
    };

    /// A list of variant instances.
    typedef QList<Variant *> Variants;

#endif // __CLIENT__

public:
    /**
     * @param manifest  Manifest derived to yield the material.
     */
    Material(de::MaterialManifest &manifest);
    ~Material();

    /**
     * Returns the MaterialManifest derived to yield the material.
     */
    de::MaterialManifest &manifest() const;

    /**
     * Returns a brief textual description/overview of the material.
     *
     * @return Human-friendly description/overview of the material.
     */
    de::String description() const;

    /**
     * Returns a textual synopsis of the full material configuration.
     *
     * @return Human-friendly synopsis of the material's configuration.
     */
    de::String synopsis() const;

    /**
     * Returns @c true if the material is considered @em valid. A material is
     * only invalidated when resources it depends on (such as the definition
     * from which it was produced) are destroyed as result of runtime file
     * unloading.
     *
     * These 'orphaned' materials cannot be immediately destroyed as the game
     * may be holding on to pointers (which are considered eternal). Invalid
     * materials are instead disabled and then ignored until such time as the
     * current game is reset or changed.
     */
    bool isValid() const;

    /**
     * Mark the material as invalid.
     *
     * @see isValid();
     */
    void markValid(bool yes);

    /// Returns @c true if the material has at least one animated layer.
    bool isAnimated() const;

#ifdef __CLIENT__
    /**
     * Returns @c true if the material has one or more (light) decorations.
     *
     * Equivalent to:
     * @code
     *    decorationCount() != 0;
     * @endcode
     *
     * @see decorationCount()
     */
    inline bool isDecorated() const { return decorationCount() != 0; }
#endif

    /// Returns @c true if the material has a detail texturing layer.
    bool isDetailed() const;

    /// Returns @c true if the material is considered drawable.
    bool isDrawable() const { return !isFlagged(NoDraw); }

    /// Returns @c true if the material has a shine texturing layer.
    bool isShiny() const;

    /// Returns @c true if the material is considered @em skymasked.
    inline bool isSkyMasked() const { return isFlagged(SkyMask); }

    /// Returns @c true if one or more of the material's layers are glowing.
    bool hasGlow() const;

#ifdef __CLIENT__

    inline bool hasAnimatedLayers() const
    {
        foreach(Layer *layer, layers())
        {
            if(layer->isAnimated()) return true;
        }
        if(isDetailed() && detailLayer().isAnimated()) return true;
        if(isShiny() && shineLayer().isAnimated()) return true;
        return false;
    }

    inline bool hasAnimatedDecorations() const
    {
        foreach(Decoration *decor, decorations())
        {
            if(decor->isAnimated()) return true;
        }
        return false;
    }

#endif // __CLIENT__

    /**
     * Returns the dimensions of the material in map coordinate space units.
     */
    de::Vector2i const &dimensions() const;

    /// Returns the width of the material in map coordinate space units.
    inline int width() const { return dimensions().x; }

    /// Returns the height of the material in map coordinate space units.
    inline int height() const { return dimensions().y; }

    /**
     * Change the world dimensions of the material.
     * @param newDimensions  New dimensions in map coordinate space units.
     */
    void setDimensions(de::Vector2i const &newDimensions);

    /**
     * Change the world width of the material.
     * @param newWidth  New width in map coordinate space units.
     */
    void setWidth(int newWidth);

    /**
     * Change the world height of the material.
     * @param newHeight  New height in map coordinate space units.
     */
    void setHeight(int newHeight);

    /**
     * Returns @c true if the material is flagged @a flagsToTest.
     */
    inline bool isFlagged(Flags flagsToTest) const { return !!(flags() & flagsToTest); }

    /**
     * Returns the flags for the material.
     */
    Flags flags() const;

    /**
     * Change the material's flags.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param operation      Logical operation to perform on the flags.
     */
    void setFlags(Flags flagsToChange, de::FlagOp operation = de::SetFlags);

    /**
     * Add a new layer to the end of the material's layer stack. Ownership of the
     * layer is @em not transferred to the caller.
     *
     * @note As this invalidates the existing logical state, any previously derived
     *       context variants are cleared in the process (they will be automatically
     *       rebuilt later if/when needed).
     *
     * @param def  Definition for the new layer. Can be @c NULL in which case a
     *             default-initialized layer will be constructed.
     */
    Layer *newLayer(ded_material_layer_t const *def);

    /**
     * Add a new detail layer to the material. Note that only one detail layer is
     * supported (any existing layer will be replaced). Ownership of the layer is
     * @em not transferred to the caller.
     *
     * @note As this invalidates the existing logical state, any previously derived
     *       context variants are cleared in the process (they will be automatically
     *       rebuilt later if/when needed).
     *
     * @param def  Definition for the new layer. Can be @c NULL in which case a
     *             default-initialized layer will be constructed.
     */
    DetailLayer *newDetailLayer(ded_detailtexture_t const *def);

    /**
     * Add a new shine layer to the material. Note that only one shine layer is
     * supported (any existing layer will be replaced). Ownership of the layer is
     * @em not transferred to the caller.
     *
     * @note As this invalidates the existing logical state, any previously derived
     *       context variants are cleared in the process (they will be automatically
     *       rebuilt later if/when needed).
     *
     * @param def  Definition for the new layer. Can be @c NULL in which case a
     *             default-initialized layer will be constructed.
     */
    ShineLayer *newShineLayer(ded_reflection_t const *def);

    /**
     * Returns the number of material layers.
     */
    inline int layerCount() const { return layers().count(); }

    /**
     * Destroys all the material's layers.
     *
     * @note As this invalidates the existing logical state, any previously derived
     *       context variants are cleared in the process (they will be automatically
     *       rebuilt later if/when needed).
     */
    void clearLayers();

    /**
     * Provides access to the list of layers for efficient traversal.
     */
    Layers const &layers() const;

    /**
     * Provides access to the detail layer.
     *
     * @see isDetailed()
     */
    DetailLayer const &detailLayer() const;

    /**
     * Provides access to the shine layer.
     *
     * @see isShiny()
     */
    ShineLayer const &shineLayer() const;

#ifdef __CLIENT__
    /**
     * Returns the identifier of the audio environment for the material.
     */
    AudioEnvironmentId audioEnvironment() const;

    /**
     * Change the audio environment for the material.
     *
     * @param newEnvironment  New audio environment to apply.
     */
    void setAudioEnvironment(AudioEnvironmentId newEnvironment);

    /**
     * Returns the number of material (light) decorations.
     */
    int decorationCount() const { return decorations().count(); }

    /**
     * Destroys all the material's decorations.
     */
    void clearDecorations();

    /**
     * Add a new (light) decoration to the material.
     *
     * @param decor     Decoration to add.
     *
     * @todo Mark existing variant snapshots as 'dirty'.
     */
    void addDecoration(Decoration &decor);

    /**
     * Provides access to the list of decorations for efficient traversal.
     */
    Decorations const &decorations() const;

    /**
     * Retrieve the animation state for the specified render @a context.
     */
    Animation &animation(MaterialContextId context) const;

    /**
     * Provides access to the set of usage context animation states,
     * for efficient traversal.
     */
    Animations const &animations() const;

    /**
     * Returns the number of context variants for the material.
     */
    inline int variantCount() const { return variants().count(); }

    /**
     * Destroys all derived context variants for the material.
     */
    void clearVariants();

    /**
     * Provides access to the list of context variants for efficient traversal.
     */
    Variants const &variants() const;

    /**
     * Choose/create a context variant of the material which fulfills @a spec.
     *
     * @param spec      Material specialization specification.
     * @param canCreate @c true= Create a new variant if no suitable one exists.
     *
     * @return  The chosen variant; otherwise @c 0 (if none suitable, when not creating).
     */
    Variant *chooseVariant(de::MaterialVariantSpec const &spec, bool canCreate = false);

    /**
     * Shorthand alternative to @c chooseVariant(@a spec, true).
     *
     * @param spec      Material specialization specification.
     * @return  The (perhaps newly created) variant.
     *
     * @see chooseVariant()
     */
    inline Variant &createVariant(de::MaterialVariantSpec const &spec)
    {
        return *chooseVariant(spec, true/*create*/);
    }

    /**
     * Choose/create a variant of the material which fulfills @a spec and then
     * immediately prepare it for render (e.g., upload textures if necessary).
     *
     * Intended as a convenient shorthand of the call tree:
     * @code
     *    chooseVariant(@a spec, true)->prepare(@a forceSnapshotUpdate)
     * @endcode
     *
     * @param spec  Specification with which to derive the variant.
     * @param forceSnapshotUpdate  @c true= Force an update of the variant's
     *      state snapshot. The snapshot is automatically updated when first
     *      prepared for a new render frame. Typically the only time force is
     *      needed is when the material variant has changed since.
     *
     * @return  Snapshot for the chosen and prepared variant of Material.
     *
     * @see ResourceSystem::materialSpec(), chooseVariant(), Variant::prepare()
     */
    inline de::MaterialSnapshot const &prepare(de::MaterialVariantSpec const &spec,
        bool forceSnapshotUpdate = false)
    {
        return createVariant(spec).prepare(forceSnapshotUpdate);
    }

#endif // __CLIENT__

protected:
    int property(DmuArgs &args) const;

public:
        /// Register the console commands, variables, etc..., of this module.
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Material::Flags)

typedef Material::Layer MaterialLayer;
typedef Material::DetailLayer MaterialDetailLayer;
typedef Material::ShineLayer MaterialShineLayer;

// Aliases.
#ifdef __CLIENT__
typedef Material::Animation MaterialAnimation;
typedef Material::Decoration MaterialDecoration;
typedef Material::Variant MaterialVariant;
#endif

#endif // DENG_RESOURCE_MATERIAL_H
