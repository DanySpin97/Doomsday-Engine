/**\file gl_hq2x.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2010 Cameron Zemek <grom@zeminvaders.net>
 *\author Copyright © 2003 Maxim Stepin <maxst@hiend3d.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * High-Quality 2x Graphics Resizing.
 *
 * Based on the routine by Maxim Stepin <maxst@hiend3d.com>
 * For more information, see: http://hiend3d.com/hq2x.html
 *
 * Now uses 32-bit data and 0xAABBGGRR pixel byte order (little endian).
 * Alpha is taken into account in the processing to preserve edges.
 * Not quite as efficient as the original version.
 */

#include <stdlib.h>

#include "dd_types.h"
#include "dd_share.h"
#include "de_console.h"
#include "image.h"

/**
 * RGB(a) color space.
 */
#define R8G8B8_PACK(r, g, b) ((uint32_t)(((r) << 16) + ((g) << 8) + (b)))
#define R8G8B8_COMP(n, c)   (((c) >> ((n) << 3)) & 0xFF)
#define R8G8B8_Rmask        ((int)0x00FF0000)
#define R8G8B8_Gmask        ((int)0x0000FF00)
#define R8G8B8_Bmask        ((int)0x000000FF)

#define R8G8B8A8_PACK(r, g, b, a) ((uint32_t)(((a) << 24) + R8G8B8_PACK((r), (g), (b))))
#define R8G8B8A8_COMP       R8G8B8_COMP
#define R8G8B8A8_Rmask      R8G8B8_Rmask
#define R8G8B8A8_Gmask      R8G8B8_Gmask
#define R8G8B8A8_Bmask      R8G8B8_Bmask
#define R8G8B8A8_Amask      ((int)0xFF000000)
#define R8G8B8A8_RGBmask    ((int)0x00FFFFFF)

// Y'uv conversion using lookup table.
#define R8G8B8toY8U8V8(v)   (lutR8G8B8toY8U8V8[v])
#define R8G8B8A8toY8U8V8(v) (lutR8G8B8toY8U8V8[(v) & R8G8B8A8_RGBmask])
#define R8G8B8A8toY8U8V8A8(v) (R8G8B8A8toY8U8V8(v) + (((v) & R8G8B8A8_Amask) << 24))

/**
 * Y'uv(a) color space.
 */
#define Y8U8V8_PACK(y, u, v) ((uint32_t)(((y) << 16) + ((u) << 8) + (v)))
#define Y8U8V8_COMP(n, c)   (((c) >> ((n) << 3)) & 0xFF)
#define Y8U8V8_Ymask        ((int)0x00FF0000)
#define Y8U8V8_Umask        ((int)0x0000FF00)
#define Y8U8V8_Vmask        ((int)0x000000FF)

#define Y8U8V8A8_PACK(y, u, v, a) ((uint32_t)(((a) << 24) + Y8U8V8_PACK((y), (u), (v))))
#define Y8U8V8A8_COMP       Y8U8V8_COMP
#define Y8U8V8A8_Ymask      Y8U8V8_Ymask
#define Y8U8V8A8_Umask      Y8U8V8_Umask
#define Y8U8V8A8_Vmask      Y8U8V8_Vmask
#define Y8U8V8A8_Amask      ((int)0xFF000000)
#define Y8U8V8A8_YUVmask    ((int)0x00FFFFFF)

#define trY             ((int)0x00300000)
#define trU             ((int)0x00000700)
#define trV             ((int)0x00000006)

