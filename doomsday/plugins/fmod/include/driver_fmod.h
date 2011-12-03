/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef __DSFMOD_DRIVER_H__
#define __DSFMOD_DRIVER_H__

#include <fmod.h>
#include <fmod.hpp>
#include <fmod_errors.h>
#include <cassert>
#include <iostream>

extern "C" {
    
int     DS_Init(void);
void    DS_Shutdown(void);
void    DS_Event(int type);

}

#define DSFMOD_TRACE(args)  std::cerr << args << std::endl;

#ifdef _DEBUG
#  define DSFMOD_ERRCHECK(result) \
    if(result != FMOD_OK) { \
        printf("FMOD error at %s, line %i: (%d) %s\n", __FILE__, __LINE__, result, FMOD_ErrorString(result)); \
    }
#else
#  define DSFMOD_ERRCHECK(result)
#endif

extern FMOD::System* fmodSystem;

#include "fmod_sfx.h"
#include "fmod_music.h"
#include "fmod_cd.h"

#endif /* end of include guard: __DSFMOD_DRIVER_H__ */
