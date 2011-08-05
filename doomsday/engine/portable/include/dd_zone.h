/**\file dd_zone.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 1999-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * along with this program; if not: http://www.opensource.org/
 */

/**
 * Memory Zone
 */

#ifndef LIBDENG_ZONE_H
#define LIBDENG_ZONE_H

/**
 * Define this to force all memory blocks to be allocated from
 * the real heap. Useful when debugging memory-related problems.
 */
//#define FAKE_MEMORY_ZONE 1

#define PU_REFRESHFLAT      10 // Flats
#define PU_REFRESHTEX       11 // Textures
//#define PU_REFRESHCM        12 // Colormap.
#define PU_REFRESHTRANS     13
//#define PU_REFRESHSPR       14
#define PU_PATCH            15
//#define PU_MODEL            16
#define PU_REFRESHRAW       17
#define PU_SPRITE           20

int             Z_Init(void);
void            Z_Shutdown(void);
void            Z_EnableFastMalloc(boolean isEnabled);
//void            Z_PrintStatus(void);
void*           Z_Malloc(size_t size, int tag, void* ptr);
void            Z_Free(void* ptr);
void            Z_FreeTags(int lowTag, int highTag);
void            Z_CheckHeap(void);
void            Z_ChangeTag2(void* ptr, int tag);
void            Z_ChangeUser(void* ptr, void* newUser);
void*           Z_GetUser(void* ptr);
int             Z_GetTag(void* ptr);
void*           Z_Realloc(void* ptr, size_t n, int mallocTag);
void*           Z_Calloc(size_t size, int tag, void* user);
void*           Z_Recalloc(void* ptr, size_t n, int callocTag);
size_t          Z_FreeMemory(void);

typedef struct memblock_s {
    size_t          size; // Including header and possibly tiny fragments.
    void**          user; // NULL if a free block.
    int             tag; // Purge level.
    int             id; // Should be ZONEID.
    struct memvolume_s* volume; // Volume this block belongs to.
    struct memblock_s* next, *prev;
    struct memblock_s* seqLast, *seqFirst;
#ifdef FAKE_MEMORY_ZONE
    void*           area; // The real memory area.
#endif
} memblock_t;

typedef struct {
    size_t          size; // Total bytes malloced, including header.
    memblock_t      blockList; // Start / end cap for linked list.
    memblock_t*     rover;
} memzone_t;

struct zblockset_block_s;

/**
 * ZBlockSet. Block memory allocator.
 *
 * These are used instead of many calls to Z_Malloc when the number of
 * required elements is unknown and when linear allocation would be too
 * slow.
 *
 * Memory is allocated as needed in blocks of "batchSize" elements. When
 * a new element is required we simply reserve a ptr in the previously
 * allocated block of elements or create a new block just in time.
 *
 * The internal state of a blockset is managed automatically.
 */
typedef struct zblockset_s {
    unsigned int _elementsPerBlock;
    size_t _elementSize;
    int _tag; /// All blocks in a blockset have the same tag.
    unsigned int _blockCount;
    struct zblockset_block_s* _blocks;
} zblockset_t;

/**
 * Creates a new block memory allocator in the Zone.
 *
 * @param sizeOfElement  Required size of each element.
 * @param batchSize  Number of elements in each block of the set.
 *
 * @return  Ptr to the newly created blockset.
 */
zblockset_t* ZBlockSet_Construct(size_t sizeOfElement, uint batchSize, int tag);

/**
 * Free an entire blockset.
 * All memory allocated is released for all elements in all blocks and any
 * used for the blockset itself.
 *
 * @param set  The blockset to be freed.
 */
void ZBlockSet_Destruct(zblockset_t* set);

/**
 * Return a ptr to the next unused element in the blockset.
 *
 * @param blockset  The blockset to return the next element from.
 *
 * @return  Ptr to the next unused element in the blockset.
 */
void* ZBlockSet_Allocate(zblockset_t* set);

#ifdef FAKE_MEMORY_ZONE
// Fake memory zone allocates memory from the real heap.
#define Z_ChangeTag(p,t) Z_ChangeTag2(p,t)

memblock_t*     Z_GetBlock(void* ptr);

#else
// Real memory zone allocates memory from a custom heap.
#define Z_GetBlock(ptr) ((memblock_t*) ((byte*)(ptr) - sizeof(memblock_t)))
#define Z_ChangeTag(p,t)                      \
{ \
if (( (memblock_t *)( (byte *)(p) - sizeof(memblock_t)))->id!=0x1d4a11) \
    Con_Error("Z_CT at "__FILE__":%i",__LINE__); \
Z_ChangeTag2(p,t); \
};
#endif


#endif /* LIBDENG_ZONE_H */
