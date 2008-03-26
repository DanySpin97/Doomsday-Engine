/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * cl_player.c: Clientside Player Management
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_play.h"

#include "def_main.h"

// MACROS ------------------------------------------------------------------

#define TOP_PSPY            (32)
#define BOTTOM_PSPY         (128)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

playerstate_t playerState[DDMAXPLAYERS];
float pspMoveSpeed = 6;
float cplrThrustMul = 1;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int fixSpeed = 15;
static float fixPos[3];
static int fixTics;
static float pspY;

// Console player demo momentum (used to smooth out abrupt momentum changes).
static float cpMom[3][LOCALCAM_WRITE_TICS];

// CODE --------------------------------------------------------------------

/**
 * Clears the player state table.
 */
void Cl_InitPlayers(void)
{
    int                 i;

    memset(playerState, 0, sizeof(playerState));
    memset(fixPos, 0, sizeof(fixPos));
    fixTics = 0;
    pspY = 0;
    memset(cpMom, 0, sizeof(cpMom));

    // Clear psprites. The server will send them.
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        memset(clients[i].lastCmd, 0, sizeof(*clients[i].lastCmd));
    }
}

/**
 * Updates the state of the local player by looking at lastCmd.
 */
void Cl_LocalCommand(void)
{
    ddplayer_t         *pl = &ddPlayers[consolePlayer];
    client_t           *cl = &clients[consolePlayer];
    playerstate_t      *s = &playerState[consolePlayer];

    if(levelTime < 0.333)
    {
        // In the very beginning of a level, moving is not allowed.
        memset(cl->lastCmd, 0, TICCMD_SIZE);
        if(s->cmo)
        {
            s->cmo->mo.mom[MX] = 0;
            s->cmo->mo.mom[MY] = 0;
        }
    }

    s->forwardMove = cl->lastCmd->forwardMove * 2048;
    s->sideMove = cl->lastCmd->sideMove * 2048;
    s->angle = pl->mo->angle; //pl->clAngle; /* $unifiedangles */
    s->turnDelta = 0;
}

/**
 * Reads a single player delta from the message buffer and applies it to the
 * player in question. Returns false only if the list of deltas ends.
 *
 * \deprecated THIS FUNCTION IS NOW OBSOLETE (only used with PSV_FRAME
 * packets).
 */
