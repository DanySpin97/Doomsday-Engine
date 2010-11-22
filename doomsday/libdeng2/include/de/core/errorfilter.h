/*
 * The Doomsday Engine Project
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

#ifndef LIBDENG2_ERRORFILTER_H
#define LIBDENG2_ERRORFILTER_H

#include <QObject>

namespace de
{
    namespace internal
    {
        class ErrorFilter : public QObject
        {
        public:
            explicit ErrorFilter(QObject* parent = 0);

            bool eventFilter(QObject* receiver, QEvent* event);
        };
    }
}

#endif // LIBDENG2_ERRORFILTER_H