#define PIXEL00_0         Transl(pOut,       w[5]);
#define PIXEL00_10       Interp1(pOut,       w[5], w[1]);
#define PIXEL00_11       Interp1(pOut,       w[5], w[4]);
#define PIXEL00_12       Interp1(pOut,       w[5], w[2]);
#define PIXEL00_20       Interp2(pOut,       w[5], w[4], w[2]);
#define PIXEL00_21       Interp2(pOut,       w[5], w[1], w[2]);
#define PIXEL00_22       Interp2(pOut,       w[5], w[1], w[4]);
#define PIXEL00_60       Interp6(pOut,       w[5], w[2], w[4]);
#define PIXEL00_61       Interp6(pOut,       w[5], w[4], w[2]);
#define PIXEL00_70       Interp7(pOut,       w[5], w[4], w[2]);
#define PIXEL00_90       Interp9(pOut,       w[5], w[4], w[2]);
#define PIXEL00_100     Interp10(pOut,       w[5], w[4], w[2]);
#define PIXEL01_0         Transl(pOut+4,     w[5]);
#define PIXEL01_10       Interp1(pOut+4,     w[5], w[3]);
#define PIXEL01_11       Interp1(pOut+4,     w[5], w[2]);
#define PIXEL01_12       Interp1(pOut+4,     w[5], w[6]);
#define PIXEL01_20       Interp2(pOut+4,     w[5], w[2], w[6]);
#define PIXEL01_21       Interp2(pOut+4,     w[5], w[3], w[6]);
#define PIXEL01_22       Interp2(pOut+4,     w[5], w[3], w[2]);
#define PIXEL01_60       Interp6(pOut+4,     w[5], w[6], w[2]);
#define PIXEL01_61       Interp6(pOut+4,     w[5], w[2], w[6]);
#define PIXEL01_70       Interp7(pOut+4,     w[5], w[2], w[6]);
#define PIXEL01_90       Interp9(pOut+4,     w[5], w[2], w[6]);
#define PIXEL01_100     Interp10(pOut+4,     w[5], w[2], w[6]);
#define PIXEL10_0         Transl(pOut+BpL,   w[5]);
#define PIXEL10_10       Interp1(pOut+BpL,   w[5], w[7]);
#define PIXEL10_11       Interp1(pOut+BpL,   w[5], w[8]);
#define PIXEL10_12       Interp1(pOut+BpL,   w[5], w[4]);
#define PIXEL10_20       Interp2(pOut+BpL,   w[5], w[8], w[4]);
#define PIXEL10_21       Interp2(pOut+BpL,   w[5], w[7], w[4]);
#define PIXEL10_22       Interp2(pOut+BpL,   w[5], w[7], w[8]);
#define PIXEL10_60       Interp6(pOut+BpL,   w[5], w[4], w[8]);
#define PIXEL10_61       Interp6(pOut+BpL,   w[5], w[8], w[4]);
#define PIXEL10_70       Interp7(pOut+BpL,   w[5], w[8], w[4]);
#define PIXEL10_90       Interp9(pOut+BpL,   w[5], w[8], w[4]);
#define PIXEL10_100     Interp10(pOut+BpL,   w[5], w[8], w[4]);
#define PIXEL11_0         Transl(pOut+BpL+4, w[5]);
#define PIXEL11_10       Interp1(pOut+BpL+4, w[5], w[9]);
#define PIXEL11_11       Interp1(pOut+BpL+4, w[5], w[6]);
#define PIXEL11_12       Interp1(pOut+BpL+4, w[5], w[8]);
#define PIXEL11_20       Interp2(pOut+BpL+4, w[5], w[6], w[8]);
#define PIXEL11_21       Interp2(pOut+BpL+4, w[5], w[9], w[8]);
#define PIXEL11_22       Interp2(pOut+BpL+4, w[5], w[9], w[6]);
#define PIXEL11_60       Interp6(pOut+BpL+4, w[5], w[8], w[6]);
#define PIXEL11_61       Interp6(pOut+BpL+4, w[5], w[6], w[8]);
#define PIXEL11_70       Interp7(pOut+BpL+4, w[5], w[6], w[8]);
#define PIXEL11_90       Interp9(pOut+BpL+4, w[5], w[6], w[8]);
#define PIXEL11_100     Interp10(pOut+BpL+4, w[5], w[6], w[8]);

static uint32_t lutR8G8B8toY8U8V8[16777216];
static uint32_t YUV1, YUV2;

void LerpColor(uint8_t* pc, uint32_t c1, uint32_t c2, uint32_t c3, int f1,
    int f2, int f3)
{
    int total = f1 + f2 + f3;
    uint8_t out[4];
    out[0] = f1 * (c1 & R8G8B8A8_Rmask) + f2 * (c2 & R8G8B8A8_Rmask);
    out[1] = f1 * (c1 & R8G8B8A8_Gmask) + f2 * (c2 & R8G8B8A8_Gmask);
    out[2] = f1 * (c1 & R8G8B8A8_Bmask) + f2 * (c2 & R8G8B8A8_Bmask);
    out[3] = f1 * (c1 & R8G8B8A8_Amask) + f2 * (c2 & R8G8B8A8_Amask);
    if(0 != f3)
    {
        out[0] += f3 * (c3 & R8G8B8A8_Rmask);
        out[1] += f3 * (c3 & R8G8B8A8_Gmask);
        out[2] += f3 * (c3 & R8G8B8A8_Bmask);
        out[3] += f3 * (c3 & R8G8B8A8_Amask);
    }
    out[0] /= total;
    out[1] /= total;
    out[2] /= total;
    out[3] /= total;
    *((uint32_t*)pc) = *((uint32_t*)out);
}

static __inline int Diff(uint32_t c1, uint32_t c2)
{
    YUV1 = R8G8B8A8toY8U8V8(c1);
    YUV2 = R8G8B8A8toY8U8V8(c2);
    return ( (((c1 & R8G8B8A8_Amask) != 0) != ((c2 & R8G8B8A8_Amask) != 0)) ||
             (abs((YUV1 & Y8U8V8_Ymask) - (YUV2 & Y8U8V8_Ymask)) > trY) ||
             (abs((YUV1 & Y8U8V8_Umask) - (YUV2 & Y8U8V8_Umask)) > trU) ||
             (abs((YUV1 & Y8U8V8_Vmask) - (YUV2 & Y8U8V8_Vmask)) > trV) );
}

static __inline void Transl(uint8_t* pc, uint32_t c)
{
    pc[0] = (c & R8G8B8A8_Rmask);
    pc[1] = (c & R8G8B8A8_Gmask);
    pc[2] = (c & R8G8B8A8_Bmask);
    pc[3] = (c & R8G8B8A8_Amask);
}

static __inline void Interp1(uint8_t* pc, uint32_t c1, uint32_t c2)
{
    if(c1 == c2)
    {
        Transl(pc, c1);
        return;
    }
    LerpColor(pc, c1, c2, 0, 3, 1, 0);
}