int Cl_ReadPlayerDelta(void)
{
    int                 df, psdf, i, idx;
    int                 num = Msg_ReadByte();
    short               junk;
    playerstate_t      *s;
    ddplayer_t         *pl;
    ddpsprite_t        *psp;

    if(num == 0xff)
        return false;           // End of list.

    // The first byte consists of a player number and some flags.
    df = (num & 0xf0) << 8;
    df |= Msg_ReadByte();       // Second byte is just flags.
    num &= 0xf;                 // Clear the upper bits of the number.

    s = &playerState[num];
    pl = &ddPlayers[num];

    if(df & PDF_MOBJ)
    {
        clmobj_t           *old = s->cmo;
        int                 newid = Msg_ReadShort();

        /**
         * Make sure the 'new' mobj is different than the old one; there
         * will be linking problems otherwise.
         * \fixme What causes the duplicate sending of mobj ids?
	     */
        if(newid != s->mobjId)
        {
            s->mobjId = newid;

            // Find the new mobj.
            s->cmo = Cl_FindMobj(s->mobjId);
#ifdef _DEBUG
Con_Message("Pl%i: mobj=%i old=%ul\n", num, s->mobjId, (uint) old);
Con_Message("  x=%f y=%f z=%f\n", s->cmo->mo.pos[VX],
            s->cmo->mo.pos[VY], s->cmo->mo.pos[VZ]);
#endif
            s->cmo->mo.dPlayer = pl;

#ifdef _DEBUG
Con_Message("Cl_RPlD: pl=%i => moid=%i\n",
            s->cmo->mo.dPlayer - ddPlayers, s->mobjId);
#endif

            // Unlink this cmo (not interactive or visible).
            Cl_UnsetMobjPosition(s->cmo);
            // Make the old clmobj a non-player one.
            if(old)
            {
                old->mo.dPlayer = NULL;
                Cl_SetMobjPosition(old);
                Cl_UpdateRealPlayerMobj(pl->mo, &s->cmo->mo, ~0);
            }
            else
            {
                //Cl_UpdatePlayerPos(pl);

                // Replace the hidden client mobj with the real player mobj.
                Cl_UpdateRealPlayerMobj(pl->mo, &s->cmo->mo, ~0);
            }
            // Update the real player mobj.
            //Cl_UpdateRealPlayerMobj(pl->mo, &s->cmo->mo, ~0);
        }
    }

    if(df & PDF_FORWARDMOVE)
        s->forwardMove = (char) Msg_ReadByte() * 2048;
    if(df & PDF_SIDEMOVE)
        s->sideMove = (char) Msg_ReadByte() * 2048;
    if(df & PDF_ANGLE)
        //s->angle = Msg_ReadByte() << 24;
        junk = Msg_ReadByte(); /* $unifiedangles */
    if(df & PDF_TURNDELTA)
    {
        s->turnDelta = ((char) Msg_ReadByte() << 24) / 16;
    }
    if(df & PDF_FRICTION)
        s->friction = Msg_ReadByte() << 8;
    if(df & PDF_EXTRALIGHT)
    {
        i = Msg_ReadByte();
        pl->fixedColorMap = i & 7;
        pl->extraLight = i & 0xf8;
    }
    if(df & PDF_FILTER)
        pl->filter = Msg_ReadLong();
    if(df & PDF_CLYAW)          // Only sent when Fixangles is used.
        //pl->clAngle = Msg_ReadShort() << 16; /* $unifiedangles */
        junk = Msg_ReadShort();
    if(df & PDF_CLPITCH)        // Only sent when Fixangles is used.
        //pl->clLookDir = Msg_ReadShort() * 110.0 / DDMAXSHORT; /* $unifiedangles */
        junk = Msg_ReadShort();
    if(df & PDF_PSPRITES)
    {
        for(i = 0; i < 2; ++i)
        {
            // First the flags.
            psdf = Msg_ReadByte();
            psp = pl->pSprites + i;
            if(psdf & PSDF_STATEPTR)
            {
                idx = Msg_ReadPackedShort();
                if(!idx)
                    psp->statePtr = 0;
                else if(idx < countStates.num)
                {
                    psp->statePtr = states + (idx - 1);
                    psp->tics = psp->statePtr->tics;
                }
            }
            //if(psdf & PSDF_SPRITE) psp->sprite = Msg_ReadPackedShort() - 1;
            //if(psdf & PSDF_FRAME) psp->frame = Msg_ReadByte();
            //if(psdf & PSDF_NEXT/*FRAME*/) psp->nextframe = (char) Msg_ReadByte();
            //if(psdf & PSDF_NEXT/*TIME*/) psp->nexttime = (char) Msg_ReadByte();
            //if(psdf & PSDF_TICS) psp->tics = (char) Msg_ReadByte();
            if(psdf & PSDF_LIGHT)
                psp->light = Msg_ReadByte() / 255.0f;
            if(psdf & PSDF_ALPHA)
                psp->alpha = Msg_ReadByte() / 255.0f;
            if(psdf & PSDF_STATE)
                psp->state = Msg_ReadByte();
            if(psdf & PSDF_OFFSET)
            {
                psp->offset[VX] = (char) Msg_ReadByte() * 2;
                psp->offset[VY] = (char) Msg_ReadByte() * 2;
            }
        }
    }

    // Continue reading.
    return true;
}

/**
 * Thrust (with a multiplier).
 */
void Cl_ThrustMul(mobj_t *mo, angle_t angle, float move, float thmul)
{
    // Make a fine angle.
    angle >>= ANGLETOFINESHIFT;
    move *= thmul;
    mo->mom[MX] += move * FIX2FLT(fineCosine[angle]);
    mo->mom[MY] += move * FIX2FLT(finesine[angle]);
}

void Cl_Thrust(mobj_t *mo, angle_t angle, float move)
{
    Cl_ThrustMul(mo, angle, move, 1);
}

/**
 * Predict the movement of the given player.
 *
 * This kind of player movement can't be used with demos. The local player
 * movement is recorded into the demo file as absolute coordinates.
 */
