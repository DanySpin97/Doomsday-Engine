/** @file inputdevice.cpp  Logical input device.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include "ui/inputdevice.h"
#include "ui/joystick.h"
#include "ui/axisinputcontrol.h"
#include "ui/buttoninputcontrol.h"
#include "ui/hatinputcontrol.h"
#include <QList>
#include <QtAlgorithms>
#include <de/Log>

using namespace de;

DENG2_PIMPL(InputDevice)
{
    bool active = false;  ///< Initially inactive.
    String title;         ///< Human-friendly title.
    String name;          ///< Symbolic name.

    QList<  AxisInputControl *> axes;
    QList<ButtonInputControl *> buttons;
    QList<   HatInputControl *> hats;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        qDeleteAll(hats);
        qDeleteAll(buttons);
        qDeleteAll(axes);
    }

    DENG2_PIMPL_AUDIENCE(ActiveChange)
};

DENG2_AUDIENCE_METHOD(InputDevice, ActiveChange)

InputDevice::InputDevice(String const &name) : d(new Impl(this))
{
    DENG2_ASSERT(!name.isEmpty());
    d->name = name;
}

InputDevice::~InputDevice()
{}

bool InputDevice::isActive() const
{
    return d->active;
}

void InputDevice::activate(bool yes)
{
    if (d->active != yes)
    {
        d->active = yes;

        // Notify interested parties.
        DENG2_FOR_AUDIENCE2(ActiveChange, i) i->inputDeviceActiveChanged(*this);
    }
}

String InputDevice::name() const
{
    return d->name;
}

String InputDevice::title() const
{
    return (d->title.isEmpty()? d->name : d->title);
}

void InputDevice::setTitle(String const &newTitle)
{
    d->title = newTitle;
}

String InputDevice::description() const
{
    String desc;
    if (!d->title.isEmpty())
    {
        desc += String(_E(D)_E(b) "%1" _E(.)_E(.) " - ").arg(d->title);
    }
    desc += String(_E(b) "%1" _E(.)_E(l) " (%2)" _E(.)).arg(name()).arg(isActive()? "active" : "inactive");

    if (int const count = axisCount())
    {
        desc += String("\n  " _E(b) "%1 axes:" _E(.)).arg(count);
        int idx = 0;
        for (Control const *axis : d->axes)
        {
            desc += String("\n    [%1] " _E(>) "%2" _E(<)).arg(idx++, 3).arg(axis->description());
        }
    }

    if (int const count = buttonCount())
    {
        desc += String("\n  " _E(b) "%1 buttons:" _E(.)).arg(count);
        int idx = 0;
        for (Control const *button : d->buttons)
        {
            desc += String("\n    [%1] " _E(>) "%2" _E(<)).arg(idx++, 3).arg(button->description());
        }
    }

    if (int const count = hatCount())
    {
        desc += String("\n  " _E(b) "%1 hats:" _E(.)).arg(count);
        int idx = 0;
        for (Control const *hat : d->hats)
        {
            desc += String("\n    [%1] " _E(>) "%2" _E(<)).arg(idx++, 3).arg(hat->description());
        }
    }

    return desc;
}

void InputDevice::reset()
{
    LOG_AS("InputDevice");
    for (Control *axis : d->axes)
    {
        axis->reset();
    }
    for (Control *button : d->buttons)
    {
        button->reset();
    }
    for (Control *hat : d->hats)
    {
        hat->reset();
    }

    if (!d->name.compareWithCase("key"))
    {
        extern bool shiftDown, altDown;

        altDown = shiftDown = false;
    }
    LOG_INPUT_VERBOSE(_E(b) "'%s'" _E(.) " controls reset") << title();
}

LoopResult InputDevice::forAllControls(std::function<de::LoopResult (Control &)> func)
{
    for (Control *axis : d->axes)
    {
        if (auto result = func(*axis)) return result;
    }
    for (Control *button : d->buttons)
    {
        if (auto result = func(*button)) return result;
    }
    for (Control *hat : d->hats)
    {
        if (auto result = func(*hat)) return result;
    }
    return LoopContinue;
}

dint InputDevice::toAxisId(String const &name) const
{
    if (!name.isEmpty())
    {
        for (int i = 0; i < d->axes.count(); ++i)
        {
            if (!d->axes.at(i)->name().compareWithoutCase(name))
                return i;
        }
    }
    return -1;
}

dint InputDevice::toButtonId(String const &name) const
{
    if (!name.isEmpty())
    {
        for (int i = 0; i < d->buttons.count(); ++i)
        {
            if (!d->buttons.at(i)->name().compareWithoutCase(name))
                return i;
        }
    }
    return -1;
}

bool InputDevice::hasAxis(de::dint id) const
{
    return (id >= 0 && id < d->axes.count());
}

AxisInputControl &InputDevice::axis(dint id) const
{
    if (hasAxis(id)) return *d->axes.at(id);
    /// @throw MissingControlError  The given id is invalid.
    throw MissingControlError("InputDevice::axis", "Invalid id:" + String::number(id));
}

void InputDevice::addAxis(AxisInputControl *axis)
{
    if (!axis) return;
    d->axes.append(axis);
    axis->setDevice(this);
}

int InputDevice::axisCount() const
{
    return d->axes.count();
}

bool InputDevice::hasButton(de::dint id) const
{
    return (id >= 0 && id < d->buttons.count());
}

ButtonInputControl &InputDevice::button(dint id) const
{
    if (hasButton(id)) return *d->buttons.at(id);
    /// @throw MissingControlError  The given id is invalid.
    throw MissingControlError("InputDevice::button", "Invalid id:" + String::number(id));
}

void InputDevice::addButton(ButtonInputControl *button)
{
    if (!button) return;
    d->buttons.append(button);
    button->setDevice(this);
}

int InputDevice::buttonCount() const
{
    return d->buttons.count();
}

bool InputDevice::hasHat(de::dint id) const
{
    return (id >= 0 && id < d->hats.count());
}

HatInputControl &InputDevice::hat(dint id) const
{
    if (hasHat(id)) return *d->hats.at(id);
    /// @throw MissingControlError  The given id is invalid.
    throw MissingControlError("InputDevice::hat", "Invalid id:" + String::number(id));
}

void InputDevice::addHat(HatInputControl *hat)
{
    if (!hat) return;
    d->hats.append(hat);
    hat->setDevice(this);
}

int InputDevice::hatCount() const
{
    return d->hats.count();
}

void InputDevice::consoleRegister()
{
    for (Control *axis : d->axes)
    {
        axis->consoleRegister();
    }
    for (Control *button : d->buttons)
    {
        button->consoleRegister();
    }
    for (Control *hat : d->hats)
    {
        hat->consoleRegister();
    }
}

//---------------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(InputDevice::Control)
{
    String name;  ///< Symbolic
    InputDevice *device = nullptr;
    BindContextAssociation flags = DefaultFlags;
    BindContext *bindContext     = nullptr;
    BindContext *prevBindContext = nullptr;
};

InputDevice::Control::Control(InputDevice *device) : d(new Impl)
{
    setDevice(device);
}

InputDevice::Control::~Control()
{}

String InputDevice::Control::name() const
{
    DENG2_GUARD(this);
    return d->name;
}

void InputDevice::Control::setName(String const &newName)
{
    DENG2_GUARD(this);
    d->name = newName;
}

String InputDevice::Control::fullName() const
{
    DENG2_GUARD(this);
    String desc;
    if (hasDevice()) desc += device().name() + "-";
    desc += (d->name.isEmpty()? "<unnamed>" : d->name);
    return desc;
}

InputDevice &InputDevice::Control::device() const
{
    DENG2_GUARD(this);
    if (d->device) return *d->device;
    /// @throw MissingDeviceError  Missing InputDevice attribution.
    throw MissingDeviceError("InputDevice::Control::device", "No InputDevice is attributed");
}

bool InputDevice::Control::hasDevice() const
{
    DENG2_GUARD(this);
    return d->device != nullptr;
}

void InputDevice::Control::setDevice(InputDevice *newDevice)
{
    DENG2_GUARD(this);
    d->device = newDevice;
}

BindContext *InputDevice::Control::bindContext() const
{
    DENG2_GUARD(this);
    return d->bindContext;
}

void InputDevice::Control::setBindContext(BindContext *newContext)
{
    DENG2_GUARD(this);
    d->bindContext = newContext;
}

InputDevice::Control::BindContextAssociation InputDevice::Control::bindContextAssociation() const
{
    DENG2_GUARD(this);
    return d->flags;
}

void InputDevice::Control::setBindContextAssociation(BindContextAssociation const &flagsToChange, FlagOp op)
{
    DENG2_GUARD(this);
    applyFlagOperation(d->flags, flagsToChange, op);
}

void InputDevice::Control::clearBindContextAssociation()
{
    DENG2_GUARD(this);
    d->prevBindContext = d->bindContext;
    d->bindContext     = nullptr;
    applyFlagOperation(d->flags, Triggered, UnsetFlags);
}

void InputDevice::Control::expireBindContextAssociationIfChanged()
{
    DENG2_GUARD(this);
    
    // No change?
    if (d->bindContext == d->prevBindContext) return;

    // No longer valid.
    applyFlagOperation(d->flags, Expired, SetFlags);
    applyFlagOperation(d->flags, Triggered, UnsetFlags); // Not any more.
}
