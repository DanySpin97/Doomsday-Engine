/**\file rend_console.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Console rendering.
 */

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_ui.h"

#include "cbuffer.h"
#include "texturevariant.h"
#include "materialvariant.h"
#include "font.h"

#include "rend_console.h"

// Console (display) Modes:
typedef enum {
    CM_HALFSCREEN, // Half vertical window height.
    CM_FULLSCREEN, // Full window height.
    CM_SINGLELINE, // Line height x1.
    CM_CUSTOM      // Some other offset positioned by the user.
} consolemode_t;

void Rend_ConsoleUpdateBackground(void);

float ConsoleOpenY; // Where the console bottom is when open.
float consoleMoveSpeed = .5f; // Speed of console opening/closing.

float consoleBackgroundAlpha = .75f;
float consoleBackgroundLight = .14f;
Uri* consoleBackgroundMaterialUri = NULL;
int consoleBackgroundTurn = 0; // The rotation variable.
float consoleBackgroundZoom = 1.0f;

byte consoleTextShadow = false;
byte consoleShowFPS = false;

static boolean inited = false;
static consolemode_t consoleMode;
static boolean needResize = false; /// @c true= We are waiting on a successful resize to draw.
static int moveLineDelta; // Number of lines to move the console when we are next able.
static float ConsoleY; // Where the console bottom is currently?
static float ConsoleDestY; // Where the console bottom should be?
static float ConsoleBlink; // Cursor blink timer (35 Hz tics).
static boolean openingOrClosing;
static float consoleAlpha, consoleAlphaTarget;
static material_t* consoleBackgroundMaterial;

static float fontSy; // Font size Y.
static float funnyAng;

static const float CcolYellow[3] = { 1, .85f, .3f };
static const char* consoleTitle = DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT;

static char secondaryTitleText[256];
static char statusText[256];