static __inline void Interp2(uint8_t* pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    LerpColor(pc, c1, c2, c3, 2, 1, 1);
}

static __inline void Interp6(uint8_t* pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    LerpColor(pc, c1, c2, c3, 5, 2, 1);
}

static __inline void Interp7(uint8_t* pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    LerpColor(pc, c1, c2, c3, 6, 1, 1);
}

static __inline void Interp9(uint8_t* pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    LerpColor(pc, c1, c2, c3, 2, 3, 3);
}

static __inline void Interp10(uint8_t* pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    LerpColor(pc, c1, c2, c3, 14, 1, 1);
}

void GL_InitSmartFilterHQ2x(void)
{
    /* Initalize RGB to YUV lookup table */
    uint32_t c, r, g, b, y, u, v;
    for(c = 0; c < 16777215; ++c)
    {
        r = R8G8B8_COMP(0, c);
        g = R8G8B8_COMP(1, c);
        b = R8G8B8_COMP(2, c);
        y = (uint32_t)( 0.299*r + 0.587*g + 0.114*b);
        u = (uint32_t)(-0.169*r - 0.331*g + 0.5  *b) + 128;
        v = (uint32_t)( 0.5  *r - 0.419*g - 0.081*b) + 128;
        lutR8G8B8toY8U8V8[c] = Y8U8V8_PACK(y, u, v);
    }
}

