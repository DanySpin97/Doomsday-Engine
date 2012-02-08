/**
 * @file gl_deferredapi.c
 * Implementation of GL API deferring. @ingroup gl
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#define LIBDENG_DEFERRED_GL_API_H // using regular GL API calls

#include "de_system.h"
#include "gl_defer.h"

static boolean __inline mustDefer(void)
{
    return !Sys_InMainThread();
}

#define GL_CALL1(form, func, p) \
    if(mustDefer()) GL_Defer##form(func, p); else func(p);

void Deferred_glEnable(GLenum e)
{
    GL_CALL1(1e, glEnable, e);
}

void Deferred_glDisable(GLenum e)
{
    GL_CALL1(1e, glDisable, e);
}
