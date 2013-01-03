/** @file materials.h Material Collection.
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_MATERIALS_H
#define LIBDENG_RESOURCE_MATERIALS_H

#include "uri.h"
#include "def_data.h"
#include "material.h"

#ifdef __cplusplus

#include <de/Error>
#include <de/Path>
#include <de/String>
#include "resource/materialbind.h"
#include "resource/materialscheme.h"
#include "resource/materialvariant.h"

namespace de {

    class MaterialBind;

    /**
     * Specialized resource collection for a set of materials.
     *
     * - Pointers to Material are @em eternal, they are always valid and continue
     *   to reference the same logical material data even after engine reset.
     *
     * - Public material identifiers (materialid_t) are similarly eternal.
     *
     * - Material name bindings are semi-independant from the materials. There
     *   may be multiple name bindings for a given material (aliases). The only
     *   requirement is that their symbolic names must be unique among those in
     *   the same scheme.
     *
     * @ingroup resource
     */
    class Materials
    {
    public:
        typedef class MaterialScheme Scheme;

        /**
         * Flags determining URI validation logic.
         *
         * @see validateUri()
         */
        enum UriValidationFlag
        {
            AnyScheme  = 0x1 ///< The scheme of the URI may be of zero-length; signifying "any scheme".
        };
        Q_DECLARE_FLAGS(UriValidationFlags, UriValidationFlag)

        /// Material system subspace schemes.
        typedef QList<Scheme*> Schemes;

        /// Material animation groups.
        typedef QList<MaterialAnim> AnimGroups;

    public:
        /// The referenced texture was not found. @ingroup errors
        DENG2_ERROR(NotFoundError);

        /// An unknown scheme was referenced. @ingroup errors
        DENG2_ERROR(UnknownSchemeError);

        /// An unknown animation group was referenced. @ingroup errors
        DENG2_ERROR(UnknownAnimGroupError);

    public:
        /**
         * Constructs a new material collection.
         */
        Materials();

        virtual ~Materials();

        /// Register the console commands, variables, etc..., of this module.
        static void consoleRegister();

        /**
         * Returns the total number of unique materials in the collection.
         */
        uint size() const;

        /**
         * Returns the total number of unique materials in the collection.
         *
         * Same as size()
         */
        inline uint count() const {
            return size();
        }

        /// Process all outstanding tasks in the cache queue.
        void processCacheQueue();

        /// Empty the Material cache queue, cancelling all outstanding tasks.
        void purgeCacheQueue();

        /// To be called during a definition database reset to clear all links to defs.
        void clearDefinitionLinks();

        /**
         * Process a tic of @a elapsed length, animating materials and anim-groups.
         * @param elapsed  Length of tic to be processed.
         */
        void ticker(timespan_t elapsed);

        /**
         * Lookup a subspace scheme by symbolic name.
         *
         * @param name  Symbolic name of the scheme.
         * @return  Scheme associated with @a name.
         *
         * @throws UnknownSchemeError If @a name is unknown.
         */
        Scheme &scheme(String name) const;

        /**
         * Create a new subspace scheme.
         *
         * @param name      Unique symbolic name of the new scheme. Must be at
         *                  least @c Scheme::min_name_length characters long.
         */
        Scheme &createScheme(String name);

        /**
         * Returns @c true iff a Scheme exists with the symbolic @a name.
         */
        bool knownScheme(String name) const;

        /**
         * Returns a list of all the schemes for efficient traversal.
         */
        Schemes const &allSchemes() const;

        /**
         * Clear all materials in all schemes.
         * @see Scheme::clear().
         */
        inline void clearAllSchemes()
        {
            Schemes schemes = allSchemes();
            DENG2_FOR_EACH(Schemes, i, schemes){ (*i)->clear(); }
        }

        /**
         * Validate @a uri to determine if it is well-formed and is usable as a
         * search argument.
         *
         * @param uri       Uri to be validated.
         * @param flags     Validation flags.
         * @param quiet     @c true= Do not output validation remarks to the log.
         *
         * @return  @c true if @a Uri passes validation.
         *
         * @todo Should throw de::Error exceptions -ds
         */
        bool validateUri(Uri const &uri, UriValidationFlags flags = 0,
                         bool quiet = false) const;

        /**
         * Determines if a bind exists for a material on @a path.
         * @return @c true, if a bind exists; otherwise @a false.
         */
        bool has(Uri const &path) const;

        /**
         * Find the material bind on @a path.
         *
         * @param search  The search term.
         * @return Found material bind.
         */
        MaterialBind &find(Uri const &search) const;

        /**
         * Update @a material according to the supplied definition @a def.
         * To be called after an engine update/reset.
         *
         * @param material  Material to be updated.
         * @param def  Material definition to update using.
         */
        void rebuild(material_t &material, ded_material_t *def = 0);

        void updateTextureLinks(MaterialBind &bind);

        /// @return  (Particle) Generator definition associated with @a material else @c NULL.
        ded_ptcgen_t const *ptcGenDef(material_t &material);

        /// @return  Decoration defintion associated with @a material else @c NULL.
        ded_decor_t const *decorationDef(material_t &material);

        /// @return  @c true if one or more light decorations are defined for this material.
        bool hasDecorations(material_t &material);

        /**
         * Create a new Material unless an existing Material is found at the path
         * (and within the same scheme) as that specified in @a def, in which case
         * it is returned instead.
         *
         * @note: May fail on invalid definitions (return= @c NULL).
         *
         * @param def  Material definition to construct from.
         * @return  The newly-created/existing material; otherwise @c NULL.
         */
        material_t *newFromDef(ded_material_t &def);

        MaterialBind &newBind(MaterialScheme &scheme, Path const &path,
                              material_t *material = 0);

        /**
         * Prepare a MaterialVariantSpecification according to a usage context. If
         * incomplete context information is supplied, suitable default values will
         * be chosen in their place.
         *
         * @param materialContext   Material (usage) context identifier.
         * @param flags             @ref textureVariantSpecificationFlags
         * @param border            Border size in pixels (all edges).
         * @param tClass            Color palette translation class.
         * @param tMap              Color palette translation map.
         * @param wrapS             GL texture wrap/clamp mode on the horizontal axis (texture-space).
         * @param wrapT             GL texture wrap/clamp mode on the vertical axis (texture-space).
         * @param minFilter         Logical DGL texture minification level.
         * @param magFilter         Logical DGL texture magnification level.
         * @param anisoFilter       @c -1= User preference else a logical DGL anisotropic filter level.
         * @param mipmapped         @c true= use mipmapping.
         * @param gammaCorrection   @c true= apply gamma correction to textures.
         * @param noStretch         @c true= disallow stretching of textures.
         * @param toAlpha           @c true= convert textures to alpha data.
         *
         * @return  Rationalized (and interned) copy of the final specification.
         */
        materialvariantspecification_t const *variantSpecificationForContext(
            materialcontext_t materialContext, int flags, byte border, int tClass,
            int tMap, int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
            bool mipmapped, bool gammaCorrection, bool noStretch, bool toAlpha);

        /**
         * Add a variant of @a material to the cache queue for deferred preparation.
         *
         * @param material      Base Material from which to derive a variant.
         * @param spec          Specification for the desired derivation of @a material.
         * @param smooth        @c true= Select the current frame if the material is group-animated.
         * @param cacheGroups   @c true= variants for all Materials in any applicable animation
         *                      groups are desired, else just this specific Material.
         */
        void cache(material_t &material, materialvariantspecification_t const &spec,
                   bool smooth, bool cacheGroups = true);

        /**
         * Choose/create a variant of @a material which fulfills @a spec and then
         * immediately prepare it for render (e.g., upload textures if necessary).
         *
         * @note A convenient shorthand of the call tree:
         * <pre>
         *    Materials::prepareVariant( chooseVariant( @a material, @a spec, @a smooth, @c true ), @a forceSnapshotUpdate )
         * </pre>
         *
         * @param material  Base Material from which to derive a variant.
         * @param spec      Specification for the derivation of @a material.
         * @param smooth    @c true= Select the current frame if the material is group-animated.
         * @param forceSnapshotUpdate  @c true= Ensure to update the state snapshot.
         *
         * @return  Snapshot for the chosen and prepared variant of Material.
         */
        MaterialSnapshot const &prepare(material_t &material, materialvariantspecification_t const &spec,
                                        bool smooth, bool forceSnapshotUpdate = false);

        /**
         * Prepare variant @a material for render (e.g., upload textures if necessary).
         *
         * @see chooseVariant()
         *
         * @param material  MaterialVariant to be prepared.
         * @param forceSnapshotUpdate  @c true= Force an update of the variant's state snapshot.
         *
         * @return  Snapshot for the chosen and prepared variant of Material.
         */
        MaterialSnapshot const &prepare(MaterialVariant &material,
                                        bool forceSnapshotUpdate = false);

        /**
         * Choose/create a variant of @a material which fulfills @a spec.
         *
         * @param material      Material to derive the variant from.
         * @param spec          Specification for the derivation of @a material.
         * @param smooth        @c true= Select the current frame if the material is group-animated.
         * @param canCreate     @c true= Create a new variant if no suitable one exists.
         *
         * @return  Chosen variant; otherwise @c NULL if none suitable and not creating.
         */
        MaterialVariant *chooseVariant(material_t &material, materialvariantspecification_t const &spec,
                                       bool smoothed, bool canCreate);

        /// @todo Refactor away -ds
        MaterialBind *toMaterialBind(materialid_t materialId);

        /**
         * Lookup an animation group by unique @a number.
         */
        MaterialAnim &animGroup(int number) const;

        /**
         * Create a new animation group.
         * @return  Unique identifier associated with the new group.
         */
        int newAnimGroup(int flags);

        /**
         * To be called to reset all animation groups back to their initial state.
         */
        void resetAllAnimGroups();

        /**
         * To be called to destroy all animation groups when they are no longer needed.
         */
        void clearAllAnimGroups();

        /**
         * Provides access to the list of animation groups for efficient traversal.
         */
        AnimGroups const &allAnimGroups() const;

        /**
         * Returns the total number of animation/precache groups in the collection.
         */
        inline int animGroupCount() const {
            return allAnimGroups().count();
        }

    private:
        struct Instance;
        Instance *d;
    };

    Q_DECLARE_OPERATORS_FOR_FLAGS(Materials::UriValidationFlags)

} // namespace de

de::Materials *App_Materials();

extern "C" {
#endif // __cplusplus

/*
 * C wrapper API:
 */

/// Initialize this module. Cannot be re-initialized, must shutdown first.
void Materials_Init(void);

/// Shutdown this module.
void Materials_Shutdown(void);

void Materials_Ticker(timespan_t elapsed);
uint Materials_Size(void);
materialid_t Materials_Id(material_t *material);
material_t *Materials_ToMaterial(materialid_t materialId);
Uri *Materials_ComposeUri(materialid_t materialId);
boolean Materials_HasDecorations(material_t *material);
ded_ptcgen_t const *Materials_PtcGenDef(material_t *material);
boolean Materials_IsMaterialInAnimGroup(material_t *material, int animGroupNum);
materialid_t Materials_ResolveUri(Uri const *uri);

/// Same as Materials::resolveUri except @a uri is a C-string.
materialid_t Materials_ResolveUriCString(char const *uri);

int Materials_AnimGroupCount(void);
boolean Materials_IsPrecacheAnimGroup(int animGroupNum);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RESOURCE_MATERIALS_H */
