/** @file action.h  Maps a key event to a signal.
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

#include "de/shell/Action"

namespace de {
namespace shell {

Action::Action(String const &label) : _event(KeyEvent("")), _label(label), _target(0), _slot(0)
{}

Action::Action(String const &label, QObject *target, char const *slot)
    : _event(KeyEvent("")), _label(label), _target(target), _slot(slot)
{
    if(target && slot)
    {
        connect(this, SIGNAL(triggered()), target, slot);
    }
}

Action::Action(String const &label, KeyEvent const &event, QObject *target, char const *slot)
    : _event(event), _label(label), _target(target), _slot(slot)
{
    if(target && slot)
    {
        connect(this, SIGNAL(triggered()), target, slot);
    }
}

Action::Action(KeyEvent const &event, QObject *target, char const *slot)
    : _event(event), _target(target), _slot(slot)
{
    if(target && slot)
    {
        connect(this, SIGNAL(triggered()), target, slot);
    }
}

Action::~Action()
{}

void Action::setLabel(String const &label)
{
    _label = label;
}

String Action::label() const
{
    return _label;
}

bool Action::tryTrigger(KeyEvent const &ev)
{
    if(ev == _event)
    {
        trigger();
        return true;
    }
    return false;
}

void Action::trigger()
{
    de::Action::trigger();
    emit triggered();
}

Action *Action::duplicate() const
{
    return new Action(_label, _event, _target, _slot);
}

} // namespace shell
} // namespace de
