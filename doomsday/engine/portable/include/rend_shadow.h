/**\file rend_shadow.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2011 Daniel Swanson <danij@dengine.net>
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

/**
 * Map Object Shadows
 */

#ifndef LIBDENG_RENDER_MOBJ_SHADOW_H
#define LIBDENG_RENDER_MOBJ_SHADOW_H

/**
 * This value defines the offset from the shadowed surface applied to
 * a shadow during render. As shadows are drawn using additional primitives
 * on top of the surface they touch a small visual offset is used in order
 * to prevent z-fighting.
 */
#define SHADOW_ZOFFSET              (.8f)

/// @return  @c true if rendering of mobj shadows is currently enabled.
boolean Rend_MobjShadowsEnabled(void);

void Rend_RenderMobjShadows(void);

#endif /* LIBDENG_RENDER_MOBJ_SHADOW_H */
