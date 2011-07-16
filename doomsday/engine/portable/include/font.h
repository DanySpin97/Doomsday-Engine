/**\file font.h
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

#ifndef LIBDENG_FONT_H
#define LIBDENG_FONT_H

#include "dd_types.h"

// Font types.
typedef enum {
    FT_FIRST = 0,
    FT_BITMAP = FT_FIRST,
    FT_BITMAPCOMPOSITE,
    FT_LAST = FT_BITMAPCOMPOSITE,
} fonttype_t;

#define VALID_FONTTYPE(v)       ((v) >= FT_FIRST && (v) <= FT_LAST)

/**
 * @defgroup fontFlags  Font Flags.
 */
/*@{*/
#define FF_COLORIZE             0x1 /// Font can be colored.
#define FF_SHADOWED             0x2 /// Font has an embedded shadow.
/*@}*/

/**
 * Abstract Font base. To be used as the basis for all types of font.
 * @ingroup refresh
 */
#define MAX_CHARS               256 // Normal 256 ANSI characters.
typedef struct font_s {
    fonttype_t _type;

    /// @c true= Font requires a full update.
    boolean _isDirty;

    /// @see fontFlags.
    int _flags;

    /// Unique identifier of the FontBind associated with this or @c NULL if not bound.
    uint _bindId;

    /// Font metrics.
    int _leading;
    int _ascent;
    int _descent;

    int _noCharWidth;
    int _noCharHeight;

    /// dj: Do fonts have margins? Is this a pixel border in the composited
    /// character map texture (perhaps per-glyph)?
    int _marginWidth, _marginHeight;
} font_t;

void Font_Init(font_t* font, fonttype_t type);

fonttype_t Font_Type(const font_t* font);
/// @return  @see fontFlags
int Font_Flags(const font_t* font);
uint Font_BindId(const font_t* font);
void Font_SetBindId(font_t* font, uint bindId);
int Font_Ascent(font_t* font);
int Font_Descent(font_t* font);
int Font_Leading(font_t* font);
boolean Font_IsPrepared(font_t* font);

#endif /* LIBDENG_FONT_H */
