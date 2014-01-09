/** @file dd_wad.cpp
 *
 * Wrapper API for accessing data stored in DOOM WAD files.
 *
 * @ingroup resource
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 1993-1996 by id Software, Inc.
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

#define DENG_NO_API_MACROS_WAD

#include <ctime>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"

#include "api_wad.h"

using namespace de;

size_t W_LumpLength(lumpnum_t lumpNum)
{
    try
    {
        return App_FileSystem().nameIndex().lump(lumpNum).info().size;
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        LOG_AS("W_LumpLength");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return 0;
}

AutoStr* W_LumpName(lumpnum_t lumpNum)
{
    try
    {
        String const& name = App_FileSystem().nameIndex().lump(lumpNum).name();
        QByteArray nameUtf8 = name.toUtf8();
        return AutoStr_FromTextStd(nameUtf8.constData());
    }
    catch(FS1::NotFoundError const &er)
    {
        LOG_AS("W_LumpName");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return AutoStr_NewStd();
}

uint W_LumpLastModified(lumpnum_t lumpNum)
{
    try
    {
        return App_FileSystem().nameIndex().lump(lumpNum).info().lastModified;
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        LOG_AS("W_LumpLastModified");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return 0;
}

AutoStr *W_LumpSourceFile(lumpnum_t lumpNum)
{
    try
    {
        de::File1 const& lump = App_FileSystem().nameIndex().lump(lumpNum);
        QByteArray path = lump.container().composePath().toUtf8();
        return AutoStr_FromText(path.constData());
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        LOG_AS("W_LumpSourceFile");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return AutoStr_NewStd();
}

boolean W_LumpIsCustom(lumpnum_t lumpNum)
{
    try
    {
        de::File1 const& lump = App_FileSystem().nameIndex().lump(lumpNum);
        return lump.container().hasCustom();
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        LOG_AS("W_LumpIsCustom");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return false;
}

lumpnum_t W_CheckLumpNumForName(char const *name)
{
    lumpnum_t lumpNum;
    if(!name || !name[0])
    {
        LOGDEV_RES_WARNING("W_CheckLumpNumForName: Empty lump name, returning invalid lumpnum");
        return -1;
    }
    lumpNum = App_FileSystem().lumpNumForName(name);
    return lumpNum;
}

lumpnum_t W_GetLumpNumForName(char const *name)
{
    lumpnum_t lumpNum = W_CheckLumpNumForName(name);
    if(lumpNum < 0)
    {
        LOG_RES_ERROR("Lump \"%s\" not found") << name;
    }
    return lumpNum;
}

size_t W_ReadLump(lumpnum_t lumpNum, uint8_t* buffer)
{
    try
    {
        de::File1& lump = App_FileSystem().nameIndex().lump(lumpNum);
        return lump.read(buffer, 0, lump.size());
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        LOG_AS("W_ReadLump");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return 0;
}

size_t W_ReadLumpSection(lumpnum_t lumpNum, uint8_t* buffer, size_t startOffset, size_t length)
{
    try
    {
        de::File1& lump = App_FileSystem().nameIndex().lump(lumpNum);
        return lump.read(buffer, startOffset, length);
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        LOG_AS("W_ReadLumpSection");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return 0;
}

uint8_t const *W_CacheLump(lumpnum_t lumpNum)
{
    try
    {
        return App_FileSystem().nameIndex().lump(lumpNum).cache();
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        LOG_AS("W_CacheLump");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return NULL;
}

void W_UnlockLump(lumpnum_t lumpNum)
{
    try
    {
        App_FileSystem().nameIndex().lump(lumpNum).unlock();
    }
    catch(LumpIndex::NotFoundError const &er)
    {
        LOG_AS("W_UnlockLump");
        LOGDEV_RES_ERROR("%s") << er.asText();
    }
    return;
}

// Public API:
DENG_DECLARE_API(W) =
{
    { DE_API_WAD },
    W_LumpLength,
    W_LumpName,
    W_LumpLastModified,
    W_LumpSourceFile,
    W_LumpIsCustom,
    W_CheckLumpNumForName,
    W_GetLumpNumForName,
    W_ReadLump,
    W_ReadLumpSection,
    W_CacheLump,
    W_UnlockLump
};
