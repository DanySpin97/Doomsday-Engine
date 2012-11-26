/**
 * @file textures.h
 * Textures collection. @ingroup resource
 *
 * "Clear"ing a Texture is to 'undefine' it - any names bound to it are deleted,
 * any GL textures acquired for it are 'released'. The Texture instance record
 * used to represent it is also deleted.
 *
 * "Release"ing a Texture will leave it defined (any names bound to it will
 * persist) and any GL textures acquired for it are so too released. Note that
 * the Texture instance record used to represent it will NOT be deleted.
 *
 * Thus there are two general states for textures in the collection:
 *
 *   1) Declared but not defined.
 *   2) Declared and defined.
 *
 * @authors Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_TEXTURES_H
#define LIBDENG_RESOURCE_TEXTURES_H

#include "dd_share.h"
#include "uri.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Unique identifier associated with each texture name in the collection.
typedef uint textureid_t;

/// Special value used to signify an invalid texture id.
#define NOTEXTUREID                 0

enum textureschemeid_e; // Defined in dd_share.h
struct texture_s;

/// Register the console commands, variables, etc..., of this module.
void Textures_Register(void);

/// Initialize this module.
void Textures_Init(void);

/// Shutdown this module.
void Textures_Shutdown(void);

/**
 * Try to interpret a texture scheme identifier from @a str. If found to match a known
 * scheme name, return the associated identifier. If the reference @a str is not valid
 * (i.e., NULL or a zero-length string) then the special identifier @c TS_ANY is returned.
 * Otherwise @c TS_INVALID.
 */
textureschemeid_t Textures_ParseSchemeName(const char* str);

/// @return  Name associated with the identified @a schemeId else a zero-length string.
const Str* Textures_SchemeName(textureschemeid_t schemeId);

/// @return  Total number of unique Textures in the collection.
uint Textures_Size(void);

/// @return  Number of unique Textures in the identified @a schemeId.
uint Textures_Count(textureschemeid_t schemeId);

/// Clear all textures in all schemes (and release any acquired GL-textures).
void Textures_Clear(void);

/// Clear all textures flagged 'runtime' (and release any acquired GL-textures).
void Textures_ClearRuntime(void);

/// Clear all textures flagged 'system' (and release any acquired GL-textures).
void Textures_ClearSystem(void);

/**
 * Clear all textures in the identified scheme(s) (and release any acquired GL-textures).
 *
 * @param schemeId  Unique identifier of the scheme to process or
 *                  @c TS_ANY to clear all textures in any scheme.
 */
void Textures_ClearScheme(textureschemeid_t schemeId);

/// @return  Unique identifier of the primary name for @a texture else @c NOTEXTUREID.
textureid_t Textures_Id(struct texture_s* texture);

/// @return  Texture associated with unique identifier @a textureId else @c NULL.
struct texture_s* Textures_ToTexture(textureid_t textureId);

/// @return  Texture associated with the scheme-unique identifier @a index else @c NOTEXTUREID.
textureid_t Textures_TextureForUniqueId(textureschemeid_t schemeId, int uniqueId);

struct texture_s *Textures_TextureForResourcePath(textureschemeid_t schemeId, Uri const *resourcePath);

/// @return  Scheme-unique identfier associated with the identified @a textureId.
int Textures_UniqueId(textureid_t textureId);

/// @return  Declared, percent-encoded path to this data resource,
///          else a "null" Uri (no scheme or path).
const Uri* Textures_ResourcePath(textureid_t textureId);

/// @return  Unique identifier of the scheme this name is in.
textureschemeid_t Textures_Scheme(textureid_t textureId);

/// @return  Symbolic, percent-encoded name/path-to this texture as a string.
///          Must be destroyed with Str_Delete().
AutoStr* Textures_ComposePath(textureid_t textureId);

/// @return  URI to this texture, percent-encoded. Must be destroyed with Uri_Delete().
Uri* Textures_ComposeUri(textureid_t textureId);

/// @return  Unique URN to this texture. Must be destroyed with Uri_Delete().
Uri* Textures_ComposeUrn(textureid_t textureId);

/**
 * Search the Textures collection for a texture associated with @a uri.
 *
 * @param uri  Either a path or a URN.
 * @return  Unique identifier of the found texture else @c NOTEXTUREID.
 */
textureid_t Textures_ResolveUri2(const Uri* uri, boolean quiet);
textureid_t Textures_ResolveUri(const Uri* uri); /*quiet=!(verbose >= 1)*/

/// Same as Textures::ResolveUri except @a uri is a C-string.
textureid_t Textures_ResolveUriCString2(const char* uri, boolean quiet);
textureid_t Textures_ResolveUriCString(const char* uri); /*quiet=!(verbose >= 1)*/

/**
 * Declare a texture in the collection. If a texture with the specified @a uri already
 * exists, its unique identifier is returned. If the given @a resourcePath differs from
 * that already defined for the pre-existing texture, any associated Texture instance
 * is released (and any GL-textures acquired for it).
 *
 * @param uri           Uri representing a path to the texture in the virtual hierarchy.
 * @param uniqueId      Scheme-unique identifier to associate with the texture.
 * @param resourcepath  The path to the underlying data resource.
 *
 * @return  Unique identifier for this texture unless @a uri is invalid, in which case
 *          @c NOTEXTUREID is returned.
 */
textureid_t Textures_Declare(Uri* uri, int uniqueId, Uri const* resourcePath);

/**
 * Create/update a Texture instance in the collection.
 *
 * @param id        Unique identifier of the previously declared Texture.
 * @param custom    @c true= this is a custom texture.
 * @param size      Logical size. Components can be @c 0 in which case their value will
 *                  be inherited from the actual pixel size of the texture at load time.
 * @param userData  User data to associate with the resultant texture.
 */
struct texture_s* Textures_CreateWithDimensions(textureid_t id, boolean custom, const Size2Raw* size, void* userData);
struct texture_s* Textures_Create(textureid_t id, boolean custom, void* userData); /* width=0, height=0*/

/**
 * Iterate over defined Textures in the collection making a callback for each visited.
 * Iteration ends when all textures have been visited or a callback returns non-zero.
 *
 * @param schemeId      If a valid scheme identifier, only consider textures in this
 *                      scheme, otherwise visit all textures.
 * @param callback      Callback function ptr.
 * @param parameters    Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Textures_Iterate2(textureschemeid_t schemeId, int (*callback)(struct texture_s* texture, void* parameters), void* parameters);
int Textures_Iterate(textureschemeid_t schemeId, int (*callback)(struct texture_s* texture, void* parameters)); /*parameters=NULL*/

/**
 * Iterate over declared textures in the collection making a callback for each visited.
 * Iteration ends when all textures have been visited or a callback returns non-zero.
 *
 * @param schemeId      If a valid scheme identifier, only consider textures in this
 *                      scheme, otherwise visit all textures.
 * @param callback      Callback function ptr.
 * @param parameters    Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Textures_IterateDeclared2(textureschemeid_t schemeId, int (*callback)(textureid_t textureId, void* parameters), void* parameters);
int Textures_IterateDeclared(textureschemeid_t schemeId, int (*callback)(textureid_t textureId, void* parameters)); /*parameters=NULL*/

int R_TextureUniqueId2(Uri const *uri, boolean quiet);
int R_TextureUniqueId(Uri const *uri/*, quiet = false */);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RESOURCE_TEXTURES_H */
