/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * dd_def.h: Internal Doomsday Macros and Constants
 */

#ifndef __DOOMSDAY_DEFS_H__
#define __DOOMSDAY_DEFS_H__

/**
 * @defgroup flags Flags (Internal)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dd_types.h"
#include "dd_api.h"

#ifdef WIN32
// Disable annoying MSVC warnings.
// 4761: integral size mismatch in argument
// 4244: conversion from 'type1' to 'type2', possible loss of data
#pragma warning (disable:4761 4244)
#endif

// if rangecheck is undefined, most parameter validation debugging code
// will not be compiled
#ifndef NORANGECHECKING
#   define RANGECHECK
#endif

#ifdef RANGECHECK
#   define DOOMSDAY_VER_ID_RANGECHECK " +R"
#else
#   define DOOMSDAY_VER_ID_RANGECHECK ""
#endif

#ifdef _DEBUG
#   define DOOMSDAY_VER_ID_DEBUG " +D"
#else
#   define DOOMSDAY_VER_ID_DEBUG ""
#endif

#ifdef __64BIT__
#   define DOOMSDAY_VER_ID_64BIT " 64-bit"
#else
#   define DOOMSDAY_VER_ID_64BIT " 32-bit"
#endif

#define DOOMSDAY_VER_ID DOOMSDAY_RELEASE_TYPE DOOMSDAY_VER_ID_64BIT DOOMSDAY_VER_ID_DEBUG DOOMSDAY_VER_ID_RANGECHECK

#define DOOMSDAY_VERSION_FULLTEXT DOOMSDAY_VERSION_TEXT" ("DOOMSDAY_VER_ID") "__DATE__" "__TIME__

#define SAFEDIV(x,y)    (!(y) || !((x)/(y))? 1 : (x)/(y))
#define ORDER(x,y,a,b)  ( (x)<(y)? ((a)=(x),(b)=(y)) : ((b)=(x),(a)=(y)) )
#define LAST_CHAR(str)  (str[strlen(str) - 1])

#ifdef _DEBUG
#  define ASSERT_64BIT(p) {if(sizeof(p) != 8) Con_Error(#p" is not 64-bit in "__FILE__" at line %i.\n", __LINE__);}
#  define ASSERT_NOT_64BIT(p) {if(sizeof(p) == 8) Con_Error(#p" is 64-bit in "__FILE__" at line %i.\n", __LINE__);}
#  define ASSERT_32BIT(p) {if(sizeof(p) != 4) Con_Error(#p" is not 32-bit in "__FILE__" at line %i.\n", __LINE__);}
#  define ASSERT_16BIT(p) {if(sizeof(p) != 2) Con_Error(#p" is not 16-bit in "__FILE__" at line %i.\n", __LINE__);}
#else
#  define ASSERT_64BIT(p)
#  define ASSERT_NOT_64BIT(p)
#  define ASSERT_32BIT(p)
#  define ASSERT_16BIT(p)
#endif

#define MAXEVENTS       256
#define SBARHEIGHT      39         // status bar height at bottom of screen
#define PI              3.14159265359
#define PI_D            3.14159265358979323846
#define DEG2RAD(a)      (a * PI_D) / 180.0
#define RAD2DEG(a)      (a / PI_D) * 180.0

#define SECONDS_TO_TICKS(sec) ((int)(sec*35))

// Heap relations.
#define HEAP_PARENT(i)  (((i) + 1)/2 - 1)
#define HEAP_LEFT(i)    (2*(i) + 1)
#define HEAP_RIGHT(i)   (2*(i) + 2)

enum { VX, VY, VZ };               // Vertex indices.
enum { CR, CG, CB, CA };           // Color indices.

// dd_pinit.c
extern game_export_t __gx;
extern game_import_t __gi;

#define gx __gx
#define gi __gi

extern byte     gammaTable[256];
extern float    texGamma;

// tab_tables.c
extern fixed_t  finesine[5 * FINEANGLES / 4];
extern fixed_t *fineCosine;

#endif
