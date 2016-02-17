/** @file homeitemwidget.cpp
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

#include "ui/home/homeitemwidget.h"

#include <de/SequentialLayout>

using namespace de;

DENG_GUI_PIMPL(HomeItemWidget)
{
    struct ClickHandler : public GuiWidget::IEventHandler
    {
        Public &owner;

        ClickHandler(Public &owner)
            : owner(owner) {}

        bool handleEvent(GuiWidget &, Event const &event)
        {
            if(event.type() == Event::MouseButton)
            {
                MouseEvent const &mouse = event.as<MouseEvent>();
                if(owner.hitTest(event))
                {
                    if(mouse.state() == MouseEvent::Pressed ||
                       mouse.state() == MouseEvent::DoubleClick)
                    {
                        owner.root().setFocus(owner.d->background);
                        emit owner.mouseActivity();
                    }
                    if(mouse.state() == MouseEvent::DoubleClick)
                    {
                        emit owner.doubleClicked();
                        return true;
                    }
                }
            }
            return false;
        }
    };

    LabelWidget *background;
    LabelWidget *icon;
    LabelWidget *label;
    QList<ButtonWidget *> buttons;
    ScalarRule *labelRightMargin;
    Rule const *buttonsWidth = nullptr;
    bool selected = false;

    Instance(Public *i) : Base(i)
    {
        labelRightMargin    = new ScalarRule(0);

        self.add(background = new LabelWidget);
        self.add(icon       = new LabelWidget);
        self.add(label      = new LabelWidget);

        label->setSizePolicy(ui::Fixed, ui::Expand);
        label->setTextLineAlignment(ui::AlignLeft);
        label->setAlignment(ui::AlignLeft);
        label->setBehavior(ChildVisibilityClipping);

        background->setBehavior(Focusable);
        background->addEventHandler(new ClickHandler(self));
    }

    ~Instance()
    {
        releaseRef(labelRightMargin);
        releaseRef(buttonsWidth);
    }

    void updateButtonLayout()
    {
        SequentialLayout layout(label->rule().right() - *labelRightMargin,
                                label->rule().top(), ui::Right);
        for(auto *button : buttons)
        {
            layout << *button;
            button->rule().setMidAnchorY(label->rule().midY());
        }
        changeRef(buttonsWidth, layout.width() + style().rules().rule("gap"));
    }

    void showButtons(bool show)
    {
        if(!buttonsWidth) return;

        TimeDelta const SPAN = .5;
        if(show)
        {
            labelRightMargin->set(*buttonsWidth, SPAN);
        }
        else
        {
            labelRightMargin->set(0, SPAN);
        }
    }
};

HomeItemWidget::HomeItemWidget(String const &name)
    : GuiWidget(name)
    , d(new Instance(this))
{
    setBehavior(Focusable);

    Rule const &iconSize = d->label->rule().height();

    d->background->rule()
            .setInput(Rule::Top,    rule().top())
            .setInput(Rule::Left,   d->icon->rule().right())
            .setInput(Rule::Right,  rule().right())
            .setInput(Rule::Bottom, d->label->rule().bottom());

    d->icon->rule()
            .setSize(iconSize, iconSize)
            .setInput(Rule::Left, rule().left())
            .setInput(Rule::Top,  rule().top());
    d->icon->set(Background(Background::BorderGlow,
                            style().colors().colorf("home.icon.shadow"), 20));

    d->label->rule()
            .setInput(Rule::Top,   rule().top())
            .setInput(Rule::Left,  d->icon->rule().right())
            .setInput(Rule::Right, rule().right());
    d->label->margins().setRight(*d->labelRightMargin +
                                 style().rules().rule("gap"));

    rule().setInput(Rule::Height, d->label->rule().height());
}

LabelWidget &HomeItemWidget::icon()
{
    return *d->icon;
}

LabelWidget &HomeItemWidget::label()
{
    return *d->label;
}

void HomeItemWidget::setSelected(bool selected)
{
    d->selected = selected;
    if(selected)
    {
        d->background->set(Background(style().colors().colorf("background")));
        d->showButtons(true);
    }
    else
    {
        d->background->set(Background());
        d->showButtons(false);
    }
}

bool HomeItemWidget::isSelected() const
{
    return d->selected;
}

void HomeItemWidget::addButton(ButtonWidget *button)
{
    // Common styling.
    button->setSizePolicy(ui::Expand, ui::Expand);

    d->buttons << button;
    d->label->add(button);
    d->updateButtonLayout();
}