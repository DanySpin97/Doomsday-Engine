/** @file render/rend_dynlight.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_render.h"

#include "WallEdge"

#include "render/rend_dynlight.h"

using namespace de;

static void drawDynlight(dynlight_t const &dyn, renderlightprojectionparams_t &parm)
{
    // If multitexturing is in use we skip the first.
    if(!(RL_IsMTexLights() && parm.lastIdx == 0))
    {
        // Allocate enough for the divisions too.
        rvertex_t *rvertices = R_AllocRendVertices(parm.realNumVertices);
        Vector2f *rtexcoords = R_AllocRendTexCoords(parm.realNumVertices);
        Vector4f *rcolors = R_AllocRendColors(parm.realNumVertices);
        bool const mustSubdivide = (parm.isWall && (parm.wall.leftEdge->divisionCount() || parm.wall.rightEdge->divisionCount() ));

        for(uint i = 0; i < parm.numVertices; ++i)
        {
            rcolors[i] = dyn.color;
        }

        if(parm.isWall)
        {
            WallEdge const &leftEdge = *parm.wall.leftEdge;
            WallEdge const &rightEdge = *parm.wall.rightEdge;

            rtexcoords[1].x = rtexcoords[0].x = dyn.s[0];
            rtexcoords[1].y = rtexcoords[3].y = dyn.t[0];
            rtexcoords[3].x = rtexcoords[2].x = dyn.s[1];
            rtexcoords[2].y = rtexcoords[0].y = dyn.t[1];

            if(mustSubdivide)
            {
                /*
                 * Need to swap indices around into fans set the position
                 * of the division vertices, interpolate texcoords and
                 * color.
                 */

                rvertex_t origVerts[4]; std::memcpy(origVerts, parm.rvertices, sizeof(rvertex_t) * 4);
                Vector2f origTexCoords[4]; std::memcpy(origTexCoords, rtexcoords, sizeof(Vector2f) * 4);
                Vector4f origColors[4]; std::memcpy(origColors, rcolors, sizeof(Vector4f) * 4);

                R_DivVerts(rvertices, origVerts, leftEdge, rightEdge);
                R_DivTexCoords(rtexcoords, origTexCoords, leftEdge, rightEdge);
                R_DivVertColors(rcolors, origColors, leftEdge, rightEdge);
            }
            else
            {
                std::memcpy(rvertices, parm.rvertices, sizeof(rvertex_t) * parm.numVertices);
            }
        }
        else
        {
            // It's a flat.
            float const width  = parm.texBR->x - parm.texTL->x;
            float const height = parm.texBR->y - parm.texTL->y;

            for(uint i = 0; i < parm.numVertices; ++i)
            {
                rtexcoords[i].x = ((parm.texBR->x - parm.rvertices[i].pos[VX]) / width * dyn.s[0]) +
                    ((parm.rvertices[i].pos[VX] - parm.texTL->x) / width * dyn.s[1]);

                rtexcoords[i].y = ((parm.texBR->y - parm.rvertices[i].pos[VY]) / height * dyn.t[0]) +
                    ((parm.rvertices[i].pos[VY] - parm.texTL->y) / height * dyn.t[1]);
            }

            std::memcpy(rvertices, parm.rvertices, sizeof(rvertex_t) * parm.numVertices);
        }

        RL_LoadDefaultRtus();
        RL_Rtu_SetTextureUnmanaged(RTU_PRIMARY, dyn.texture,
                                   GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        if(mustSubdivide)
        {
            WallEdge const &leftEdge = *parm.wall.leftEdge;
            WallEdge const &rightEdge = *parm.wall.rightEdge;

            RL_AddPolyWithCoords(PT_FAN, RPF_DEFAULT|RPF_LIGHT,
                                 3 + rightEdge.divisionCount(),
                                 rvertices  + 3 + leftEdge.divisionCount(),
                                 rcolors    + 3 + leftEdge.divisionCount(),
                                 rtexcoords + 3 + leftEdge.divisionCount(), 0);

            RL_AddPolyWithCoords(PT_FAN, RPF_DEFAULT|RPF_LIGHT,
                                 3 + leftEdge.divisionCount(),
                                 rvertices, rcolors, rtexcoords, 0);
        }
        else
        {
            RL_AddPolyWithCoords(parm.isWall? PT_TRIANGLE_STRIP : PT_FAN, RPF_DEFAULT|RPF_LIGHT,
                                 parm.numVertices, rvertices, rcolors, rtexcoords, 0);
        }

        R_FreeRendVertices(rvertices);
        R_FreeRendTexCoords(rtexcoords);
        R_FreeRendColors(rcolors);
    }
    parm.lastIdx++;
}

/// Generates a new primitive for each light projection.
static int drawDynlightWorker(dynlight_t const *dyn, void *parameters)
{
    renderlightprojectionparams_t *p = (renderlightprojectionparams_t *)parameters;
    drawDynlight(*dyn, *p);
    return 0; // Continue iteration.
}

uint Rend_RenderLightProjections(uint listIdx, renderlightprojectionparams_t &p)
{
    uint numRendered = p.lastIdx;

    LO_IterateProjections(listIdx, drawDynlightWorker, (void *)&p);

    numRendered = p.lastIdx - numRendered;
    if(RL_IsMTexLights())
        numRendered -= 1;
    return numRendered;
}
