/** @file data.cpp  UI data context.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/ui/Data"
#include "de/ui/Item"
#include "de/LabelWidget"

#include <QtAlgorithms>

namespace de {

using namespace ui;

dsize const Data::InvalidPos = dsize(-1);

static bool itemLessThan(Item const &a, Item const &b)
{
    return a.sortKey().compareWithoutCase(b.sortKey()) < 0;
}

static bool itemGreaterThan(Item const &a, Item const &b)
{
    return a.sortKey().compareWithoutCase(b.sortKey()) > 0;
}

void Data::sort(SortMethod method)
{
    switch(method)
    {
    case Ascending:
        sort(itemLessThan);
        break;

    case Descending:
        sort(itemGreaterThan);
        break;
    }
}

} // namespace de
