/**\file resourceresourceord.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010-2012 Daniel Swanson <danij@dengine.net>
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
 * You should have resourceeived a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBDENG_ABSTRACTRESOURCE_H
#define LIBDENG_ABSTRACTRESOURCE_H

#include "dd_string.h"
#include "uri.h"

/**
 * AbstractResource. (Record) Stores high-level metadata for a known resource.
 *
 * @ingroup core
 */
struct AbstractResource_s; // The abstractresource instance (opaque).
typedef struct AbstractResource_s AbstractResource;

AbstractResource* AbstractResource_NewWithName(resourceclass_t rclass, int rflags, const ddstring_t* name);
AbstractResource* AbstractResource_New(resourceclass_t rclass, int rflags);
void AbstractResource_Delete(AbstractResource* resource);

/**
 * Add a new symbolic name to the list of names for this.
 *
 * @param name  New name for this resource. Newer names have precedence.
 */
void AbstractResource_AddName(AbstractResource* resource, const ddstring_t* name);

/**
 * Add a new sub-resource identity key to the list for this.
 *
 * @param identityKey  New identity key (e.g., a lump/file name).
 */
void AbstractResource_AddIdentityKey(AbstractResource* resource, const ddstring_t* identityKey);

/**
 * Attempt to resolve a path to this resource.
 *
 * @return  Found path.
 */
const ddstring_t* AbstractResource_ResolvedPath(AbstractResource* resource, boolean canLocate);

void AbstractResource_Print(AbstractResource* resource, boolean printStatus);

/**
 * @return  String list of (potential) symbolic names delimited with semicolons.
 */
ddstring_t* AbstractResource_NameStringList(AbstractResource* resource);

/**
 * Accessor methods.
 */

/// @return  ResourceClass associated with this.
resourceclass_t AbstractResource_ResourceClass(AbstractResource* resource);

/// @return  ResourceFlags for this resource.
int AbstractResource_ResourceFlags(AbstractResource* resource);

/// @return  Array of IdentityKey(s) associated with sub-resources of this.
ddstring_t* const* AbstractResource_IdentityKeys(AbstractResource* resource);

Uri* const* AbstractResource_SearchPaths(AbstractResource* resource);

#endif /* LIBDENG_ABSTRACTRESOURCE_H */
