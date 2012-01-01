/**\file rend_dynlight.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RENDER_DYNLIGHT_H
#define LIBDENG_RENDER_DYNLIGHT_H

/// Paramaters for Rend_RenderLightProjections (POD).
typedef struct {
    uint lastIdx;
    const rvertex_t* rvertices;
    uint numVertices, realNumVertices;
    const float* texTL, *texBR;
    boolean isWall;
    const walldiv_t* divs;
} renderlightprojectionparams_t;

/**
 * Render all dynlights in projection list @a listIdx according to @a paramaters
 * writing them to the renderering lists for the current frame.
 *
 * \note If multi-texturing is being used for the first light; it is skipped.
 *
 * @return  Number of lights rendered.
 */
uint Rend_RenderLightProjections(uint listIdx, renderlightprojectionparams_t* paramaters);

#endif /* LIBDENG_RENDER_DYNLIGHT_H */
