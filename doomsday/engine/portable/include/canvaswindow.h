/**
 * @file canvaswindow.h
 * Canvas window. @ingroup base
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_CANVASWINDOW_H
#define LIBDENG_CANVASWINDOW_H

#include <QMainWindow>
#include "canvas.h"

/**
 * Top-level window that contains an OpenGL drawing canvas. @ingroup base
 * @see Canvas
 */
class CanvasWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CanvasWindow(QWidget *parent = 0);
    ~CanvasWindow();
    
    Canvas& canvas();

    /**
     * Sets a callback function for notifying about window movement.
     */
    void setMoveFunc(void (*func)(CanvasWindow&));

    // Events.
    void moveEvent(QMoveEvent* ev);
    void hideEvent(QHideEvent* ev);

    /**
     * Must be called before any canvas windows are created. Defines the
     * default OpenGL format settings for the contained canvases.
     */
    static void setDefaultGLFormat();

signals:
    
public slots:

private:
    struct Instance;
    Instance* d;
};

#endif // CANVASWINDOW_H
