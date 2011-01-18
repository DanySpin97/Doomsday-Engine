/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "window.h"
#include "surface.h"
#include "video.h"
#include "rules.h"
#include <QDebug>

using namespace de;

Window::Window(const QGLFormat& format, QWidget* parent, const QGLWidget* shareWidget)
    : GLWindowSurface(format, parent, shareWidget)
{
    // Define rules for the root visual's placement.
    _root.setRule(Visual::Left, new ConstantRule(0));
    _root.setRule(Visual::Top, new ConstantRule(0));
    _root.setRule(Visual::Width, new ConstantRule(width()));
    _root.setRule(Visual::Height, new ConstantRule(height()));
}

Window::~Window()
{}

void Window::setSelectedFlags(Flags selectedFlags, bool set)
{
    Flags flags = _flags;
    if(set)
    {
        flags |= selectedFlags;
    }
    else
    {
        flags &= ~selectedFlags;
    }
    setFlags(flags);
}

void Window::setFlags(Flags allFlags)
{
    _flags = allFlags;
}

void Window::surfaceResized(const QSize& size)
{
    qDebug() << "Window: Surface resized" << size;

    // Update the root visual's dimensions.
    _root.ruleAs<ConstantRule>(Visual::Width).set(size.width());
    _root.ruleAs<ConstantRule>(Visual::Height).set(size.height());
}

void Window::draw()
{
    // Draw all the visuals.
    _root.draw();
}
