/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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

/*
 * sys_timer.c: Timing Subsystem
 *
 * Uses Win32 multimedia timing routines.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"

#ifdef WIN32
#   include <mmsystem.h>
#endif

#ifdef UNIX
#   include <SDL.h>
#endif

#include "de_base.h"
#include "de_console.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float   ticsPerSecond = TICSPERSEC;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static double timeOffset = 0;

// CODE --------------------------------------------------------------------

void Sys_ShutdownTimer(void)
{
#ifdef WIN32
    timeEndPeriod(1);
#endif
}

void Sys_InitTimer(void)
{
    Con_Message("Sys_InitTimer.\n");
#ifdef WIN32
    timeBeginPeriod(1);
#endif
}

/*
 * Returns the time in milliseconds.
 */
unsigned int Sys_GetRealTime(void)
{
    static boolean first = true;
    static DWORD start;

#ifdef WIN32
    DWORD   now = timeGetTime();
#endif
#ifdef UNIX
    Uint32  now = SDL_GetTicks();
#endif

    if(first)
    {
        first = false;
        start = now;
        return 0;
    }

    // Wrapped around? (Every 50 days...)
    if(now < start)
        return 0xffffffff - start + now + 1;

    return now - start;
}

/*
 * Returns the timer value in seconds.
 */
double Sys_GetSeconds(void)
{
    return (double) ((Sys_GetRealTime() / 1000.0) * (ticsPerSecond / 35)) +
        timeOffset;
}

/*
 * Returns time in 35 Hz floating point tics.
 */
double Sys_GetTimef(void)
{
    //return ((Sys_GetRealTime() / 1000.0 + timeOffset) * ticsPerSecond);
    return Sys_GetSeconds() * 35;
}

/*
 * Returns time in 35 Hz tics.
 */
int Sys_GetTime(void)
{
    return (int) Sys_GetTimef();
}

/*
 * Set the number of game tics per second.
 */
void Sys_TicksPerSecond(float newTics)
{
    double  nowTime = Sys_GetRealTime() / 1000.0;

    if(newTics <= 0)
        newTics = TICSPERSEC;

    // Update the time offset so that after the change time will
    // continue from the same value.
    timeOffset += nowTime * (ticsPerSecond - newTics) / 35;

    ticsPerSecond = newTics;
}
