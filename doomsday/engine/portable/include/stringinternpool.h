/**\file stringinternpool.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010-2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_STRINGINTERNPOOL_H
#define LIBDENG_STRINGINTERNPOOL_H

#include "dd_string.h"

struct stringinternpool_namerecord_s;

// Intern name identifier.
typedef uint stringinternpool_nameid_t;

/**
 * String Intern Pool.
 *
 * @ingroup data
 */
typedef struct stringinternpool_s {
    uint _namesCount;
    stringinternpool_namerecord_t* _names;

    /// Sorted redirection table.
    stringinternpool_nameid_t* _sortedNameIdMap;
} stringinternpool_t;

#endif /* LIBDENG_STRINGINTERNPOOL_H */
