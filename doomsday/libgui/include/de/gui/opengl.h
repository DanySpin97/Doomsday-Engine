/** @file opengl.h  Headers for OpenGL (ES) 2.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBGUI_SYSTEM_OPENGL_H
#define LIBGUI_SYSTEM_OPENGL_H

#ifdef WIN32
#  include "glentrypoints.h"
#endif

#if defined(UNIX) && !defined(MACOSX)
#  define GL_GLEXT_PROTOTYPES
#  include <GL/gl.h>
#  include <GL/glext.h>
#endif

#include <QtOpenGL>

#ifdef MACOSX
#  include <OpenGL/gl.h>
#endif

// Defined in GLES2.
#ifndef GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS
#  define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS 0x8CD9
#endif

/*
 * The LIBGUI_GLES2 macro is defined when the OpenGL ES 2.0 API is in use.
 */
//#define LIBGUI_GLES2

#endif // LIBGUI_SYSTEM_OPENGL_H