void Rend_ConsoleRegister(void)
{
    C_VAR_FLOAT("con-background-alpha", &consoleBackgroundAlpha, 0, 0, 1);
    C_VAR_FLOAT("con-background-light", &consoleBackgroundLight, 0, 0, 1);
    C_VAR_URIPTR2("con-background-material", &consoleBackgroundMaterialUri,
        0, 0, 0, Rend_ConsoleUpdateBackground);
    C_VAR_INT("con-background-turn", &consoleBackgroundTurn, CVF_NO_MIN|CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("con-background-zoom", &consoleBackgroundZoom, 0, 0.1f, 100.0f);
    C_VAR_BYTE("con-fps", &consoleShowFPS, 0, 0, 1);
    C_VAR_FLOAT("con-move-speed", &consoleMoveSpeed, 0, 0, 1);
    C_VAR_BYTE("con-text-shadow", &consoleTextShadow, 0, 0, 1);
}

static float calcConsoleTitleBarHeight(void)
{
    int oldFont, border = theWindow->geometry.size.width / 120, height;
    assert(inited);

    oldFont = FR_Font();
    FR_SetFont(fontVariable[FS_BOLD]);
    height = FR_SingleLineHeight("Con") + border;
    FR_SetFont(oldFont);
    return height;
}

static __inline int calcConsoleMinHeight(void)
{
    assert(inited);
    return fontSy * 1.5f + calcConsoleTitleBarHeight() / theWindow->geometry.size.height * SCREENHEIGHT;
}

void Rend_ConsoleInit(void)
{
    if(!inited)
    {   // First init.
        consoleMode = CM_HALFSCREEN;
        ConsoleY = 0;
        ConsoleOpenY = SCREENHEIGHT/2;
        ConsoleDestY = 0;
        moveLineDelta = 0;
        openingOrClosing = false;
        consoleAlpha = 0;
        consoleAlphaTarget = 0;
        funnyAng = 0;
        ConsoleBlink = 0;
        memset(secondaryTitleText, 0, sizeof(secondaryTitleText));
        memset(statusText, 0, sizeof(statusText));
    }

    consoleBackgroundMaterial = 0;
    funnyAng = 0;

    if(inited)
    {
        Rend_ConsoleUpdateTitle();
        Rend_ConsoleUpdateBackground();
    }

    needResize = true;
    inited = true;
}

boolean Rend_ConsoleResize(boolean force)
{
    if(!inited) return false;

    // Are we forcing a resize?
    if(force) needResize = true;

    // If there is no pending resize we can get out of here.
    if(!needResize) return false;

    // We can only resize if the font renderer is available.
    if(FR_Available())
    {
        float scale[2], fontScaledY, gtosMulY;
        int lineHeight;

        FR_SetFont(Con_Font());
        FR_LoadDefaultAttrib();
        FR_SetTracking(Con_FontTracking());

        gtosMulY = theWindow->geometry.size.height / 200.0f;
        lineHeight = FR_SingleLineHeight("Con");
        Con_FontScale(&scale[0], &scale[1]);

        fontScaledY = lineHeight * Con_FontLeading() * scale[1];
        fontSy = fontScaledY / gtosMulY;

        if(consoleMode == CM_SINGLELINE)
        {
            ConsoleDestY = calcConsoleMinHeight();
        }

        // Rendering of the console can now continue.
        needResize = false;
    }

    return needResize;
}

void Rend_ConsoleCursorResetBlink(void)
{
    if(!inited) return;
    ConsoleBlink = 0;
}

// Calculate the average of the given color flags.
static void calcAvgColor(int fl, float rgb[3])
{
    int count = 0;
    assert(inited && rgb);

    rgb[CR] = rgb[CG] = rgb[CB] = 0;

    if(fl & CBLF_BLACK)
    {
        ++count;
    }
    if(fl & CBLF_BLUE)
    {
        rgb[CB] += 1;
        ++count;
    }
    if(fl & CBLF_GREEN)
    {
        rgb[CG] += 1;
        ++count;
    }
    if(fl & CBLF_CYAN)
    {
        rgb[CG] += 1;
        rgb[CB] += 1;
        ++count;
    }
    if(fl & CBLF_RED)
    {
        rgb[CR] += 1;
        ++count;
    }
    if(fl & CBLF_MAGENTA)
    {
        rgb[CR] += 1;
        rgb[CB] += 1;
        ++count;
    }
    if(fl & CBLF_YELLOW)
    {
        rgb[CR] += CcolYellow[0];
        rgb[CG] += CcolYellow[1];
        rgb[CB] += CcolYellow[2];
        ++count;
    }
    if(fl & CBLF_WHITE)
    {
        rgb[CR] += 1;
        rgb[CG] += 1;
        rgb[CB] += 1;
        ++count;
    }
    // Calculate the average.
    if(count > 1)
    {
        rgb[CR] /= count;
        rgb[CG] /= count;
        rgb[CB] /= count;
    }
    if(fl & CBLF_LIGHT)
    {
        rgb[CR] += (1 - rgb[CR]) / 2;
        rgb[CG] += (1 - rgb[CG]) / 2;
        rgb[CB] += (1 - rgb[CB]) / 2;
    }
}

static void drawRuler(int x, int y, int lineWidth, int lineHeight, float alpha)
{
    int xoff = 3, yoff = lineHeight / 4, rh = MIN_OF(5, lineHeight / 2);
    Point2Raw origin;
    Size2Raw size;
    assert(inited);

    origin.x = x + xoff;
    origin.y = y + yoff + (lineHeight - rh) / 2;
    size.width  = lineWidth - 2 * xoff;
    size.height = rh;

    UI_GradientEx(&origin, &size,  rh / 3, UI_Color(UIC_SHADOW), UI_Color(UIC_BG_DARK), alpha / 2, alpha);
    UI_DrawRectEx(&origin, &size, -rh / 3, false, UI_Color(UIC_BRD_HI), 0, 0, alpha / 3);
}

/**
 * Initializes the Doomsday console user interface. This is called when
 * engine startup is complete.
 */
void Rend_ConsoleUpdateTitle(void)
{
    if(isDedicated || !inited) return;

    // Update the secondary title and the game status.
    if(DD_GameLoaded())
    {
        dd_snprintf(secondaryTitleText, sizeof(secondaryTitleText)-1, "%s %s", (char*) gx.GetVariable(DD_PLUGIN_NAME), (char*) gx.GetVariable(DD_PLUGIN_VERSION_SHORT));
        strncpy(statusText, Str_Text(Game_Title(theGame)), sizeof(statusText) - 1);
        return;
    }

    // No game currently loaded.
    memset(secondaryTitleText, 0, sizeof(secondaryTitleText));
    memset(statusText, 0, sizeof(statusText));
}

void Rend_ConsoleUpdateBackground(void)
{
    assert(inited);
    if(!consoleBackgroundMaterialUri || Str_IsEmpty(Uri_Path(consoleBackgroundMaterialUri))) return;
    consoleBackgroundMaterial = Materials_ToMaterial(Materials_ResolveUri(consoleBackgroundMaterialUri));
}

void Rend_ConsoleToggleFullscreen(void)
{
    float y;

    if(isDedicated || !inited) return;
    if(needResize)
    {
        /// \todo enqueue toggle (don't resize here, do it in the ticker).
        return;
    }

    if(++consoleMode > CM_SINGLELINE) consoleMode = CM_HALFSCREEN;
    switch(consoleMode)
    {
    case CM_HALFSCREEN: y = SCREENHEIGHT/2; break;
    case CM_FULLSCREEN: y = SCREENHEIGHT; break;
    case CM_SINGLELINE: y = calcConsoleMinHeight(); break;
    }

    ConsoleDestY = ConsoleOpenY = y;
}

void Rend_ConsoleOpen(int yes)
{
    if(isDedicated || !inited) return;

    if(yes)
    {
        consoleAlphaTarget = 1;
        ConsoleDestY = ConsoleOpenY;
        Rend_ConsoleCursorResetBlink();
    }
    else
    {
        consoleAlphaTarget = 0;
        ConsoleDestY = 0;
    }
}

void Rend_ConsoleMove(int numLines)
{
    if(isDedicated || !inited) return;

    moveLineDelta += numLines;

    if(needResize || moveLineDelta == 0) return;

    consoleMode = CM_CUSTOM;
    if(moveLineDelta < 0)
    {
        ConsoleOpenY -= fontSy * -moveLineDelta;
    }
    else
    {
        ConsoleOpenY += fontSy * moveLineDelta;
    }

    if(INRANGE_OF(ConsoleOpenY, SCREENHEIGHT/2, 2))
    {
        ConsoleOpenY = SCREENHEIGHT/2;
        consoleMode = CM_HALFSCREEN;
    }
    else if(ConsoleOpenY >= SCREENHEIGHT)
    {
        ConsoleOpenY = SCREENHEIGHT;
        consoleMode = CM_FULLSCREEN;
    }
    else
    {
        int minHeight = calcConsoleMinHeight();
        if(ConsoleOpenY <= minHeight)
        {
            ConsoleOpenY = minHeight;
            consoleMode = CM_SINGLELINE;
        }
    }

    moveLineDelta = 0;
    ConsoleDestY = ConsoleOpenY;
}

void Rend_ConsoleTicker(timespan_t time)
{
    float step;

    if(isDedicated || !inited) return;

    step = time * 35;

    // Move the console alpha to the target.
    if(consoleAlphaTarget > consoleAlpha)
    {
        float diff = MAX_OF(consoleAlphaTarget - consoleAlpha, .0001f) * consoleMoveSpeed;

        consoleAlpha += diff * step;
        if(consoleAlpha > consoleAlphaTarget)
            consoleAlpha = consoleAlphaTarget;
    }
    else if(consoleAlphaTarget < consoleAlpha)
    {
        float diff = MAX_OF(consoleAlpha - consoleAlphaTarget, .0001f) * consoleMoveSpeed;

        consoleAlpha -= diff * step;
        if(consoleAlpha < consoleAlphaTarget)
            consoleAlpha = consoleAlphaTarget;
    }

    if(ConsoleY == 0)
        openingOrClosing = true;

    if(!needResize)
    {
        // Move the console to the destination Y.
        if(ConsoleDestY > ConsoleY)
        {
            float diff = (ConsoleDestY - ConsoleY) * consoleMoveSpeed;
            if(diff < 1) diff = 1;

            ConsoleY += diff * step;
            if(ConsoleY > ConsoleDestY)
                ConsoleY = ConsoleDestY;
        }
        else if(ConsoleDestY < ConsoleY)
        {
            float diff = (ConsoleY - ConsoleDestY) * consoleMoveSpeed;
            if(diff < 1) diff = 1;

            ConsoleY -= diff * step;
            if(ConsoleY < ConsoleDestY)
                ConsoleY = ConsoleDestY;
        }
    }

    if(ConsoleY == ConsoleOpenY)
        openingOrClosing = false;

    if(!Con_IsActive())
        return; // We have nothing further to do here.

    if(consoleBackgroundTurn != 0)
        funnyAng += step * consoleBackgroundTurn / 10000;

    ConsoleBlink += step; // Cursor blink timer (0 = visible).
}

void Rend_ConsoleFPS(const Point2Raw* origin)
{
    Point2Raw topLeft, labelOrigin;
    char buf[160];
    Size2Raw size;
    assert(origin);

    if(isDedicated || !inited) return;
    if(!consoleShowFPS) return;

    // Try to fulfill any pending resize.
    if(Rend_ConsoleResize(false/*no force*/)) return; // No FPS counter for you...

    sprintf(buf, "%.1f FPS", DD_GetFrameRate());
    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);
    size.width  = FR_TextWidth(buf) + 16;
    size.height = FR_SingleLineHeight(buf)  + 16;

    glEnable(GL_TEXTURE_2D);

    topLeft.x = origin->x - size.width;
    topLeft.y = origin->y;
    UI_GradientEx(&topLeft, &size, 6, UI_Color(UIC_BG_MEDIUM), UI_Color(UIC_BG_LIGHT), .5f, .8f);
    UI_DrawRectEx(&topLeft, &size, 6, false, UI_Color(UIC_BRD_HI), UI_Color(UIC_BG_MEDIUM), .2f, -1);

    labelOrigin.x = origin->x - 8;
    labelOrigin.y = origin->y + size.height / 2;
    UI_SetColor(UI_Color(UIC_TEXT));
    UI_TextOutEx2(buf, &labelOrigin, UI_Color(UIC_TITLE), 1, ALIGN_RIGHT, DTF_ONLY_SHADOW);

    glDisable(GL_TEXTURE_2D);
}

