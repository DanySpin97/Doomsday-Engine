/**\file sys_direc.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2011 Daniel Swanson <danij@dengine.net>
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

#if defined(WIN32)
#  include <direct.h>
#endif

#if defined(UNIX)
#  include <unistd.h>
#  include <limits.h>
#  include <sys/types.h>
#  include <pwd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "de_platform.h"
#include "de_base.h"
#include "de_system.h"
#include "m_misc.h"

static void setPathFromPathDir(directory_t* dir, const char* path);

static void prependBasePath(char* newPath, const char* path, size_t maxLen);
static void resolveAppRelativeDirectives(char* translated, const char* path, size_t maxLen);
#if defined(UNIX)
static void resolveHomeRelativeDirectives(char* path, size_t maxLen);
#endif
static void resolvePathRelativeDirectives(char* path);

directory_t* Dir_New(const char* path)
{
    directory_t* dir = (directory_t*) malloc(sizeof(*dir));
    if(NULL == dir)
        printf("Dir::Construct: Failed on allocation of %lu bytes for new Dir.", (unsigned long) sizeof(*dir));
    Dir_SetPath(dir, path);
    return dir;
}

directory_t* Dir_NewFromCWD(void)
{
    directory_t* dir = (directory_t*) malloc(sizeof(*dir));
    size_t lastIndex;
    char* cwd;
    if(NULL == dir)
        printf("Dir::ConstructFromWorkDir: Failed on allocation of %lu bytes for new Dir.\n",
            (unsigned long) sizeof(*dir));

    cwd = Dir_CurrentPath();
    lastIndex = strlen(cwd);
    lastIndex = MIN_OF(lastIndex, FILENAME_T_LASTINDEX);

    dir->drive = _getdrive();
    memcpy(dir->path, cwd, lastIndex);
    dir->path[lastIndex] = '\0';
    free(cwd);
    return dir;
}

directory_t* Dir_ConstructFromPathDir(const char* path)
{
    directory_t* dir;
    if(NULL == path || !path[0])
        return Dir_NewFromCWD();

    dir = (directory_t*) malloc(sizeof(*dir));
    if(NULL == dir)
        printf("Dir::ConstructFromFileDir: Failed on allocation of %lu bytes for new Dir.", (unsigned long) sizeof(*dir));
    setPathFromPathDir(dir, path);
    return dir;
}

void Dir_Delete(directory_t* dir)
{
    assert(NULL != dir);
    free(dir);
}

boolean Dir_IsEqual(directory_t* a, directory_t* b)
{
    if(a == b) return true;
    if(a->drive != b->drive)
        return false;
    return !stricmp(a->path, b->path);
}

const char* Dir_Path(directory_t* dir)
{
    assert(NULL != dir);
    return dir->path;
}

void Dir_SetPath(directory_t* dir, const char* path)
{
    assert(NULL != dir);
    {
    filename_t fileName;
    setPathFromPathDir(dir, path);
    Dir_FileName(fileName, path, FILENAME_T_MAXLEN);
    strncat(dir->path, fileName, FILENAME_T_MAXLEN);
    // Ensure we've a well-formed path.
    Dir_CleanPath(dir->path, FILENAME_T_MAXLEN);
    }
}

static void setPathFromPathDir(directory_t* dir, const char* path)
{
    assert(NULL != dir && NULL != path && path[0]);
    {
    filename_t temp, transPath;

    resolveAppRelativeDirectives(transPath, path, FILENAME_T_MAXLEN);
    Dir_ToNativeSeparators(transPath, FILENAME_T_MAXLEN);

    _fullpath(temp, transPath, FILENAME_T_MAXLEN);
    _splitpath(temp, dir->path, transPath, 0, 0);
    strncat(dir->path, transPath, FILENAME_T_MAXLEN);
#if defined(WIN32)
    dir->drive = toupper(dir->path[0]) - 'A' + 1;
#endif
    }
}

/// Class-Static Members:

static void prependBasePath(char* newPath, const char* path, size_t maxLen)
{
    assert(NULL != newPath && NULL != path);
    // Cannot prepend to absolute paths.
    if(!Dir_IsAbsolutePath(path))
    {
        filename_t buf;
        dd_snprintf(buf, maxLen, "%s%s", ddBasePath, path);
        memcpy(newPath, buf, maxLen);
        return;
    }
    strncpy(newPath, path, maxLen);
}

static void resolveAppRelativeDirectives(char* translated, const char* path, size_t maxLen)
{
    assert(NULL != translated && NULL != path);
    {
    filename_t buf;
    if(path[0] == '>' || path[0] == '}')
    {
        path++;
        if(!Dir_IsAbsolutePath(path))
            prependBasePath(buf, path, maxLen);
        else
            strncpy(buf, path, maxLen);
        strncpy(translated, buf, maxLen);
    }
    else if(translated != path)
    {
        strncpy(translated, path, maxLen);
    }
    }
}

#if defined(UNIX)
static void resolveHomeRelativeDirectives(char* path, size_t maxLen)
{
    assert(NULL != path);
    {
    filename_t buf;

    if(!path[0] || 0 == maxLen || path[0] != '~')
        return;

    memset(buf, 0, sizeof(buf));

    if(path[1] == '/')
    {
        // Replace it with the HOME environment variable.
        strncpy(buf, getenv("HOME"), FILENAME_T_MAXLEN);
        if(LAST_CHAR(buf) != '/')
            strncat(buf, DIR_SEP_STR, FILENAME_T_MAXLEN);

        // Append the rest of the original path.
        strncat(buf, path + 2, FILENAME_T_MAXLEN);
    }
    else
    {
        char userName[PATH_MAX], *end = NULL;
        struct passwd *pw;

        end = strchr(path + 1, '/');
        strncpy(userName, path, end - path - 1);
        userName[end - path - 1] = 0;

        pw = getpwnam(userName);
        if(NULL != pw)
        {
            strncpy(buf, pw->pw_dir, FILENAME_T_MAXLEN);
            if(LAST_CHAR(buf) != DIR_SEP_CHAR)
                strncat(buf, DIR_SEP_STR, FILENAME_T_MAXLEN);
        }

        strncat(buf, path + 1, FILENAME_T_MAXLEN);
    }

    // Replace the original.
    strncpy(path, buf, maxLen - 1);
    }
}
#endif

static void resolvePathRelativeDirectives(char* path)
{
    assert(NULL != path);
    {
    char* ch = path;
    char* end = path + strlen(path);
    char* prev = path; // Assume an absolute path.

    for(; *ch; ch++)
    {
        if(ch[0] == '/' && ch[1] == '.')
        {
            if(ch[2] == '/')
            {
                memmove(ch, ch + 2, end - ch - 1);
                ch--;
            }
            else if(ch[2] == '.' && ch[3] == '/')
            {
                memmove(prev, ch + 3, end - ch - 2);
                // Must restart from the beginning.
                // This is a tad inefficient, though.
                ch = path - 1;
                continue;
            }
        }
        if(*ch == '/')
            prev = ch;
    }
    }
}

void Dir_CleanPath(char* path, size_t len)
{
    if(NULL == path || 0 == len) return;

    M_Strip(path, len);
    Dir_ToNativeSeparators(path, len);
#if defined(UNIX)
    resolveHomeRelativeDirectives(path, len);
#endif
}

char* Dir_CurrentPath(void)
{
    char* path = _getcwd(NULL, 0);
    size_t len = strlen(path);
    // Why oh why does the OS not do this for us?
    if(len != 0 && path[len - 1] != DIR_SEP_CHAR)
    {
        path = (char*) realloc(path, len+2);
        if(NULL == path)
        {
            Sys_CriticalMessagef("Dir::WorkDir: Failed on reallocation of %lu bytes for out string.", (unsigned long) (len+2));
            return NULL;
        }
        strcat(path, DIR_SEP_STR);
    }
    return path;
}

void Dir_FileName(char* name, const char* path, size_t len)
{
    char ext[100];
    if(NULL == path || NULL == name || 0 == len) return;
    _splitpath(path, 0, 0, name, ext);
    strncat(name, ext, len);
}

int Dir_IsAbsolutePath(const char* path)
{
    if(NULL == path || !path[0]) return 0;
    if(path[0] == '\\' || path[0] == '/' || path[1] == ':')
        return true;
#if defined(UNIX)
    if(path[0] == '~')
        return true;
#endif
    return false;
}

boolean Dir_mkpath(const char* path)
{
#if !defined(WIN32) && !defined(UNIX)
#  error Dir_mkpath has no implementation for this platform.
#endif

    filename_t full, buf;
    char* ptr, *endptr;

    if(NULL == path || !path[0]) return false;

    // Convert all backslashes to normal slashes.
    strncpy(full, path, FILENAME_T_MAXLEN);
    Dir_ToNativeSeparators(full, FILENAME_T_MAXLEN);

    // Does this path already exist?
    if(0 == access(full, 0))
        return true;

    // Check and create the path in segments.
    ptr = full;
    memset(buf, 0, sizeof(buf));
    do
    {
        endptr = strchr(ptr, DIR_SEP_CHAR);
        if(!endptr)
            strncat(buf, ptr, FILENAME_T_MAXLEN);
        else
            strncat(buf, ptr, endptr - ptr);
        if(access(buf, 0))
        {
            // Path doesn't exist, create it.
#if defined(WIN32)
            mkdir(buf);
#elif defined(UNIX)
            mkdir(buf, 0775);
#endif
        }
        strncat(buf, DIR_SEP_STR, FILENAME_T_MAXLEN);
        ptr = endptr + 1;

    } while(endptr);

    return (0 == access(full, 0));
}

void Dir_MakeAbsolutePath(char* path, size_t len)
{
    filename_t buf;
    if(NULL == path || !path[0] || 0 == len) return;

#if defined(UNIX)
    resolveHomeRelativeDirectives(path, len);
#endif
    _fullpath(buf, path, FILENAME_T_MAXLEN);
    strncpy(path, buf, len);
}

void Dir_ToNativeSeparators(char* path, size_t len)
{
    if(NULL == path || !path[0] || 0 == len) return;
    { size_t i;
    for(i = 0; i < len && path[i]; ++i)
    {
        if(path[i] == DIR_WRONG_SEP_CHAR)
            path[i] = DIR_SEP_CHAR;
    }}
}

boolean Dir_SetCurrent(const char* path)
{
    boolean success = false;
    if(NULL != path && path[0])
    {
        success = !_chdir(path);
    }
    printf("Dir::ChangeWorkDir: %s: %s\n", success ? "Succeeded" : "Failed", path);
    return success;
}
