/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2008 Daniel Swanson <danij@dengine.net>
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
 * sys_direc.c: Directory Utilities
 *
 * Directory and file system related stuff.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"

#if defined(WIN32)
#   include <direct.h>
#endif
#if defined(UNIX)
#   include <unistd.h>
#endif

#include "de_platform.h"

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

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Dir_GetDir(directory_t *dir)
{
    memset(dir, 0, sizeof(*dir));

    dir->drive = _getdrive();
    _getcwd(dir->path, 255);

    if(LAST_CHAR(dir->path) != DIR_SEP_CHAR)
        strcat(dir->path, DIR_SEP_STR);

    /* VERBOSE2( printf("Dir_GetDir: %s\n", dir->path) ); */
}

int Dir_ChDir(directory_t *dir)
{
    int         success;

    _chdrive(dir->drive);
    success = !_chdir(dir->path);   // Successful if == 0.

    VERBOSE2(Con_Printf
             ("Dir_ChDir: %s: %s\n", success ? "Succeeded" : "Failed",
              M_PrettyPath(dir->path)));

    return success;
}

void Dir_MakeDir(const char *path, directory_t *dir)
{
    char        temp[256];

    Dir_FileDir(path, dir);
    Dir_FileName(path, temp);
    strcat(dir->path, temp);
    Dir_ValidDir(dir->path);    // Make it a well formed path.
}

/**
 * Translates the given filename (>,} => basedir).
 */
void Dir_FileDir(const char *str, directory_t *dir)
{
    char        temp[256], pth[256];

    M_TranslatePath(str, pth);
    _fullpath(temp, pth, 255);
    _splitpath(temp, dir->path, pth, 0, 0);
    strcat(dir->path, pth);
#ifdef WIN32
    dir->drive = toupper(dir->path[0]) - 'A' + 1;
#endif
}

void Dir_FileName(const char *str, char *name)
{
    char        ext[100];

    _splitpath(str, 0, 0, name, ext);
    strcat(name, ext);
}

/**
 * Calculate an identifier for the file based on its full path name.
 * The identifier is the MD5 hash of the path.
 */
void Dir_FileID(const char *str, byte identifier[16])
{
    char        temp[256];
    md5_ctx_t   context;

    // First normalize the name.
    memset(temp, 0, sizeof(temp));
    _fullpath(temp, str, 255);
    Dir_FixSlashes(temp);

#if defined(WIN32) || defined(MACOSX)
    // This is a case insensitive operation.
    strupr(temp);
#endif

    md5_init(&context);
    md5_update(&context, (byte*) temp, (unsigned int) strlen(temp));
    md5_final(&context, identifier);
}

/**
 * @return              @c true, if the directories are equal.
 */
boolean Dir_IsEqual(directory_t *a, directory_t *b)
{
    if(a->drive != b->drive)
        return false;
    return !stricmp(a->path, b->path);
}

/**
 * @return              @c true, if the given path is absolute
 *                      (starts with \ or / or the second character is
 *                      a ':' (drive).
 */
int Dir_IsAbsolute(const char *str)
{
    if(!str)
        return 0;

    if(str[0] == '\\' || str[0] == '/' || str[1] == ':')
        return true;
#ifdef UNIX
    if(str[0] == '~')
        return true;
#endif
    return false;
}

/**
 * Converts directory slashes to the correct type of slash.
 */
void Dir_FixSlashes(char *path)
{
    size_t          i, len = strlen(path);

    for(i = 0; i < len; ++i)
    {
        if(path[i] == DIR_WRONG_SEP_CHAR)
            path[i] = DIR_SEP_CHAR;
    }
}

#ifdef UNIX
/**
 * If the path begins with a tilde, replace it with either the value
 * of the HOME environment variable or a user's home directory (from
 * passwd).
 */
void Dir_ExpandHome(char *str)
{
    char            buf[PATH_MAX];

    if(str[0] != '~')
        return;

    memset(buf, 0, sizeof(buf));

    if(str[1] == '/')
    {
        // Replace it with the HOME environment variable.
        strcpy(buf, getenv("HOME"));
        if(LAST_CHAR(buf) != '/')
            strcat(buf, "/");

        // Append the rest of the original path.
        strcat(buf, str + 2);
    }
    else
    {
        char        userName[PATH_MAX], *end = NULL;
        struct passwd *pw;

        end = strchr(str + 1, '/');
        strncpy(userName, str, end - str - 1);
        userName[end - str - 1] = 0;

        if((pw = getpwnam(userName)) != NULL)
        {
            strcpy(buf, pw->pw_dir);
            if(LAST_CHAR(buf) != '/')
                strcat(buf, "/");
        }

        strcat(buf, str + 1);
    }

    // Replace the original.
    strcpy(str, buf);
}
#endif

/**
 * Appends a backslash, if necessary. Also converts forward slashes into
 * backward ones. Does not check if the directory actually exists, just
 * that it's a well-formed path name.
 */
void Dir_ValidDir(char *str)
{
    size_t          idx, len = strlen(str);

    if(!len)
        return;                 // Nothing to do.

    Dir_FixSlashes(str);

    // Remove whitespace from the end.
    idx = len - 1;
    while(idx > 0 && isspace(str[idx]))
        str[idx--] = 0;

    // Make sure it ends in a directory separator character.
    if(str[idx] != DIR_SEP_CHAR)
        strcat(str, DIR_SEP_STR);

#ifdef UNIX
    Dir_ExpandHome(str);
#endif
}

/**
 * Converts a possibly relative path to a full path.
 */
void Dir_MakeAbsolute(char *path)
{
    char    buf[256];

    _fullpath(buf, path, 255);
    strncpy(path, buf, 255);
}
