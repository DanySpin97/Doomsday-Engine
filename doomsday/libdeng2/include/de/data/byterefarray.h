/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BYTEREFARRAY_H
#define BYTEREFARRAY_H

#include "../IByteArray"

namespace de
{
    /**
     * Byte array that operates on a pointer-based array. Instances of
     * ByteRefArray are fixed size: one cannot write past the end of
     * the array.
     */
    class ByteRefArray : public IByteArray
    {
    public:
        /**
         * Constructs a new byte reference array.
         *
         * @param base  Pointer to the start of the array.
         * @param size  Total size of the array.
         */
        ByteRefArray(Byte* base, Size size);

        // Implements IByteArray.
        Size size() const;
        void get(Offset at, Byte* values, Size count) const;
        void set(Offset at, const Byte* values, Size count);

    private:
        Byte* _base;
        Size _size;
    };
}

#endif // BYTEREFARRAY_H
