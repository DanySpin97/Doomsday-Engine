/** @file canvas.cpp  OpenGL drawing surface (QWidget).
 *
 * @authors Copyright (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Canvas"
#include "de/CanvasWindow"
#include "de/GLState"
#include "de/GLTexture"
#include "de/gui/opengl.h"

#include <de/App>
#include <de/Log>
#include <de/Drawable>

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QImage>
#include <QTimer>
#include <QTime>
#include <QDebug>

namespace de {

static const int MOUSE_WHEEL_CONTINUOUS_THRESHOLD_MS = 100;

DENG2_PIMPL(Canvas)
{
    enum FramebufferMode {
        AutomaticFramebuffer,
        ManualFramebuffer
    };
    FramebufferMode framebufMode;

    GLTarget target;
    GLTexture backBuffer; ///< Texture where color values are written.
    GLTexture depthStencilBuffer; ///< Texture where framebuffer depth values are stored.

    Drawable bufSwap;
    GLUniform uMvpMatrix;
    GLUniform uBufTex;
    typedef GLBufferT<Vertex2Tex> VBuf;

    CanvasWindow *parent;
    bool readyNotified;
    Size currentSize;
    bool mouseGrabbed;
#ifdef WIN32
    bool altIsDown;
#endif
    QPoint prevMousePos;
    QTime prevWheelAt;
    int wheelDir[2];

    Instance(Public *i, CanvasWindow *parentWindow)
        : Base(i)
        , framebufMode(AutomaticFramebuffer)
        , uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
        , uBufTex("uTex", GLUniform::Sampler2D)
        , parent(parentWindow)
        , readyNotified(false)
        , mouseGrabbed(false)
    {        
        wheelDir[0] = wheelDir[1] = 0;
#ifdef WIN32
        altIsDown = false;
#endif
        //mouseDisabled = App::commandLine().has("-nomouse");
    }

    void grabMouse()
    {
        if(!self.isVisible()/* || mouseDisabled*/) return;

        LOG_DEBUG("grabbing mouse (already grabbed? %b)") << mouseGrabbed;

        if(!mouseGrabbed)
        {
            mouseGrabbed = true;

            DENG2_FOR_PUBLIC_AUDIENCE(MouseStateChange, i)
            {
                i->mouseStateChanged(Trapped);
            }
        }
    }

    void ungrabMouse()
    {
        if(!self.isVisible()/* || mouseDisabled*/) return;

        LOG_DEBUG("ungrabbing mouse (presently grabbed? %b)") << mouseGrabbed;

        if(mouseGrabbed)
        {
            // Tell the mouse driver that the mouse is untrapped.
            mouseGrabbed = false;

            DENG2_FOR_PUBLIC_AUDIENCE(MouseStateChange, i)
            {
                i->mouseStateChanged(Untrapped);
            }
        }
    }

    static int nativeCode(QKeyEvent const *ev)
    {
#if defined(UNIX) && !defined(MACOSX)
        return ev->nativeScanCode();
#else
        return ev->nativeVirtualKey();
#endif
    }

    void handleKeyEvent(QKeyEvent *ev)
    {
        //LOG_AS("Canvas");

        ev->accept();
        //if(ev->isAutoRepeat()) return; // Ignore repeats, we do our own.

        /*
        qDebug() << "Canvas: key press" << ev->key() << QString("0x%1").arg(ev->key(), 0, 16)
                 << "text:" << ev->text()
                 << "native:" << ev->nativeVirtualKey()
                 << "scancode:" << ev->nativeScanCode();
        */

#ifdef WIN32
        // We must track the state of the alt key ourselves as the OS grabs the up event...
        if(ev->key() == Qt::Key_Alt)
        {
            if(ev->type() == QEvent::KeyPress)
            {
                if(altIsDown) return; // Ignore repeat down events(!)?
                altIsDown = true;
            }
            else if(ev->type() == QEvent::KeyRelease)
            {
                if(!altIsDown)
                {
                    LOG_DEBUG("Ignoring repeat alt up.");
                    return; // Ignore repeat up events.
                }
                altIsDown = false;
                //LOG_DEBUG("Alt is up.");
            }
        }
#endif

        DENG2_FOR_PUBLIC_AUDIENCE(KeyEvent, i)
        {
            i->keyEvent(KeyEvent(ev->isAutoRepeat()?             KeyEvent::Repeat :
                                 ev->type() == QEvent::KeyPress? KeyEvent::Pressed :
                                                                 KeyEvent::Released,
                                 ev->key(),
                                 KeyEvent::ddKeyFromQt(ev->key(), ev->nativeVirtualKey(), ev->nativeScanCode()),
                                 nativeCode(ev),
                                 ev->text(),
                                 (ev->modifiers().testFlag(Qt::ShiftModifier)?   KeyEvent::Shift   : KeyEvent::NoModifiers) |
                                 (ev->modifiers().testFlag(Qt::ControlModifier)? KeyEvent::Control : KeyEvent::NoModifiers) |
                                 (ev->modifiers().testFlag(Qt::AltModifier)?     KeyEvent::Alt     : KeyEvent::NoModifiers) |
                                 (ev->modifiers().testFlag(Qt::MetaModifier)?    KeyEvent::Meta    : KeyEvent::NoModifiers)));
        }
    }

    void reconfigureFramebuffer()
    {
        if(framebufMode == ManualFramebuffer)
        {
            /// @todo For multisampling there should be a bigger back buffer.

            // Set up a color buffer.
            backBuffer.setUndefinedImage(currentSize, Image::RGB_888);
            backBuffer.setWrap(gl::ClampToEdge, gl::ClampToEdge);
            backBuffer.setFilter(gl::Nearest, gl::Nearest, gl::MipNone);

            // Set up a depth/stencil buffer.
            depthStencilBuffer.setDepthStencilContent(currentSize);
            depthStencilBuffer.setWrap(gl::ClampToEdge, gl::ClampToEdge);
            depthStencilBuffer.setFilter(gl::Nearest, gl::Nearest, gl::MipNone);

            target.configure(&backBuffer, &depthStencilBuffer);
        }
    }

    void glInit()
    {
        DENG2_ASSERT(parent != 0);

        VBuf *buf = new VBuf;
        bufSwap.addBuffer(buf);
        bufSwap.program().build(// Vertex shader:
                                Block("uniform highp mat4 uMvpMatrix; "
                                      "attribute highp vec4 aVertex; "
                                      "attribute highp vec2 aUV; "
                                      "varying highp vec2 vUV; "
                                      "void main(void) {"
                                          "gl_Position = uMvpMatrix * aVertex; "
                                          "vUV = aUV; }"),
                                // Fragment shader:
                                Block("uniform sampler2D uTex; "
                                      "varying highp vec2 vUV; "
                                      "void main(void) { "
                                          "gl_FragColor = texture2D(uTex, vUV); }"))
                << uMvpMatrix
                << uBufTex;

        buf->setVertices(gl::TriangleStrip,
                         VBuf::Builder().makeQuad(Rectanglef(0, 0, 1, 1), Rectanglef(0, 1, 1, -1)),
                         gl::Static);

        uMvpMatrix = Matrix4f::ortho(0, 1, 0, 1);
        uBufTex = backBuffer;
    }

    void glDeinit()
    {
        bufSwap.clear();
    }

    void swapBuffers()
    {
        switch(framebufMode)
        {
        case AutomaticFramebuffer:
            self.QGLWidget::swapBuffers();
            break;

        case ManualFramebuffer: {
            /// @todo Double buffering is not really needed in manual FB mode.
            GLTarget defaultTarget;
            GLState::push()
                    .setTarget(defaultTarget)
                    .setViewport(Rectangleui::fromSize(self.size()));

            // Draw the back buffer texture to the main framebuffer.
            bufSwap.draw();

            self.QGLWidget::swapBuffers();

            GLState::pop().apply();
            break; }
        }

    }
};

