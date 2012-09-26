/**
 * @file wadfile.h
 * Specialization of AbstractFile for working with Wad archives.
 *
 * @ingroup fs
 *
 * @see abstractfile.h, AbstractFile
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_WADFILE_H
#define LIBDENG_FILESYS_WADFILE_H

#include "lumpinfo.h"
#include "abstractfile.h"

#ifdef __cplusplus
extern "C" {
#endif

struct lumpdirectory_s;
struct pathdirectorynode_s;

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

namespace de {

class PathDirectoryNode;

/**
 * WadFile. Runtime representation of an opened WAD file.
 */
class WadFile
{
public:
    /// Base file instance.
    /// @todo Inherit from this instead.
    abstractfile_t base;

public:
    WadFile(DFile& file, char const* path, LumpInfo const& info);
    ~WadFile();

    /// @return @c true= @a lumpIdx is a valid logical index for a lump in this file.
    bool isValidIndex(int lumpIdx);

    /// @return Logical index of the last lump in this file's directory or @c 0 if empty.
    int lastIndex();

    /// @return Number of lumps contained by this file or @c 0 if empty.
    int lumpCount();

    /// @return @c true= There are no lumps in this file's directory.
    bool empty();

    /**
     * Lookup a directory node for a lump contained by this file.
     *
     * @param lumpIdx       Logical index for the lump in this file's directory.
     *
     * @return  Found directory node else @c NULL if @a lumpIdx is not valid.
     */
    PathDirectoryNode* lumpDirectoryNode(int lumpIdx);

    /**
     * Compose the absolute VFS path to a lump contained by this file.
     *
     * @note Always returns a valid string object. If @a lumpIdx is not valid a
     *       zero-length string is returned.
     *
     * @param lumpIdx       Logical index for the lump.
     * @param delimiter     Delimit directory separators using this character.
     *
     * @return String containing the absolute path.
     */
    AutoStr* composeLumpPath(int lumpIdx, char delimiter = '/');

    /**
     * Lookup a lump info descriptor for a lump contained by this file.
     *
     * @param lumpIdx       Logical index for the lump in this file's directory.
     *
     * @return Found lump info.
     *
     * @throws de::Error    If @a lumpIdx is not valid.
     */
    LumpInfo const* lumpInfo(int lumpIdx);

    /**
     * Lookup the uncompressed size of lump contained by this file.
     *
     * @param lumpIdx       Logical index for the lump in this file's directory.
     *
     * @return Size of the lump in bytes.
     *
     * @throws de::Error    If @a lumpIdx is not valid.
     *
     * @note This method is intended mainly for convenience. @see lumpInfo() for
     *       a better method of looking up multiple @ref LumpInfo properties.
     */
    size_t lumpSize(int lumpIdx);

    /**
     * Read the data associated with lump @a lumpIdx into @a buffer.
     *
     * @param lumpIdx       Lump index associated with the data to be read.
     * @param buffer        Buffer to read into. Must be at least large enough to
     *                      contain the whole lump.
     * @param tryCache      @c true= try the lump cache first.
     *
     * @return Number of bytes read.
     *
     * @see lumpSize() or lumpInfo() to determine the size of buffer needed.
     */
    size_t readLump(int lumpIdx, uint8_t* buffer, bool tryCache = true);

    /**
     * Read a subsection of the data associated with lump @a lumpIdx into @a buffer.
     *
     * @param lumpIdx       Lump index associated with the data to be read.
     * @param buffer        Buffer to read into. Must be at least @a length bytes.
     * @param startOffset   Offset from the beginning of the lump to start reading.
     * @param length        Number of bytes to read.
     * @param tryCache      @c true= try the lump cache first.
     *
     * @return Number of bytes read.
     */
    size_t readLumpSection(int lumpIdx, uint8_t* buffer, size_t startOffset, size_t length,
                           bool tryCache = true);

    /**
     * Read the data associated with lump @a lumpIdx into the cache.
     *
     * @param lumpIdx   Lump index associated with the data to be cached.
     * @param tag       Zone purge level/cache tag to use.
     *
     * @return Pointer to the cached copy of the associated data.
     */
    uint8_t const* cacheLump(int lumpIdx, int tag);