void Cl_MovePlayer(ddplayer_t *pl)
{
    int                 num = pl - ddPlayers;
    playerstate_t      *st = &playerState[num];
    mobj_t             *mo = pl->mo;

    if(!mo)
        return;

    // If we are playing a demo, we shouldn't be here...
    if(playback && num == consolePlayer)
        return;

    // Move.
    P_MobjMovement2(mo, st);
    P_MobjZMovement(mo);

    /**
     * Predict change in movement (thrust).
     * The console player is always affected by the thrust multiplier
     * (Other players are never handled because clients only receive mobj
     * information about non-local player movement).
     */
    if(num == consolePlayer)
    {
        float       airThrust = 1.0f / 32;
        boolean     airborne =
            (mo->pos[VZ] > mo->floorZ && !(mo->ddFlags & DDMF_FLY));

        if(!(pl->flags & DDPF_DEAD) && !mo->reactionTime) // Dead players do not move willfully.
        {
            float       mul = (airborne? airThrust : cplrThrustMul);

            if(st->forwardMove)
                Cl_ThrustMul(mo, st->angle, FIX2FLT(st->forwardMove), mul);
            if(st->sideMove)
                Cl_ThrustMul(mo, st->angle - ANG90, FIX2FLT(st->sideMove), mul);
        }
        // Turn delta on move prediction angle.
        st->angle += st->turnDelta;
        //mo->angle += st->turnDelta;
    }

    // Mirror changes in the (hidden) client mobj.
    Cl_UpdatePlayerPos(pl);
}

/**
 * Move the (hidden, unlinked) client player mobj to the same coordinates
 * where the real mobj of the player is.
 */
void Cl_UpdatePlayerPos(ddplayer_t *pl)
{
    int                 num = pl - ddPlayers;
    mobj_t             *clmo, *mo;

    if(!playerState[num].cmo || !pl->mo)
        return;                 // Must have a mobj!

    clmo = &playerState[num].cmo->mo;
    mo = pl->mo;
    clmo->angle = mo->angle;
    // The player's client mobj is not linked to any lists, so position
    // can be updated without any hassles.
    memcpy(clmo->pos, mo->pos, sizeof(mo->pos));
    P_MobjLink(clmo, 0);       // Update subsector pointer.
    clmo->floorZ = mo->floorZ;
    clmo->ceilingZ = mo->ceilingZ;
    clmo->mom[MX] = mo->mom[MX];
    clmo->mom[MY] = mo->mom[MY];
    clmo->mom[MZ] = mo->mom[MZ];
}

void Cl_CoordsReceived(void)
{
    if(playback)
        return;

#ifdef _DEBUG
Con_Printf("Cl_CoordsReceived\n");
#endif

    fixPos[VX] = (float) Msg_ReadShort();
    fixPos[VY] = (float) Msg_ReadShort();
    fixTics = fixSpeed;
    fixPos[VX] /= fixSpeed;
    fixPos[VY] /= fixSpeed;
}

