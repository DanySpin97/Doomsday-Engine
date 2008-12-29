/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * sys_audiod_loader.h.c: Loader for ds*.so
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#ifdef UNIX
#  include "sys_dylib.h"
#endif

#include "de_console.h"

#include "sys_audiod.h"
#include "sys_audiod_sfx.h"
#include "sys_audiod_mus.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

audiodriver_t sfxdExternal;

sfxinterface_sfx_t sfxdExternalISFX;
musinterface_mus_t musdExternalIMus;
musinterface_ext_t musdExternalIExt;
musinterface_cd_t musdExternalICD;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static lt_dlhandle handle = NULL;

// CODE --------------------------------------------------------------------

static void* Imp(const char* fn)
{
    return lt_dlsym(handle, fn);
}

void DS_UnloadExternal(void)
{
    driverShutdown();
    lt_dlclose(handle);
    handle = NULL;
}

audiodriver_t* DS_ImportExternal(void)
{
    audiodriver_t*        d = &sfxdExternal;

    // Clear everything.
    memset(d, 0, sizeof(*d));

    d->Init = Imp("DS_Init");
    d->Shutdown = Imp("DS_Shutdown");
    d->Event = Imp("DS_Event");

    // The driver may provide SFX playback functionality.
    if(Imp("DS_SFX_Init"))
    {   // The driver offers a SFX playback interface.
        sfxinterface_sfx_t* i = &sfxdExternalISFX;

        i->gen.Init = Imp("DS_SFX_Init");
        i->gen.Create = Imp("DS_SFX_CreateBuffer");
        i->gen.Destroy = Imp("DS_SFX_DestroyBuffer");
        i->gen.Load = Imp("DS_SFX_Load");
        i->gen.Reset = Imp("DS_SFX_Reset");
        i->gen.Play = Imp("DS_SFX_Play");
        i->gen.Stop = Imp("DS_SFX_Stop");
        i->gen.Refresh = Imp("DS_SFX_Refresh");

        i->gen.Set = Imp("DS_SFX_Set");
        i->gen.Setv = Imp("DS_SFX_Setv");
        i->gen.Listener = Imp("DS_SFX_Listener");
        i->gen.Listenerv = Imp("DS_SFX_Listenerv");
        i->gen.Getv = Imp("DS_SFX_Getv");
    }

    // The driver may provide music playback functionality.
    if(Imp("DM_Mus_Init"))
    {   // The driver offers a MUS music playback interface.
        musinterface_mus_t* i = &musdExternalIMus;

        i->gen.Init = Imp("DM_Mus_Init");
        i->gen.Update = Imp("DM_Mus_Update");
        i->gen.Get = Imp("DM_Mus_Get");
        i->gen.Set = Imp("DM_Mus_Set");
        i->gen.Pause = Imp("DM_Mus_Pause");
        i->gen.Stop = Imp("DM_Mus_Stop");
        i->Play = Imp("DM_Mus_Play");
        i->SongBuffer = Imp("DM_Mus_SongBuffer");
    }

    if(Imp("DM_Ext_Init"))
    {   // The driver offers an Ext music playback interface.
        musinterface_ext_t* i = &musdExternalIExt;

        i->gen.Init = Imp("DM_Ext_Init");
        i->gen.Update = Imp("DM_Ext_Update");
        i->gen.Get = Imp("DM_Ext_Get");
        i->gen.Set = Imp("DM_Ext_Set");
        i->gen.Pause = Imp("DM_Ext_Pause");
        i->gen.Stop = Imp("DM_Ext_Stop");
        i->PlayFile = Imp("DM_Ext_PlayFile");
        i->PlayBuffer = Imp("DM_Ext_PlayBuffer");
        i->SongBuffer = Imp("DM_Ext_SongBuffer");
    }

    // We should release the lib at shutdown.
    d->Shutdown = DS_UnloadExternal;
    return d;
}

/**
 * "OpenAL" and "SDL_sound" are supported.
 */
audiodriver_t* DS_SFX_Load(const char* name)
{
    filename_t          fn;

#ifdef MACOSX
    sprintf(fn, "ds%s.bundle", name);
#else
    // Compose the name, use the prefix "ds".
    sprintf(fn, "libds%s", name);
    strcat(fn, ".so");
#endif

    if((handle = lt_dlopenext(fn)) == NULL)
    {
        Con_Message("DS_SFX_Load: Loading of %s failed.\n", fn);
        return NULL;
    }

    return DS_ImportExternal();
}