    /**
     * Change the Zone purge level/cache tag for a cached data lump.
     *
     * @param lumpIdx   Lump index associated with the cached data to be changed.
     * @param tag       New Zone purge level/cache tag to assign.
     */
    WadFile& changeLumpCacheTag(int lumpIdx, int tag);

    /**
     * Clear any cached data for lump @a lumpIdx from the lump cache.
     *
     * @param lumpIdx       Lump index associated with the cached data to be cleared.
     * @param retCleared    If not @c NULL write @c true to this address if data was
     *                      present and subsequently cleared from the cache.
     *
     * @return This instance.
     */
    WadFile& clearCachedLump(int lumpIdx, bool* retCleared = 0);

    /**
     * Purge the lump cache, clearing all cached data lumps.
     *
     * @return This instance.
     */
    WadFile& clearLumpCache();

    /**
     * Publish lumps to the end of the specified @a directory.
     *
     * @param directory Directory to publish to.
     *
     * @return Number of lumps published to the directory. Note that this is not
     *         necessarily equal to the the number of lumps in the file.
     */
    int publishLumpsToDirectory(struct lumpdirectory_s* directory);

    /**
     * @attention Uses an extremely simple formula which does not conform to any CRC
     *            standard. Should not be used for anything critical.
     */
    uint calculateCRC();

// Static members ------------------------------------------------------------------

    /**
     * Does @a file appear to be in a format which can be represented with WadFile?
     *
     * @param file      Stream file handle/wrapper to be recognised.
     *
     * @return @c true= @a file can be represented with WadFile.
     */
    static bool recognise(DFile& file);

private:
    struct Instance;
    Instance* d;
};

} // namespace de

extern "C" {
#endif // __cplusplus

/**
 * C wrapper API:
 */

struct wadfile_s; // The wadfile instance (opaque)
typedef struct wadfile_s WadFile;

/**
 * Constructs a new WadFile instance which must be destroyed with WadFile_Delete()
 * once it is no longer needed.
 *
 * @param file      Virtual file handle to the underlying file resource.
 * @param path      Virtual file system path to associate with the resultant WadFile.
 * @param info      File info descriptor for the resultant WadFile. A copy is made.
 */
WadFile* WadFile_New(DFile* file, char const* path, LumpInfo const* info);

/**
 * Destroy WadFile instance @a wad.
 */
void WadFile_Delete(WadFile* wad);

int WadFile_PublishLumpsToDirectory(WadFile* wad, struct lumpdirectory_s* directory);

struct pathdirectorynode_s* WadFile_LumpDirectoryNode(WadFile* wad, int lumpIdx);

AutoStr* WadFile_ComposeLumpPath(WadFile* wad, int lumpIdx, char delimiter);

LumpInfo const* WadFile_LumpInfo(WadFile* wad, int lumpIdx);

size_t WadFile_ReadLump2(WadFile* wad, int lumpIdx, uint8_t* buffer, boolean tryCache);
size_t WadFile_ReadLump(WadFile* wad, int lumpIdx, uint8_t* buffer/*, tryCache = true*/);

size_t WadFile_ReadLumpSection2(WadFile* wad, int lumpIdx, uint8_t* buffer, size_t startOffset, size_t length, boolean tryCache);
size_t WadFile_ReadLumpSection(WadFile* wad, int lumpIdx, uint8_t* buffer, size_t startOffset, size_t length/*, tryCache = true*/);

uint8_t const* WadFile_CacheLump(WadFile* wad, int lumpIdx, int tag);

void WadFile_ChangeLumpCacheTag(WadFile* wad, int lumpIdx, int tag);

void WadFile_ClearLumpCache(WadFile* wad);

uint WadFile_CalculateCRC(WadFile* wad);

boolean WadFile_IsValidIndex(WadFile* wad, int lumpIdx);

int WadFile_LastIndex(WadFile* wad);

int WadFile_LumpCount(WadFile* wad);

boolean WadFile_Empty(WadFile* wad);

boolean WadFile_Recognise(DFile* file);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_WADFILE_H */