void Cl_HandlePlayerFix(void)
{
    ddplayer_t *plr = &ddPlayers[consolePlayer];
    int         fixes = Msg_ReadLong();
    angle_t     angle;
    float       lookdir;
    mobj_t     *mo = plr->mo;
    clmobj_t   *clmo = playerState[consolePlayer].cmo;

    if(fixes & 1) // fix angles?
    {
        plr->fixCounter.angles = plr->fixAcked.angles = Msg_ReadLong();
        angle = Msg_ReadLong();
        lookdir = FIX2FLT(Msg_ReadLong());

#ifdef _DEBUG
Con_Message("Cl_HandlePlayerFix: Fix angles %i. Angle=%f, lookdir=%f\n",
            plr->fixAcked.angles, FIX2FLT(angle), lookdir);
#endif
        if(mo)
        {
#ifdef _DEBUG
Con_Message("  Applying to mobj %p...\n", mo);
#endif
            mo->angle = angle;
            plr->lookDir = lookdir;
        }
        if(clmo)
        {
#ifdef _DEBUG
Con_Message("  Applying to clmobj %i...\n", clmo->mo.thinker.id);
#endif
            clmo->mo.angle = angle;
        }
    }

    if(fixes & 2) // fix pos?
    {
        float       pos[3];

        plr->fixCounter.pos = plr->fixAcked.pos = Msg_ReadLong();
        pos[VX] = FIX2FLT(Msg_ReadLong());
        pos[VY] = FIX2FLT(Msg_ReadLong());
        pos[VZ] = FIX2FLT(Msg_ReadLong());

#ifdef _DEBUG
Con_Message("Cl_HandlePlayerFix: Fix pos %i. Pos=%f, %f, %f\n",
            plr->fixAcked.pos, pos[VX], pos[VY], pos[VZ]);
#endif
        if(mo)
        {
#ifdef _DEBUG
Con_Message("  Applying to mobj %p...\n", mo);
#endif
            Sv_PlaceMobj(mo, pos[VX], pos[VY], pos[VZ], false);
            mo->reactionTime = 18;
        }
        if(clmo)
        {
#ifdef _DEBUG
Con_Message("  Applying to clmobj %i...\n", clmo->mo.thinker.id);
#endif
            Cl_UpdatePlayerPos(plr);
        }
    }

    if(fixes & 4) // fix momentum?
    {
        float       pos[3];

        plr->fixCounter.mom = plr->fixAcked.mom = Msg_ReadLong();

        pos[0] = FIX2FLT(Msg_ReadLong());
        pos[1] = FIX2FLT(Msg_ReadLong());
        pos[2] = FIX2FLT(Msg_ReadLong());

#ifdef _DEBUG
Con_Message("Cl_HandlePlayerFix: Fix momentum %i. Mom=%f, %f, %f\n",
            plr->fixAcked.mom, pos[0], pos[1], pos[2]);
#endif
        if(mo)
        {
#ifdef _DEBUG
Con_Message("  Applying to mobj %p...\n", mo);
#endif
            mo->mom[MX] = pos[0];
            mo->mom[MY] = pos[1];
            mo->mom[MZ] = pos[2];
        }
        if(clmo)
        {
#ifdef _DEBUG
Con_Message("  Applying to clmobj %i...\n", clmo->mo.thinker.id);
#endif
            clmo->mo.mom[MX] = pos[0];
            clmo->mo.mom[MY] = pos[1];
            clmo->mo.mom[MZ] = pos[2];
        }
    }

    // Send an acknowledgement.
    Msg_Begin(PCL_ACK_PLAYER_FIX);
    Msg_WriteLong(plr->fixAcked.angles);
    Msg_WriteLong(plr->fixAcked.pos);
    Msg_WriteLong(plr->fixAcked.mom);
    Net_SendBuffer(0, SPF_ORDERED | SPF_CONFIRM);
}

/**
 * Used in DEMOS. (Not in regular netgames.)
 * Applies the given dx and dy to the local player's coordinates.
 *
 * @param z             Absolute viewpoint height.
 * @param onground      If @c true the mobj's Z will be set to floorz, and
 *                      the player's viewheight is set so that the viewpoint
 *                      height is param 'z'.
 *                      If @c false the mobj's Z will be param 'z' and
 *                      viewheight is zero.
 */
void Cl_MoveLocalPlayer(float dx, float dy, float z, boolean onground)
{
    ddplayer_t         *pl = &ddPlayers[consolePlayer];
    mobj_t             *mo;
    int                 i;
    float               mom[3];

    mo = pl->mo;
    if(!mo)
        return;

    // Place the new momentum in the appropriate place.
    cpMom[MX][SECONDS_TO_TICKS(gameTime) % LOCALCAM_WRITE_TICS] = dx;
    cpMom[MY][SECONDS_TO_TICKS(gameTime) % LOCALCAM_WRITE_TICS] = dy;

    // Calculate an average.
    mom[MX] = mom[MY] = 0;
    for(i = 0; i < LOCALCAM_WRITE_TICS; ++i)
    {
        mom[MX] += cpMom[MX][i];
        mom[MY] += cpMom[MY][i];
    }
    mom[MX] /= LOCALCAM_WRITE_TICS;
    mom[MY] /= LOCALCAM_WRITE_TICS;

    mo->mom[MX] = mom[MX];
    mo->mom[MY] = mom[MY];

    if(dx != 0 || dy != 0)
    {
        P_MobjUnlink(mo);
        mo->pos[VX] += dx;
        mo->pos[VY] += dy;
        P_MobjLink(mo, DDLINK_SECTOR | DDLINK_BLOCKMAP);
    }

    mo->subsector = R_PointInSubsector(mo->pos[VX], mo->pos[VY]);
    mo->floorZ = mo->subsector->sector->SP_floorheight;
    mo->ceilingZ = mo->subsector->sector->SP_ceilheight;

    if(onground)
    {
        mo->pos[VZ] = z - 1;
        pl->viewHeight = 1;
    }
    else
    {
        mo->pos[VZ] = z;
        pl->viewHeight = 0;
    }

    Cl_UpdatePlayerPos(&ddPlayers[consolePlayer]);
}

