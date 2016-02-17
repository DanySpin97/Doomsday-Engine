/** @file multiplayerpanelbuttonwidget.cpp
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/home/multiplayerpanelbuttonwidget.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/clientwindow.h"
#include "clientapp.h"
#include "dd_share.h" // serverinfo_s
#include "dd_main.h"

#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>
#include <doomsday/console/exec.h>
#include <de/charsymbols.h>
#include <de/CallbackAction>
#include <de/PopupButtonWidget>
#include <de/PopupMenuWidget>

#include <QRegExp>

using namespace de;

DENG_GUI_PIMPL(MultiplayerPanelButtonWidget)
{
    serverinfo_t serverInfo;
    ButtonWidget *joinButton;
    String gameConfig;
    LabelWidget *info;
    PopupButtonWidget *extra;
    PopupMenuWidget *extraMenu;

    Instance(Public *i) : Base(i)
    {
        joinButton = new ButtonWidget;
        joinButton->setText(tr("Join"));
        joinButton->useInfoStyle();
        joinButton->setSizePolicy(ui::Expand, ui::Expand);
        joinButton->setAction(new CallbackAction([this] () { joinButtonPressed(); }));
        self.addButton(joinButton);

        info = new LabelWidget;
        info->setSizePolicy(ui::Fixed, ui::Expand);
        info->setAlignment(ui::AlignLeft);
        info->setTextLineAlignment(ui::AlignLeft);
        info->rule().setInput(Rule::Width, self.rule().width());
        info->margins().setLeft(self.icon().rule().width());

        // Menu for additional functions.
        extra = new PopupButtonWidget;
        extra->hide();
        extra->setSizePolicy(ui::Expand, ui::Expand);
        extra->setText("...");
        extra->setFont("small");
        extra->margins().setTopBottom("unit");
        extra->rule()
                .setInput(Rule::Bottom, info->rule().bottom() - info->margins().bottom())
                .setMidAnchorX(info->rule().left() + self.icon().rule().width()/2);
        info->add(extra);

        self.panel().setContent(info);
        self.panel().open();
    }

    void joinButtonPressed() const
    {
        // Switch locally to the game running on the server.
        BusyMode_FreezeGameForBusyMode();
        ClientWindow::main().taskBar().close();

        // Automatically leave the current MP game.
        if(netGame && isClient)
        {
            ClientApp::serverLink().disconnect();
        }

        DoomsdayApp::app().changeGame(
                    DoomsdayApp::games()[serverInfo.gameIdentityKey],
                    DD_ActivateGameWorker);
        Con_Execute(CMDS_DDAY, String("connect %1 %2")
                    .arg(serverInfo.address).arg(serverInfo.port).toLatin1(),
                    false, false);
    }

    bool hasConfig(String const &token) const
    {
        return QRegExp("\\b" + token + "\\b").indexIn(gameConfig) >= 0;
    }
};

MultiplayerPanelButtonWidget::MultiplayerPanelButtonWidget()
    : d(new Instance(this))
{}

void MultiplayerPanelButtonWidget::setSelected(bool selected)
{
    PanelButtonWidget::setSelected(selected);
    d->extra->show(selected);
}

void MultiplayerPanelButtonWidget::updateContent(serverinfo_s const &info)
{
    d->serverInfo = info;
    d->gameConfig = info.gameConfig;

    //label().setText(info.name);
    String meta;
    if(info.numPlayers > 0)
    {
        meta = String("%1 player%2 " DENG2_CHAR_MDASH " ")
                .arg(info.numPlayers)
                .arg(info.numPlayers != 1? "s" : "");
    }

    meta += String("%1").arg(tr(d->hasConfig("coop")? "Co-op" :
                                d->hasConfig("dm2")?  "Deathmatch v2" :
                                                      "Deathmatch"));

    label().setText(String(_E(b) "%1\n" _E(l) "%2")
                    .arg(info.name)
                    .arg(meta));

    // Additional information.
    String infoText = String(info.map) + " " DENG2_CHAR_MDASH " ";
    if(DoomsdayApp::games().contains(info.gameIdentityKey))
    {
        infoText += DoomsdayApp::games()[info.gameIdentityKey].title();
        d->joinButton->enable();
    }
    else
    {
        infoText += tr("Unknown game");
        d->joinButton->disable();
    }
    infoText += "\n" _E(C) + String(info.description);

    d->info->setFont("small");
    d->info->setText(infoText);
}