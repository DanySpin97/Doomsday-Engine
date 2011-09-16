/**\file sys_dylib.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "m_misc.h"
#include "m_args.h"
#include "sys_dylib.h"

#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <string.h>

static filename_t appDir;

void lt_dlinit(void)
{
    getcwd(appDir, sizeof(appDir));
}

void lt_dlexit(void)
{
}

const char* lt_dlerror(void)
{
    return dlerror();
}

void lt_dladdsearchdir(const char* searchPath)
{
}

static void getBundlePath(char* path, size_t len)
{
    if(ArgCheckWith("-libdir", 1))
    {
        strncpy(path, ArgNext(), len);
        return;
    }

    if(ArgCheckWith("-appdir", 1))
    {
        dd_snprintf(path, len, "%s/%s", appDir, ArgNext());
        return;
    }

#ifdef MACOSX
    // This is the default location where bundles are.
    dd_snprintf(path, len, "%s/Bundles", appDir);
#endif
#ifdef UNIX
#ifdef DENG_LIBRARY_DIR
    strncpy(path, DENG_LIBRARY_DIR, len);
#else
    // Assume they are in the cwd.
    strncpy(path, appDir, len);
#endif
#endif
}

int lt_dlforeachfile(const char* searchPath,
    int (*func) (const char* fileName, lt_ptr data), lt_ptr data)
{
    struct dirent* entry = 0;
    DIR* dir = 0;
    filename_t bundlePath;

    // This is the default location where bundles are.
    getBundlePath(bundlePath, FILENAME_T_MAXLEN);

    if(NULL == searchPath)
        searchPath = bundlePath;

    dir = opendir(searchPath);
    while(NULL != (entry = readdir(dir)))
    {
#ifndef MACOSX
        if(entry->d_type != DT_DIR &&
           !strncmp(entry->d_name, "libdp", 5) &&
           !strncmp(entry->d_name + strlen(entry->d_name) - 3, ".so", 3))
#endif
#ifdef MACOSX
        if(entry->d_type == DT_DIR &&
           !strncmp(entry->d_name, "dp", 2))
#endif
        {
            if(func(entry->d_name, data))
                break;
        }
    }
    closedir(dir);
    return 0;
}

lt_dlhandle lt_dlopenext(const char* libraryName)
{
    lt_dlhandle handle;
    filename_t bundlePath;
#ifdef MACOSX
    char* ptr;
#endif

    getBundlePath(bundlePath, FILENAME_T_MAXLEN);
#ifdef MACOSX
    strncat(bundlePath, "/", FILENAME_T_MAXLEN);
    strncat(bundlePath, libraryName, FILENAME_T_MAXLEN);
    strncat(bundlePath, "/Contents/MacOS/", FILENAME_T_MAXLEN);
#endif
    strncat(bundlePath, libraryName, FILENAME_T_MAXLEN);
#ifdef MACOSX
    // Get rid of the ".bundle" in the end.
    if(NULL != (ptr = strrchr(bundlePath, '.')))
        *ptr = 0;
#endif
    handle = dlopen(bundlePath, RTLD_NOW);
    if(NULL == handle)
    {
        printf("lt_dlopenext: Error opening \"%s\" (%s).\n", bundlePath, dlerror());
    }
    return handle;
}

lt_ptr lt_dlsym(lt_dlhandle module, const char* symbolName)
{
    return dlsym(module, symbolName);
}

int lt_dlclose(lt_dlhandle module)
{
    if(!module)
        return 1;
    return dlclose(module);
}