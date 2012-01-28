/**
 * @file r_common.h
 * Common routines for refresh.
 *
 * @ingroup libcommon
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_REFRESH_H
#define LIBCOMMON_REFRESH_H

// Translate between fixed screen dimensions to actual, current.
#define FIXXTOSCREENX(x) (portSize.width * ((x) / (float) SCREENWIDTH))
#define FIXYTOSCREENY(y) (portSize.height * ((y) / (float) SCREENHEIGHT))
#define SCREENXTOFIXX(x) ((float) SCREENWIDTH * ((x) / portSize.width))
#define SCREENYTOFIXY(y) ((float) SCREENHEIGHT * ((y) / portSize.height))

/**
 * @defgroup resizeViewWindowFlags  Resize ViewWindow Flags.
 */
///@{
#define RWF_FORCE               0x1 /// Update regardless of tracked state.
#define RWF_NO_LERP             0x2 /// Do not interpolate.
///@}

/**
 * @param flags  @ref resizeViewWindowFlags
 */
void R_ResizeViewWindow(int flags);

/**
 * To be called when a viewport changes dimensions.
 */
int R_UpdateViewport(int hookType, int param, void* data);

void R_UpdateConsoleView(int player);

void R_PrecachePSprites(void);

void R_GetGammaMessageStrings(void);
void R_CycleGammaLevel(void);

#endif /* LIBCOMMON_REFRESH_H */