Canvas::Canvas(CanvasWindow* parent, QGLWidget* shared)
    : QGLWidget(parent, shared), d(new Instance(this, parent))
{
    LOG_AS("Canvas");
    LOG_DEBUG("swap interval: ") << format().swapInterval();
    LOG_DEBUG("multisample: %b") << format().sampleBuffers();

    // We will be doing buffer swaps manually (for timing purposes).
    setAutoBufferSwap(false);

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void Canvas::setParent(CanvasWindow *parent)
{
    d->parent = parent;
}

QImage Canvas::grabImage(QSize const &outputSize)
{
    return grabImage(rect(), outputSize);
}

QImage Canvas::grabImage(QRect const &area, QSize const &outputSize)
{
    // We will be grabbing the visible, latest complete frame.
    glReadBuffer(GL_FRONT);
    QImage grabbed = grabFrameBuffer(); // no alpha
    if(area.size() != grabbed.size())
    {
        // Just take a portion of the full image.
        grabbed = grabbed.copy(area);
    }
    glReadBuffer(GL_BACK);
    if(outputSize.isValid())
    {
        grabbed = grabbed.scaled(outputSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return grabbed;
}

GLuint Canvas::grabAsTexture(QSize const &outputSize)
{
    return grabAsTexture(rect(), outputSize);
}

GLuint Canvas::grabAsTexture(QRect const &area, QSize const &outputSize)
{
    return bindTexture(grabImage(area, outputSize), GL_TEXTURE_2D, GL_RGB,
                       QGLContext::LinearFilteringBindOption);
}

Canvas::Size Canvas::size() const
{
    return d->currentSize;
}

void Canvas::trapMouse(bool trap)
{
    if(trap)
    {
        d->grabMouse();
    }
    else
    {
        d->ungrabMouse();
    }
}

bool Canvas::isMouseTrapped() const
{
    return d->mouseGrabbed;
}

bool Canvas::isGLReady() const
{
    return d->readyNotified;
}

void Canvas::copyAudiencesFrom(Canvas const &other)
{
    audienceForGLReady          = other.audienceForGLReady;
    audienceForGLInit           = other.audienceForGLInit;
    audienceForGLResize         = other.audienceForGLResize;
    audienceForGLDraw           = other.audienceForGLDraw;
    audienceForFocusChange      = other.audienceForFocusChange;

    audienceForKeyEvent         = other.audienceForKeyEvent;

    audienceForMouseStateChange = other.audienceForMouseStateChange;
    audienceForMouseEvent       = other.audienceForMouseEvent;
}

GLTarget &Canvas::renderTarget() const
{
    return d->target;
}

GLTexture &Canvas::depthBufferTexture() const
{
    return d->depthStencilBuffer;
}

void Canvas::swapBuffers()
{
    d->swapBuffers();
}

void Canvas::initializeGL()
{
    LOG_AS("Canvas");
    LOG_DEBUG("Notifying GL init (during paint)");

#ifdef LIBGUI_USE_GLENTRYPOINTS
    getAllOpenGLEntryPoints();
#endif

    DENG2_FOR_AUDIENCE(GLInit, i) i->canvasGLInit(*this);
}

void Canvas::resizeGL(int w, int h)
{
    Size newSize(max(0, w), max(0, h));

    // Only react if this is actually a resize.
    if(d->currentSize != newSize)
    {
        d->currentSize = newSize;
        d->reconfigureFramebuffer();

        DENG2_FOR_AUDIENCE(GLResize, i) i->canvasGLResized(*this);
    }
}

void Canvas::showEvent(QShowEvent* ev)
{
    LOG_AS("Canvas");

    QGLWidget::showEvent(ev);

    // The first time the window is shown, run the initialization callback. On
    // some platforms, OpenGL is not fully ready to be used before the window
    // actually appears on screen.
    if(isVisible() && !d->readyNotified)
    {
        LOG_DEBUG("Received first show event, scheduling GL ready notification");

#ifdef LIBGUI_USE_GLENTRYPOINTS
        makeCurrent();
        getAllOpenGLEntryPoints();
#endif
        QTimer::singleShot(1, this, SLOT(notifyReady()));
    }
}

void Canvas::notifyReady()
{
    if(d->readyNotified) return;

    d->readyNotified = true;

#if 1
    {
        d->framebufMode = Instance::ManualFramebuffer;

        d->glInit();

        // Set up a rendering target with depth texture.
        LOG_DEBUG("Using manually configured framebuffer in Canvas, size ") << size().asText();
        d->reconfigureFramebuffer();
    }
#endif

    LOG_DEBUG("Notifying GL ready");

    DENG2_FOR_AUDIENCE(GLReady, i) i->canvasGLReady(*this);

    // This Canvas instance might have been destroyed now.
}

void Canvas::paintGL()
{
    // Make sure any changes to the state stack become effective.
    GLState::current().apply();

    DENG2_FOR_AUDIENCE(GLDraw, i) i->canvasGLDraw(*this);
}

void Canvas::focusInEvent(QFocusEvent*)
{
    LOG_AS("Canvas");
    LOG_INFO("Gained focus.");

    DENG2_FOR_AUDIENCE(FocusChange, i) i->canvasFocusChanged(*this, true);
}

void Canvas::focusOutEvent(QFocusEvent*)
{
    LOG_AS("Canvas");
    LOG_INFO("Lost focus.");

    // Automatically ungrab the mouse if focus is lost.
    d->ungrabMouse();

    DENG2_FOR_AUDIENCE(FocusChange, i) i->canvasFocusChanged(*this, false);
}

void Canvas::keyPressEvent(QKeyEvent *ev)
{
    d->handleKeyEvent(ev);
}

void Canvas::keyReleaseEvent(QKeyEvent *ev)
{
    d->handleKeyEvent(ev);
}

static MouseEvent::Button translateButton(Qt::MouseButton btn)
{
    if(btn == Qt::LeftButton)   return MouseEvent::Left;
#ifdef DENG2_QT_4_7_OR_NEWER
    if(btn == Qt::MiddleButton) return MouseEvent::Middle;
#else
    if(btn == Qt::MidButton)    return MouseEvent::Middle;
#endif
    if(btn == Qt::RightButton)  return MouseEvent::Right;
    if(btn == Qt::XButton1)     return MouseEvent::XButton1;
    if(btn == Qt::XButton2)     return MouseEvent::XButton2;

    return MouseEvent::Unknown;
}

void Canvas::mousePressEvent(QMouseEvent *ev)
{
    /*
    if(!d->mouseGrabbed)
    {
        // The mouse will be grabbed when the button is released.
        ev->ignore();
        return;
    }*/

    ev->accept();

    DENG2_FOR_AUDIENCE(MouseEvent, i)
    {
        i->mouseEvent(MouseEvent(translateButton(ev->button()), MouseEvent::Pressed,
                                 Vector2i(ev->pos().x(), ev->pos().y())));
    }
}

void Canvas::mouseReleaseEvent(QMouseEvent* ev)
{
    /*if(d->mouseDisabled)
    {
        ev->ignore();
        return;
    }*/

    ev->accept();

    DENG2_FOR_AUDIENCE(MouseEvent, i)
    {
        i->mouseEvent(MouseEvent(translateButton(ev->button()), MouseEvent::Released,
                                 Vector2i(ev->pos().x(), ev->pos().y())));
    }
}

void Canvas::mouseMoveEvent(QMouseEvent *ev)
{
    ev->accept();

    // Absolute events are only emitted when the mouse is untrapped.
    if(!d->mouseGrabbed)
    {
        DENG2_FOR_AUDIENCE(MouseEvent, i)
        {
            i->mouseEvent(MouseEvent(MouseEvent::Absolute,
                                     Vector2i(ev->pos().x(), ev->pos().y())));
        }
    }
}

void Canvas::wheelEvent(QWheelEvent *ev)
{
    /*if(d->mouseDisabled)
    {
        ev->ignore();
        return;
    }*/
    ev->accept();

    bool continuousMovement = (d->prevWheelAt.elapsed() < MOUSE_WHEEL_CONTINUOUS_THRESHOLD_MS);
    int axis = (ev->orientation() == Qt::Horizontal? 0 : 1);
    int dir = (ev->delta() < 0? -1 : 1);

    DENG2_FOR_AUDIENCE(MouseEvent, i)
    {
        i->mouseEvent(MouseEvent(MouseEvent::FineAngle,
                                 axis == 0? Vector2i(ev->delta(), 0) :
                                            Vector2i(0, ev->delta()),
                                 Vector2i(ev->pos().x(), ev->pos().y())));
    }

    if(!continuousMovement || d->wheelDir[axis] != dir)
    {
        d->wheelDir[axis] = dir;

        DENG2_FOR_AUDIENCE(MouseEvent, i)
        {
            i->mouseEvent(MouseEvent(MouseEvent::Step,
                                     axis == 0? Vector2i(dir, 0) :
                                     axis == 1? Vector2i(0, dir) : Vector2i(),
                                     !d->mouseGrabbed? Vector2i(ev->pos().x(), ev->pos().y()) : Vector2i()));
        }
    }

    d->prevWheelAt.start();
}

} // namespace de
