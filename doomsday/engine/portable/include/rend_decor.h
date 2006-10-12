/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * rend_decor.h: Decorations
 */

#ifndef __DOOMSDAY_RENDER_DECOR_H__
#define __DOOMSDAY_RENDER_DECOR_H__

extern byte     useDecorations;
extern float    decorWallMaxDist;  // No decorations are visible beyond this.
extern float    decorPlaneMaxDist;
extern float    decorWallFactor;
extern float    decorPlaneFactor;
extern float    decorFadeAngle;

void            Rend_DecorRegister(void);
void            Rend_InitDecorationsForFrame(void);
void            Rend_ProjectDecorations(void);

#endif
