/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_STATEMENT_H
#define LIBDENG2_STATEMENT_H

namespace de
{
    class Context;
    
    /**
     * The Statement class is the abstract base class for all statements.
     */
    class Statement
    {
    public:
        Statement() : next_(0) {}
        
        virtual ~Statement() {}

        virtual void execute(Context& context) const = 0;

        Statement* next() const { return next_; }
        
        void setNext(Statement* statement) { next_ = statement; }
       
    private:
        /// Pointer to the statement that follows this one, or NULL if
        /// this is the final statement.
        Statement* next_;
    };
}

#endif /* LIBDENG2_STATEMENT_H */
