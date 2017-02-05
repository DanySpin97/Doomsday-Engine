/** @file gl_deferredapi.cpp GL API deferring.
 * @ingroup gl
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#define LIBDENG_DISABLE_DEFERRED_GL_API // using regular GL API calls

#include "de_platform.h"

#include <de/concurrency.h>
#include <de/GLInfo>
#include "gl/gl_defer.h"

static dd_bool __inline mustDefer(void)
{
    return !Sys_InMainThread();
}

static void GL_CALL deng_glEnable(GLenum e)
{
    LIBGUI_GL.glEnable(e);
}

static void GL_CALL deng_glDisable(GLenum e)
{
    LIBGUI_GL.glDisable(e);
}

static void GL_CALL deng_glDeleteTextures(GLsizei num, GLuint const *names)
{
    LIBGUI_GL.glDeleteTextures(num, names);
}

static void GL_CALL deng_glFogi(GLenum p, GLint v)
{
    LIBGUI_GL.glFogi(p, v);
}

static void GL_CALL deng_glFogf(GLenum p, GLfloat v)
{
    LIBGUI_GL.glFogf(p, v);
}

static void GL_CALL deng_glFogfv(GLenum p, GLfloat const *v)
{
    LIBGUI_GL.glFogfv(p, v);
}

#define GL_CALL1(form, func, x) \
    if(mustDefer()) GL_Defer_##form(func, x); else func(x);
#define GL_CALL2(form, func, x, y) \
    if(mustDefer()) GL_Defer_##form(func, x, y); else func(x, y);

DENG_EXTERN_C void Deferred_glEnable(GLenum e)
{
    GL_CALL1(e, deng_glEnable, e);
}

DENG_EXTERN_C void Deferred_glDisable(GLenum e)
{
    GL_CALL1(e, deng_glDisable, e);
}

DENG_EXTERN_C void Deferred_glDeleteTextures(GLsizei num, GLuint const *names)
{
    GL_CALL2(uintArray, deng_glDeleteTextures, num, names);
}

DENG_EXTERN_C void Deferred_glFogi(GLenum p, GLint v)
{
    GL_CALL2(i, deng_glFogi, p, v);
}

DENG_EXTERN_C void Deferred_glFogf(GLenum p, GLfloat v)
{
    GL_CALL2(f, deng_glFogf, p, v);
}

DENG_EXTERN_C void Deferred_glFogfv(GLenum p, GLfloat const *v)
{
    GL_CALL2(fv4, deng_glFogfv, p, v);
}
