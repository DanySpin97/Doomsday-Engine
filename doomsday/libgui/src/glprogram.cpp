/** @file glprogram.cpp  GL shader program.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/GLProgram"
#include "de/GLUniform"
#include "de/GLBuffer"
#include "de/GLShader"
#include "de/GLTexture"
#include "de/GuiApp"
#include "de/gui/opengl.h"
#include <de/Block>
#include <de/Log>

#include <QSet>
#include <QList>

namespace de {

using namespace internal;

DENG2_PIMPL(GLProgram)
, DENG2_OBSERVES(GLUniform, ValueChange)
, DENG2_OBSERVES(GLUniform, Deletion)
, DENG2_OBSERVES(GuiApp, GLContextChange)
{
    typedef QSet<GLUniform const *> Uniforms;
    typedef QList<GLUniform const *> UniformList;
    typedef QSet<GLShader const *> Shaders;

    Uniforms bound;
    Uniforms changed;
    UniformList textures;
    bool texturesChanged;

    GLuint name;
    Shaders shaders;
    bool inUse;
    bool needRebuild;

    Instance(Public *i)
        : Base(i)
        , texturesChanged(false)
        , name(0)
        , inUse(false)
        , needRebuild(false)
    {
        //DENG2_GUI_APP->audienceForGLContextChange += this;
    }

    ~Instance()
    {
        //DENG2_GUI_APP->audienceForGLContextChange -= this;
        release();
    }

    void appGLContextChanged()
    {
        /*
        if(name && !needRebuild)
        {
            qDebug() << "rebuild program" << name << "before use";
            self.rebuildBeforeNextUse();
        }
        */
    }

    void alloc()
    {
        if(!name)
        {
            name = glCreateProgram();
            if(!name)
            {
                throw AllocError("GLProgram::alloc", "Failed to create program");
            }
        }
    }

    void releaseButRetainBindings()
    {
        self.setState(NotReady);
        detachAllShaders();
        if(name)
        {
            glDeleteProgram(name);
            name = 0;
        }
    }

    void release()
    {
        unbindAll();
        releaseButRetainBindings();
    }

    void attach(GLShader const *shader)
    {
        DENG2_ASSERT(shader->isReady());
        alloc();
        glAttachShader(name, shader->glName());
        LIBGUI_ASSERT_GL_OK();
        shaders.insert(holdRef(shader));
    }

    void detach(GLShader const *shader)
    {
        if(shader->isReady())
        {
            glDetachShader(name, shader->glName());
        }
        shaders.remove(shader);
        shader->release();
    }

    void detachAllShaders()
    {
        foreach(GLShader const *shader, shaders)
        {
            detach(shader);
        }
        shaders.clear();
    }

    void unbindAll()
    {
        foreach(GLUniform const *u, bound)
        {
            u->audienceForValueChange() -= this;
            u->audienceForDeletion() -= this;
        }
        texturesChanged = false;
        bound.clear();
        textures.clear();
        changed.clear();
    }

    /**
     * Bind all known vertex attributes to the indices used by GLBuffer. The
     * program is automatically (re)linked after binding the vertex attributes,
     * if there are already shaders attached.
     */
    void bindVertexAttribs()
    {
        alloc();

        // The names of shader attributes are defined here:
        static struct {
            AttribSpec::Semantic semantic;
            char const *varName;
        }
        const names[] = {
            { AttribSpec::Position,   "aVertex"    },
            { AttribSpec::TexCoord0,  "aUV"        },
            { AttribSpec::TexCoord1,  "aUV2"       },
            { AttribSpec::TexCoord2,  "aUV3"       },
            { AttribSpec::TexCoord3,  "aUV4"       },
            { AttribSpec::TexBounds0, "aBounds"    },
            { AttribSpec::Color,      "aColor"     },
            { AttribSpec::Normal,     "aNormal"    },
            { AttribSpec::Tangent,    "aTangent"   },
            { AttribSpec::Bitangent,  "aBitangent" }
        };

        for(uint i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
        {
            glBindAttribLocation(name, GLuint(names[i].semantic), names[i].varName);
            LIBGUI_ASSERT_GL_OK();
        }

        if(!shaders.isEmpty())
        {
            link();
        }
    }

    void link()
    {
        DENG2_ASSERT(name != 0);

        glLinkProgram(name);

        // Was linking successful?
        GLint ok;
        glGetProgramiv(name, GL_LINK_STATUS, &ok);
        if(!ok)
        {
            dint32 logSize;
            dint32 count;
            glGetProgramiv(name, GL_INFO_LOG_LENGTH, &logSize);

            Block log(logSize);
            glGetProgramInfoLog(name, logSize, &count, reinterpret_cast<GLchar *>(log.data()));

            throw LinkerError("GLProgram::link", "Linking failed:\n" + log);
        }
    }

    void markAllBoundUniformsChanged()
    {
        foreach(GLUniform const *u, bound)
        {
            changed.insert(u);
        }
    }

    void updateUniforms()
    {
        if(changed.isEmpty()) return;

        // Apply the uniform values in this program.
        foreach(GLUniform const *u, changed)
        {
            if(u->type() != GLUniform::Sampler2D)
            {
                u->applyInProgram(self);
            }
        }

        if(texturesChanged)
        {
            // Update the sampler uniforms.
            for(int unit = 0; unit < textures.size(); ++unit)
            {
                int loc = self.glUniformLocation(textures[unit]->name().latin1());
                if(loc >= 0)
                {
                    glUniform1i(loc, unit);
                    LIBGUI_ASSERT_GL_OK();
                }
            }
            texturesChanged = false;
        }

        changed.clear();
    }

    void bindTextures()
    {
        // Update the sampler uniforms.
        for(int unit = textures.size() - 1; unit >= 0; --unit)
        {
            GLTexture const *tex = *textures[unit];
            if(tex)
            {
                tex->glBindToUnit(unit);
            }
        }
    }

    void rebuild()
    {
        qDebug() << "Rebuilding GL program" << name;

        if(name)
        {
            glDeleteProgram(name);
            name = 0;
        }

        alloc();

        foreach(GLShader const *shader, shaders)
        {
            glAttachShader(name, shader->glName());
            LIBGUI_ASSERT_GL_OK();
        }

        bindVertexAttribs();
        markAllBoundUniformsChanged();
    }

    void uniformValueChanged(GLUniform &uniform)
    {
        changed.insert(&uniform);
    }

    void uniformDeleted(GLUniform &uniform)
    {
        self.unbind(uniform);
    }
};