/**
 * Animates the player sprites based on their states (up, down, etc.)
 */
#if 0 // Currently unused.
void Cl_MovePsprites(void)
{
    ddplayer_t *pl = players + consolePlayer;
    ddpsprite_t *psp = pl->pSprites;
    int         i;

    for(i = 0; i < 2; ++i)
        if(psp[i].tics > 0)
            psp[i].tics--;

    switch(psp->state)
    {
    case DDPSP_UP:
        pspY -= pspMoveSpeed;
        if(pspY <= TOP_PSPY)
        {
            pspY = TOP_PSPY;
            psp->state = DDPSP_BOBBING;
        }
        psp->y = pspY;
        break;

    case DDPSP_DOWN:
        pspY += pspMoveSpeed;
        if(pspY > BOTTOM_PSPY)
            pspY = BOTTOM_PSPY;
        psp->y = pspY;
        break;

    case DDPSP_FIRE:
        pspY = TOP_PSPY;
        //psp->x = 0;
        psp->y = pspY;
        break;

    case DDPSP_BOBBING:
        pspY = TOP_PSPY;
        // Get bobbing from the Game DLL.
        psp->x = *((float*) gx.GetVariable(DD_PSPRITE_BOB_X));
        psp->y = *((float*) gx.GetVariable(DD_PSPRITE_BOB_Y));
        break;
    }

    if(psp->state != DDPSP_BOBBING)
    {
        if(psp->offX)
            psp->x = psp->offX;
        if(psp->offY)
            psp->y = psp->offY;
    }

    // The other psprite gets the same coords.
    psp[1].x = psp->x;
    psp[1].y = psp->y;
}
#endif

/**
 * Reads a single PSV_FRAME2 player delta from the message buffer and
 * applies it to the player in question.
 */
