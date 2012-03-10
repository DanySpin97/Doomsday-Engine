/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * po_man.h: Polyobjects.
 */

#ifndef __PO_MAN_H__
#define __PO_MAN_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

typedef struct polyobj_s {
    // Defined in dd_share.h; required polyobj elements.
    DD_BASE_POLYOBJ_ELEMENTS()

    // Hexen-specific data:
    void* specialData; /* Pointer a thinker, if the poly is moving. */
} polyobj_t;

typedef enum {
    PODOOR_NONE,
    PODOOR_SLIDE,
    PODOOR_SWING,
} podoortype_t;

typedef struct {
    thinker_t       thinker;
    int             polyobj;
    int             intSpeed;
    unsigned int    dist;
    int             fangle;
    float           speed[2]; // for sliding walls
} polyevent_t;

typedef struct {
    thinker_t       thinker;
    int             polyobj;
    int             intSpeed;
    int             dist;
    int             totalDist;
    int             direction;
    float           speed[2];
    int             tics;
    int             waitTics;
    podoortype_t    type;
    boolean         close;
} polydoor_t;

enum {
    PO_ANCHOR_DOOMEDNUM = 3000,
    PO_SPAWN_DOOMEDNUM,
    PO_SPAWNCRUSH_DOOMEDNUM
};

void PO_InitForMap(void);
boolean PO_Busy(int polyobj);

boolean PO_FindAndCreatePolyobj(int tag, boolean crush, float startX, float startY);

/**
 * Lookup a Polyobj instance by unique ID or tag.
 *
 * @deprecated Prefer using P_PolyobjByID() or P_PolyobjByTag().
 *
 * @param num  If the MSB is set this is interpreted as a unique ID.
 *             Otherwise this value is interpreted as a tag that *should*
 *             match one polyobj.
 */
polyobj_t* P_GetPolyobj(uint num);

void T_PolyDoor(polydoor_t* pd);
void T_RotatePoly(polyevent_t* pe);
boolean EV_RotatePoly(LineDef* line, byte* args, int direction, boolean override);

void T_MovePoly(polyevent_t* pe);
boolean EV_MovePoly(LineDef* line, byte* args, boolean timesEight, boolean override);
boolean EV_OpenPolyDoor(LineDef* line, byte* args, podoortype_t type);

#endif