uint8_t* GL_SmartFilterHQ2x(const uint8_t* src, int width, int height, int flags)
{
#define BPP             (4) // Bytes Per Pixel.
#define OFFSET(x, y)    (BPP*(y)*width + BPP*(x))

    assert(src);
    {
    boolean wrapH = (flags & ICF_UPSCALE_SAMPLE_WRAPH) != 0;
    boolean wrapV = (flags & ICF_UPSCALE_SAMPLE_WRAPV) != 0;
    int pattern, flag, BpL, xA, xB, yA, yB;
    uint8_t* pOut, *dst;
    uint32_t w[10];

    if(width <= 0 || height <= 0)
        return 0;

    // +----+----+----+
    // | w1 | w2 | w3 |
    // +----+----+----+
    // | w4 | w5 | w6 |
    // +----+----+----+
    // | w7 | w8 | w9 |
    // +----+----+----+

    if(0 == (dst = malloc(BPP * 2 * width * height * 2)))
        Con_Error("GL_SmartFilterHQ2x: Failed on allocation of %lu bytes for "
                  "output buffer.", (unsigned long) (BPP * 2 * width * height * 2));

    pOut = dst;
    BpL = BPP * 2 * width; // (Out) Bytes per Line.
    { int y;
    for(y = 0; y < height; ++y)
    {
        { int x;
        for(x = 0; x < width; ++x)
        {
            w[5] = ULONG( *( (uint32_t*)(src + OFFSET(x, y)) ) );

            // Horizontal neighbors.
            if(wrapH)
            {
                w[4] = ULONG( *( (uint32_t*)(src + OFFSET(x == 0? width-1 : x-1, y)) ) );
                w[6] = ULONG( *( (uint32_t*)(src + OFFSET(x == width-1? 0 : x+1, y)) ) );
            }
            else
            {
                if(x != 0)
                    w[4] = ULONG( *( (uint32_t*)(src + OFFSET(x-1, y)) ) );
                else
                    w[4] = w[5];

                if(x != width-1)
                    w[6] = ULONG( *( (uint32_t*)(src + OFFSET(x+1, y)) ) );
                else
                    w[6] = w[5];
            }

            // Vertical neighbors.
            if(wrapV)
            {
                w[2] = ULONG( *( (uint32_t*)(src + OFFSET(x, y == 0? height-1 : y-1)) ) );
                w[8] = ULONG( *( (uint32_t*)(src + OFFSET(x, y == height-1? 0 : y+1)) ) );
            }
            else
            {
                if(y != 0)
                    w[2] = ULONG( *( (uint32_t*)(src + OFFSET(x, y-1)) ) );
                else
                    w[2] = w[5];

                if(y != height-1)
                    w[8] = ULONG( *( (uint32_t*)(src + OFFSET(x, y+1)) ) );
                else
                    w[8] = w[5];
            }

            // Corners.
            xA =        x == 0? ( wrapH?  width-1 : 0) : x-1;
            xB =  x == width-1? (!wrapH?  width-1 : 0) : x+1;
            yA =        y == 0? ( wrapV? height-1 : 0) : y-1;
            yB = y == height-1? (!wrapV? height-1 : 0) : y+1;

            w[1] = ULONG( *( (uint32_t*)(src + OFFSET(xA, yA)) ) );
            w[7] = ULONG( *( (uint32_t*)(src + OFFSET(xA, yB)) ) );
            w[3] = ULONG( *( (uint32_t*)(src + OFFSET(xB, yA)) ) );
            w[9] = ULONG( *( (uint32_t*)(src + OFFSET(xB, yB)) ) );

            pattern = 0;
            flag = 1;
            YUV1 = R8G8B8A8toY8U8V8(w[5]);

            { int k;
            for(k = 1; k <= 9; ++k)
            {
                if(k == 5)
                    continue;

                if(w[k] != w[5])
                {
                    YUV2 = R8G8B8A8toY8U8V8(w[k]);
                    if((((w[5] & R8G8B8A8_Amask) != 0) != ((w[k] & R8G8B8A8_Amask) != 0)) ||
                       (abs((YUV1 & Y8U8V8_Ymask) - (YUV2 & Y8U8V8_Ymask)) > trY) ||
                       (abs((YUV1 & Y8U8V8_Umask) - (YUV2 & Y8U8V8_Umask)) > trU) ||
                       (abs((YUV1 & Y8U8V8_Vmask) - (YUV2 & Y8U8V8_Vmask)) > trV))
                        pattern |= flag;
                }
                flag <<= 1;
            }}

            switch(pattern)
            {
            case 0:
            case 1:
            case 4:
            case 32:
            case 128:
            case 5:
            case 132:
            case 160:
            case 33:
            case 129:
            case 36:
            case 133:
            case 164:
            case 161:
            case 37:
            case 165: {
                    PIXEL00_20 PIXEL01_20 PIXEL10_20 PIXEL11_20 break;
              }
            case 2:
            case 34:
            case 130:
            case 162: {
                    PIXEL00_22 PIXEL01_21 PIXEL10_20 PIXEL11_20 break;
              }
            case 16:
            case 17:
            case 48:
            case 49: {
                    PIXEL00_20 PIXEL01_22 PIXEL10_20 PIXEL11_21 break;
              }
            case 64:
            case 65:
            case 68:
            case 69: {
                    PIXEL00_20 PIXEL01_20 PIXEL10_21 PIXEL11_22 break;
              }
            case 8:
            case 12:
            case 136:
            case 140: {
                    PIXEL00_21 PIXEL01_20 PIXEL10_22 PIXEL11_20 break;
              }
            case 3:
            case 35:
            case 131:
            case 163: {
                    PIXEL00_11 PIXEL01_21 PIXEL10_20 PIXEL11_20 break;
              }
            case 6:
            case 38:
            case 134:
            case 166: {
                    PIXEL00_22 PIXEL01_12 PIXEL10_20 PIXEL11_20 break;
              }
            case 20:
            case 21:
            case 52:
            case 53: {
                    PIXEL00_20 PIXEL01_11 PIXEL10_20 PIXEL11_21 break;
              }
            case 144:
            case 145:
            case 176:
            case 177: {
                    PIXEL00_20 PIXEL01_22 PIXEL10_20 PIXEL11_12 break;
              }
            case 192:
            case 193:
            case 196:
            case 197: {
                    PIXEL00_20 PIXEL01_20 PIXEL10_21 PIXEL11_11 break;
              }
            case 96:
            case 97:
            case 100:
            case 101: {
                    PIXEL00_20 PIXEL01_20 PIXEL10_12 PIXEL11_22 break;
              }
            case 40:
            case 44:
            case 168:
            case 172: {
                    PIXEL00_21 PIXEL01_20 PIXEL10_11 PIXEL11_20 break;
              }
            case 9:
            case 13:
            case 137:
            case 141: {
                    PIXEL00_12 PIXEL01_20 PIXEL10_22 PIXEL11_20 break;
              }
            case 18:
            case 50: {
                    PIXEL00_22 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_10}
                    else
                    {
                    PIXEL01_20}
                    PIXEL10_20 PIXEL11_21 break;
              }
            case 80:
            case 81: {
                    PIXEL00_20 PIXEL01_22 PIXEL10_21 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_10}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 72:
            case 76: {
                    PIXEL00_21 PIXEL01_20 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_10}
                    else
                    {
                    PIXEL10_20}
                    PIXEL11_22 break;
              }
            case 10:
            case 138: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_10}
                    else
                    {
                    PIXEL00_20}
                    PIXEL01_21 PIXEL10_22 PIXEL11_20 break;
              }
            case 66: {
                    PIXEL00_22 PIXEL01_21 PIXEL10_21 PIXEL11_22 break;
              }
            case 24: {
                    PIXEL00_21 PIXEL01_22 PIXEL10_22 PIXEL11_21 break;
              }
            case 7:
            case 39:
            case 135: {
                    PIXEL00_11 PIXEL01_12 PIXEL10_20 PIXEL11_20 break;
              }
            case 148:
            case 149:
            case 180: {
                    PIXEL00_20 PIXEL01_11 PIXEL10_20 PIXEL11_12 break;
              }
            case 224:
            case 228:
            case 225: {
                    PIXEL00_20 PIXEL01_20 PIXEL10_12 PIXEL11_11 break;
              }
            case 41:
            case 169:
            case 45: {
                    PIXEL00_12 PIXEL01_20 PIXEL10_11 PIXEL11_20 break;
              }
            case 22:
            case 54: {
                    PIXEL00_22 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    PIXEL10_20 PIXEL11_21 break;
              }
            case 208:
            case 209: {
                    PIXEL00_20 PIXEL01_22 PIXEL10_21 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 104:
            case 108: {
                    PIXEL00_21 PIXEL01_20 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    PIXEL11_22 break;
              }
            case 11:
            case 139: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    PIXEL01_21 PIXEL10_22 PIXEL11_20 break;
              }
            case 19:
            case 51: {
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL00_11 PIXEL01_10}
                    else {
                    PIXEL00_60 PIXEL01_90}
                    PIXEL10_20 PIXEL11_21 break;
              }
            case 146:
            case 178: {
                    PIXEL00_22 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_10 PIXEL11_12}
                    else {
                    PIXEL01_90 PIXEL11_61}
                    PIXEL10_20 break;
              }
            case 84:
            case 85: {
                    PIXEL00_20 if(Diff(w[6], w[8]))
                    {
                    PIXEL01_11 PIXEL11_10}
                    else {
                    PIXEL01_60 PIXEL11_90}
                    PIXEL10_21 break;
              }
            case 112:
            case 113: {
                    PIXEL00_20 PIXEL01_22 if(Diff(w[6], w[8]))
                    {
                    PIXEL10_12 PIXEL11_10}
                    else {
                    PIXEL10_61 PIXEL11_90}
                    break;
              }
            case 200:
            case 204: {
                    PIXEL00_21 PIXEL01_20 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_10 PIXEL11_11}
                    else {
                    PIXEL10_90 PIXEL11_60}
                    break;
              }
            case 73:
            case 77: {
                    if(Diff(w[8], w[4]))
                    {
                    PIXEL00_12 PIXEL10_10}
                    else {
                    PIXEL00_61 PIXEL10_90}
                    PIXEL01_20 PIXEL11_22 break;
              }
            case 42:
            case 170: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_10 PIXEL10_11}
                    else {
                    PIXEL00_90 PIXEL10_60}
                    PIXEL01_21 PIXEL11_20 break;
              }
            case 14:
            case 142: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_10 PIXEL01_12}
                    else {
                    PIXEL00_90 PIXEL01_61}
                    PIXEL10_22 PIXEL11_20 break;
              }
            case 67: {
                    PIXEL00_11 PIXEL01_21 PIXEL10_21 PIXEL11_22 break;
              }
            case 70: {
                    PIXEL00_22 PIXEL01_12 PIXEL10_21 PIXEL11_22 break;
              }
            case 28: {
                    PIXEL00_21 PIXEL01_11 PIXEL10_22 PIXEL11_21 break;
              }
            case 152: {
                    PIXEL00_21 PIXEL01_22 PIXEL10_22 PIXEL11_12 break;
              }
            case 194: {
                    PIXEL00_22 PIXEL01_21 PIXEL10_21 PIXEL11_11 break;
              }
            case 98: {
                    PIXEL00_22 PIXEL01_21 PIXEL10_12 PIXEL11_22 break;
              }
            case 56: {
                    PIXEL00_21 PIXEL01_22 PIXEL10_11 PIXEL11_21 break;
              }
            case 25: {
                    PIXEL00_12 PIXEL01_22 PIXEL10_22 PIXEL11_21 break;
              }
            case 26:
            case 31: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    PIXEL10_22 PIXEL11_21 break;
              }
            case 82:
            case 214: {
                    PIXEL00_22 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    PIXEL10_21 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 88:
            case 248: {
                    PIXEL00_21 PIXEL01_22 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 74:
            case 107: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    PIXEL01_21 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    PIXEL11_22 break;
              }
            case 27: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    PIXEL01_10 PIXEL10_22 PIXEL11_21 break;
              }
            case 86: {
                    PIXEL00_22 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    PIXEL10_21 PIXEL11_10 break;
              }
            case 216: {
                    PIXEL00_21 PIXEL01_22 PIXEL10_10 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 106: {
                    PIXEL00_10 PIXEL01_21 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    PIXEL11_22 break;
              }
            case 30: {
                    PIXEL00_10 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    PIXEL10_22 PIXEL11_21 break;
              }
            case 210: {
                    PIXEL00_22 PIXEL01_10 PIXEL10_21 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 120: {
                    PIXEL00_21 PIXEL01_22 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    PIXEL11_10 break;
              }
            case 75: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    PIXEL01_21 PIXEL10_10 PIXEL11_22 break;
              }
            case 29: {
                    PIXEL00_12 PIXEL01_11 PIXEL10_22 PIXEL11_21 break;
              }
            case 198: {
                    PIXEL00_22 PIXEL01_12 PIXEL10_21 PIXEL11_11 break;
              }
            case 184: {
                    PIXEL00_21 PIXEL01_22 PIXEL10_11 PIXEL11_12 break;
              }
            case 99: {
                    PIXEL00_11 PIXEL01_21 PIXEL10_12 PIXEL11_22 break;
              }
            case 57: {
                    PIXEL00_12 PIXEL01_22 PIXEL10_11 PIXEL11_21 break;
              }
            case 71: {
                    PIXEL00_11 PIXEL01_12 PIXEL10_21 PIXEL11_22 break;
              }
            case 156: {
                    PIXEL00_21 PIXEL01_11 PIXEL10_22 PIXEL11_12 break;
              }
            case 226: {
                    PIXEL00_22 PIXEL01_21 PIXEL10_12 PIXEL11_11 break;
              }
            case 60: {
                    PIXEL00_21 PIXEL01_11 PIXEL10_11 PIXEL11_21 break;
              }
            case 195: {
                    PIXEL00_11 PIXEL01_21 PIXEL10_21 PIXEL11_11 break;
              }
            case 102: {
                    PIXEL00_22 PIXEL01_12 PIXEL10_12 PIXEL11_22 break;
              }
            case 153: {
                    PIXEL00_12 PIXEL01_22 PIXEL10_22 PIXEL11_12 break;
              }
            case 58: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_10}
                    else
                    {
                    PIXEL00_70}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_10}
                    else
                    {
                    PIXEL01_70}
                    PIXEL10_11 PIXEL11_21 break;
              }
            case 83: {
                    PIXEL00_11 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_10}
                    else
                    {
                    PIXEL01_70}
                    PIXEL10_21 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_10}
                    else
                    {
                    PIXEL11_70}
                    break;
              }
            case 92: {
                    PIXEL00_21 PIXEL01_11 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_10}
                    else
                    {
                    PIXEL10_70}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_10}
                    else
                    {
                    PIXEL11_70}
                    break;
              }
            case 202: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_10}
                    else
                    {
                    PIXEL00_70}
                    PIXEL01_21 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_10}
                    else
                    {
                    PIXEL10_70}
                    PIXEL11_11 break;
              }
            case 78: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_10}
                    else
                    {
                    PIXEL00_70}
                    PIXEL01_12 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_10}
                    else
                    {
                    PIXEL10_70}
                    PIXEL11_22 break;
              }
            case 154: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_10}
                    else
                    {
                    PIXEL00_70}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_10}
                    else
                    {
                    PIXEL01_70}
                    PIXEL10_22 PIXEL11_12 break;
              }
            case 114: {
                    PIXEL00_22 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_10}
                    else
                    {
                    PIXEL01_70}
                    PIXEL10_12 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_10}
                    else
                    {
                    PIXEL11_70}
                    break;
              }
            case 89: {
                    PIXEL00_12 PIXEL01_22 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_10}
                    else
                    {
                    PIXEL10_70}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_10}
                    else
                    {
                    PIXEL11_70}
                    break;
              }
            case 90: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_10}
                    else
                    {
                    PIXEL00_70}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_10}
                    else
                    {
                    PIXEL01_70}
                    if(Diff(w[8], w[4]))
                    {
                    PIXEL10_10}
                    else
                    {
                    PIXEL10_70}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_10}
                    else
                    {
                    PIXEL11_70}
                    break;
              }
            case 55:
            case 23: {
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL00_11 PIXEL01_0}
                    else {
                    PIXEL00_60 PIXEL01_90}
                    PIXEL10_20 PIXEL11_21 break;
              }
            case 182:
            case 150: {
                    PIXEL00_22 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0 PIXEL11_12}
                    else {
                    PIXEL01_90 PIXEL11_61}
                    PIXEL10_20 break;
              }
            case 213:
            case 212: {
                    PIXEL00_20 if(Diff(w[6], w[8]))
                    {
                    PIXEL01_11 PIXEL11_0}
                    else {
                    PIXEL01_60 PIXEL11_90}
                    PIXEL10_21 break;
              }
            case 241:
            case 240: {
                    PIXEL00_20 PIXEL01_22 if(Diff(w[6], w[8]))
                    {
                    PIXEL10_12 PIXEL11_0}
                    else {
                    PIXEL10_61 PIXEL11_90}
                    break;
              }
            case 236:
            case 232: {
                    PIXEL00_21 PIXEL01_20 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0 PIXEL11_11}
                    else {
                    PIXEL10_90 PIXEL11_60}
                    break;
              }
            case 109:
            case 105: {
                    if(Diff(w[8], w[4]))
                    {
                    PIXEL00_12 PIXEL10_0}
                    else {
                    PIXEL00_61 PIXEL10_90}
                    PIXEL01_20 PIXEL11_22 break;
              }
            case 171:
            case 43: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0 PIXEL10_11}
                    else {
                    PIXEL00_90 PIXEL10_60}
                    PIXEL01_21 PIXEL11_20 break;
              }
            case 143:
            case 15: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0 PIXEL01_12}
                    else {
                    PIXEL00_90 PIXEL01_61}
                    PIXEL10_22 PIXEL11_20 break;
              }
            case 124: {
                    PIXEL00_21 PIXEL01_11 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    PIXEL11_10 break;
              }
            case 203: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    PIXEL01_21 PIXEL10_10 PIXEL11_11 break;
              }
            case 62: {
                    PIXEL00_10 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    PIXEL10_11 PIXEL11_21 break;
              }
            case 211: {
                    PIXEL00_11 PIXEL01_10 PIXEL10_21 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 118: {
                    PIXEL00_22 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    PIXEL10_12 PIXEL11_10 break;
              }
            case 217: {
                    PIXEL00_12 PIXEL01_22 PIXEL10_10 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 110: {
                    PIXEL00_10 PIXEL01_12 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    PIXEL11_22 break;
              }
            case 155: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    PIXEL01_10 PIXEL10_22 PIXEL11_12 break;
              }
            case 188: {
                    PIXEL00_21 PIXEL01_11 PIXEL10_11 PIXEL11_12 break;
              }
            case 185: {
                    PIXEL00_12 PIXEL01_22 PIXEL10_11 PIXEL11_12 break;
              }
            case 61: {
                    PIXEL00_12 PIXEL01_11 PIXEL10_11 PIXEL11_21 break;
              }
            case 157: {
                    PIXEL00_12 PIXEL01_11 PIXEL10_22 PIXEL11_12 break;
              }
            case 103: {
                    PIXEL00_11 PIXEL01_12 PIXEL10_12 PIXEL11_22 break;
              }
            case 227: {
                    PIXEL00_11 PIXEL01_21 PIXEL10_12 PIXEL11_11 break;
              }
            case 230: {
                    PIXEL00_22 PIXEL01_12 PIXEL10_12 PIXEL11_11 break;
              }
            case 199: {
                    PIXEL00_11 PIXEL01_12 PIXEL10_21 PIXEL11_11 break;
              }
            case 220: {
                    PIXEL00_21 PIXEL01_11 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_10}
                    else
                    {
                    PIXEL10_70}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 158: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_10}
                    else
                    {
                    PIXEL00_70}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    PIXEL10_22 PIXEL11_12 break;
              }
            case 234: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_10}
                    else
                    {
                    PIXEL00_70}
                    PIXEL01_21 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    PIXEL11_11 break;
              }
            case 242: {
                    PIXEL00_22 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_10}
                    else
                    {
                    PIXEL01_70}
                    PIXEL10_12 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 59: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_10}
                    else
                    {
                    PIXEL01_70}
                    PIXEL10_11 PIXEL11_21 break;
              }
            case 121: {
                    PIXEL00_12 PIXEL01_22 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_10}
                    else
                    {
                    PIXEL11_70}
                    break;
              }
            case 87: {
                    PIXEL00_11 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    PIXEL10_21 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_10}
                    else
                    {
                    PIXEL11_70}
                    break;
              }
            case 79: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    PIXEL01_12 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_10}
                    else
                    {
                    PIXEL10_70}
                    PIXEL11_22 break;
              }
            case 122: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_10}
                    else
                    {
                    PIXEL00_70}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_10}
                    else
                    {
                    PIXEL01_70}
                    if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_10}
                    else
                    {
                    PIXEL11_70}
                    break;
              }
            case 94: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_10}
                    else
                    {
                    PIXEL00_70}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    if(Diff(w[8], w[4]))
                    {
                    PIXEL10_10}
                    else
                    {
                    PIXEL10_70}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_10}
                    else
                    {
                    PIXEL11_70}
                    break;
              }
            case 218: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_10}
                    else
                    {
                    PIXEL00_70}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_10}
                    else
                    {
                    PIXEL01_70}
                    if(Diff(w[8], w[4]))
                    {
                    PIXEL10_10}
                    else
                    {
                    PIXEL10_70}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 91: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_10}
                    else
                    {
                    PIXEL01_70}
                    if(Diff(w[8], w[4]))
                    {
                    PIXEL10_10}
                    else
                    {
                    PIXEL10_70}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_10}
                    else
                    {
                    PIXEL11_70}
                    break;
              }
            case 229: {
                    PIXEL00_20 PIXEL01_20 PIXEL10_12 PIXEL11_11 break;
              }
            case 167: {
                    PIXEL00_11 PIXEL01_12 PIXEL10_20 PIXEL11_20 break;
              }
            case 173: {
                    PIXEL00_12 PIXEL01_20 PIXEL10_11 PIXEL11_20 break;
              }
            case 181: {
                    PIXEL00_20 PIXEL01_11 PIXEL10_20 PIXEL11_12 break;
              }
            case 186: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_10}
                    else
                    {
                    PIXEL00_70}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_10}
                    else
                    {
                    PIXEL01_70}
                    PIXEL10_11 PIXEL11_12 break;
              }
            case 115: {
                    PIXEL00_11 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_10}
                    else
                    {
                    PIXEL01_70}
                    PIXEL10_12 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_10}
                    else
                    {
                    PIXEL11_70}
                    break;
              }
            case 93: {
                    PIXEL00_12 PIXEL01_11 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_10}
                    else
                    {
                    PIXEL10_70}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_10}
                    else
                    {
                    PIXEL11_70}
                    break;
              }
            case 206: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_10}
                    else
                    {
                    PIXEL00_70}
                    PIXEL01_12 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_10}
                    else
                    {
                    PIXEL10_70}
                    PIXEL11_11 break;
              }
            case 205:
            case 201: {
                    PIXEL00_12 PIXEL01_20 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_10}
                    else
                    {
                    PIXEL10_70}
                    PIXEL11_11 break;
              }
            case 174:
            case 46: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_10}
                    else
                    {
                    PIXEL00_70}
                    PIXEL01_12 PIXEL10_11 PIXEL11_20 break;
              }
            case 179:
            case 147: {
                    PIXEL00_11 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_10}
                    else
                    {
                    PIXEL01_70}
                    PIXEL10_20 PIXEL11_12 break;
              }
            case 117:
            case 116: {
                    PIXEL00_20 PIXEL01_11 PIXEL10_12 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_10}
                    else
                    {
                    PIXEL11_70}
                    break;
              }
            case 189: {
                    PIXEL00_12 PIXEL01_11 PIXEL10_11 PIXEL11_12 break;
              }
            case 231: {
                    PIXEL00_11 PIXEL01_12 PIXEL10_12 PIXEL11_11 break;
              }
            case 126: {
                    PIXEL00_10 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    PIXEL11_10 break;
              }
            case 219: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    PIXEL01_10 PIXEL10_10 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 125: {
                    if(Diff(w[8], w[4]))
                    {
                    PIXEL00_12 PIXEL10_0}
                    else {
                    PIXEL00_61 PIXEL10_90}
                    PIXEL01_11 PIXEL11_10 break;
              }
            case 221: {
                    PIXEL00_12 if(Diff(w[6], w[8]))
                    {
                    PIXEL01_11 PIXEL11_0}
                    else {
                    PIXEL01_60 PIXEL11_90}
                    PIXEL10_10 break;
              }
            case 207: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0 PIXEL01_12}
                    else {
                    PIXEL00_90 PIXEL01_61}
                    PIXEL10_10 PIXEL11_11 break;
              }
            case 238: {
                    PIXEL00_10 PIXEL01_12 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0 PIXEL11_11}
                    else {
                    PIXEL10_90 PIXEL11_60}
                    break;
              }
            case 190: {
                    PIXEL00_10 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0 PIXEL11_12}
                    else {
                    PIXEL01_90 PIXEL11_61}
                    PIXEL10_11 break;
              }
            case 187: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0 PIXEL10_11}
                    else {
                    PIXEL00_90 PIXEL10_60}
                    PIXEL01_10 PIXEL11_12 break;
              }
            case 243: {
                    PIXEL00_11 PIXEL01_10 if(Diff(w[6], w[8]))
                    {
                    PIXEL10_12 PIXEL11_0}
                    else {
                    PIXEL10_61 PIXEL11_90}
                    break;
              }
            case 119: {
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL00_11 PIXEL01_0}
                    else {
                    PIXEL00_60 PIXEL01_90}
                    PIXEL10_12 PIXEL11_10 break;
              }
            case 237:
            case 233: {
                    PIXEL00_12 PIXEL01_20 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_100}
                    PIXEL11_11 break;
              }
            case 175:
            case 47: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_100}
                    PIXEL01_12 PIXEL10_11 PIXEL11_20 break;
              }
            case 183:
            case 151: {
                    PIXEL00_11 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_100}
                    PIXEL10_20 PIXEL11_12 break;
              }
            case 245:
            case 244: {
                    PIXEL00_20 PIXEL01_11 PIXEL10_12 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_100}
                    break;
              }
            case 250: {
                    PIXEL00_10 PIXEL01_10 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 123: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    PIXEL01_10 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    PIXEL11_10 break;
              }
            case 95: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    PIXEL10_10 PIXEL11_10 break;
              }
            case 222: {
                    PIXEL00_10 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    PIXEL10_10 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 252: {
                    PIXEL00_21 PIXEL01_11 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_100}
                    break;
              }
            case 249: {
                    PIXEL00_12 PIXEL01_22 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_100}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 235: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    PIXEL01_21 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_100}
                    PIXEL11_11 break;
              }
            case 111: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_100}
                    PIXEL01_12 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    PIXEL11_22 break;
              }
            case 63: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_100}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    PIXEL10_11 PIXEL11_21 break;
              }
            case 159: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_100}
                    PIXEL10_22 PIXEL11_12 break;
              }
            case 215: {
                    PIXEL00_11 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_100}
                    PIXEL10_21 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 246: {
                    PIXEL00_22 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    PIXEL10_12 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_100}
                    break;
              }
            case 254: {
                    PIXEL00_10 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_100}
                    break;
              }
            case 253: {
                    PIXEL00_12 PIXEL01_11 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_100}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_100}
                    break;
              }
            case 251: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    PIXEL01_10 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_100}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 239: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_100}
                    PIXEL01_12 if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_100}
                    PIXEL11_11 break;
              }
            case 127: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_100}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_20}
                    if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_20}
                    PIXEL11_10 break;
              }
            case 191: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_100}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_100}
                    PIXEL10_11 PIXEL11_12 break;
              }
            case 223: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_20}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_100}
                    PIXEL10_10 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_20}
                    break;
              }
            case 247: {
                    PIXEL00_11 if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_100}
                    PIXEL10_12 if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_100}
                    break;
              }
            case 255: {
                    if(Diff(w[4], w[2]))
                    {
                    PIXEL00_0}
                    else
                    {
                    PIXEL00_100}
                    if(Diff(w[2], w[6]))
                    {
                    PIXEL01_0}
                    else
                    {
                    PIXEL01_100}
                    if(Diff(w[8], w[4]))
                    {
                    PIXEL10_0}
                    else
                    {
                    PIXEL10_100}
                    if(Diff(w[6], w[8]))
                    {
                    PIXEL11_0}
                    else
                    {
                    PIXEL11_100}
                    break;
              }
            default:
                Con_Error("GL_SmartFilterHQ2x: Invalid pattern %i.", pattern);
                break;
            }
            pOut += 2 * BPP;
        }}
        pOut += BpL;
    }}

    return dst;
    }

#undef OFFSET
#undef BPP
}
