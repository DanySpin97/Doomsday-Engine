/** @file action.h  Abstract base class for UI actions.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ACTION_H
#define LIBDENG2_ACTION_H

#include <de/Observers>
#include <de/Counted>

namespace de {

/**
 * Abstract base class for user interface actions.
 *
 * Actions are reference-counted so that they can be shared by widgets and data model
 * items. Also, the same actions could be used by different sets of widgets representing
 * a single data item.
 *
 * @ingroup widgets
 */
class DENG2_PUBLIC Action : public Counted
{
public:
    /**
     * Audience to be notified when the action is triggerd.
     */
    DENG2_DEFINE_AUDIENCE(Triggered, void actionTriggered(Action &))

    /**
     * Perform the action this instance represents. Derived classes must call
     * this or manually notify the Triggered audience in their own
     * implementation of the method.
     */
    virtual void trigger();

protected:
    virtual ~Action(); // ref counted, hence not publicly deletable
};

} // namespace de

#endif // LIBDENG2_ACTION_H