void Cl_ReadPlayerDelta2(boolean skip)
{
    int         df = 0, psdf, i, idx;
    playerstate_t *s;
    ddplayer_t *pl;
    ddpsprite_t *psp;
    static playerstate_t dummyState;
    static ddplayer_t dummyPlayer;
    int         num, newId;
    short       junk;

    // The first byte consists of a player number and some flags.
    num = Msg_ReadByte();
    df = (num & 0xf0) << 8;
    df |= Msg_ReadByte();       // Second byte is just flags.
    num &= 0xf;                 // Clear the upper bits of the number.

    if(!skip)
    {
        s = &playerState[num];
        pl = &ddPlayers[num];
    }
    else
    {
        // We're skipping, read the data into dummies.
        s = &dummyState;
        pl = &dummyPlayer;
    }

    if(df & PDF_MOBJ)
    {
        clmobj_t *old = s->cmo;

        newId = Msg_ReadShort();

        // Make sure the 'new' mobj is different than the old one;
        // there will be linking problems otherwise.
        if(!skip && newId != s->mobjId)
        {
            boolean justCreated = false;

            s->mobjId = newId;

            // Find the new mobj.
            s->cmo = Cl_FindMobj(s->mobjId);
            if(!s->cmo)
            {
                // This mobj hasn't yet been sent to us.
                // We should be receiving the rest of the info very shortly.
                s->cmo = Cl_CreateMobj(s->mobjId);
                if(num == consolePlayer)
                {
                    // Mark everything known about our local player.
                    s->cmo->flags |= CLMF_KNOWN;
                }
                justCreated = true;
            }
            else
            {
                // The client mobj is already known to us.
                // Unlink it (not interactive or visible).
                Cl_UnsetMobjPosition(s->cmo);
            }

            s->cmo->mo.dPlayer = pl;

            // Make the old clmobj a non-player one (if any).
            if(old)
            {
                old->mo.dPlayer = NULL;
                Cl_SetMobjPosition(old);
            }

            // If it was just created, the coordinates are not yet correct.
            // The update will be made when the mobj data is received.
            if(!justCreated)
            {
                // Replace the hidden client mobj with the real player mobj.
                Cl_UpdateRealPlayerMobj(pl->mo, &s->cmo->mo, 0xffffffff);
            }
            else if(pl->mo)
            {
                // Update the new client mobj's information from the real
                // mobj, which is already known.
                s->cmo->mo.pos[VX] = pl->mo->pos[VX];
                s->cmo->mo.pos[VY] = pl->mo->pos[VY];
                s->cmo->mo.pos[VZ] = pl->mo->pos[VZ];
                s->cmo->mo.angle = pl->mo->angle;
                Cl_UpdatePlayerPos(pl);
            }

#if _DEBUG
Con_Message("Cl_RdPlrD2: Pl%i: mobj=%i old=%ul\n", num, s->mobjId,
            (unsigned int) old);
Con_Message("  x=%g y=%g z=%g fz=%g cz=%g\n", s->cmo->mo.pos[VX],
            s->cmo->mo.pos[VY], s->cmo->mo.pos[VZ],
            s->cmo->mo.floorZ, s->cmo->mo.ceilingZ);
Con_Message("Cl_RdPlrD2: pl=%i => moid=%i\n",
            s->cmo->mo.dPlayer - ddPlayers, s->mobjId);
#endif
        }
    }

    if(df & PDF_FORWARDMOVE)
        s->forwardMove = (char) Msg_ReadByte() * 2048;
    if(df & PDF_SIDEMOVE)
        s->sideMove = (char) Msg_ReadByte() * 2048;
    if(df & PDF_ANGLE)
        //s->angle = Msg_ReadByte() << 24;
        junk = Msg_ReadByte();
    if(df & PDF_TURNDELTA)
    {
        s->turnDelta = ((char) Msg_ReadByte() << 24) / 16;
    }
    if(df & PDF_FRICTION)
        s->friction = Msg_ReadByte() << 8;
    if(df & PDF_EXTRALIGHT)
    {
        i = Msg_ReadByte();
        pl->fixedColorMap = i & 7;
        pl->extraLight = i & 0xf8;
    }
    if(df & PDF_FILTER)
        pl->filter = Msg_ReadLong();
    if(df & PDF_CLYAW)          // Only sent when Fixangles is used.
        //pl->clAngle = Msg_ReadShort() << 16; /* $unifiedangles */
        junk = Msg_ReadShort();
    if(df & PDF_CLPITCH)        // Only sent when Fixangles is used.
        ///pl->clLookDir = Msg_ReadShort() * 110.0 / DDMAXSHORT; /* $unifiedangles */
        junk = Msg_ReadShort();
    if(df & PDF_PSPRITES)
    {
        for(i = 0; i < 2; ++i)
        {
            // First the flags.
            psdf = Msg_ReadByte();
            psp = pl->pSprites + i;
            if(psdf & PSDF_STATEPTR)
            {
                idx = Msg_ReadPackedShort();
                if(!idx)
                    psp->statePtr = 0;
                else if(idx < countStates.num)
                {
                    psp->statePtr = states + (idx - 1);
                    psp->tics = psp->statePtr->tics;
                }
            }
            if(psdf & PSDF_LIGHT)
                psp->light = Msg_ReadByte() / 255.0f;
            if(psdf & PSDF_ALPHA)
                psp->alpha = Msg_ReadByte() / 255.0f;
            if(psdf & PSDF_STATE)
                psp->state = Msg_ReadByte();
            if(psdf & PSDF_OFFSET)
            {
                psp->offset[VX] = (char) Msg_ReadByte() * 2;
                psp->offset[VY] = (char) Msg_ReadByte() * 2;
            }
        }
    }
}

/**
 * Used by the client plane mover.
 *
 * @return              @c true, if the player is free to move according to
 *                      floorz and ceilingz.
 */
boolean Cl_IsFreeToMove(int player)
{
    mobj_t     *mo = ddPlayers[player].mo;

    if(!mo)
        return false;
    return (mo->pos[VZ] >= mo->floorZ &&
            mo->pos[VZ] + mo->height <= mo->ceilingZ);
}
