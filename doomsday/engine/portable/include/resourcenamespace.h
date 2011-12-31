/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBDENG_SYSTEM_RESOURCENAMESPACE_H
#define LIBDENG_SYSTEM_RESOURCENAMESPACE_H

#include "dd_string.h"
#include "uri.h"

typedef enum {
    SPG_OVERRIDE = 0, // Override paths
    SPG_EXTRA, // Extra/runtime paths
    SPG_DEFAULT, // Default paths
    SPG_FALLBACK, // Last-resort paths
    SEARCHPATHGROUP_COUNT
} resourcenamespace_searchpathgroup_t;

#define VALID_RESOURCENAMESPACE_SEARCHPATHGROUP(g) ((g) >= SPG_OVERRIDE && (g) < SEARCHPATHGROUP_COUNT)

/**
 * Resource Namespace.
 *
 * @ingroup core
 */
// Number of entries in the hash table.
#define RESOURCENAMESPACE_HASHSIZE 512
typedef unsigned short resourcenamespace_namehash_key_t;

typedef struct {
    /// Sets of search paths known by this namespace.
    /// Each set is in order of greatest-importance, right to left.
    Uri** _searchPaths[SEARCHPATHGROUP_COUNT];
    uint _searchPathsCount[SEARCHPATHGROUP_COUNT];

    /// Name hash table.
    struct resourcenamespace_namehash_s* _nameHash;

    /// Resource name hashing callback.
    resourcenamespace_namehash_key_t (*_hashName) (const ddstring_t* name);
} resourcenamespace_t;

resourcenamespace_t* ResourceNamespace_New(
    resourcenamespace_namehash_key_t (*hashNameFunc) (const ddstring_t* name));

void ResourceNamespace_Delete(resourcenamespace_t* rn);

/**
 * Reset the namespace back to it's "empty" state (i.e., no known symbols).
 */
void ResourceNamespace_Clear(resourcenamespace_t* rnamespace);

/**
 * Add a new path to this namespace.
 *
 * @param path  New unresolved path to add.
 * @param group  Group to add this path to. @see resourcenamespace_searchpathgroup_t
 * @return  @c true if the path is correctly formed and present in the namespace.
 */
boolean ResourceNamespace_AddSearchPath(resourcenamespace_t* rn, const Uri* path,
    resourcenamespace_searchpathgroup_t group);

/**
 * Clear search paths in this namespace.
 * @param group  Search path group to be cleared.
 */
void ResourceNamespace_ClearSearchPaths(resourcenamespace_t* rn, resourcenamespace_searchpathgroup_t group);

/**
 * Compose the list of search paths into a @a delimited string.
 *
 * @param delimiter  Discreet paths will be delimited by this character.
 * @return  Resultant string which should be released with Str_Delete().
 */
ddstring_t* ResourceNamespace_ComposeSearchPathList2(resourcenamespace_t* rn, char delimiter);
ddstring_t* ResourceNamespace_ComposeSearchPathList(resourcenamespace_t* rn); /*delimiter= ';'*/

/**
 * Add a new named resource into the namespace. Multiple names for a given
 * resource may coexist however duplicates are automatically pruned.
 *
 * \post Name hash may have been rebuilt.
 *
 * @param name  Name of the resource being added.
 * @param node  PathDirectoryNode representing the resource in the owning PathDirectory.
 * @param userData  User data to be attached to the new resource record.
 * @return  @c true= If the namespace did not already contain this resource.
 */
boolean ResourceNamespace_Add(resourcenamespace_t* rn, const ddstring_t* name,
    PathDirectoryNode* node, void* userData);

/**
 * Iterate over resources in this namespace. Iteration ends when all
 * selected resources have been visited or a callback returns non-zero.
 *
 * @param name  If not @c NULL, only consider resources with this name.
 * @param callback  Callback function ptr.
 * @param paramaters  Passed to the callback.
 * @return  @c 0 iff iteration completed wholly.
 */
int ResourceNamespace_Iterate2(resourcenamespace_t* rn, const ddstring_t* name,
    int (*callback) (PathDirectoryNode* node, void* paramaters), void* paramaters);
int ResourceNamespace_Iterate(resourcenamespace_t* rn, const ddstring_t* name,
    int (*callback) (PathDirectoryNode* node, void* paramaters)); /*paramaters=NULL*/

#if _DEBUG
void ResourceNamespace_Print(resourcenamespace_t* rn);
#endif

#endif /* LIBDENG_SYSTEM_RESOURCENAMESPACE_H */
