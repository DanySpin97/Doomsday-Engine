/**\file blockset.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_BLOCKSET_H
#define LIBDENG_BLOCKSET_H

struct blockset_block_s;

/**
 * BlockSet. A block memory allocator.
 *
 * These are used instead of many calls to Malloc when the number of
 * required elements is unknown and when linear allocation would be too
 * slow.
 *
 * Memory is allocated as needed in blocks of "batchSize" elements. When
 * a new element is required we simply reserve a ptr in the previously
 * allocated block of elements or create a new block just in time.
 *
 * The internal state of a blockset is managed automatically.
 */
typedef struct blockset_s {
    /// Number of elements to allocate in each block.
    size_t _elementsPerBlock;

    /// Running total of the number of used elements across all blocks.
    size_t _elementsInUse;

    /// sizeof an individual element in the set.
    size_t _elementSize;

    /// Number of blocks in the set.
    size_t _blockCount;

    /// Vector of blocks in the set.
    struct blockset_block_s* _blocks;
} blockset_t;

/**
 * Creates a new block memory allocator.
 *
 * @param sizeOfElement  Required size of each element.
 * @param batchSize  Number of elements in each block of the set.
 *
 * @return  Ptr to the newly created blockset.
 */
blockset_t* BlockSet_Construct(size_t sizeOfElement, size_t batchSize);

/**
 * Free an entire blockset.
 * All memory allocated is released for all elements in all blocks and any
 * used for the blockset itself.
 *
 * @param set  The blockset to be freed.
 */
void BlockSet_Destruct(blockset_t* set);

/**
 * Return a ptr to the next unused element in the blockset.
 *
 * @param blockset  The blockset to return the next element from.
 *
 * @return  Ptr to the next unused element in the blockset.
 */
void* BlockSet_Allocate(blockset_t* set);

/// @return  Total number of elements from the set that are currently in use.
size_t BlockSet_Count(blockset_t* set);

#endif /* LIBDENG_BLOCKSET_H */
