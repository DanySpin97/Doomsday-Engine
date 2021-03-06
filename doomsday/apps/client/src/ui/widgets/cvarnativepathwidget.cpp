/** @file cvarnativepathwidget.cpp  Console variable with a native path.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "ui/widgets/cvarnativepathwidget.h"
#include "ui/clientwindow.h"
#include "clientapp.h"

#include <doomsday/console/var.h>
#include <de/PopupMenuWidget>
#include <de/SignalAction>
#include <QFileDialog>

using namespace de;

DENG2_PIMPL(CVarNativePathWidget)
, DENG2_OBSERVES(NativePathWidget, UserChange)
{
    char const *cvar;

    Impl(Public *i) : Base(i)
    {}

    cvar_t *var() const
    {
        cvar_t *cv = Con_FindVariable(cvar);
        DENG2_ASSERT(cv != 0);
        return cv;
    }

    void pathChangedByUser(NativePathWidget &)
    {
        self().setCVarValueFromWidget();
    }
};

CVarNativePathWidget::CVarNativePathWidget(char const *cvarPath)
    : d(new Impl(this))
{
    d->cvar = cvarPath;
    updateFromCVar();
    setPrompt(QString("Select File for \"%1\"").arg(d->cvar));
    audienceForUserChange() += d;
}

char const *CVarNativePathWidget::cvarPath() const
{
    return d->cvar;
}

void CVarNativePathWidget::updateFromCVar()
{
    setPath(CVar_String(d->var()));
}

void CVarNativePathWidget::setCVarValueFromWidget()
{
    CVar_SetString(d->var(), path().toString().toUtf8());
}