static void drawConsoleTitleBar(float alpha)
{
    int border, barHeight;
    Point2Raw origin;
    Size2Raw size;
    assert(inited);

    if(alpha < .0001f) return;

    border = theWindow->geometry.size.width / 120;
    barHeight = calcConsoleTitleBarHeight();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glEnable(GL_TEXTURE_2D);

    origin.x = 0;
    origin.y = 0;
    size.width  = theWindow->geometry.size.width;
    size.height = barHeight;
    UI_Gradient(&origin, &size, UI_Color(UIC_BG_MEDIUM), UI_Color(UIC_BG_LIGHT), .8f * alpha, alpha);

    origin.x = 0;
    origin.y = barHeight;
    size.width  = theWindow->geometry.size.width;
    size.height = border;
    UI_Gradient(&origin, &size, UI_Color(UIC_SHADOW), UI_Color(UIC_BG_DARK), .6f * alpha, 0);

    origin.x = 0;
    origin.y = barHeight;
    size.width  = theWindow->geometry.size.width;
    size.height = border*2;
    UI_Gradient(&origin, &size, UI_Color(UIC_BG_DARK), UI_Color(UIC_SHADOW), .2f * alpha, 0);

    FR_SetFont(fontVariable[FS_BOLD]);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    origin.x = border;
    origin.y = barHeight / 2;
    UI_TextOutEx2(consoleTitle, &origin, UI_Color(UIC_TITLE), alpha, ALIGN_LEFT, DTF_ONLY_SHADOW);
    if(secondaryTitleText[0])
    {
        int width = FR_TextWidth(consoleTitle) + FR_TextWidth("  ");
        FR_SetFont(fontVariable[FS_LIGHT]);
        origin.x = border + width;
        origin.y = barHeight / 2;
        UI_TextOutEx2(secondaryTitleText, &origin, UI_Color(UIC_TEXT), .33f * alpha, ALIGN_LEFT, DTF_ONLY_SHADOW);
    }
    if(statusText[0])
    {
        FR_SetFont(fontVariable[FS_LIGHT]);
        origin.x = theWindow->geometry.size.width - border;
        origin.y = barHeight / 2;
        UI_TextOutEx2(statusText, &origin, UI_Color(UIC_TEXT), .75f * alpha, ALIGN_RIGHT, DTF_ONLY_SHADOW);
    }

    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

static void drawConsoleBackground(const Point2Raw* origin, const Size2Raw* size, float closeFade)
{
    int bgX = 0, bgY = 0;
    assert(inited);

    if(consoleBackgroundMaterial)
    {
        const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
            MC_UI, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, 0, 1, 0, false, false, false, false);
        const materialsnapshot_t* ms = Materials_Prepare(consoleBackgroundMaterial, spec, Con_IsActive());

        GL_BindTexture(MSU_gltexture(ms, MTU_PRIMARY), MSU(ms, MTU_PRIMARY).magMode);

        bgX = (int) (ms->size.width  * consoleBackgroundZoom);
        bgY = (int) (ms->size.height * consoleBackgroundZoom);

        glEnable(GL_TEXTURE_2D);
        if(consoleBackgroundTurn != 0)
        {
            glMatrixMode(GL_TEXTURE);
            glPushMatrix();
            glLoadIdentity();
            glTranslatef(2 * sin(funnyAng / 4), 2 * cos(funnyAng / 4), 0);
            glRotatef(funnyAng * 3, 0, 0, 1);
        }
    }

    glColor4f(consoleBackgroundLight, consoleBackgroundLight, consoleBackgroundLight, closeFade * consoleBackgroundAlpha);
    GL_DrawRectf2Tiled(origin->x, origin->y, size->width, size->height, bgX, bgY);

    if(consoleBackgroundMaterial)
    {
        glDisable(GL_TEXTURE_2D);
        if(funnyAng != 0)
        {
            glMatrixMode(GL_TEXTURE);
            glPopMatrix();
        }
    }
}

