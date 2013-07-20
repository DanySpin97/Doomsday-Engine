/** @file widget.h Base class for widgets.
 *
 * @defgroup widgets  Widget Framework
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

#ifndef LIBDENG2_WIDGET_H
#define LIBDENG2_WIDGET_H

#include "../String"
#include "../Event"
#include "../Id"
#include "../Observers"
#include "../DotPath"

#include <QList>

namespace de {

class Widget;
class RootWidget;

typedef QList<Widget *> WidgetList;

/**
 * Base class for widgets.
 * @ingroup widgets
 */
class DENG2_PUBLIC Widget
{
public:
    /// Widget that was expected to exist was not found. @ingroup errors
    DENG2_ERROR(NotFoundError);

    enum Behavior
    {
        /// Widget is invisible: not drawn. Hidden widgets also receive no events.
        Hidden = 0x1,

        /// Widget is disabled. The meaning of this is Widget-specific. Events
        /// will still be dispatched to the widget even though it's disabled.
        Disabled = 0x2,

        /// Widget will only receive events if it has focus.
        HandleEventsOnlyWhenFocused = 0x4,

        /// Widget cannot be hit by a pointer device.
        Unhittable = 0x8,

        /// Widget's content will not extend visually beyoud its boundaries.
        ContentClipping = 0x10,

        /// Children cannot be hit outside this widget's boundaries.
        ChildHitClipping = 0x20,

        /// No events will be dispatched to the widget (or its children).
        DisableEventDispatch = 0x40,

        /// No events will be dispatched to the children of the widget.
        DisableEventDispatchToChildren = 0x80,

        DefaultBehavior = 0
    };
    Q_DECLARE_FLAGS(Behaviors, Behavior)

    typedef WidgetList Children;

    /**
     * Notified when the widget's parent changes.
     */
    DENG2_DEFINE_AUDIENCE(ParentChange, void widgetParentChanged(Widget &child, Widget *oldParent, Widget *newParent))

public:
    Widget(String const &name = "");
    virtual ~Widget();

    template <typename Type>
    Type &as() {
        DENG2_ASSERT(dynamic_cast<Type *>(this) != 0);
        return *static_cast<Type *>(this);
    }

    template <typename Type>
    Type const &as() const {
        DENG2_ASSERT(dynamic_cast<Type const *>(this) != 0);
        return *static_cast<Type const *>(this);
    }

    /**
     * Returns the automatically generated, unique identifier of the widget.
     */
    Id id() const;

    String name() const;
    void setName(String const &name);

    /**
     * Forms the dotted path of the widget. Assumes that the names of this
     * widget and its parents use dots to indicate hierarchy.
     */
    DotPath path() const;

    bool hasRoot() const;
    RootWidget &root() const;
    bool hasFocus() const;

    void show(bool doShow = true);
    inline void hide() { show(false); }
    void enable(bool yes = true) { setBehavior(Disabled, yes? UnsetFlags : SetFlags); }
    void disable(bool yes = true) { setBehavior(Disabled, yes? SetFlags : UnsetFlags); }

    bool isHidden() const;
    bool isVisible() const;
    inline bool isEnabled() const { return !behavior().testFlag(Disabled); }
    inline bool isDisabled() const { return behavior().testFlag(Disabled); }

    /**
     * Sets or clears one or more behavior flags.
     *
     * @param behavior   Flags to modify.
     * @param operation  Operation to perform on the flags.
     */
    void setBehavior(Behaviors behavior, FlagOp operation = SetFlags);

    /**
     * Clears one or more behavior flags.
     *
     * @param behavior   Flags to unset.
     */
    void unsetBehavior(Behaviors behavior);

    Behaviors behavior() const;

    /**
     * Sets the identifier of the widget that will receive focus when
     * a forward focus navigation is requested.
     *
     * @param name  Name of a widget for forwards navigation.
     */
    void setFocusNext(String const &name);

    /**
     * Sets the identifier of the widget that will receive focus when
     * a backwards focus navigation is requested.
     *
     * @param name  Name of a widget for backwards navigation.
     */
    void setFocusPrev(String const &name);

    String focusNext() const;
    String focusPrev() const;

    /**
     * Routines specific types of events to another widget. It will be
     * the only one offered those events.
     *
     * @param types    List of event types.
     * @param routeTo  Widget to route events to. Caller must ensure
     *                 that the widget is not destroyed while the
     *                 routing is in effect.
     */
    void setEventRouting(QList<int> const &types, Widget *routeTo);

    void clearEventRouting();

    bool isEventRouted(int type, Widget *to) const;

    // Tree organization.
    void clear();
    Widget &add(Widget *child);
    Widget *remove(Widget &child);
    Widget *find(String const &name);
    Widget const *find(String const &name) const;
    Children children() const;
    dsize childCount() const;
    Widget *parent() const;

    // Utilities.
    String uniqueName(String const &name) const;

    /**
     * Arguments for notifyTree() and notifyTreeReversed().
     */
    struct NotifyArgs {
        enum Result {
            Abort,
            Continue
        };
        void (Widget::*notifyFunc)();
        bool (Widget::*conditionFunc)() const;
        void (Widget::*preNotifyFunc)();
        void (Widget::*postNotifyFunc)();
        Widget *until;

        NotifyArgs(void (Widget::*notify)()) : notifyFunc(notify),
            conditionFunc(0), preNotifyFunc(0), postNotifyFunc(0),
            until(0) {}
    };

    NotifyArgs::Result notifyTree(NotifyArgs const &args);
    void notifyTreeReversed(NotifyArgs const &args);
    bool dispatchEvent(Event const &event, bool (Widget::*memberFunc)(Event const &));

    // Events.
    virtual void initialize();
    virtual void deinitialize();
    virtual void viewResized();
    virtual void focusGained();
    virtual void focusLost();
    virtual void update();
    virtual void draw();
    virtual void preDrawChildren();
    virtual void postDrawChildren();
    virtual bool handleEvent(Event const &event);

public:
    static void setFocusCycle(WidgetList const &order);

protected:
    virtual void addedChildWidget(Widget &widget);
    virtual void removedChildWidget(Widget &widget);

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Widget::Behaviors)

} // namespace de

#endif // LIBDENG2_WIDGET_H
