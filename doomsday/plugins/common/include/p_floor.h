/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * p_floor.h: Common playsim routines relating to moving floors.
 */

#ifndef __COMMON_THINKER_FLOOR_H__
#define __COMMON_THINKER_FLOOR_H__

typedef enum {
    FS_DOWN = -1, // Moving down.
    FS_WAIT, // Currently unused.
    FS_UP // Moving up.
} floorstate_e;

typedef enum {
    FT_LOWER, // Lower floor to highest surrounding floor.
    FT_LOWERTOLOWEST, // Lower floor to lowest surrounding floor.
#if __JHEXEN__
    FT_LOWERBYVALUE,
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__ || __WOLFTC__
    FT_LOWERTURBO, // Lower floor to highest surrounding floor VERY FAST.
#endif
#if __JDOOM64__
    FT_TOHIGHESTPLUS8, // jd64
    FT_TOHIGHESTPLUSBITMIP, // jd64
    FT_CUSTOMCHANGESEC, // jd64
#endif
    FT_RAISEFLOOR, // Raise floor to lowest surrounding CEILING.
    FT_RAISEFLOORTONEAREST, // Raise floor to next highest surrounding floor.
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__ || __WOLFTC__
    FT_RAISETOTEXTURE, // Raise floor to shortest height texture around it.
    FT_LOWERANDCHANGE, // Lower floor to lowest surrounding floor and change floorpic.
    FT_RAISE24,
    FT_RAISE24ANDCHANGE,
#endif
#if __JHEXEN__
    FT_RAISEFLOORBYVALUE,
#endif
    FT_RAISEFLOORCRUSH,
#if __JDOOM__ || __JDOOM64__ || __WOLFTC__
    FT_RAISEFLOORTURBO, // Raise to next highest floor, turbo-speed.
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__ || __WOLFTC__
    FT_RAISEDONUT,
#endif
#if __JDOOM__ || __JDOOM64__ || __WOLFTC__
    FT_RAISE512,
#endif
#if __JDOOM64__
    FT_RAISE32, // jd64
#endif
#if __JHERETIC__ || __JHEXEN__
    FT_RAISEBUILDSTEP,
#endif
#if __JHEXEN__
    FT_RAISEBYVALUEMUL8,
    FT_LOWERBYVALUEMUL8,
    FT_LOWERMUL8INSTANT,
    FT_RAISEMUL8INSTANT,
    FT_TOVALUEMUL8,
#endif
    NUMFLOORTYPES
} floortype_e;

typedef struct {
    thinker_t       thinker;
    floortype_e     type;
    boolean         crush;
    sector_t*       sector;
    floorstate_e    state;
    int             newSpecial;
    short           texture;
    float           floorDestHeight;
    float           speed;
#if __JHEXEN__
    int             delayCount;
    int             delayTotal;
    float           stairsDelayHeight;
    float           stairsDelayHeightDelta;
    float           resetHeight;
    short           resetDelay;
    short           resetDelayCount;
    byte            textureChange;
#endif
} floor_t;

#define FLOORSPEED          (1)

void        T_MoveFloor(floor_t* floor);
#if __JHEXEN__
int         EV_DoFloor(linedef_t* li, byte* args, floortype_e type);
#else
int         EV_DoFloor(linedef_t* li, floortype_e type);
#endif

#if __JHEXEN__
int         EV_DoFloorAndCeiling(linedef_t* li, byte* args, int ftype, int ctype);
#elif __JDOOM64__
int         EV_DoFloorAndCeiling(linedef_t* li, int ftype, int ctype);
#endif

#endif