/**
 * Draw a 'side' text in the console. This is intended for extra
 * information about the current game mode.
 *
 * \note Currently unused.
 */
static void drawSideText(const char* text, int line, float alpha)
{
    float gtosMulY = theWindow->geometry.size.height / 200.0f, fontScaledY, y, scale[2];
    char buf[300];
    int ssw;
    assert(inited);

    FR_SetFont(Con_Font());
    FR_LoadDefaultAttrib();
    Con_FontScale(&scale[0], &scale[1]);
    fontScaledY = FR_SingleLineHeight("Con") * scale[1];
    y = ConsoleY * gtosMulY - fontScaledY * (1 + line);

    if(y > -fontScaledY)
    {
        con_textfilter_t printFilter = Con_PrintFilter();

        // Scaled screen width.
        ssw = (int) (theWindow->geometry.size.width / scale[0]);

        if(printFilter)
        {
            strncpy(buf, text, sizeof(buf));
            printFilter(buf);
            text = buf;
        }

        FR_SetColorAndAlpha(CcolYellow[0], CcolYellow[1], CcolYellow[2], alpha * .75f);
        FR_DrawTextXY3(text, ssw - 3, y / scale[1], ALIGN_TOPRIGHT, DTF_NO_TYPEIN|DTF_NO_GLITTER|(!consoleTextShadow?DTF_NO_SHADOW:0));
    }
}

