/**
 * @file canvas.cpp
 * OpenGL drawing surface implementation. @ingroup gl
 *
 * @todo Merge mouse_qt.c with this source file since the mouse tracking
 * functionality is implemented here.
 *
 * @authors Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QImage>
#include <QCursor>
#include <QTimer>
#include <QTime>
#include <QDebug>

#include "sys_opengl.h"
#include "sys_input.h"
#include "mouse_qt.h"
#include "displaymode.h"
#include "keycode.h"
#include "canvas.h"

static const int MOUSE_WHEEL_CONTINUOUS_THRESHOLD_MS = 100;

struct Canvas::Instance
{
    Canvas* self;
    bool initNotified;
    void (*initCallback)(Canvas&);
    void (*drawCallback)(Canvas&);
    void (*resizedCallback)(Canvas&);
    void (*focusCallback)(Canvas&, bool);
    bool mouseGrabbed;
    QPoint prevMousePos;
    QTime prevWheelAt;
    int wheelDir[2];

    Instance(Canvas* c)
        : self(c),
          initNotified(false), initCallback(0),
          drawCallback(0),
          resizedCallback(0),
          focusCallback(0),
          mouseGrabbed(false)
    {
        wheelDir[0] = wheelDir[1] = 0;
    }

    void grabMouse()
    {
        if(mouseGrabbed) return;

        // Tell the mouse driver that the mouse is supposed to be trapped now.
        mouseGrabbed = true;
        Mouse_Trap(true);

#ifndef WIN32
        // Start tracking the mouse now.
        QCursor::setPos(self->mapToGlobal(self->rect().center()));
        self->grabMouse();
        self->setCursor(QCursor(Qt::BlankCursor));
        qApp->setOverrideCursor(QCursor(Qt::BlankCursor));
        QTimer::singleShot(1, self, SLOT(trackMousePosition()));
#endif
    }

    void ungrabMouse()
    {
        if(!mouseGrabbed) return;

#ifndef WIN32
        self->releaseMouse();
        qApp->restoreOverrideCursor();
        self->setCursor(QCursor(Qt::ArrowCursor)); // Default cursor.
#endif

        // Tell the mouse driver that the mouse is untrapepd.
        mouseGrabbed = false;
        Mouse_Trap(false);
    }
};

Canvas::Canvas(QWidget *parent) : QGLWidget(parent)
{
    d = new Instance(this);

    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true); // receive moves always

    // We will be doing buffer swaps manually (for timing purposes).
    setAutoBufferSwap(false);
}

Canvas::~Canvas()
{
    delete d;
}

void Canvas::setInitFunc(void (*canvasInitializeFunc)(Canvas&))
{
    d->initCallback = canvasInitializeFunc;
}

void Canvas::setDrawFunc(void (*canvasDrawFunc)(Canvas&))
{
    d->drawCallback = canvasDrawFunc;
}

void Canvas::setResizedFunc(void (*canvasResizedFunc)(Canvas&))
{
    d->resizedCallback = canvasResizedFunc;
}

void Canvas::setFocusFunc(void (*canvasFocusChanged)(Canvas&, bool))
{
    d->focusCallback = canvasFocusChanged;
}

QImage Canvas::grabImage(const QSize& outputSize)
{
    QImage grabbed = grabFrameBuffer(); // no alpha
    if(outputSize.isValid())
    {
        grabbed = grabbed.scaled(outputSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return grabbed;
}

GLuint Canvas::grabAsTexture(const QSize& outputSize)
{
    return bindTexture(grabImage(outputSize), GL_TEXTURE_2D, GL_RGB,
                       QGLContext::LinearFilteringBindOption);
}

void Canvas::grab(image_t* img, const QSize& outputSize)
{
    QImage grabbed = grabImage(outputSize);

    GL_InitImage(img);
    img->size.width = grabbed.width();
    img->size.height = grabbed.height();
    img->pixels = (uint8_t*) malloc(grabbed.byteCount());
    memcpy(img->pixels, grabbed.constBits(), grabbed.byteCount());
    img->pixelSize = grabbed.depth()/8;

#ifdef _DEBUG
    qDebug() << "Canvas: grabbed" << grabbed.width() << "x" << grabbed.height()
             << "byteCount:" << grabbed.byteCount()
             << "depth:" << grabbed.depth() << "format" << grabbed.format();
#endif

    Q_ASSERT(img->pixelSize != 0);
}

void Canvas::trapMouse(bool trap)
{
    if(!Mouse_IsPresent()) return;

    if(trap)
    {
        d->grabMouse();
    }
    else
    {
        d->ungrabMouse();
    }
}

void Canvas::initializeGL()
{
    Sys_GLConfigureDefaultState();
}

void Canvas::resizeGL(int w, int h)
{
    qDebug() << "Canvas: resized" << w << "x" << h;

    if(d->resizedCallback)
    {
        d->resizedCallback(*this);
    }
}

void Canvas::showEvent(QShowEvent* ev)
{
    QGLWidget::showEvent(ev);

    // The first time the window is shown, run the initialization callback. On
    // some platforms, OpenGL is not fully ready to be used before the window
    // actually appears on screen.
    if(isVisible() && !d->initNotified)
    {
        QTimer::singleShot(1, this, SLOT(notifyInit()));
    }
}

void Canvas::notifyInit()
{
    if(!d->initNotified && d->initCallback)
    {
        d->initNotified = true;
        d->initCallback(*this);
    }
}

void Canvas::trackMousePosition(bool keepTracking)
{
    if(d->mouseGrabbed)
    {
        QPoint curPos = mapFromGlobal(QCursor::pos());
        if(!d->prevMousePos.isNull())
        {
            QPoint delta = curPos - d->prevMousePos;
            if(!delta.isNull())
            {
                Mouse_Qt_SubmitMotion(IMA_POINTER, delta.x(), delta.y());

                // Keep the cursor centered.
                QPoint mid = rect().center();
                QCursor::setPos(mapToGlobal(mid));
                d->prevMousePos = mid;
            }
        }
        else
        {
            d->prevMousePos = curPos;
        }
        if(keepTracking) QTimer::singleShot(1, this, SLOT(trackMousePosition()));
    }
    else
    {
        // Mouse was ungrabbed; reset the tracking.
        d->prevMousePos = QPoint();
    }
}

void Canvas::paintGL()
{
    if(d->drawCallback)
    {
        d->drawCallback(*this);
    }
    else
    {
        qDebug() << "Canvas: drawing with default";

        // If we don't know what else to draw, just draw a black screen.
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        swapBuffers();
    }
}

void Canvas::focusInEvent(QFocusEvent*)
{
    qDebug() << "Canvas: focus in";

    if(d->focusCallback) d->focusCallback(*this, true);
}

void Canvas::focusOutEvent(QFocusEvent*)
{
    qDebug() << "Canvas: focus out";

    d->ungrabMouse();

    if(d->focusCallback) d->focusCallback(*this, false);
}

static int nativeCode(QKeyEvent* ev)
{
#if defined(UNIX) && !defined(MACOSX)
    return ev->nativeScanCode();
#else
    return ev->nativeVirtualKey();
#endif
}

void Canvas::keyPressEvent(QKeyEvent *ev)
{
    ev->accept();
    if(ev->isAutoRepeat()) return; // Ignore repeats, we do our own.

    qDebug() << "Canvas: key press" << ev->key() << QString("0x%1").arg(ev->key(), 0, 16)
             << "text:" << ev->text()
             << "native:" << ev->nativeVirtualKey()
             << "scancode:" << ev->nativeScanCode();

    /// @todo Use the Unicode text instead.

    Keyboard_Submit(IKE_DOWN,
                    Keycode_TranslateFromQt(ev->key(), ev->nativeVirtualKey(), ev->nativeScanCode()),
                    nativeCode(ev),
                    ev->text().isEmpty()? 0 : ev->text().toLatin1().constData());
}

void Canvas::keyReleaseEvent(QKeyEvent *ev)
{
    ev->accept();
    if(ev->isAutoRepeat()) return; // Ignore repeats, we do our own.

    qDebug() << "Canvas: key release" << ev->key() << "text:" << ev->text()
             << "native:" << ev->nativeVirtualKey();

    Keyboard_Submit(IKE_UP,
                    Keycode_TranslateFromQt(ev->key(), ev->nativeVirtualKey(), ev->nativeScanCode()),
                    nativeCode(ev),
                    ev->text().isEmpty()? 0 : ev->text().toLatin1().constData());
}

static int translateButton(Qt::MouseButton btn)
{
    if(btn == Qt::LeftButton) return IMB_LEFT;
    if(btn == Qt::MiddleButton) return IMB_MIDDLE;
    if(btn == Qt::RightButton) return IMB_RIGHT;
    return -1;
}

void Canvas::mousePressEvent(QMouseEvent* ev)
{
    if(!d->mouseGrabbed)
    {
        // The mouse will be grabbed when the button is released.
        ev->ignore();
        return;
    }

    ev->accept();

    Mouse_Qt_SubmitButton(translateButton(ev->button()), true);

    qDebug() << "Canvas: mouse press at" << ev->pos();
}

void Canvas::mouseReleaseEvent(QMouseEvent* ev)
{
    ev->accept();

    if(!d->mouseGrabbed)
    {
        // Start grabbing after a click.
        trapMouse();
        return;
    }

    Mouse_Qt_SubmitButton(translateButton(ev->button()), false);

    qDebug() << "Canvas: mouse release at" << ev->pos();
}

void Canvas::wheelEvent(QWheelEvent *ev)
{
    ev->accept();

    bool continuousMovement = (d->prevWheelAt.elapsed() < MOUSE_WHEEL_CONTINUOUS_THRESHOLD_MS);
    int axis = (ev->orientation() == Qt::Horizontal? 0 : 1);
    int dir = (ev->delta() < 0? -1 : 1);

    if(!continuousMovement || d->wheelDir[axis] != dir)
    {
        d->wheelDir[axis] = dir;
        qDebug() << "Canvas: signal wheel axis" << axis << "dir" << dir;

        Mouse_Qt_SubmitMotion(IMA_WHEEL, axis == 0? dir : 0, axis == 1? dir : 0);
    }

        /*
    if(ev->orientation() == Qt::Vertical)
    {
        qDebug() << "Canvas: vertical mouse wheel" << ev->delta()/8.f << "continous" << continuum;
    }
    else
    {
        qDebug() << "Canvas: horizontal mouse wheel" << ev->delta()/8.f << "continous" << continuum;
    }*/

    d->prevWheelAt.start();
}