GLProgram::GLProgram() : d(new Instance(this))
{}

void GLProgram::clear()
{
    d->release();
}

GLProgram &GLProgram::build(GLShader const *vertexShader, GLShader const *fragmentShader)
{
    DENG2_ASSERT(vertexShader != 0);
    DENG2_ASSERT(vertexShader->isReady());
    DENG2_ASSERT(vertexShader->type() == GLShader::Vertex);
    DENG2_ASSERT(fragmentShader != 0);
    DENG2_ASSERT(fragmentShader->isReady());
    DENG2_ASSERT(fragmentShader->type() == GLShader::Fragment);

    d->releaseButRetainBindings();
    d->attach(vertexShader);
    d->attach(fragmentShader);
    d->bindVertexAttribs();    
    d->markAllBoundUniformsChanged();

    setState(Ready);

    return *this;
}

GLProgram &GLProgram::build(IByteArray const &vertexShaderSource,
                            IByteArray const &fragmentShaderSource)
{
    return build(refless(new GLShader(GLShader::Vertex,   vertexShaderSource)),
                 refless(new GLShader(GLShader::Fragment, fragmentShaderSource)));
}

void GLProgram::rebuildBeforeNextUse()
{
    d->needRebuild = true;
}

void GLProgram::rebuild()
{
    d->rebuild();
}

GLProgram &GLProgram::operator << (GLUniform const &uniform)
{
    return bind(uniform);
}

GLProgram &GLProgram::bind(GLUniform const &uniform)
{
    if(!d->bound.contains(&uniform))
    {
        d->bound.insert(&uniform);
        d->changed.insert(&uniform);

        uniform.audienceForValueChange() += d;
        uniform.audienceForDeletion() += d;

        if(uniform.type() == GLUniform::Sampler2D)
        {
            d->textures << &uniform;
            d->texturesChanged = true;
        }
    }
    return *this;
}

GLProgram &GLProgram::unbind(GLUniform const &uniform)
{
    if(d->bound.contains(&uniform))
    {
        d->bound.remove(&uniform);
        d->changed.remove(&uniform);

        uniform.audienceForValueChange() -= d.get();
        uniform.audienceForDeletion() -= d.get();

        if(uniform.type() == GLUniform::Sampler2D)
        {
            d->textures.removeOne(&uniform);
            d->texturesChanged = true;
        }
    }
    return *this;
}

void GLProgram::beginUse() const
{
    LIBGUI_ASSERT_GL_OK();
    DENG2_ASSERT_IN_MAIN_THREAD();
    DENG2_ASSERT(QGLContext::currentContext() != 0);
    DENG2_ASSERT(isReady());
    DENG2_ASSERT(!d->inUse);

    if(d->needRebuild)
    {
        d->needRebuild = false;
        const_cast<GLProgram *>(this)->rebuild();
    }

    DENG2_ASSERT(glIsProgram(d->name));

    d->inUse = true;

    // The program is now ready for use.
    glUseProgram(d->name);

    LIBGUI_ASSERT_GL_OK();

    d->updateUniforms();
    d->bindTextures();

    LIBGUI_ASSERT_GL_OK();
}

void GLProgram::endUse() const
{
    DENG2_ASSERT(d->inUse);

    d->inUse = false;
    glUseProgram(0);
}

GLuint GLProgram::glName() const
{
    return d->name;
}

int GLProgram::glUniformLocation(char const *uniformName) const
{
    GLint loc = glGetUniformLocation(d->name, uniformName);
    if(loc < 0)
    {
        LOG_AS("GLProgram");
        LOGDEV_GL_WARNING("Could not find uniform '%s'") << uniformName;
    }
    return loc;
}

} // namespace de