/**
 * \note Slightly messy...
 */
static void drawConsole(float consoleAlpha)
{
#define XORIGIN             (0)
#define YORIGIN             (0)
#define PADDING             (2)
#define LOCALBUFFSIZE       (CMDLINE_SIZE +1/*prompt length*/ +1/*terminator*/)

    static const cbline_t** lines = 0;
    static int bufferSize = 0;

    CBuffer* buffer = Con_HistoryBuffer();
    uint cmdCursor = Con_CommandLineCursorPosition();
    char* cmdLine = Con_CommandLine();
    float scale[2], y, fontScaledY, gtosMulY = theWindow->geometry.size.height / 200.0f;
    char buff[LOCALBUFFSIZE];
    font_t* cfont;
    int lineHeight, textOffsetY;
    con_textfilter_t printFilter = Con_PrintFilter();
    uint reqLines, maxLineLength;
    Point2Raw origin;
    Size2Raw size;
    assert(inited);

    FR_SetFont(Con_Font());
    FR_LoadDefaultAttrib();
    FR_SetTracking(Con_FontTracking());
    FR_SetColorAndAlpha(1, 1, 1, consoleAlpha);

    cfont = Fonts_ToFont(FR_Font());
    lineHeight = FR_SingleLineHeight("Con");
    Con_FontScale(&scale[0], &scale[1]);
    fontScaledY = lineHeight * Con_FontLeading() * scale[1];
    textOffsetY = PADDING + fontScaledY / 4;

    origin.x = XORIGIN;
    origin.y = YORIGIN + (int) (ConsoleY * gtosMulY);
    size.width  = theWindow->geometry.size.width;
    size.height = -theWindow->geometry.size.height;
    drawConsoleBackground(&origin, &size, consoleAlpha);

    // The border.
    origin.x = XORIGIN;
    origin.y = YORIGIN + (int) ((ConsoleY - 10) * gtosMulY);
    size.width  = theWindow->geometry.size.width;
    size.height = 10 * gtosMulY;
    UI_Gradient(&origin, &size, UI_Color(UIC_BG_DARK), UI_Color(UIC_BRD_HI), 0,
                consoleAlpha * consoleBackgroundAlpha * .06f);

    origin.x = XORIGIN;
    origin.y = YORIGIN + (int) (ConsoleY * gtosMulY);
    size.width  = theWindow->geometry.size.width;
    size.height = 2;
    UI_Gradient(&origin, &size, UI_Color(UIC_BG_LIGHT), UI_Color(UIC_BG_LIGHT),
                consoleAlpha * consoleBackgroundAlpha, -1);

    origin.x = XORIGIN;
    origin.y = YORIGIN + (int) (ConsoleY * gtosMulY);
    size.width  = theWindow->geometry.size.width;
    size.height = 2 * gtosMulY;
    UI_Gradient(&origin, &size, UI_Color(UIC_SHADOW), UI_Color(UIC_SHADOW),
                consoleAlpha * consoleBackgroundAlpha * .75f, 0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glScalef(scale[0], scale[1], 1);

    // The console history log is drawn from bottom to top.
    y = ConsoleY * gtosMulY - (lineHeight * scale[1] + fontScaledY) - textOffsetY;

    reqLines = MAX_OF(0, ceil(y / fontScaledY)+1);
    if(reqLines != 0)
    {
        uint count, totalLines = CBuffer_NumLines(buffer);
        int firstIdx;

        firstIdx = -((long)(reqLines + Con_HistoryOffset()));
        if(firstIdx < -((long)totalLines))
            firstIdx = -((long)totalLines);

        // Need to enlarge the buffer?
        if(reqLines > (uint) bufferSize)
        {
            lines = Z_Realloc((void*) lines, sizeof(cbline_t *) * (reqLines + 1), PU_APPSTATIC);
            bufferSize = reqLines;
        }

        count = CBuffer_GetLines2(buffer, reqLines, firstIdx, lines, BLF_OMIT_EMPTYLINE);
        if(count != 0)
        {
            glEnable(GL_TEXTURE_2D);

            { uint i;
            for(i = count; i-- > 0;)
            {
                const cbline_t* line = lines[i];

                if(line->flags & CBLF_RULER)
                {   // Draw a ruler here, and nothing else.
                    drawRuler(XORIGIN + PADDING, (YORIGIN + y) / scale[1],
                               theWindow->geometry.size.width / scale[0] - PADDING*2, lineHeight,
                               consoleAlpha);
                }
                else
                {
                    int alignFlags = 0;
                    short textFlags = DTF_NO_TYPEIN|DTF_NO_GLITTER|(!consoleTextShadow?DTF_NO_SHADOW:0);
                    float xOffset;

                    memset(buff, 0, sizeof(buff));
                    strncpy(buff, line->text, LOCALBUFFSIZE-1);

                    if(line->flags & CBLF_CENTER)
                    {
                        alignFlags |= ALIGN_TOP;
                        xOffset = (theWindow->geometry.size.width / scale[0]) / 2;
                    }
                    else
                    {
                        alignFlags |= ALIGN_TOPLEFT;
                        xOffset = 0;
                    }

                    if(printFilter)
                        printFilter(buff);

                    // Set the color.
                    if(Font_Flags(cfont) & FF_COLORIZE)
                    {
                        float rgb[3];
                        calcAvgColor(line->flags, rgb);
                        FR_SetColorv(rgb);
                    }
                    FR_DrawTextXY3(buff, XORIGIN + PADDING + xOffset, YORIGIN + y / scale[1], alignFlags, textFlags);
                }

                // Move up.
                y -= fontScaledY;
            }}

            glDisable(GL_TEXTURE_2D);
        }
    }

    // The command line.
    { boolean abbrevLeft = 0, abbrevRight = 0;
    int offset = 0;
    uint cmdLineLength;

    y = ConsoleY * gtosMulY - (lineHeight * scale[1]) - textOffsetY;

    cmdLineLength = (uint)strlen(cmdLine);
    maxLineLength = CBuffer_MaxLineLength(buffer) - 1/*prompt length*/;

    if(cmdLineLength >= maxLineLength)
    {
        maxLineLength -= 5; /*abbrev vis length*/

        if((signed)cmdCursor - (signed)maxLineLength > 0 || cmdCursor > maxLineLength)
        {
            abbrevLeft = true;
            maxLineLength -= 5; /*abbrev vis length*/
        }

        offset = MAX_OF(0, (signed)cmdCursor - (signed)maxLineLength);
        abbrevRight = (offset + maxLineLength < cmdLineLength);
        if(!abbrevRight)
        {
            maxLineLength += 5; /*abbrev vis length*/
            offset = MAX_OF(0, (signed)cmdCursor - (signed)maxLineLength);
        }
    }

    dd_snprintf(buff, LOCALBUFFSIZE -1/*terminator*/, ">%s%.*s%s",
        abbrevLeft?  "{alpha=.5}[...]{alpha=1}" : "",
        maxLineLength, cmdLine + offset,
        abbrevRight? "{alpha=.5}[...]" : "");

    if(printFilter)
        printFilter(buff);

    glEnable(GL_TEXTURE_2D);
    if(Font_Flags(cfont) & FF_COLORIZE)
    {
        FR_SetColorAndAlpha(CcolYellow[0], CcolYellow[1], CcolYellow[2], consoleAlpha);
    }
    else
    {
        FR_SetColorAndAlpha(1, 1, 1, consoleAlpha);
    }

    FR_DrawTextXY3(buff, XORIGIN + PADDING, YORIGIN + y / scale[1], ALIGN_TOPLEFT, DTF_NO_TYPEIN|DTF_NO_GLITTER|(!consoleTextShadow?DTF_NO_SHADOW:0));
    glDisable(GL_TEXTURE_2D);

    // Draw the cursor in the appropriate place.
    if(Con_IsActive() && !Con_IsLocked())
    {
        float width, height, halfInterlineHeight = (lineHeight * scale[1]) / 8.f;
        int xOffset, yOffset = 2 * scale[1];
        char temp[LOCALBUFFSIZE];

        // Where is the cursor?
        memset(temp, 0, sizeof(temp));
        strncpy(temp, buff, MIN_OF(LOCALBUFFSIZE -1/*prompt length*/ -1/*vis clamp*/, cmdCursor-offset + (abbrevLeft? 24/*abbrev length*/:0) + 1));
        xOffset = FR_TextWidth(temp);
        if(Con_InputMode())
        {
            height  = lineHeight * scale[1];
            yOffset += halfInterlineHeight;
        }
        else
        {
            height  = halfInterlineHeight;
            yOffset += lineHeight * scale[1];
        }

        // Size of the current character.
        width = FR_CharWidth(cmdLine[cmdCursor] == '\0'? ' ' : cmdLine[cmdCursor]);

        glColor4f(CcolYellow[0], CcolYellow[1], CcolYellow[2],
                  consoleAlpha * (((int) ConsoleBlink) & 0x10 ? .2f : .5f));
        GL_DrawRectf2(XORIGIN + PADDING + xOffset, (int)((YORIGIN + y + yOffset) / scale[1]),
                    (int)width, MAX_OF(1, (int)(height / scale[1])));
    }

    // Restore the original matrices.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    }
#undef LOCALBUFFSIZE
#undef PADDING
#undef YORIGIN
#undef XORIGIN
}

void Rend_Console(void)
{
    boolean consoleShow;

    if(isDedicated || !inited) return;

    // Try to fulfill any pending resize.
    if(Rend_ConsoleResize(false/*no force*/)) return; // No console on this frame at least...

    consoleShow = (ConsoleY > 0);// || openingOrClosing);
    if(!consoleShow && !consoleShowFPS) return;

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->geometry.size.width, theWindow->geometry.size.height, 0, -1, 1);

    if(consoleShow)
    {
        drawConsole(consoleAlpha);
        drawConsoleTitleBar(consoleAlpha);
    }

    if(consoleShowFPS && !UI_IsActive())
    {
        Point2Raw origin;
        origin.x = theWindow->geometry.size.width - 10;
        origin.y = 10 + (ConsoleY > 0? ROUND(consoleAlpha * calcConsoleTitleBarHeight()) : 0);
        Rend_ConsoleFPS(&origin);
    }

    // Restore original matrix.
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}
