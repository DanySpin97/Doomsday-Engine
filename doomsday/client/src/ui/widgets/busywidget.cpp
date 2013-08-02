/** @file busywidget.cpp
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

#include "de_platform.h"
#include "ui/widgets/busywidget.h"
#include "ui/widgets/progresswidget.h"
#include "ui/busyvisual.h"
#include "busymode.h"
#include "sys_system.h"
#include "render/r_main.h"
#include "ui/ui_main.h"
#include "ui/clientwindow.h"

#include <de/RootWidget>
#include <de/GLState>
#include <de/GLTarget>

using namespace de;

DENG2_PIMPL(BusyWidget)
{
    ProgressWidget *progress;

    Instance(Public *i) : Base(i)
    {
        progress = new ProgressWidget;
        progress->setRange(Rangei(0, 200));
        progress->setImageScale(.2f);
        progress->rule().setRect(self.rule());
        self.add(progress);
    }
};

BusyWidget::BusyWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{}

ProgressWidget &BusyWidget::progress()
{
    return *d->progress;
}

void BusyWidget::viewResized()
{
    if(!BusyMode_Active() || isDisabled() || Sys_IsShuttingDown()) return;

    ClientWindow::main().glActivate(); // needed for legacy stuff

    //DENG_ASSERT(BusyMode_Active());

    LOG_AS("BusyWidget");
    LOG_DEBUG("View resized to ") << root().viewSize().asText();

    // Update viewports.
    R_SetViewGrid(0, 0);
    R_UseViewPort(0);
    R_LoadSystemFonts();

    if(UI_IsActive())
    {
        UI_UpdatePageLayout();
    }
}

void BusyWidget::update()
{
    GuiWidget::update();

    DENG_ASSERT(BusyMode_Active());
    BusyMode_Loop();
}

void BusyWidget::drawContent()
{
    //DENG_ASSERT(BusyMode_Active());
    //BusyVisual_Render();

    root().window().canvas().renderTarget().clear(GLTarget::ColorDepth);
}

bool BusyWidget::handleEvent(Event const &)
{
    // Eat events and ignore them.
    return true;
}
