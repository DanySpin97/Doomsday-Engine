/**
 * @file uri.h
 * Universal Resource Identifier.
 *
 * @ingroup base
 *
 * Convenient interface class designed to assist working with URIs (Universal
 * Resource Identifier) to engine-managed resources. This class is based on the
 * semantics defined for the QUrl class, a component of the Qt GUI Toolkit.
 *
 * @todo Derive from Qt::QUrl
 *
 * @authors Daniel Swanson <danij@dengine.net>
 * @authors Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
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

#ifndef LIBDENG_API_URI_H
#define LIBDENG_API_URI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dd_string.h"
#include "reader.h"
#include "writer.h"

struct uri_s; // The uri instance (opaque).

/**
 * Uri instance. Created with Uri_New() or one of the other constructors.
 */
typedef struct uri_s Uri;

/// Schemes must be at least this many characters.
#define URI_MINSCHEMELENGTH         2

/**
 * @defgroup uriComponentFlags  Uri Component Flags
 *
 * Flags which identify the logical components of a uri. Used with Uri_Write() to
 * indicate which components of the Uri should be serialized.
 */
///{
#define UCF_SCHEME                   0x1 ///< Scheme.
#define UCF_USER                     0x2 ///< User. (Reserved) Not presently implemented.
#define UCF_PASSWORD                 0x4 ///< Password. (Reserved) Not presently implemented.
#define UCF_HOST                     0x8 ///< Host. (Reserved) Not presently implemented.
#define UCF_PORT                    0x10 ///< Port. (Reserved) Not presently implemented.
#define UCF_PATH                    0x20 ///< Path.
#define UCF_FRAGMENT                0x40 ///< Fragment. (Reserved) Not presently implemented.
#define UCF_QUERY                   0x80 ///< Query. (Reserved) Not presently implemented.
///}

/**
 * Constructs a default (empty) Uri instance. The uri should be destroyed with
 * Uri_Delete() once it is no longer needed.
 */
Uri* Uri_New(void);

/**
 * Constructs a Uri instance from @a path. The uri should be destroyed with
 * Uri_Delete() once it is no longer needed.
 *
 * @param path           Path to be parsed. Assumed to be in percent-encoded representation.
 * @param resourceClass  If no scheme is defined in @a path and this is not @c RC_NULL,
 *                       look for an appropriate default scheme for this class of resource.
 */
Uri* Uri_NewWithPath2(const char* path, resourceclass_t defaultResourceClass);
Uri* Uri_NewWithPath(const char* path);

/**
 * Constructs a Uri instance by duplicating @a other. The uri should be destroyed
 * with Uri_Delete() once it is no longer needed.
 */
Uri* Uri_NewCopy(const Uri* other);

/**
 * Constructs a Uri instance by reading it from @a reader.  The uri should be
 * destroyed with Uri_Delete() once it is no longer needed.
 */
Uri* Uri_NewFromReader(Reader* reader);

/**
 * Destroys the uri.
 */
void Uri_Delete(Uri* uri);

/**
 * Clears the uri, returning it to an empty state.
 * @param uri  Uri instance.
 */
void Uri_Clear(Uri* uri);

/**
 * Copy the contents of @a other into this uri.
 *
 * @param uri  Uri instance.
 * @param other  Uri to be copied.
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_Copy(Uri* uri, const Uri* other);

/**
 * Attempt to compose a resolved copy of this Uri. Substitutes known symbolics
 * in the possibly templated path. Resulting path is a well-formed, filesys
 * compatible path (perhaps base-relative).
 *
 * @return  Resolved path else @c NULL if non-resolvable. Caller should ensure
 *          to Str_Delete() when no longer needed.
 */
ddstring_t* Uri_Resolved(const Uri* uri);

/**
 * @param uri  Uri instance.
 * @return  Plain-text String representation of the current scheme.
 */
const ddstring_t* Uri_Scheme(const Uri* uri);

/**
 * @param uri  Uri instance.
 * @return  Plain-text String representation of the current path.
 */
const ddstring_t* Uri_Path(const Uri* uri);

/**
 * @param path  New scheme to be parsed.
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_SetScheme(Uri* uri, const char* scheme);

/**
 * @param path  New path to be parsed.
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_SetPath(Uri* uri, const char* path);

/**
 * Update uri by parsing new values from the specified arguments.
 *
 * @param uri            Uri instance.
 * @param path           Path to be parsed. Assumed to be in percent-encoded representation.
 * @param resourceClass  If no scheme is defined in @a path and this is not @c RC_NULL,
 *                       look for an appropriate default scheme for this class of resource.
 *
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_SetUri3(Uri* uri, const char* path, resourceclass_t defaultResourceClass);
Uri* Uri_SetUri2(Uri* uri, const char* path);
Uri* Uri_SetUri(Uri* uri, const ddstring_t* path);

/**
 * Transform the uri into a plain-text representation. Any internal encoding method
 * or symbolic identifiers will be included in their original, unresolved form in
 * the resultant string.
 *
 * @param uri  Uri instance.
 * @return  Plain-text String representation. Should be destroyed with Str_Delete()
 *          once no longer needed.
 */
ddstring_t* Uri_Compose(const Uri* uri);

/**
 * Transform the uri into a human-friendly representation.
 *
 * @param uri  Uri instance.
 * @param percentDecode  Apply percent-decoding to the composed path.
 *
 * @return  Human-friendly String representation. Should be destroyed with Str_Delete()
 *          once no longer needed.
 */
ddstring_t* Uri_ToString2(const Uri* uri, boolean percentDecode);
ddstring_t* Uri_ToString(const Uri* uri); /*percentDecode=true*/

/**
 * Are these two uri instances considered equal once resolved?
 *
 * @todo Return a delta of lexicographical difference.
 *
 * @param uri  Uri instance.
 * @param other  Other uri instance.
 */
boolean Uri_Equality(const Uri* uri, const Uri* other);

/**
 * Serialize @a uri using @a writer.
 *
 * @note Scheme should only be omitted when it can be unambiguously deduced from context.
 *
 * @param uri  Uri instance.
 * @param omitComponents  @ref uriComponentFlags
 */
void Uri_Write2(const Uri* uri, Writer* writer, int omitComponents);
void Uri_Write(const Uri* uri, Writer* writer);

/**
 * Deserializes @a uri using @a reader.
 *
 * @param uri  Uri instance.
 * @param reader  Reader instance.
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_Read(Uri* uri, Reader* reader);

/**
 * Deserializes @a uri using @a reader. If the deserialized Uri lacks a scheme,
 * @a defaultScheme will be used instead.
 *
 * @param uri  Uri instance.
 * @param reader  Reader instance.
 * @param defaultScheme  Default scheme.
 */
void Uri_ReadWithDefaultScheme(Uri* uri, Reader* reader, const char* defaultScheme);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_API_URI_H */
