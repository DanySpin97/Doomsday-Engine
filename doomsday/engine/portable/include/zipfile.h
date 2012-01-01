/**\file zipfile.h
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

#ifndef LIBDENG_FILESYS_ZIPFILE_H
#define LIBDENG_FILESYS_ZIPFILE_H

#include "lumpinfo.h"
#include "abstractfile.h"

struct lumpdirectory_s;

typedef struct {
    size_t baseOffset;
    lumpinfo_t info;
} zipfile_lumprecord_t;

/**
 * ZipFile. Runtime representation of Zip files.
 *
 * Uses zlib for decompression of "Deflated" files.
 *
 * @ingroup FS
 */
typedef struct {
    // Base file.
    abstractfile_t _base;
    int _lumpCount;
    zipfile_lumprecord_t* _lumpRecords;
    void** _lumpCache;
} zipfile_t;

zipfile_t* ZipFile_New(DFile* file, const lumpinfo_t* info);
void ZipFile_Delete(zipfile_t* zip);

int ZipFile_PublishLumpsToDirectory(zipfile_t* zip, struct lumpdirectory_s* directory);

const lumpinfo_t* ZipFile_LumpInfo(zipfile_t* zip, int lumpIdx);

/**
 * Read the data associated with the specified lump index into @a buffer.
 *
 * @param lumpIdx  Lump index associated with the data being read.
 * @param buffer  Buffer to read into. Must be at least W_LumpLength() bytes.
 * @param tryCache  @c true = try the lump cache first.
 * @return  Number of bytes read.
 */
size_t ZipFile_ReadLump2(zipfile_t* zip, int lumpIdx, uint8_t* buffer, boolean tryCache);
size_t ZipFile_ReadLump(zipfile_t* zip, int lumpIdx, uint8_t* buffer);

/**
 * Read a subsection of the data associated with the specified lump index into @a buffer.
 *
 * @param lumpIdx  Lump index associated with the data being read.
 * @param buffer  Buffer to read into. Must be at least W_LumpLength() bytes.
 * @param startOffset  Offset from the beginning of the lump to start reading.
 * @param length  Number of bytes to be read.
 * @param tryCache  @c true = try the lump cache first.
 * @return  Number of bytes read.
 */
size_t ZipFile_ReadLumpSection2(zipfile_t* zip, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length, boolean tryCache);
size_t ZipFile_ReadLumpSection(zipfile_t* zip, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length);

/**
 * Read the data associated with the specified lump index into the cache.
 *
 * @param lumpIdx  Lump index associated with the data being read.
 * @param tag  Zone purge level/cache tag to use.
 * @return  Ptr to the cached copy of the associated data.
 */
const uint8_t* ZipFile_CacheLump(zipfile_t* zip, int lumpIdx, int tag);

/**
 * Change the Zone purge level/cache tag associated with a cached data lump.
 *
 * @param lumpIdx  Lump index associated with the cached data being changed.
 * @param tag  Zone purge level/cache tag to use.
 */
void ZipFile_ChangeLumpCacheTag(zipfile_t* zip, int lumpIdx, int tag);

void ZipFile_ClearLumpCache(zipfile_t* zip);

/**
 * Accessors:
 */

/// @return  Number of lumps contained within this file.
int ZipFile_LumpCount(zipfile_t* zip);

/**
 * Static members:
 */

/**
 * Does the specified file appear to be in Zip format.
 * @param file  Stream file handle/wrapper to the file being interpreted.
 * @return  @c true iff this is a file that can be represented using ZipFile.
 */
boolean ZipFile_Recognise(DFile* file);

#endif
