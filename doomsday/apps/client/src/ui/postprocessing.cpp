/** @file postprocessing.cpp World frame post processing.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/postprocessing.h"
#include "ui/clientwindow.h"
#include "ui/viewcompositor.h"
#include "clientapp.h"
#include "clientplayer.h"

#include <de/Animation>
#include <de/Drawable>
#include <de/GLInfo>
#include <de/GLTextureFramebuffer>
#include <de/LogBuffer>

#include <QList>

using namespace de;

D_CMD(PostFx);

DENG2_PIMPL(PostProcessing)
{
    enum { PassThroughProgram = 0, ActiveProgram = 1 };

    //GLTextureFramebuffer framebuf;
    Drawable frame;
    GLUniform uMvpMatrix { "uMvpMatrix", GLUniform::Mat4 };
    GLUniform uFrame     { "uTex",       GLUniform::Sampler2D };
    GLUniform uFadeInOut { "uFadeInOut", GLUniform::Float };
    Animation fade       { 0, Animation::Linear };
    float opacity        { 1.f };

    struct QueueEntry {
        String shaderName;
        float fade;
        TimeDelta span;

        QueueEntry(String const &name, float f, TimeDelta const &s)
            : shaderName(name), fade(f), span(s) {}
    };
    typedef QList<QueueEntry> Queue;
    Queue queue;

    typedef GLBufferT<Vertex2Tex> VBuf;

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        DENG2_ASSERT(!frame.isReady()); // deinited earlier
    }

    void attachUniforms(GLProgram &program)
    {
        program << uMvpMatrix << uFrame << uFadeInOut;
    }

    bool setShader(String const &name)
    {
        try
        {
            auto &shaders = ClientApp::shaders();
            GLProgram &prog = frame.addProgram(ActiveProgram);
            shaders.build(prog, "fx.post." + name);
            attachUniforms(prog);
            frame.setProgram(prog);
            LOG_GL_MSG("Post-processing shader \"fx.post.%s\"") << name;
            return true;
        }
        catch (Error const &er)
        {
            LOG_GL_WARNING("Failed to set shader to \"fx.post.%s\":\n%s")
                    << name << er.asText();
        }
        return false;
    }

    /// Determines if the post-processing shader will be applied.
    bool isActive() const
    {
        return !fade.done() || fade > 0 || !queue.isEmpty();
    }

    void glInit()
    {
        if (frame.isReady()) return;

        // Drawable for drawing stuff back to the original target.
        VBuf *buf = new VBuf;
        buf->setVertices(gl::TriangleStrip,
                         VBuf::Builder().makeQuad(Rectanglef(0, 0, 1, 1),
                                                  Rectanglef(0, 1, 1, -1)),
                         gl::Static);
        frame.addBuffer(buf);

        // The default program is a pass-through shader.
        ClientApp::shaders().build(frame.program(), "generic.texture");
        attachUniforms(frame.program());
    }

    void glDeinit()
    {
        if (!frame.isReady()) return;

        LOGDEV_GL_XVERBOSE("Releasing GL resources", "");
        frame.clear();
    }

    void checkQueue()
    {
        // An ongoing fade?
        if (!fade.done()) return; // Let's check back later.

        if (!queue.isEmpty())
        {
            QueueEntry entry = queue.takeFirst();
            if (!entry.shaderName.isEmpty())
            {
                if (!setShader(entry.shaderName))
                {
                    fade = 0;
                    return;
                }
            }
            fade.setValue(entry.fade, entry.span);
            LOGDEV_GL_VERBOSE("Shader '%s' fade:%s") << entry.shaderName << fade.asText();
        }
    }

    void draw()
    {
        if (isActive())
        {
            uFadeInOut = fade * opacity;
        }
        frame.draw();
    }
};

PostProcessing::PostProcessing()
    : d(new Impl(this))
{}

bool PostProcessing::isActive() const
{
    return d->isActive();
}

void PostProcessing::fadeInShader(String const &fxPostShader, TimeDelta const &span)
{
    d->queue.append(Impl::QueueEntry(fxPostShader, 1, span));
}

void PostProcessing::fadeOut(TimeDelta const &span)
{
    d->queue.append(Impl::QueueEntry("", 0, span));
}

void PostProcessing::setOpacity(float opacity)
{
    d->opacity = opacity;
}

void PostProcessing::glInit()
{
    LOG_AS("PostProcessing");
    d->glInit();
}

void PostProcessing::glDeinit()
{
    LOG_AS("PostProcessing");
    d->glDeinit();
}

void PostProcessing::update()
{
    LOG_AS("PostProcessing");

    if (d->isActive())
    {
        d->checkQueue();
    }
    else
    {
        d->frame.setProgram(Impl::PassThroughProgram);
    }
}

void PostProcessing::draw(Matrix4f const &mvpMatrix, GLTexture const &frame)
{
    d->uMvpMatrix = mvpMatrix;
    d->uFrame     = frame;

    d->draw();
}

void PostProcessing::consoleRegister() // static
{
    C_CMD("postfx", "is",  PostFx);
    C_CMD("postfx", "isf", PostFx);
}

D_CMD(PostFx)
{
    DENG2_UNUSED(src);

    int console = String(argv[1]).toInt();
    String const shader = argv[2];
    TimeDelta const span = (argc == 4? String(argv[3]).toDouble() : 0);

    if (console < 0 || console >= DDMAXPLAYERS)
    {
        LOG_SCR_WARNING("Invalid console %i") << console;
        return false;
    }

    PostProcessing &post = ClientApp::player(console).viewCompositor().postProcessing();

    // Special case to clear out the current shader.
    if (shader == "none")
    {
        post.fadeOut(span);
        return true;
    }
    else if (shader == "opacity") // Change opacity.
    {
        post.setOpacity(float(span));
        return true;
    }

    post.fadeInShader(shader, span);
    return true;
}
