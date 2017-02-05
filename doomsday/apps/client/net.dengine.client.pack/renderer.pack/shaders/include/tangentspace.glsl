/*
 * The Doomsday Engine Project
 * Common OpenGL Shaders: Tangent space
 *
 * Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

varying highp vec3 vTSNormal;
varying highp vec3 vTSTangent;
varying highp vec3 vTSBitangent;

#ifdef DENG_VERTEX_SHADER

attribute highp vec3 aNormal;
attribute highp vec3 aTangent;
attribute highp vec3 aBitangent;

highp vec3 transformVector(highp vec3 dir, highp mat4 matrix) 
{
    return (matrix * vec4(dir, 0.0)).xyz;
}

void setTangentSpace(highp mat4 modelSpace)
{
    vTSNormal    = transformVector(aNormal,    modelSpace);
    vTSTangent   = transformVector(aTangent,   modelSpace);
    vTSBitangent = transformVector(aBitangent, modelSpace);
}

#endif // DENG_VERTEX_SHADER

#ifdef DENG_FRAGMENT_SHADER

highp mat3 fragmentTangentSpace()
{
    return mat3(normalize(vTSTangent), normalize(vTSBitangent), normalize(vTSNormal));
}

highp vec3 modelSpaceNormalVector(highp vec2 uv)
{
    return fragmentTangentSpace() * normalVector(uv);
}

#endif // DENG_FRAGMENT_SHADER
