/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Martin Eyre <martineyre@btinternet.com>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
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
 * p_enemy.c: Enemy thinking, AI.
 *
 * Action Pointer Functions that are associated with states/frames.
 *
 * Enemies are allways spawned with targetplayer = -1, threshold = 0
 * Most monsters are spawned unaware of all players,
 * but some can be made preaware
 */

// HEADER FILES ------------------------------------------------------------

#ifdef MSVC
#  pragma optimize("g", off)
#endif

#include "wolftc.h"

#include "dmu_lib.h"
#include "p_mapspec.h"
#include "p_map.h"
#include "p_ceiling.h"
#include "p_door.h"
#include "p_floor.h"

// MACROS ------------------------------------------------------------------

#define FATSPREAD   (ANG90/8)
#define SKULLSPEED  (20*FRACUNIT)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void C_DECL A_ReFire(player_t *player, pspdef_t * psp);
void C_DECL A_Fall(mobj_t *actor);
void C_DECL A_Fire(mobj_t *actor);
void C_DECL A_SpawnFly(mobj_t *mo);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean fellDown;        //$dropoff_fix: used to flag pushed off ledge
extern linedef_t *blockLine;       // $unstuck: blocking linedef
extern float tmBBox[4];     // for line intersection checks

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean bossKilled;

mobj_t *corpsehit;
mobj_t *vileobj;
fixed_t viletryx;
fixed_t viletryy;

mobj_t **brainTargets;
int     numBrainTargets;
int     numBrainTargetsAlloc;

struct brain_s brain;   // killough 3/26/98: global state of boss brain

int     TRACEANGLE = 0xc000000;

fixed_t xspeed[8] =
    { FRACUNIT, 47000, 0, -47000, -FRACUNIT, -47000, 0, 47000 };
fixed_t yspeed[8] =
    { 0, 47000, FRACUNIT, 47000, 0, -47000, -FRACUNIT, -47000 };

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static fixed_t dropoff_deltax, dropoff_deltay, floorz;

// CODE --------------------------------------------------------------------

/**
 * If a monster yells at a player, it will alert other monsters to the
 * player's whereabouts.
 */
void P_NoiseAlert(mobj_t *target, mobj_t *emitter)
{
    VALIDCOUNT++;
    P_RecursiveSound(target, P_GetPtrp(emitter->subsector, DMU_SECTOR), 0);
}

boolean P_CheckMeleeRange(mobj_t *actor)
{
    mobj_t             *pl;
    float               dist, range;

    if(!actor->target)
        return false;

    pl = actor->target;
    dist = P_ApproxDistance(pl->pos[VX] - actor->pos[VX],
                            pl->pos[VY] - actor->pos[VY]);
    if(!cfg.netNoMaxZMonsterMeleeAttack)
    {   // Account for Z height difference.
        if(pl->pos[VZ] > actor->pos[VZ] + actor->height ||
           pl->pos[VZ] + pl->height < actor->pos[VZ])
            return false;
    }

    range = MELEERANGE - 20 + pl->info->radius;
    if(dist >= range)
        return false;

    if(!P_CheckSight(actor, actor->target))
        return false;

    return true;
}

boolean P_CheckMissileRange(mobj_t *actor)
{
    fixed_t dist;

    if(!P_CheckSight(actor, actor->target))
        return false;

    if(actor->flags & MF_JUSTHIT)
    {
        // the target just hit the enemy,
        // so fight back!
        actor->flags &= ~MF_JUSTHIT;
        return true;
    }

    if(actor->reactionTime)
        return false;           // do not attack yet

    // OPTIMIZE: get this from a global checksight
    dist =
        P_ApproxDistance(actor->pos[VX] - actor->target->pos[VX],
                         actor->pos[VY] - actor->target->pos[VY]) - 64 * FRACUNIT;

    if(!actor->info->meleeState)
        dist -= 128 * FRACUNIT; // no melee attack, so fire more

    dist >>= 16;

    if(actor->type == MT_VILE)
    {
        if(dist > 14 * 64)
            return false;       // too far away
    }

    if(actor->type == MT_UNDEAD)
    {
        if(dist < 196)
            return false;       // close for fist attack
        dist >>= 1;
    }

    if(actor->type == MT_CYBORG || actor->type == MT_SPIDER ||
       actor->type == MT_SKULL)
    {
        dist >>= 1;
    }

    if(dist > 200)
        dist = 200;

    if(actor->type == MT_CYBORG && dist > 160)
        dist = 160;

    if(P_Random() < dist)
        return false;

    return true;
}

/*
 * Move in the current direction,
 * returns false if the move is blocked.
 * killough $dropoff_fix
 */
boolean P_Move(mobj_t *actor, boolean dropoff)
{
    fixed_t tryx, stepx;
    fixed_t tryy, stepy;
    linedef_t *ld;
    boolean good;

    if(actor->moveDir == DI_NODIR)
        return false;

    if((unsigned) actor->moveDir >= DI_NODIR)
        Con_Error("Weird actor->moveDir!");

    stepx = actor->info->speed / FRACUNIT * xspeed[actor->moveDir];
    stepy = actor->info->speed / FRACUNIT * yspeed[actor->moveDir];
    tryx = actor->pos[VX] + stepx;
    tryy = actor->pos[VY] + stepy;

    // killough $dropoff_fix.
    if(!P_TryMove(actor, tryx, tryy, dropoff, false))
    {
        // Open any specials.
        if((actor->flags & MF_FLOAT) && floatOk)
        {
            // Must adjust height.
            if(actor->pos[VZ] < tmFloorZ)
                actor->pos[VZ] += FLOATSPEED;
            else
                actor->pos[VZ] -= FLOATSPEED;

            actor->flags |= MF_INFLOAT;
            return true;
        }

        if(!P_IterListSize(spechit))
            return false;

        actor->moveDir = DI_NODIR;
        good = false;
        while((ld = P_PopIterList(spechit)) != NULL)
        {
            // if the special is not a door that can be opened, return false
            //
            // killough $unstuck: this is what caused monsters to get stuck in
            // doortracks, because it thought that the monster freed itself
            // by opening a door, even if it was moving towards the doortrack,
            // and not the door itself.
            //
            // If a line blocking the monster is activated, return true 90%
            // of the time. If a line blocking the monster is not activated,
            // but some other line is, return false 90% of the time.
            // A bit of randomness is needed to ensure it's free from
            // lockups, but for most cases, it returns the correct result.
            //
            // Do NOT simply return false 1/4th of the time (causes monsters to
            // back out when they shouldn't, and creates secondary stickiness).

            if(P_ActivateLine(ld, actor, 0, SPAC_USE))
                good |= ld == blockLine ? 1 : 2;
        }

        if(!good || cfg.monstersStuckInDoors)
            return good;
        else
            return (P_Random() >= 230) || (good & 1);
    }
    else
    {
        P_MobjSetSRVO(actor, FIX2FLT(stepx), FIX2FLT(stepy));
        actor->flags &= ~MF_INFLOAT;
    }

    // $dropoff_fix: fall more slowly, under gravity, if fellDown==true
    if(!(actor->flags & MF_FLOAT) && !fellDown)
    {
        if(actor->pos[VZ] > actor->floorZ)
            P_HitFloor(actor);

        actor->pos[VZ] = actor->floorZ;
    }
    return true;
}

/*
 * Attempts to move actor on in its current (ob->moveangle) direction.
 * If blocked by either a wall or an actor returns FALSE
 * If move is either clear or blocked only by a door, returns TRUE and sets...
 * If a door is in the way, an OpenDoor call is made to start it opening.
 */
boolean P_TryWalk(mobj_t *actor)
{
    // killough $dropoff_fix
    if(!P_Move(actor, false))
    {
        return false;
    }

    actor->moveCount = P_Random() & 15;
    return true;
}

static void P_DoNewChaseDir(mobj_t *actor, fixed_t deltax, fixed_t deltay)
{
    dirtype_t xdir, ydir, tdir;
    dirtype_t olddir = actor->moveDir;
    dirtype_t turnaround = olddir;

    if(turnaround != DI_NODIR)  // find reverse direction
        turnaround ^= 4;

    xdir =
        deltax > 10 * FRACUNIT ? DI_EAST : deltax <
        -10 * FRACUNIT ? DI_WEST : DI_NODIR;

    ydir =
        deltay < -10 * FRACUNIT ? DI_SOUTH : deltay >
        10 * FRACUNIT ? DI_NORTH : DI_NODIR;

    // try direct route
    if(xdir != DI_NODIR && ydir != DI_NODIR &&
       turnaround != (actor->moveDir =
                      deltay < 0 ? deltax >
                      0 ? DI_SOUTHEAST : DI_SOUTHWEST : deltax >
                      0 ? DI_NORTHEAST : DI_NORTHWEST) && P_TryWalk(actor))
        return;

    // try other directions
    if(P_Random() > 200 || abs(deltay) > abs(deltax))
        tdir = xdir, xdir = ydir, ydir = tdir;

    if((xdir == turnaround ? xdir = DI_NODIR : xdir) != DI_NODIR &&
       (actor->moveDir = xdir, P_TryWalk(actor)))
        return;                 // either moved forward or attacked

    if((ydir == turnaround ? ydir = DI_NODIR : ydir) != DI_NODIR &&
       (actor->moveDir = ydir, P_TryWalk(actor)))
        return;

    // there is no direct path to the player, so pick another direction.
    if(olddir != DI_NODIR && (actor->moveDir = olddir, P_TryWalk(actor)))
        return;

    // randomly determine direction of search
    if(P_Random() & 1)
    {
        for(tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
            if(tdir != turnaround && (actor->moveDir = tdir, P_TryWalk(actor)))
                return;
    }
    else
        for(tdir = DI_SOUTHEAST; tdir != DI_EAST - 1; tdir--)
            if(tdir != turnaround && (actor->moveDir = tdir, P_TryWalk(actor)))
                return;

    if((actor->moveDir = turnaround) != DI_NODIR && !P_TryWalk(actor))
        actor->moveDir = DI_NODIR;
}

/**
 * Monsters try to move away from tall dropoffs.
 *
 * In Doom, they were never allowed to hang over dropoffs, and would remain
 * stuck if involuntarily forced over one. This logic, combined with
 * p_map.c::P_TryMove(), allows monsters to free themselves without making
 * them tend to hang over dropoffs.
 */
static boolean PIT_AvoidDropoff(linedef_t* line, void* data)
{
    sector_t*           backsector = P_GetPtrp(line, DMU_BACK_SECTOR);
    float*              bbox = P_GetPtrp(line, DMU_BOUNDING_BOX);

    if(backsector &&
       tmBBox[BOXRIGHT]  > bbox[BOXLEFT] &&
       tmBBox[BOXLEFT]   < bbox[BOXRIGHT]  &&
       tmBBox[BOXTOP]    > bbox[BOXBOTTOM] && // Linedef must be contacted
       tmBBox[BOXBOTTOM] < bbox[BOXTOP]    &&
       P_BoxOnLineSide(tmBBox, line) == -1)
    {
        sector_t*           frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);
        float               front = P_GetFloatp(frontsector, DMU_FLOOR_HEIGHT);
        float               back = P_GetFloatp(backsector, DMU_FLOOR_HEIGHT);
        float               d1[2];
        angle_t             angle;

        P_GetFloatpv(line, DMU_DXY, &d1);

        // The monster must contact one of the two floors, and the other
        // must be a tall drop off (more than 24).
        if(back == floorZ && front < floorZ - 24)
        {
            angle = R_PointToAngle2(0, 0, d1[0], d1[1]); // front side drop off
        }
        else
        {
            if(front == floorZ && back < floorZ - 24)
                angle = R_PointToAngle2(d1[0], d1[1], 0, 0); // back side drop off
            else
                return true;
        }

        // Move away from drop off at a standard speed.
        // Multiple contacted linedefs are cumulative (e.g. hanging over corner)
        dropoffDelta[VX] -= FIX2FLT(finesine[angle >> ANGLETOFINESHIFT]) * 32;
        dropoffDelta[VY] += FIX2FLT(finecosine[angle >> ANGLETOFINESHIFT]) * 32;
    }
    return true;
}

/**
 * Driver for above
 */
static float P_AvoidDropoff(mobj_t *actor)
{
    floorZ = actor->pos[VZ]; // Remember floor height.

    dropoffDelta[VX] = dropoffDelta[VY] = 0;

    VALIDCOUNT++;

    // Check lines
    P_MobjLinesIterator(actor, PIT_AvoidDropoff, 0);

    // Non-zero if movement prescribed.
    return !(dropoffDelta[VX] == 0 || dropoffDelta[VY] == 0);
}

void P_NewChaseDir(mobj_t *actor)
{
    mobj_t *target = actor->target;
    fixed_t deltax = target->pos[VX] - actor->pos[VX];
    fixed_t deltay = target->pos[VY] - actor->pos[VY];

    if(actor->floorZ - actor->dropOffZ > 24 &&
       actor->pos[VZ] <= FLT2FIX(actor->floorZ) &&
       !(actor->flags & (MF_DROPOFF | MF_FLOAT)) &&
       !cfg.avoidDropoffs && P_AvoidDropoff(actor))
    {
        // Move away from dropoff
        P_DoNewChaseDir(actor, dropoff_deltax, dropoff_deltay);

        // $dropoff_fix
        // If moving away from drop off, set movecount to 1 so that
        // small steps are taken to get monster away from drop off.

        actor->moveCount = 1;
        return;
    }

    P_DoNewChaseDir(actor, deltax, deltay);
}

/*
 * If allaround is false, only look 180 degrees in front.
 * Returns true if a player is targeted.
 */
boolean P_LookForPlayers(mobj_t *actor, boolean allaround)
{
    int     c;
    int     stop;
    player_t *player;
    angle_t an;
    fixed_t dist;
    int     playerCount;

    for(c = playerCount = 0; c < MAXPLAYERS; c++)
        if(players[c].plr->inGame)
            playerCount++;

    // Are there any players?
    if(!playerCount)
        return false;

    c = 0;
    stop = (actor->lastLook - 1) & 3;

    for(;; actor->lastLook = (actor->lastLook + 1) & 3)
    {
        if(!players[actor->lastLook].plr->inGame)
            continue;

        if(c++ == 2 || actor->lastLook == stop)
        {
            // done looking
            return false;
        }

        player = &players[actor->lastLook];

        if(player->health <= 0)
            continue;           // dead

        if(!P_CheckSight(actor, player->plr->mo))
            continue;           // out of sight

        if(!allaround)
        {
            an = R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                                 player->plr->mo->pos[VX], player->plr->mo->pos[VY]);
            an -= actor->angle;

            if(an > ANG90 && an < ANG270)
            {
                dist =
                    P_ApproxDistance(player->plr->mo->pos[VX] - actor->pos[VX],
                                     player->plr->mo->pos[VY] - actor->pos[VY]);
                // if real close, react anyway
                if(dist > FLT2FIX(MELEERANGE))
                    continue;   // behind back
            }
        }

        actor->target = player->plr->mo;
        return true;
    }
}

int P_Massacre(void)
{
    int         count = 0;
    mobj_t     *mo;
    thinker_t  *think;

    // Only massacre when in a level.
    if(G_GetGameState() != GS_MAP)
        return 0;

    for(think = thinkerCap.next; think != &thinkerCap; think = think->next)
    {
        if(think->function != P_MobjThinker)
        {                       // Not a mobj thinker
            continue;
        }
        mo = (mobj_t *) think;
        if(mo->type == MT_SKULL ||
           (mo->flags & MF_COUNTKILL && mo->health > 0))
        {
            P_DamageMobj(mo, NULL, NULL, 10000);
            count++;
        }
    }
    return count;
}

/*
 * DOOM II special, map 32. Uses special tag 666.
 */
void C_DECL A_KeenDie(mobj_t *mo)
{
    thinker_t *th;
    mobj_t *mo2;
    linedef_t* dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkerCap.next; th != &thinkerCap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 666;
    EV_DoDoor(dummyLine, DT_OPEN);
    P_FreeDummyLine(dummyLine);
}

/*
 * Stay in state until a player is sighted.
 */
void C_DECL A_Look(mobj_t *actor)
{
    sector_t* sec = NULL;
    mobj_t *targ;

    actor->threshold = 0;       // any shot will wake up
    sec = P_GetPtrp(actor->subsector, DMU_SECTOR);
    targ = P_ToXSector(sec)->soundTarget;

    if(targ && (targ->flags & MF_SHOOTABLE))
    {
        actor->target = targ;

        if(actor->flags & MF_AMBUSH)
        {
            if(P_CheckSight(actor, actor->target))
                goto seeyou;
        }
        else
            goto seeyou;
    }

    if(!P_LookForPlayers(actor, false))
        return;

    // Go into chase state.
  seeyou:
    if(actor->info->seeSound)
    {
        int                 sound;

        switch(actor->info->seeSound)
        {
        case SFX_POSIT1:
        case SFX_POSIT2:
        case SFX_POSIT3:
            sound = SFX_POSIT1 + P_Random() % 3;
            break;

        case SFX_BGSIT1:
        case SFX_BGSIT2:
            sound = SFX_BGSIT1 + P_Random() % 2;
            break;

        default:
            sound = actor->info->seeSound;
            break;
        }

        if(actor->flags2 & MF2_BOSS)
            S_StartSound(sound | DDSF_NO_ATTENUATION, actor); // full volume
        else
            S_StartSound(sound, actor);
    }

    P_MobjChangeState(actor, actor->info->seeState);
}

/*
 * Actor has a melee attack,
 * so it tries to close as fast as possible
 */
void C_DECL A_Chase(mobj_t *actor)
{
    int     delta;

    if(actor->reactionTime)
        actor->reactionTime--;

    // modify target threshold
    if(actor->threshold)
    {
        if(!actor->target || actor->target->health <= 0)
        {
            actor->threshold = 0;
        }
        else
            actor->threshold--;
    }

    // turn towards movement direction if not there yet
    if(actor->moveDir < DI_NODIR)
    {
        actor->angle &= (7 << 29);
        delta = actor->angle - (actor->moveDir << 29);

        if(delta > 0)
            actor->angle -= ANG90 / 2;
        else if(delta < 0)
            actor->angle += ANG90 / 2;
    }

    if(!actor->target || !(actor->target->flags & MF_SHOOTABLE))
    {
        // look for a new target
        if(P_LookForPlayers(actor, true))
            return;             // got a new target

        P_MobjChangeState(actor, actor->info->spawnState);
        return;
    }

    // do not attack twice in a row
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(gameskill != SM_NIGHTMARE && !fastparm)
            P_NewChaseDir(actor);
        return;
    }

    // check for melee attack
    if(actor->info->meleeState && P_CheckMeleeRange(actor))
    {
        if(actor->info->attackSound)
            S_StartSound(actor->info->attackSound, actor);

        P_MobjChangeState(actor, actor->info->meleeState);
        return;
    }

    // check for missile attack
    if(actor->info->missileState)
    {
        if(gameskill < SM_NIGHTMARE && !fastparm && actor->moveCount)
        {
            goto nomissile;
        }

        if(!P_CheckMissileRange(actor))
            goto nomissile;

        P_MobjChangeState(actor, actor->info->missileState);
        actor->flags |= MF_JUSTATTACKED;
        return;
    }

    // ?
  nomissile:
    // possibly choose another target
    if(IS_NETGAME && !actor->threshold && !P_CheckSight(actor, actor->target))
    {
        if(P_LookForPlayers(actor, true))
            return;             // got a new target
    }

    // chase towards player
    if(--actor->moveCount < 0 || !P_Move(actor, false))
    {
        P_NewChaseDir(actor);
    }

    // make active sound
    if(actor->info->activeSound && P_Random() < 3)
    {
        S_StartSound(actor->info->activeSound, actor);
    }
}

void C_DECL A_FaceTarget(mobj_t *actor)
{
    if(!actor->target)
        return;

    actor->turnTime = true;     // $visangle-facetarget
    actor->flags &= ~MF_AMBUSH;
    actor->angle =
        R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                        actor->target->pos[VX], actor->target->pos[VY]);

    if(actor->target->flags & MF_SHADOW)
        actor->angle += (P_Random() - P_Random()) << 21;
}

void C_DECL A_PosAttack(mobj_t *actor)
{
    int         angle, damage;
    float       slope;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    angle = actor->angle;
    slope = P_AimLineAttack(actor, angle, MISSILERANGE);

    S_StartSound(SFX_PISTOL, actor);
    angle += (P_Random() - P_Random()) << 20;
    damage = ((P_Random() % 5) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void C_DECL A_SPosAttack(mobj_t *actor)
{
    int         i, angle, bangle, damage;
    float       slope;

    if(!actor->target)
        return;

    S_StartSound(SFX_SHOTGN, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    for(i = 0; i < 3; i++)
    {
        angle = bangle + ((P_Random() - P_Random()) << 20);
        damage = ((P_Random() % 5) + 1) * 3;
        P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
    }
}

void C_DECL A_CPosAttack(mobj_t *actor)
{
    int         angle, bangle, damage;
    float       slope;

    if(!actor->target)
        return;

    S_StartSound(SFX_SHOTGN, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random() - P_Random()) << 20);
    damage = ((P_Random() % 5) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void C_DECL A_CPosRefire(mobj_t *actor)
{
    // keep firing unless target got out of sight
    A_FaceTarget(actor);

    if(P_Random() < 40)
        return;

    if(!actor->target || actor->target->health <= 0 ||
       !P_CheckSight(actor, actor->target))
    {
        P_MobjChangeState(actor, actor->info->seeState);
    }
}

void C_DECL A_SpidRefire(mobj_t *actor)
{
    // keep firing unless target got out of sight
    A_FaceTarget(actor);

    if(P_Random() < 10)
        return;

    if(!actor->target || actor->target->health <= 0 ||
       !P_CheckSight(actor, actor->target))
    {
        P_MobjChangeState(actor, actor->info->seeState);
    }
}

void C_DECL A_BspiAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // launch a missile
    P_SpawnMissile(MT_ARACHPLAZ, actor, actor->target);
}

void C_DECL A_TroopAttack(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor))
    {
        S_StartSound(SFX_CLAW, actor);
        damage = (P_Random() % 8 + 1) * 3;
        P_DamageMobj(actor->target, actor, actor, damage);
        return;
    }

    // launch a missile
    P_SpawnMissile(MT_TROOPSHOT, actor, actor->target);
}

void C_DECL A_SargAttack(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = ((P_Random() % 10) + 1) * 4;
        P_DamageMobj(actor->target, actor, actor, damage);
    }
}

void C_DECL A_HeadAttack(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = (P_Random() % 6 + 1) * 10;
        P_DamageMobj(actor->target, actor, actor, damage);
        return;
    }

    // launch a missile
    P_SpawnMissile(MT_HEADSHOT, actor, actor->target);
}

void C_DECL A_CyberAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    P_SpawnMissile(MT_ROCKET, actor, actor->target);
}

void C_DECL A_BruisAttack(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
    {
        S_StartSound(SFX_CLAW, actor);
        damage = (P_Random() % 8 + 1) * 10;
        P_DamageMobj(actor->target, actor, actor, damage);
        return;
    }

    // launch a missile
    P_SpawnMissile(MT_BRUISERSHOT, actor, actor->target);
}

void C_DECL A_SkelMissile(mobj_t *actor)
{
    mobj_t *mo;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    mo = P_SpawnMissile(MT_TRACER, actor, actor->target);
    if(mo)
    {
        mo->pos[VX] += mo->mom[MX];
        mo->pos[VY] += mo->mom[MY];
        mo->tracer = actor->target;
    }
}

void C_DECL A_Tracer(mobj_t *actor)
{
    angle_t     exact;
    float       dist, slope;
    mobj_t     *dest, *th;

    if(gametic & 3)
        return;

    // spawn a puff of smoke behind the rocket.
    P_SpawnCustomPuff(MT_ROCKETPUFF, actor->pos[VX], actor->pos[VY], actor->pos[VZ]);

    th = P_SpawnMobj3f(MT_SMOKE,
                       actor->pos[VX] - actor->mom[MX],
                       actor->pos[VY] - actor->mom[MY],
                       actor->pos[VZ]);

    th->mom[MZ] = 1;
    th->tics -= P_Random() & 3;
    if(th->tics < 1)
        th->tics = 1;

    // Adjust direction.
    dest = actor->tracer;

    if(!dest || dest->health <= 0)
        return;

    // Change angle.
    exact = R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                            dest->pos[VX], dest->pos[VY]);

    if(exact != actor->angle)
    {
        if(exact - actor->angle > 0x80000000)
        {
            actor->angle -= TRACEANGLE;
            if(exact - actor->angle < 0x80000000)
                actor->angle = exact;
        }
        else
        {
            actor->angle += TRACEANGLE;
            if(exact - actor->angle > 0x80000000)
                actor->angle = exact;
        }
    }

    exact = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = FIX2FLT(FixedMul(actor->info->speed, finecosine[exact]));
    actor->mom[MY] = FIX2FLT(FixedMul(actor->info->speed, finesine[exact]));

    // change slope
    dist = P_ApproxDistance(dest->pos[VX] - actor->pos[VX],
                            dest->pos[VY] - actor->pos[VY]);

    dist /= FIX2FLT(actor->info->speed);
    if(dist < 1)
        dist = 1;

    slope = (dest->pos[VZ] + 40 - actor->pos[VZ]) / dist;

    if(slope < actor->mom[MZ])
        actor->mom[MZ] -= 1 / 8;
    else
        actor->mom[MZ] += 1 / 8;
}

void C_DECL A_SkelWhoosh(mobj_t *actor)
{
    if(!actor->target)
        return;
    A_FaceTarget(actor);
    S_StartSound(SFX_SKESWG, actor);
}

void C_DECL A_SkelFist(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);

    if(P_CheckMeleeRange(actor))
    {
        damage = ((P_Random() % 10) + 1) * 6;
        S_StartSound(SFX_SKEPCH, actor);
        P_DamageMobj(actor->target, actor, actor, damage);
    }
}

/**
 * Detect a corpse that could be raised.
 */
boolean PIT_VileCheck(mobj_t *thing, void *data)
{
    int     maxdist;
    boolean check;

    if(!(thing->flags & MF_CORPSE))
        return true;            // not a monster

    if(thing->tics != -1)
        return true;            // not lying still yet

    if(thing->info->raiseState == S_NULL)
        return true;            // monster doesn't have a raise state

    maxdist = thing->info->radius + mobjInfo[MT_VILE].radius;

    if(abs(thing->pos[VX] - viletryx) > maxdist ||
       abs(thing->pos[VY] - viletryy) > maxdist)
        return true;            // not actually touching

    corpsehit = thing;
    corpsehit->mom[MX] = corpsehit->mom[MY] = 0;

// DJS - Used the PRBoom method to fix archvile raising ghosts
//  If !raiseghosts then ressurect a "normal" MF_SOLID one.

    if(cfg.raiseGhosts)
    {
        corpsehit->height *= 2*2;
        check = P_CheckPosition2f(corpsehit, corpsehit->pos[VX], corpsehit->pos[VY]);
        corpsehit->height /= 2*2;
    }
    else
    {
        int radius;
        float height;

        height = corpsehit->height; // save temporarily
        radius = corpsehit->radius; // save temporarily
        corpsehit->height = FIX2FLT(corpsehit->info->height);
        corpsehit->radius = corpsehit->info->radius;
        corpsehit->flags |= MF_SOLID;

        check = P_CheckPosition2f(corpsehit, corpsehit->pos[VX], corpsehit->pos[VY]);

        corpsehit->height = height; // restore
        corpsehit->radius = radius; // restore
        corpsehit->flags &= ~MF_SOLID;
    }
// raiseghosts

    if(!check)
        return true;            // doesn't fit here

    return false;               // got one, so stop checking
}

/**
 * Check for ressurecting a body.
 */
void C_DECL A_VileChase(mobj_t *actor)
{
    mobjinfo_t *info;
    mobj_t     *temp;
    float       box[4];

    if(actor->moveDir != DI_NODIR)
    {
        // Check for corpses to raise.
        vileTry[VX] = actor->pos[VX] +
            actor->info->speed * dirSpeed[actor->moveDir][VX];
        vileTry[VY] = actor->pos[VY] +
            actor->info->speed * dirSpeed[actor->moveDir][VY];

        box[BOXLEFT]   = vileTry[VX] - MAXRADIUS * 2;
        box[BOXRIGHT]  = vileTry[VX] + MAXRADIUS * 2;
        box[BOXBOTTOM] = vileTry[VY] - MAXRADIUS * 2;
        box[BOXTOP]    = vileTry[VY] + MAXRADIUS * 2;

        vileobj = actor;
        // Call PIT_VileCheck to check whether object is a corpse
        // that can be raised.
        if(!P_MobjsBoxIterator(box, PIT_VileCheck, 0))
        {
            // Got one!
            temp = actor->target;
            actor->target = corpsehit;
            A_FaceTarget(actor);
            actor->target = temp;

            P_MobjChangeState(actor, S_VILE_HEAL1);
            S_StartSound(SFX_SLOP, corpsehit);
            info = corpsehit->info;

            P_MobjChangeState(corpsehit, info->raiseState);

            if(cfg.raiseGhosts)
            {
                corpsehit->height *= 2*2;
            }
            else
            {
                corpsehit->height = info->height;
                corpsehit->radius = info->radius;
            }

            corpsehit->flags = info->flags;
            corpsehit->health = info->spawnHealth;
            corpsehit->target = NULL;
            corpsehit->corpseTics = 0;
            return;
        }
    }

    // Return to normal attack.
    A_Chase(actor);
}

void C_DECL A_VileStart(mobj_t *actor)
{
    S_StartSound(SFX_VILATK, actor);
}

void C_DECL A_StartFire(mobj_t *actor)
{
    S_StartSound(SFX_FLAMST, actor);
    A_Fire(actor);
}

void C_DECL A_FireCrackle(mobj_t *actor)
{
    S_StartSound(SFX_FLAME, actor);
    A_Fire(actor);
}

/*
 * Keep fire in front of player unless out of sight.
 */
void C_DECL A_Fire(mobj_t *actor)
{
    mobj_t *dest;
    unsigned an;

    dest = actor->tracer;
    if(!dest)
        return;

    // don't move it if the vile lost sight
    if(!P_CheckSight(actor->target, dest))
        return;

    an = dest->angle >> ANGLETOFINESHIFT;

    P_MobjUnsetPosition(actor);
    memcpy(actor->pos, dest->pos, sizeof(actor->pos));
    actor->pos[VX] += FixedMul(24 * FRACUNIT, finecosine[an]);
    actor->pos[VY] += FixedMul(24 * FRACUNIT, finesine[an]);
    P_MobjSetPosition(actor);
}

/*
 * Spawn the archviles' hellfire
 */
void C_DECL A_VileTarget(mobj_t *actor)
{
    mobj_t *fog;

    if(!actor->target)
        return;

    A_FaceTarget(actor);

    fog = P_SpawnMobj3f(actor->target->pos[VX], actor->target->pos[VY],
                      actor->target->pos[VZ], MT_FIRE);

    actor->tracer = fog;
    fog->target = actor;
    fog->tracer = actor->target;
    A_Fire(fog);
}

void C_DECL A_VileAttack(mobj_t* actor)
{
    mobj_t*             fire;
    uint                an;

    if(!actor->target)
        return;

    A_FaceTarget(actor);

    if(!P_CheckSight(actor, actor->target))
        return;

    S_StartSound(SFX_BAREXP, actor);
    P_DamageMobj(actor->target, actor, actor, 20);
    actor->target->mom[MZ] =
        FIX2FLT(1000 * FRACUNIT / actor->target->info->mass);

    an = actor->angle >> ANGLETOFINESHIFT;
    fire = actor->tracer;

    if(!fire)
        return;

    // Move the fire between the Vile and the player.
    fire->pos[VX] = actor->target->pos[VX] - 24 * FIX2FLT(finecosine[an]);
    fire->pos[VY] = actor->target->pos[VY] - 24 * FIX2FLT(finesine[an]);
    P_RadiusAttack(fire, actor, 70, 69);
}

/*
 * Mancubus attack:
 * Firing three missiles (bruisers) in three different directions?
 * ...Doesn't look like it.
 */
void C_DECL A_FatRaise(mobj_t *actor)
{
    A_FaceTarget(actor);
    S_StartSound(SFX_MANATK, actor);
}

void C_DECL A_FatAttack1(mobj_t *actor)
{
    mobj_t *mo;
    int     an;

    A_FaceTarget(actor);
    // Change direction  to ...
    actor->angle += FATSPREAD;
    P_SpawnMissile(MT_FATSHOT, actor, actor->target);

    mo = P_SpawnMissile(MT_FATSHOT, actor, actor->target);
    if(mo)
    {
        mo->angle += FATSPREAD;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = FixedMul(mo->info->speed, finecosine[an]);
        mo->mom[MY] = FixedMul(mo->info->speed, finesine[an]);
    }
}

void C_DECL A_FatAttack2(mobj_t *actor)
{
    mobj_t *mo;
    int     an;

    A_FaceTarget(actor);
    // Now here choose opposite deviation.
    actor->angle -= FATSPREAD;
    P_SpawnMissile(MT_FATSHOT, actor, actor->target);

    mo = P_SpawnMissile(MT_FATSHOT, actor, actor->target);
    if(mo)
    {
        mo->angle -= FATSPREAD * 2;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = FixedMul(mo->info->speed, finecosine[an]);
        mo->mom[MY] = FixedMul(mo->info->speed, finesine[an]);
    }
}

void C_DECL A_FatAttack3(mobj_t *actor)
{
    mobj_t *mo;
    int     an;

    A_FaceTarget(actor);

    mo = P_SpawnMissile(MT_FATSHOT, actor, actor->target);
    if(mo)
    {
        mo->angle -= FATSPREAD / 2;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = FixedMul(mo->info->speed, finecosine[an]);
        mo->mom[MY] = FixedMul(mo->info->speed, finesine[an]);
    }

    mo = P_SpawnMissile(MT_FATSHOT, actor, actor->target);
    if(mo)
    {
        mo->angle += FATSPREAD / 2;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = FixedMul(mo->info->speed, finecosine[an]);
        mo->mom[MY] = FixedMul(mo->info->speed, finesine[an]);
    }
}

/*
 * LostSoul Attack:
 * Fly at the player like a missile.
 */
void C_DECL A_SkullAttack(mobj_t *actor)
{
    mobj_t *dest;
    angle_t an;
    int     dist;

    if(!actor->target)
        return;

    dest = actor->target;
    actor->flags |= MF_SKULLFLY;

    S_StartSound(actor->info->attackSound, actor);
    A_FaceTarget(actor);
    an = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = FixedMul(SKULLSPEED, finecosine[an]);
    actor->mom[MY] = FixedMul(SKULLSPEED, finesine[an]);
    dist = P_ApproxDistance(dest->pos[VX] - actor->pos[VX],
                            dest->pos[VY] - actor->pos[VY]);
    dist = dist / SKULLSPEED;

    if(dist < 1)
        dist = 1;

    actor->mom[MZ] = (dest->pos[VZ] + FIX2FLT(dest->height /2) - actor->pos[VZ]) / dist;
}

/*
 * PainElemental Attack:
 * Spawn a lost soul and launch it at the target
 */
void A_PainShootSkull(mobj_t *actor, angle_t angle)
{
    float       pos[3];
    mobj_t     *newmobj;
    angle_t     an;
    float       prestep;
    int         count;
    sector_t   *sec;
    thinker_t  *currentthinker;

    // DJS - Compat option for unlimited lost soul spawns
    if(cfg.maxSkulls)
    {
        // count total number of skull currently on the level
        count = 0;

        currentthinker = thinkerCap.next;
        while(currentthinker != &thinkerCap)
        {
            if((currentthinker->function == P_MobjThinker) &&
            ((mobj_t *) currentthinker)->type == MT_SKULL)
                count++;
            currentthinker = currentthinker->next;
        }

        // if there are allready 20 skulls on the level,
        // don't spit another one
        if(count > 20)
            return;
    }

    // okay, there's playe for another one
    an = angle >> ANGLETOFINESHIFT;

    prestep = 4 +
        3 * ((actor->info->radius + mobjInfo[MT_SKULL].radius) / 2);

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += prestep * FIX2FLT(finecosine[an]);
    pos[VY] += prestep * FIX2FLT(finesine[an]);
    pos[VZ] += 8;

    // DJS - Compat option to prevent spawning lost souls inside walls /from prBoom
    if(cfg.allowSkullsInWalls)
    {
        newmobj = P_SpawnMobj3fv(MT_SKULL, pos);
    }
    else
    {
        // Check whether the Lost Soul is being fired through a 1-sided
        // wall or an impassible line, or a "monsters can't cross" line.
        // If it is, then we don't allow the spawn. This is a bug fix, but
        // it should be considered an enhancement, since it may disturb
        // existing demos, so don't do it in compatibility mode.

        if(P_CheckSides(actor, pos[VX], pos[VY]))
            return;

        newmobj = P_SpawnMobj3f(FLT2FIX(pos[VX]), FLT2FIX(pos[VY]), FLT2FIX(pos[VZ]), MT_SKULL);
        sec = P_GetPtrp(newmobj->subsector, DMU_SECTOR);

        // Check to see if the new Lost Soul's z value is above the
        // ceiling of its new sector, or below the floor. If so, kill it.
        if((newmobj->pos[VZ] >
                (P_GetFloatp(sec, DMU_CEILING_HEIGHT) - newmobj->height)) ||
           (newmobj->pos[VZ] < P_GetFloatp(sec, DMU_FLOOR_HEIGHT)))
        {
            // kill it immediately
            P_DamageMobj(newmobj, actor, actor, 10000);
            return;
        }
    }

    // Check for movements.
    // killough $dropoff_fix
    if(!P_TryMove(newmobj, FLT2FIX(newmobj->pos[VX]), FLT2FIX(newmobj->pos[VY]), false, false))
    {
        // kill it immediately
        P_DamageMobj(newmobj, actor, actor, 10000);
        return;
    }

    newmobj->target = actor->target;
    A_SkullAttack(newmobj);
}

/*
 * PainElemental Attack:
 * Spawn a lost soul and launch it at the target
 */
void C_DECL A_PainAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    A_PainShootSkull(actor, actor->angle);
}

void C_DECL A_PainDie(mobj_t *actor)
{
    A_Fall(actor);
    A_PainShootSkull(actor, actor->angle + ANG90);
    A_PainShootSkull(actor, actor->angle + ANG180);
    A_PainShootSkull(actor, actor->angle + ANG270);
}

void C_DECL A_Scream(mobj_t *actor)
{
    int     sound;

    switch (actor->info->deathSound)
    {
    case 0:
        return;

    case SFX_PODTH1:
    case SFX_PODTH2:
    case SFX_PODTH3:
        sound = SFX_PODTH1 + P_Random() % 3;
        break;

    case SFX_BGDTH1:
    case SFX_BGDTH2:
        sound = SFX_BGDTH1 + P_Random() % 2;
        break;

#if __WOLFTC__
    case SFX_GDDTH1:
    case SFX_GDDTH2:
    case SFX_GDDTH3:
    case SFX_GDDTH4:
    case SFX_GDDTH5:
    case SFX_GDDTH6:
    case SFX_GDDTH7:
    case SFX_GDDTH8:
        sound = SFX_GDDTH1 + P_Random() % 8;
        break;

    case SFX_LGRDD1:
    case SFX_LGRDD2:
    case SFX_LGRDD3:
    case SFX_LGRDD4:
    case SFX_LGRDD5:
    case SFX_LGRDD6:
    case SFX_LGRDD7:
    case SFX_LGRDD8:
        sound = SFX_LGRDD1 + P_Random() % 8;
        break;

    case SFX_UGRBD1:
    case SFX_UGRBD2:
        sound = SFX_UGRBD1 + P_Random() % 2;
        break;
#endif
    default:
        sound = actor->info->deathSound;
        break;
    }

    // Check for bosses.
    if(actor->type == MT_SPIDER || actor->type == MT_CYBORG)
    {
        // full volume
        S_StartSound(sound | DDSF_NO_ATTENUATION, NULL);
    }
    else
        S_StartSound(sound, actor);
}

void C_DECL A_XScream(mobj_t *actor)
{
    S_StartSound(SFX_SLOP, actor);
}

void C_DECL A_Pain(mobj_t *actor)
{
    if(actor->info->painSound)
        S_StartSound(actor->info->painSound, actor);
}

void C_DECL A_Fall(mobj_t *actor)
{
    // actor is on ground, it can be walked over
    actor->flags &= ~MF_SOLID;

    // So change this if corpse objects
    // are meant to be obstacles.
}


void C_DECL A_Explode(mobj_t *thingy)
{
    P_RadiusAttack(thingy, thingy->target, 128, 127);
}

/*
 * Possibly trigger special effects if on first boss level
 */
void C_DECL A_BossDeath(mobj_t *mo)
{
    thinker_t *th;
    mobj_t *mo2;
    linedef_t*  dummyLine;
    int     i;

    // Has the boss already been killed?
    if(bossKilled)
        return;

    if(gamemode == commercial)
    {
        if(gamemap != 7)
            return;
        if((mo->type != MT_FATSO) && (mo->type != MT_BABY))
            return;
    }
    else
    {
        switch (gameepisode)
        {
        case 1:
            if(gamemap != 8)
                return;

            // Ultimate DOOM behavioral change
            // This test was added so that the (variable) effects of the
            // 666 special would only take effect when the last Baron
            // died and not ANY monster.
            // Many classic PWADS such as "Doomsday of UAC" (UAC_DEAD.wad)
            // relied on the old behaviour and cannot be completed.

            // Added compatibility option.
            if(!cfg.anyBossDeath)
                if(mo->type != MT_BRUISER)
                    return;
            break;

        case 2:
            if(gamemap != 8)
                return;

            if(mo->type != MT_CYBORG)
                return;
            break;

        case 3:
            if(gamemap != 8)
                return;

            if(mo->type != MT_SPIDER)
                return;

            break;

        case 4:
            switch (gamemap)
            {
            case 6:
                if(mo->type != MT_CYBORG)
                    return;
                break;

            case 8:
                if(mo->type != MT_SPIDER)
                    return;
                break;

            default:
                return;
                break;
            }
            break;

        default:
            if(gamemap != 8)
                return;
            break;
        }

    }

    // make sure there is a player alive for victory
    for(i = 0; i < MAXPLAYERS; i++)
        if(players[i].plr->inGame && players[i].health > 0)
            break;

    if(i == MAXPLAYERS)
        return;                 // no one left alive, so do not end game

    // scan the remaining thinkers to see
    // if all bosses are dead
    for(th = thinkerCap.next; th != &thinkerCap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other boss not dead
            return;
        }
    }

    // victory!
    if(gamemode == commercial)
    {
        if(gamemap == 7)
        {
            if(mo->type == MT_FATSO)
            {
                dummyLine = P_AllocDummyLine();
                P_ToXLine(dummyLine)->tag = 666;
                EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
                P_FreeDummyLine(dummyLine);
                return;
            }

            if(mo->type == MT_BABY)
            {
                dummyLine = P_AllocDummyLine();
                P_ToXLine(dummyLine)->tag = 667;
                EV_DoFloor(dummyLine, FT_RAISETOTEXTURE);
                P_FreeDummyLine(dummyLine);

                // Only activate once (rare Dead simple bug)
                bossKilled = true;
                return;
            }
        }
    }
    else
    {
        switch (gameepisode)
        {
        case 1:
            dummyLine = P_AllocDummyLine();
            P_ToXLine(dummyLine)->tag = 666;
            EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
            P_FreeDummyLine(dummyLine);
            bossKilled = true;
            return;
            break;

        case 4:
            switch (gamemap)
            {
            case 6:
                dummyLine = P_AllocDummyLine();
                P_ToXLine(dummyLine)->tag = 666;
                EV_DoFloor(dummyLine, DT_BLAZEOPEN);
                P_FreeDummyLine(dummyLine);
                bossKilled = true;
                return;
                break;

            case 8:
                dummyLine = P_AllocDummyLine();
                P_ToXLine(dummyLine)->tag = 666;
                EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
                P_FreeDummyLine(dummyLine);
                bossKilled = true;
                return;
                break;
            }
        }
    }

    G_LeaveMap(G_GetMapNumber(gameepisode, gamemap), 0, false);
}

void C_DECL A_Hoof(mobj_t *mo)
{
    // HACKAMAXIMO: Only play very loud sounds in map 8.
    S_StartSound(SFX_HOOF |
                 (gamemode != commercial &&
                  gamemap == 8 ? DDSF_NO_ATTENUATION : 0), mo);
    A_Chase(mo);
}

void C_DECL A_Metal(mobj_t *mo)
{
    // HACKAMAXIMO: Only play very loud sounds in map 8.
    S_StartSound(SFX_METAL |
                 (gamemode != commercial &&
                  gamemap == 8 ? DDSF_NO_ATTENUATION : 0), mo);
    A_Chase(mo);
}

void C_DECL A_BabyMetal(mobj_t *mo)
{
    S_StartSound(SFX_BSPWLK, mo);
    A_Chase(mo);
}

void C_DECL A_OpenShotgun2(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_DBOPN, player->plr->mo);
}

void C_DECL A_LoadShotgun2(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_DBLOAD, player->plr->mo);
}


void C_DECL A_CloseShotgun2(player_t *player, pspdef_t * psp)
{
    S_StartSound(SFX_DBCLS, player->plr->mo);
    A_ReFire(player, psp);
}

/*
 * killough 3/26/98: initialize icon landings at level startup,
 * rather than at boss wakeup, to prevent savegame-related crashes
 */
void P_SpawnBrainTargets(void)
{
    thinker_t *thinker;
    mobj_t *m;

    // find all the target spots
    for(thinker = thinkerCap.next; thinker != &thinkerCap;
        thinker = thinker->next)
    {
        if(thinker->function != P_MobjThinker)
            continue;  // not a mobj

        m = (mobj_t *) thinker;

        if(m->type == MT_SPAWNERTARGET)
        {   // killough 2/7/98: remove limit on icon landings:
            if(numBrainTargets >= numBrainTargetsAlloc)
            {
                // Do we need to alloc more targets?
                if(numBrainTargets == numBrainTargetsAlloc)
                {
                    numBrainTargetsAlloc *= 2;
                    brainTargets =
                        Z_Realloc(brainTargets,
                                  numBrainTargetsAlloc * sizeof(*brainTargets),
                                  PU_MAP);
                }
                else
                {
                    numBrainTargetsAlloc = 32;
                    brainTargets =
                        Z_Malloc(numBrainTargetsAlloc * sizeof(*brainTargets),
                                 PU_MAP, NULL);
                }
            }

            brainTargets[numBrainTargets++] = m;
        }
    }
}

void C_DECL A_BrainAwake(mobj_t *mo)
{
    S_StartSound(SFX_BOSSIT, NULL);
}

void C_DECL A_BrainPain(mobj_t *mo)
{
    S_StartSound(SFX_BOSPN, NULL);
}

void C_DECL A_BrainScream(mobj_t *mo)
{
    int     x;
    int     y;
    int     z;
    mobj_t *th;

    for(x = mo->pos[VX] - 196 * FRACUNIT; x < mo->pos[VX] + 320 * FRACUNIT;
        x += FRACUNIT * 8)
    {
        y = mo->pos[VY] - 320 * FRACUNIT;
        z = 128 + P_Random() * 2 * FRACUNIT;
        th = P_SpawnMobj3f(x, y, z, MT_ROCKET);
        th->mom[MZ] = P_Random() * 512;

        P_MobjChangeState(th, S_BRAINEXPLODE1);

        th->tics -= P_Random() & 7;
        if(th->tics < 1)
            th->tics = 1;
    }

    S_StartSound(SFX_BOSDTH, NULL);
}

void C_DECL A_BrainExplode(mobj_t *mo)
{
    int     x;
    int     y;
    int     z;
    mobj_t *th;

    x = mo->pos[VX] + (P_Random() - P_Random()) * 2048;
    y = mo->pos[VY];
    z = 128 + P_Random() * 2 * FRACUNIT;
    th = P_SpawnMobj3f(x, y, z, MT_ROCKET);
    th->mom[MZ] = P_Random() * 512;

    P_MobjChangeState(th, S_BRAINEXPLODE1);

    th->tics -= P_Random() & 7;
    if(th->tics < 1)
        th->tics = 1;
}

void C_DECL A_BrainDie(mobj_t *mo)
{
    G_LeaveMap(G_GetMapNumber(gameepisode, gamemap), 0, false);
}

void C_DECL A_BrainSpit(mobj_t *mo)
{
    mobj_t *targ;
    mobj_t *newmobj;

    if(!numBrainTargets)     // killough 4/1/98: ignore if no targets
        return;

    brain.easy ^= 1;
    if(gameskill <= SM_EASY && (!brain.easy))
        return;

    // shoot a cube at current target
    targ = brainTargets[brain.targeton++];
    brain.targeton %= numBrainTargets;

    // spawn brain missile
    newmobj = P_SpawnMissile(MT_SPAWNSHOT, mo, targ);
    if(newmobj)
    {
        newmobj->target = targ;
        newmobj->reactionTime =
            ((targ->pos[VY] - mo->pos[VY]) / newmobj->mom[MY]) / newmobj->state->tics;
    }

    S_StartSound(SFX_BOSPIT, NULL);
}

/*
 * Travelling cube sound
 */
void C_DECL A_SpawnSound(mobj_t *mo)
{
    S_StartSound(SFX_BOSCUB, mo);
    A_SpawnFly(mo);
}

void C_DECL A_SpawnFly(mobj_t *mo)
{
    mobj_t *newmobj;
    mobj_t *fog;
    mobj_t *targ;
    int     r;
    mobjtype_t type;

    if(--mo->reactionTime)
        return;                 // still flying

    targ = mo->target;

    // First spawn teleport fog.
    fog = P_SpawnMobj3f(targ->pos[VX], targ->pos[VY], targ->pos[VZ], MT_SPAWNFIRE);
    S_StartSound(SFX_TELEPT, fog);

    // Randomly select monster to spawn.
    r = P_Random();

    // Probability distribution (kind of :),
    // decreasing likelihood.
    if(r < 50)
        type = MT_TROOP;
    else if(r < 90)
        type = MT_SERGEANT;
    else if(r < 120)
        type = MT_SHADOWS;
    else if(r < 130)
        type = MT_PAIN;
    else if(r < 160)
        type = MT_HEAD;
    else if(r < 162)
        type = MT_VILE;
    else if(r < 172)
        type = MT_UNDEAD;
    else if(r < 192)
        type = MT_BABY;
    else if(r < 222)
        type = MT_FATSO;
    else if(r < 246)
        type = MT_KNIGHT;
    else
        type = MT_BRUISER;

    newmobj = P_SpawnMobj3f(targ->pos[VX], targ->pos[VY], targ->pos[VZ], type);
    if(P_LookForPlayers(newmobj, true))
        P_MobjChangeState(newmobj, newmobj->info->seeState);

    // telefrag anything in this spot
    P_TeleportMove(newmobj, newmobj->pos[VX], newmobj->pos[VY], false);

    // remove self (i.e., cube).
    P_MobjRemove(mo, false);
}

void C_DECL A_PlayerScream(mobj_t *mo)
{
    // Default death sound.
    int     sound = SFX_PLDETH;

    if((gamemode == commercial) && (mo->health < -50))
    {
        // IF THE PLAYER DIES
        // LESS THAN -50% WITHOUT GIBBING
        sound = SFX_PDIEHI;
    }

    S_StartSound(sound, mo);
}

//
// WOLFTC ACTIONS
//

//
// WOLFTC ITEM DROPS
//

//
// MACHINE GUN
//


void C_DECL A_SpawnMachineGun(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_SHOTGUN);
}

//
// WOLF3D FIRST AID KIT
//

void C_DECL A_SpawnFirstAidKit(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_FIRSTAIDKIT);
}

//
// WOLF3D AMMO CLIP
//

void C_DECL A_SpawnClip(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_AMMOCLIP);
}

//
// WOLF3D FLAMETHROWER AMMO SMALL
//

void C_DECL A_SpawnFlameTAmmo(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_FLAMETHROWERAMMOS);
}

//
// WOLF3D SILVER KEY
//

void C_DECL A_SpawnSilverKey(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_SILVERKEY);
}

//
// WOLF3D GOLD KEY
//

void C_DECL A_SpawnGoldKey(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_GOLDKEY);
}

//
// SODMP AMMO CLIP
//

void C_DECL A_SpawnSClip(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_LOSTAMMOCLIP);
}

//
// 3 SODMP AMMO CLIP
//

void C_DECL A_Spawn3SClip(mobj_t *actor)
{
    mobj_t *mo;
    mobj_t *mo2;
    mobj_t *mo3;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_LOSTAMMOCLIP);
    mo2 = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_LOSTAMMOCLIP);
    mo3 = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_LOSTAMMOCLIP);
}

//
// MACHINE GUN AND SODMP AMMOCLIP
//

void C_DECL A_SpawnMachineGunSClip(mobj_t *actor)
{
    mobj_t *mo;
    mobj_t *mo2;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_SHOTGUN);
    mo2 = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_LOSTAMMOCLIP);
}

//
// SODMP FLAMETHROWER AMMO SMALL
//

void C_DECL A_SpawnSFlameTAmmo(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_LOSTFLAMETHROWERAMMOS);
}

//
// SODMP SILVER KEY
//

void C_DECL A_SpawnSSilverKey(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_LOSTSILVERKEY);
}

//
// SODMP GOLD KEY
//

void C_DECL A_SpawnSGoldKey(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_LOSTGOLDKEY);
}

//
// ALPHA AMMO CLIP
//

void C_DECL A_SpawnAClip(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_ALPHAAMMOCLIP);
}

//
// CATACOMB YELLOW KEY
//

void C_DECL A_SpawnCYellowKey(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATAYELLOWKEY);
}

//
// OMS AMMO CLIP
//

void C_DECL A_SpawnOClip(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_OMSAMMOCLIP);
}

//
// IST AMMO CLIP
//

void C_DECL A_SpawnIClip(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_ISTAMMOCLIP);
}

//
// Uranus AMMO CLIP
//

void C_DECL A_SpawnUClip(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_UAMMOCLIP);
}

//
// IST FLAMETHROWER AMMO SMALL
//

void C_DECL A_SpawnIFlameTAmmo(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_ISTFLAMETHROWERAMMOS);
}

//
// WOLFTC BADGUYS
//

//
// BAD GUY BULLET ATTACKS
//

//
// STANDARD BULLET ATTACK
//

void C_DECL A_WolfBullet(mobj_t *actor)
{
    int         angle, bangle, damage;
    float       slope;

    if(!actor->target)
        return;

    S_StartSound(SFX_BGYFIR, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random() - P_Random()) << 16);
    damage = ((P_Random() % 10) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

//
// BOSS BULLET ATTACK
//

void C_DECL A_BossBullet(mobj_t *actor)
{
    int         angle, bangle, damage;
    float       slope;

    if(!actor->target)
        return;

    S_StartSound(SFX_BOSFIR, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random() - P_Random()) << 18);
    damage = ((P_Random() % 10) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

//
// SS GUARD BULLET ATTACK (CposAttack with different sound)
//

void C_DECL A_SSAttack(mobj_t *actor)
{
    int         angle, bangle, damage;
    float       slope;

    if(!actor->target)
        return;

    S_StartSound(SFX_SSGFIR, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random() - P_Random()) << 18);
    damage = ((P_Random() % 10) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

/**
 * Officer: Range attack.
 */
void C_DECL A_OfficerAttack(mobj_t *actor)
{
    int         angle, bangle, damage;
    float       slope;

    if(!actor->target)
        return;

    S_StartSound(SFX_BGYFIR, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random() - P_Random()) << 14);
    damage = ((P_Random() % 10) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

/**
 * Elite Guard: Range attack.
 */
void C_DECL A_EliteGuardAttack(mobj_t *actor)
{
    int         angle, bangle, damage;
    float       slope;

    if(!actor->target)
        return;

    S_StartSound(SFX_SSGFIR, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random() - P_Random()) << 16);
    damage = ((P_Random() % 10) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

/**
 * SS Guard (SODMP): Range attack.
 *
 * \note Same as CposAttack with different sound.
 */
void C_DECL A_SODMPSSAttack(mobj_t *actor)
{
    int         angle, bangle, damage;
    float       slope;

    if(!actor->target)
        return;

    S_StartSound(SFX_LSSFIR, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random() - P_Random()) << 18);
    damage = ((P_Random() % 10) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

/**
 * Elite Guard (SODMP): Com range attack.
 */
void C_DECL A_SODMPEliteGuardAttack(mobj_t *actor)
{
    int         angle, bangle, damage;
    float       slope;

    if(!actor->target)
        return;

    S_StartSound(SFX_LSSFIR, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random() - P_Random()) << 16);
    damage = ((P_Random() % 10) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

/**
 * SS Guard (OMS2): Range attack.
 *
 * \note Same as CposAttack with different sound.
 */
void C_DECL A_OMS2SSAttack(mobj_t *actor)
{
    int         angle, bangle, damage;
    float       slope;

    if(!actor->target)
        return;

    S_StartSound(SFX_OMACHI, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random() - P_Random()) << 18);
    damage = ((P_Random() % 10) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

/**
 * Silver Fox (OMS): Range attack.
 *
 * \note Same as SposAttack with different sound.
 */
void C_DECL A_FoxAttack(mobj_t *actor)
{
    int         i, angle, bangle, damage;
    float       slope;

    if(!actor->target)
        return;

    S_StartSound(SFX_OREVOL, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    for(i = 0; i < 3; ++i)
    {
        angle = bangle + ((P_Random() - P_Random()) << 20);
        damage = ((P_Random() % 10) + 1) * 3;
        P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
    }
}

/**
 * Sael Chaingunner: Range attack.
 */
void C_DECL A_ChaingunZombieAttack(mobj_t *actor)
{
    int         angle, bangle, damage;
    float       slope;

    if(!actor->target)
        return;

    S_StartSound(SFX_SZOFIR, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random() - P_Random()) << 18);
    damage = ((P_Random() % 10) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

/**
 * Bad Guy: Range attack (rocket).
 */
void C_DECL A_WRocketAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    P_SpawnMissile(MT_WROCKET, actor, actor->target);
}

void C_DECL A_Explosion(mobj_t *thingy)
{
    P_RadiusAttack(thingy, thingy->target, 128, 127);
}

/**
 * Bad Guy (SODMP): Range attack (rocket).
 */
void C_DECL A_LRocketAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    P_SpawnMissile(MT_LROCKET, actor, actor->target);
}

/**
 * Schabbs Missile:
 * So you may get "pricked" by a narrowly missing syringe.
 */
void C_DECL A_Graze(mobj_t *thingy)
{
    P_RadiusAttack(thingy, thingy->target, 32, 31);
}

/**
 * Schabbs Missile:
 * So you may be cut by "glass" from an exploding syringe.
 */
void C_DECL A_Shatter(mobj_t *thingy)
{
    P_RadiusAttack(thingy, thingy->target, 48, 47);
}

/**
 * Schabbs: Range attack.
 */
void C_DECL A_SchabbsAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    P_SpawnMissile(MT_SCHABBSPROJECTILE, actor, actor->target);
}

//
// FAKE HITLER PROJECTILE
//

//
// A_Fsplash (A little splash damage)
//
void C_DECL A_Fsplash(mobj_t *thingy)
{
    P_RadiusAttack(thingy, thingy->target, 16, 15);
}

//
// FAKE HITLER MISSILE
//

void C_DECL A_FakeHitlerAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // launch a missile
    P_SpawnMissile(MT_FAKEHITLERPROJECTILE, actor, actor->target);
}

//
// WOLF + SODMP FLAME THROWER GUARD PROJECTILE
//

void C_DECL A_BGFlameAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // launch a missile
    P_SpawnMissile(MT_FLAMEGUARDPROJECTILE, actor, actor->target);
}

//
// DEATH KNIGHT ATTACK 1
//

void C_DECL A_DeathKnightAttack1(mobj_t *actor)
{
    mobj_t *mo;
    mobj_t *mo2;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    actor->pos[VX] += 24 * FRACUNIT;  // Right Missile
    mo = P_SpawnMissile(MT_DKMISSILE, actor, actor->target);
    actor->pos[VX] -= 48 * FRACUNIT;  // Left Missile
    mo2 = P_SpawnMissile(MT_DKMISSILE, actor, actor->target);
    actor->pos[VX] += 24 * FRACUNIT;  // back to original position
    P_NewChaseDir(actor);
}

//
// DEATH KNIGHT ATTACK 2
//

void C_DECL A_DeathKnightAttack2(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    actor->pos[VZ] += 8 * FRACUNIT;   // Right Missile B
    P_SpawnMissile(MT_WROCKET, actor, actor->target);
    P_SpawnMissile(MT_WROCKET, actor, actor->target);
    actor->pos[VZ] -= 8 * FRACUNIT;   // back to original position
    P_NewChaseDir(actor);
}

//
// ANGEL OF DEATH ATTACK 1
//

void C_DECL A_AngelAttack1(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // launch a missile
    P_SpawnMissile(MT_ANGMISSILE1, actor, actor->target);
}

//
// ANGEL OF DEATH ATTACK 2
//

void C_DECL A_AngelAttack2(mobj_t *actor)
{
    mobj_t *mo;
    int     an;

    A_FaceTarget(actor);
    // Change direction  to ...
    actor->angle += FATSPREAD;
    P_SpawnMissile(MT_ANGMISSILE2, actor, actor->target);

    mo = P_SpawnMissile(MT_ANGMISSILE2, actor, actor->target);
    if(mo)
    {
        mo->angle += FATSPREAD;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = FixedMul(mo->info->speed, finecosine[an]);
        mo->mom[MY] = FixedMul(mo->info->speed, finesine[an]);
    }

    mo = P_SpawnMissile(MT_ANGMISSILE2, actor, actor->target);
    if(mo)
    {
        mo->angle -= FATSPREAD;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = FixedMul(mo->info->speed, finecosine[an]);
        mo->mom[MY] = FixedMul(mo->info->speed, finesine[an]);
    }
}

//
// ROBOT PROJECTILE
//

//
// TRACKING CODE (same as A_Tracer excpet always tracks)
//

void C_DECL A_TrackingAlways(mobj_t *actor)
{
    angle_t     exact;
    float       dist, slope;
    mobj_t     *dest, *th;

    // spawn a puff of smoke behind the rocket
    P_SpawnCustomPuff(MT_ROBOTMISSILEPUFF, actor->pos[VX], actor->pos[VY], actor->pos[VZ]);

    th = P_SpawnMobj3f(MT_ROBOTMISSILESMOKE,
                       actor->pos[VX] - actor->mom[MX],
                       actor->pos[VY] - actor->mom[MY],
                       actor->pos[VZ]);
    th->mom[MZ] = 1;

    // Adjust direction.
    dest = actor->tracer;

    if(!dest || dest->health <= 0)
        return;

    // Change angle.
    exact = R_PointToAngle2(actor->pos[VX], actor->pos[VY], dest->pos[VX], dest->pos[VY]);

    if(exact != actor->angle)
    {
        if(exact - actor->angle > 0x80000000)
        {
            actor->angle -= TRACEANGLE;
            if(exact - actor->angle < 0x80000000)
                actor->angle = exact;
        }
        else
        {
            actor->angle += TRACEANGLE;
            if(exact - actor->angle > 0x80000000)
                actor->angle = exact;
        }
    }

    exact = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = FIX2FLT(FixedMul(actor->info->speed, finecosine[exact]));
    actor->mom[MY] = FIX2FLT(FixedMul(actor->info->speed, finesine[exact]));

    // Change slope.
    dist = P_ApproxDistance(dest->pos[VX] - actor->pos[VX], dest->pos[VY] - actor->pos[VY]);

    dist /= FIX2FLT(actor->info->speed);
    if(dist < 1)
        dist = 1;
    slope = (dest->pos[VZ] + 40 - actor->pos[VZ]) / dist;

    if(slope < actor->mom[MZ])
        actor->mom[MZ] -= 1 / 8;
    else
        actor->mom[MZ] += 1 / 8;
}

//
// ROBOT MISSILE
//

void C_DECL A_RobotAttack(mobj_t *actor)
{
    mobj_t *mo;

    if(!actor->target)
        return;

    A_FaceTarget(actor);

    actor->pos[VZ] += 16 * FRACUNIT;  // so missile spawns higher
    mo = P_SpawnMissile(MT_ROBOTPROJECTILE, actor, actor->target);
    actor->pos[VZ] -= 16 * FRACUNIT;  // back to normal

    if(mo)
    {
        mo->pos[VX] += mo->mom[MX];
        mo->pos[VY] += mo->mom[MY];
        mo->tracer = actor->target;
    }
}

//
// DEVIL INCARNATE ATTACK 1
//

void C_DECL A_DevilAttack1(mobj_t *actor)
{
    P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_SPIRIT);
}

//
// DEVIL INCARNATE ATTACK 2
//

void C_DECL A_DevilAttack2(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // launch a missile
    P_SpawnMissile(MT_DEVMISSILE1, actor, actor->target);
}

//
// DEVIL INCARNATE ATTACK 3
//

void C_DECL A_DevilAttack3(mobj_t *actor)
{
    mobj_t *mo;
    int     an;

    A_FaceTarget(actor);
    // Change direction  to ...
    actor->angle += FATSPREAD;
    P_SpawnMissile(MT_DEVMISSILE2, actor, actor->target);

    mo = P_SpawnMissile(MT_DEVMISSILE2, actor, actor->target);
    if(mo)
    {
        mo->angle += FATSPREAD;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = FixedMul(mo->info->speed, finecosine[an]);
        mo->mom[MY] = FixedMul(mo->info->speed, finesine[an]);
    }

    mo = P_SpawnMissile(MT_DEVMISSILE2, actor, actor->target);
    if(mo)
    {
        mo->angle -= FATSPREAD;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = FixedMul(mo->info->speed, finecosine[an]);
        mo->mom[MY] = FixedMul(mo->info->speed, finesine[an]);
    }
}

//
// MISSILE BAT MISSILE
//

void C_DECL A_MBatAttack(mobj_t *actor)
{
    mobj_t *mo;

    if(!actor->target)
        return;

    A_FaceTarget(actor);

    actor->pos[VZ] -= 24 * FRACUNIT; // Since Bats start at ceiling height
    mo = P_SpawnMissile(MT_ROBOTPROJECTILE, actor, actor->target);
    actor->pos[VZ] += 24 * FRACUNIT; // Back to Normal
    if(mo)
    {
        mo->pos[VX] += mo->mom[MX];
        mo->pos[VY] += mo->mom[MY];
        mo->tracer = actor->target;
    }

    actor->pos[VZ] -= 24 * FRACUNIT; // Since Bats start at ceiling height
    mo = P_SpawnMissile(MT_ROBOTPROJECTILE, actor, actor->target);
    actor->pos[VZ] += 24 * FRACUNIT; // Back to Normal
    if(mo)
    {
        mo->pos[VX] += mo->mom[MX];
        mo->pos[VY] += mo->mom[MY];
        mo->tracer = actor->target;
    }
}

//
// CATACOMB PROJECTILE 1
//

void C_DECL A_CatMissle1(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // launch a missile
    P_SpawnMissile(MT_CATAMISSILE1, actor, actor->target);
    P_NewChaseDir(actor);
}

//
// CATACOMB PROJECTILE 2
//

void C_DECL A_CatMissle2(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // launch a missile
    P_SpawnMissile(MT_CATAMISSILE2, actor, actor->target);
}

//
// NEMESIS ATTACK 1
//

void C_DECL A_NemesisAttack(mobj_t *actor)
{
    mobj_t *mo;

    if(!actor->target)
        return;

    A_FaceTarget(actor);

    actor->pos[VZ] += 16 * FRACUNIT;  // top missile
    mo = P_SpawnMissile(MT_NEMESISMISSILE, actor, actor->target);
    actor->pos[VZ] -= 16 * FRACUNIT;  // bottom missle
    if(mo)
    {
        mo->pos[VX] += mo->mom[MX];
        mo->pos[VY] += mo->mom[MY];
        mo->tracer = actor->target;
    }

    actor->pos[VZ] -= 16 * FRACUNIT;  // top missile
    mo = P_SpawnMissile(MT_NEMESISMISSILE, actor, actor->target);
    actor->pos[VZ] += 16 * FRACUNIT;  // right missile A
    if(mo)
    {
        mo->pos[VX] += mo->mom[MX];
        mo->pos[VY] += mo->mom[MY];
        mo->tracer = actor->target;
    }

    mo = P_SpawnMissile(MT_NEMESISMISSILE, actor, actor->target);
    if(mo)
    {
        mo->pos[VX] += mo->mom[MX];
        mo->pos[VY] += mo->mom[MY];
        mo->tracer = actor->target;
    }

    mo = P_SpawnMissile(MT_NEMESISMISSILE, actor, actor->target);
    if(mo)
    {
        mo->pos[VX] += mo->mom[MX];
        mo->pos[VY] += mo->mom[MY];
        mo->tracer = actor->target;
    }
}

void C_DECL A_Tracking(mobj_t *actor)
{
    angle_t     exact;
    float       dist, slope;
    mobj_t     *dest, *th;

    if(gametic & 3)
        return;

    // Spawn a puff of smoke behind the rocket.
    P_SpawnCustomPuff(MT_ROBOTMISSILEPUFF,
                      actor->pos[VX], actor->pos[VY], actor->pos[VZ]);

    th = P_SpawnMobj3f(MT_ROBOTMISSILESMOKE,
                       actor->pos[VX] - actor->mom[MX],
                       actor->pos[VY] - actor->mom[MY],
                       actor->pos[VZ]);

    th->mom[MZ] = 1;
    th->tics -= P_Random() & 3;
    if(th->tics < 1)
        th->tics = 1;

    // Adjust direction.
    dest = actor->tracer;

    if(!dest || dest->health <= 0)
        return;

    // Change angle.
    exact = R_PointToAngle2(actor->pos[VX], actor->pos[VY], dest->pos[VX], dest->pos[VY]);

    if(exact != actor->angle)
    {
        if(exact - actor->angle > 0x80000000)
        {
            actor->angle -= TRACEANGLE;
            if(exact - actor->angle < 0x80000000)
                actor->angle = exact;
        }
        else
        {
            actor->angle += TRACEANGLE;
            if(exact - actor->angle > 0x80000000)
                actor->angle = exact;
        }
    }

    exact = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = FIX2FLT(FixedMul(actor->info->speed, finecosine[exact]));
    actor->mom[MY] = FIX2FLT(FixedMul(actor->info->speed, finesine[exact]));

    // Change slope.
    dist = P_ApproxDistance(dest->pos[VX] - actor->pos[VX], dest->pos[VY] - actor->pos[VY]);

    dist /= FIX2FLT(actor->info->speed);
    if(dist < 1)
        dist = 1;
    slope = (dest->pos[VZ] + 40 - actor->pos[VZ]) / dist;

    if(slope < actor->mom[MZ])
        actor->mom[MZ] -= 1 / 8;
    else
        actor->mom[MZ] += 1 / 8;
}

//
// OMS MAD DOCTOR PROJECTILE
//

void C_DECL A_MadDocAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // launch a missile
    P_SpawnMissile(MT_MADDOCPROJECTILE, actor, actor->target);
}

//
// OMS BIO BLASTER ATTACK
//

void C_DECL A_BioBlasterAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // DJS - NOTE:
    // Vermil, if you want to change X/Y spawn location of a projectile
    // you must rotate the point about the center of the mobj and not
    // just a linear X/Y adjustment. Otherwise, when the mobj rotates
    // the spawn location will stay fixed as if it were facing EAST.

    actor->pos[VZ] += 12 * FRACUNIT;  // top left missile
    P_SpawnMissile(MT_BIOBLASTERPROJECTILE, actor, actor->target);
    actor->pos[VZ] -= 12 * FRACUNIT;

    actor->pos[VZ] += 12 * FRACUNIT;  // top right missile
    P_SpawnMissile(MT_BIOBLASTERPROJECTILE, actor, actor->target);
    actor->pos[VZ] -= 12 * FRACUNIT;

    actor->pos[VZ] += 4 * FRACUNIT;   // middle left missile
    P_SpawnMissile(MT_BIOBLASTERPROJECTILE, actor, actor->target);
    actor->pos[VZ] -= 4 * FRACUNIT;

    actor->pos[VZ] += 4 * FRACUNIT;   // middle right missile
    P_SpawnMissile(MT_BIOBLASTERPROJECTILE, actor, actor->target);
    actor->pos[VZ] -= 4 * FRACUNIT;

    actor->pos[VZ] -= 4 * FRACUNIT;   // bottom left missile
    P_SpawnMissile(MT_BIOBLASTERPROJECTILE, actor, actor->target);
    actor->pos[VZ] += 4 * FRACUNIT;

    actor->pos[VZ] -= 4 * FRACUNIT;   // bottom right missile
    P_SpawnMissile(MT_BIOBLASTERPROJECTILE, actor, actor->target);
    actor->pos[VZ] += 4 * FRACUNIT;
}

//
// A_ChimeraAttack1 (Tentacles)
//
void C_DECL A_ChimeraAttack1(mobj_t *actor)
{
    mobj_t *fog;

    A_FaceTarget(actor);

    fog =
        P_SpawnMobj3f(actor->target->pos[VX], actor->target->pos[VX], actor->target->pos[VZ],
                    MT_FIRE);
    actor->tracer = fog;
    fog->target = actor;
    fog->tracer = actor->target;
}

//
// OMS2 BALROG ATTACK
//

void C_DECL A_BalrogAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // launch a missile
    P_SpawnMissile(MT_BALROGPROJECTILE, actor, actor->target);
}

//
// OMEGA MUTANT ATTACKS
//

void C_DECL A_OmegaAttack1(mobj_t *actor)
{
    mobj_t *mo;
    int     an;

    A_FaceTarget(actor);
    // Change direction  to ...
    actor->angle += FATSPREAD;
    P_SpawnMissile(MT_DKMISSILE, actor, actor->target);

    mo = P_SpawnMissile(MT_DKMISSILE, actor, actor->target);
    if(mo)
    {
        mo->angle += FATSPREAD;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = FixedMul(mo->info->speed, finecosine[an]);
        mo->mom[MY] = FixedMul(mo->info->speed, finesine[an]);
    }
}

void C_DECL A_OmegaAttack2(mobj_t *actor)
{
    mobj_t *mo;
    int     an;

    A_FaceTarget(actor);
    // Now here choose opposite deviation.
    actor->angle -= FATSPREAD;
    P_SpawnMissile(MT_DKMISSILE, actor, actor->target);

    mo = P_SpawnMissile(MT_DKMISSILE, actor, actor->target);
    if(mo)
    {
        mo->angle -= FATSPREAD * 2;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = FixedMul(mo->info->speed, finecosine[an]);
        mo->mom[MY] = FixedMul(mo->info->speed, finesine[an]);
    }
}

void C_DECL A_OmegaAttack3(mobj_t *actor)
{
    mobj_t *mo;
    int     an;

    A_FaceTarget(actor);
    mo = P_SpawnMissile(MT_DKMISSILE, actor, actor->target);
    if(mo)
    {
        mo->angle -= FATSPREAD / 2;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = FixedMul(mo->info->speed, finecosine[an]);
        mo->mom[MY] = FixedMul(mo->info->speed, finesine[an]);
    }

    mo = P_SpawnMissile(MT_DKMISSILE, actor, actor->target);
    if(mo)
    {
        mo->angle += FATSPREAD / 2;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = FixedMul(mo->info->speed, finecosine[an]);
        mo->mom[MY] = FixedMul(mo->info->speed, finesine[an]);
    }
}

//
// DRAKE ATTACKS
//

void C_DECL A_DrakeAttack1(mobj_t *actor)
{
    mobj_t *mo;
    int     an;

    A_FaceTarget(actor);
    // Change direction  to ...
    actor->angle += FATSPREAD;

    actor->pos[VZ] += 160 * FRACUNIT; // Up
    P_SpawnMissile(MT_DRAKEMISSILE, actor, actor->target);
    actor->pos[VZ] -= 160 * FRACUNIT; // back to original position

    actor->pos[VZ] += 160 * FRACUNIT; // Up
    mo = P_SpawnMissile(MT_DRAKEMISSILE, actor, actor->target);
    actor->pos[VZ] -= 160 * FRACUNIT; // back to original position
    if(mo)
    {
        mo->angle += FATSPREAD;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = FixedMul(mo->info->speed, finecosine[an]);
        mo->mom[MY] = FixedMul(mo->info->speed, finesine[an]);
    }
}

void C_DECL A_DrakeAttack2(mobj_t *actor)
{
    mobj_t *mo;
    int     an;

    A_FaceTarget(actor);

    // Now here choose opposite deviation.
    actor->angle -= FATSPREAD;

    actor->pos[VZ] += 160 * FRACUNIT; // Up
    P_SpawnMissile(MT_DRAKEMISSILE, actor, actor->target);
    actor->pos[VZ] -= 160 * FRACUNIT; // back to original position

    actor->pos[VZ] += 160 * FRACUNIT; // Up
    mo = P_SpawnMissile(MT_DRAKEMISSILE, actor, actor->target);
    actor->pos[VZ] -= 160 * FRACUNIT; // back to original position
    if(mo)
    {
        mo->angle -= FATSPREAD * 2;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = FixedMul(mo->info->speed, finecosine[an]);
        mo->mom[MY] = FixedMul(mo->info->speed, finesine[an]);
    }
}

void C_DECL A_DrakeAttack3(mobj_t *actor)
{
    mobj_t *mo;
    int     an;

    A_FaceTarget(actor);

    actor->pos[VZ] += 160 * FRACUNIT; // Up
    mo = P_SpawnMissile(MT_DRAKEMISSILE, actor, actor->target);
    actor->pos[VZ] -= 160 * FRACUNIT; // back to original position
    if(mo)
    {
        mo->angle -= FATSPREAD / 2;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = FixedMul(mo->info->speed, finecosine[an]);
        mo->mom[MY] = FixedMul(mo->info->speed, finesine[an]);
    }


    mo = P_SpawnMissile(MT_DRAKEMISSILE, actor, actor->target);
    actor->pos[VZ] -= 160 * FRACUNIT; // back to original position
    if(mo)
    {
        mo->angle += FATSPREAD / 2;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = FixedMul(mo->info->speed, finecosine[an]);
        mo->mom[MY] = FixedMul(mo->info->speed, finesine[an]);
    }
}

//
// SAEL STALKER PROJECTILE
//

void C_DECL A_StalkerAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // launch a missile
    P_SpawnMissile(MT_STALKERPROJECTILE, actor, actor->target);
}

//
// SAEL HELLGUARD PROJECTILE
//

void C_DECL A_HellGuardAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // launch a missile
    P_SpawnMissile(MT_HELLGUARDMISSILE, actor, actor->target);
}

//
// SAEL SCHABBS DEMON PROJECTILE
//

void C_DECL A_SchabbsDemonAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // launch a missile
    P_SpawnMissile(MT_SCHABBSDEMONPROJECTILE, actor, actor->target);
}

//
// BAD GUY MELEE ATTACKS
//

void C_DECL A_NormalMelee(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = ((P_Random() % 15) + 1) * 4;
        P_DamageMobj(actor->target, actor, actor, damage);
    }
}

//
// MUTANT MELEE ATTACK (Half Damage Of Sarg Attack)
//

void C_DECL A_LightMelee(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = ((P_Random() % 10) + 1) * 4;
        P_DamageMobj(actor->target, actor, actor, damage);
    }
}

//
// DRAIN ATTACK (Health "Drain")
//

void C_DECL A_DrainAttack(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = ((P_Random() % 1) + 1) * 2;
        P_DamageMobj(actor->target, actor, actor, damage);
    }
}

//
// GHOST DRAIN ATTACK (Ghost, Lost Ghost, Green Mist + Respawned Green Mist)
//

void C_DECL A_GhostDrainAttack(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = ((P_Random() % 1) + 1) * 2;
        P_DamageMobj(actor->target, actor, actor, damage);
    }
    else
    {
        P_MobjChangeState(actor, actor->info->seeState);
    }
}

//
// RGHOST DRAIN ATTACK (Respawned Ghost)
//

void C_DECL A_RGhostDrainAttack(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = ((P_Random() % 1) + 1) * 2;
        P_DamageMobj(actor->target, actor, actor, damage);
    }
    else
    {
        P_MobjChangeState(actor, S_GBOS_RUN1);
    }
}

//
// LOST RGHOST DRAIN ATTACK (Respawned Lost Ghost)
//

void C_DECL A_RLGhostDrainAttack(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = ((P_Random() % 1) + 1) * 2;
        P_DamageMobj(actor->target, actor, actor, damage);
    }
    else
    {
        P_MobjChangeState(actor, S_LGBS_RUN1);
    }
}

//
// TROLL MELEE ATTACK
//

void C_DECL A_TrollMelee(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = ((P_Random() % 15) + 1) * 4;
        P_DamageMobj(actor->target, actor, actor, damage);
        S_StartSound(SFX_CTROLH, actor);
    }
}

//
// RED DEMON MELEE ATTACK
//

void C_DECL A_HeavyMelee(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = ((P_Random() % 28) + 1) * 4;
        P_DamageMobj(actor->target, actor, actor, damage);
    }
}

//
// BAD GUY MOVEMENT ACTIONS
//

//
// Metal Feet Walk
//

void C_DECL A_MetalWalk(mobj_t *mo)
{
    S_StartSound(SFX_METWLK, mo);
    A_Chase(mo);
}

//
// Tread Move Noisy
//

void C_DECL A_TreadMoveN(mobj_t *mo)
{
    S_StartSound(SFX_ALORRY, mo);
    A_Chase(mo);
}

//
// Tread Moves Smooth
//

void C_DECL A_TreadMoveS(mobj_t *mo)
{
    S_StartSound(SFX_TRMOVS, mo);
    A_Chase(mo);
}

//
// A_WaterTrollSwim (A_Chase Minus Melee Attack Check)
//

void C_DECL A_WaterTrollSwim(mobj_t *actor)
{
    int     delta;

    if(actor->reactionTime)
        actor->reactionTime--;

    // modify target threshold
    if(P_Random() < 128)
    if(actor->threshold)
    {
        if(!actor->target || actor->target->health <= 0)
        {
            actor->threshold = 0;
        }
        else
            actor->threshold--;
    }

    // turn towards movement direction if not there yet
    if(actor->moveDir < DI_NODIR)
    {
        actor->angle &= (7 << 29);
        delta = actor->angle - (actor->moveDir << 29);

        if(delta > 0)
            actor->angle -= ANG90 / 2;
        else if(delta < 0)
            actor->angle += ANG90 / 2;
    }

    if(!actor->target || !(actor->target->flags & MF_SHOOTABLE))
    {
        // look for a new target
        if(P_LookForPlayers(actor, true))
            return;             // got a new target

        P_MobjChangeState(actor, actor->info->spawnState);
        return;
    }

    // do not attack twice in a row
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(gameskill != SM_NIGHTMARE && !fastparm)
            P_NewChaseDir(actor);
        return;
    }

    // check for missile attack
    if(actor->info->missileState)
    {
        if(gameskill < SM_NIGHTMARE && !fastparm && actor->moveCount)
        {
            goto nomissile;
        }

        if(!P_CheckMissileRange(actor))
            goto nomissile;

        P_MobjChangeState(actor, actor->info->missileState);
        actor->flags |= MF_JUSTATTACKED;
        return;
    }

    // ?
  nomissile:
    // possibly choose another target
    if(IS_NETGAME && !actor->threshold && !P_CheckSight(actor, actor->target))
    {
        if(P_LookForPlayers(actor, true))
            return;             // got a new target
    }

    // chase towards player
    // killough $dropoff_fix
    if(--actor->moveCount < 0 || !P_Move(actor, false))
    {
        P_NewChaseDir(actor);
    }

    // make active sound
    if(actor->info->activeSound && P_Random() < 3)
    {
        S_StartSound(actor->info->activeSound, actor);
    }
}

//
// A_WaterTrollChase (A_Chase Minus Ranged Attack Check)
//

void C_DECL A_WaterTrollChase(mobj_t *actor)
{
    int     delta;

    if(actor->reactionTime)
        actor->reactionTime--;

    // modify target threshold
    if(actor->threshold)
    {
        if(!actor->target || actor->target->health <= 0)
        {
            actor->threshold = 0;
        }
        else
            actor->threshold--;
    }

    // turn towards movement direction if not there yet
    if(actor->moveDir < DI_NODIR)
    {
        actor->angle &= (7 << 29);
        delta = actor->angle - (actor->moveDir << 29);

        if(delta > 0)
            actor->angle -= ANG90 / 2;
        else if(delta < 0)
            actor->angle += ANG90 / 2;
    }

    if(!actor->target || !(actor->target->flags & MF_SHOOTABLE))
    {
        // look for a new target
        if(P_LookForPlayers(actor, true))
            return;             // got a new target

        P_MobjChangeState(actor, actor->info->spawnState);
        return;
    }

    // do not attack twice in a row
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(gameskill != SM_NIGHTMARE && !fastparm)
            P_NewChaseDir(actor);
        return;
    }

    // check for melee attack
    if(actor->info->meleeState && P_CheckMeleeRange(actor))
    {
        if(actor->info->attackSound)
            S_StartSound(actor->info->attackSound, actor);

        P_MobjChangeState(actor, actor->info->meleeState);
        return;
    }

    // possibly choose another target
    if(IS_NETGAME && !actor->threshold && !P_CheckSight(actor, actor->target))
    {
        if(P_LookForPlayers(actor, true))
            return;             // got a new target
    }

    // chase towards player
    // killough $dropoff_fix
    if(--actor->moveCount < 0 || !P_Move(actor, false))
    {
        P_NewChaseDir(actor);
    }

    // make active sound
    if(actor->info->activeSound && P_Random() < 3)
    {
        S_StartSound(actor->info->activeSound, actor);
    }
}

//
// A_ChaseNA (Same as A_Chase minus attack checks)
//
void C_DECL A_ChaseNA(mobj_t *actor)
{
    int     delta;

    if(actor->reactionTime)
        actor->reactionTime--;

    // modify target threshold
    if(actor->threshold)
    {
        if(!actor->target || actor->target->health <= 0)
        {
            actor->threshold = 0;
        }
        else
            actor->threshold--;
    }

    // turn towards movement direction if not there yet
    if(actor->moveDir < DI_NODIR)
    {
        actor->angle &= (7 << 29);
        delta = actor->angle - (actor->moveDir << 29);

        if(delta > 0)
            actor->angle -= ANG90 / 2;
        else if(delta < 0)
            actor->angle += ANG90 / 2;
    }

    if(!actor->target || !(actor->target->flags & MF_SHOOTABLE))
    {
        // look for a new target
        if(P_LookForPlayers(actor, true))
            return;             // got a new target

        P_MobjChangeState(actor, actor->info->spawnState);
        return;
    }

    // do not attack twice in a row
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(gameskill != SM_NIGHTMARE && !fastparm)
            P_NewChaseDir(actor);
        return;
    }

    // chase towards player
    // killough $dropoff_fix
    if(--actor->moveCount < 0 || !P_Move(actor, false))
    {
        P_NewChaseDir(actor);
    }

    // make active sound
    if(actor->info->activeSound && P_Random() < 3)
    {
        S_StartSound(actor->info->activeSound, actor);
    }
}

//
// BAD GUY "SPAWN" ACTIONS
//

//
// HITLER APPEAR
//
void C_DECL A_Hitler(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_HITLER);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// SPECTRE STAGE 2 RETURN
//
void C_DECL A_SpectreRespawn(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_GHOSTR);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// SODMP SPECTRE STAGE 2 RETURN
//
void C_DECL A_LSpectreRespawn(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_LOSTGHOSTR);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// SODMP GREENMIST SPLIT
//
void C_DECL A_LGreenMistSplit(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_GREENMISTR);
    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_GREENMISTR);
}

//
// NEW SKELETON
//
void C_DECL A_NewSkeleton(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATASKELETONR);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// NEW SKELETON2
//
void C_DECL A_NewSkeleton2(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATASKELETONR2);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// SHADOW NEMESIS RETURN
//
void C_DECL A_ShadowNemRespawn(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATASHADOWNEMESISR);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// CHIMERA STAGE 2
//
void C_DECL A_ChimeraStage2(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CHIMERASTAGE2);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// STATE CHOOSING ACTIONS
//

//
// FatFace Chose Attack
//
void C_DECL A_GeneralDecide(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_FATFACE_ATK1_1);
    }
    else
    {
        P_MobjChangeState(actor, S_FATFACE_ATK2_1);
    }
}

//
// Willhelm Chose Attack
//
void C_DECL A_WillDecide(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_WILL_ATK1_1);
    }
    else
    {
        P_MobjChangeState(actor, S_WILL_ATK2_1);
    }
}

//
// Death Knight Chose Attack
//
void C_DECL A_DeathKnightDecide(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_ABOS_ATK1_1);
    }
    else
    {
        P_MobjChangeState(actor, S_ABOS_ATK2_1);
    }
}

//
// Angel Of Death Chose Attack
//
void C_DECL A_AngelDecide(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_ANG_ATK2_1);
    }
    else
    {
        P_MobjChangeState(actor, S_ANG_ATK3_1);
    }
}

//
// Angel Triple Fireball Attack Decide
//
void C_DECL A_AngelAttack1Continue1(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_ANG_ATK1_6);
    }
    else
    {
        P_MobjChangeState(actor, S_ANG_ATK2_5);
    }
}

void C_DECL A_AngelAttack1Continue2(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_ANG_ATK1_6);
    }
    else
    {
        P_MobjChangeState(actor, S_ANG_ATK2_7);
    }
}

//
// QuarkBlitz Chose Attack
//
void C_DECL A_QuarkDecide(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_QUARK_ATK1_1);
    }
    else
    {
        P_MobjChangeState(actor, S_QUARK_ATK2_1);
    }
}

//
// Robot Chose Attack
//
void C_DECL A_RobotDecide(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_ROBOT_ATK1_1);
    }
    else
    {
        P_MobjChangeState(actor, S_ROBOT_ATK2_1);
    }
}

//
// Devil Incarnate Chose Attack
//
void C_DECL A_DevilDecide(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_DEVIL_ATK1_1);
    }
    else
    {
        P_MobjChangeState(actor, S_DEVIL_ATK2_1);
    }
}


//
// Devil Triple Fireball Attack Decide
//
void C_DECL A_DevilAttack2Continue1(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_DEVIL_ATK1_6);
    }
    else
    {
        P_MobjChangeState(actor, S_DEVIL_ATK2_5);
    }
}

void C_DECL A_DevilAttack2Continue2(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_DEVIL_ATK1_6);
    }
    else
    {
        P_MobjChangeState(actor, S_DEVIL_ATK2_7);
    }
}

//
// Devil Incarnate2 Chose Attack
//
void C_DECL A_Devil2Decide(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_DEVIL2_ATK1_1);
    }
    else
    {
        P_MobjChangeState(actor, S_DEVIL2_ATK2_1);
    }
}

//
// Devil2 Triple Fireball Attack Decide
//
void C_DECL A_Devil2Attack2Continue1(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_DEVIL2_ATK1_6);
    }
    else
    {
        P_MobjChangeState(actor, S_DEVIL2_ATK2_5);
    }
}

void C_DECL A_Devil2Attack2Continue2(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_DEVIL2_ATK1_6);
    }
    else
    {
        P_MobjChangeState(actor, S_DEVIL2_ATK2_7);
    }
}

//
// Black Demon Invisibility
//
void C_DECL A_BlackDemonDecide(mobj_t *actor)
{
    if(P_Random() < 16)
    {
        P_MobjChangeState(actor, S_CBLACKDEMON_INVISIBLE1);
    }
    else
    {
    A_Chase(actor);
    }
}

void C_DECL A_BlackDemonDecide2(mobj_t *actor)
{
    if(P_Random() < 16)
    {
        P_MobjChangeState(actor, S_CBLACKDEMON_APPEAR1);
    }
    else
    {
        A_ChaseNA(actor);
    }
}

//
// Skeleton Resurection
//

//
// Skeleton Decide Wheter Resurect
//
void C_DECL A_SkelDecideResurect(mobj_t *actor)
{
    if(P_Random() < 32)
    {
        P_MobjChangeState(actor, S_CSKE_DIE4);
    }
}

//
// Skeleton Decide Wheter To Resurect Or Wait
//
void C_DECL A_SkelDecideResurectTime(mobj_t *actor)
{
    if(P_Random() < 32)
    {
        P_MobjChangeState(actor, S_CSKE_DIE6);
    }
}

//
// Skeleton2 Resurection
//

//
// Skeleton2 Decide Wheter Resurect
//
void C_DECL A_Skel2DecideResurect(mobj_t *actor)
{
    if(P_Random() < 32)
    {
        P_MobjChangeState(actor, S_CSKE_DIE4);
    }
}

//
// Skeleton2 Decide Wheter To Resurect Or Wait
//
void C_DECL A_Skel2DecideResurectTime(mobj_t *actor)
{
    if(P_Random() < 32)
    {
        P_MobjChangeState(actor, S_CSKE_DIE6);
    }
}

//
// Water Troll Chose To Dive
//
void C_DECL A_WaterTrollDecide(mobj_t *actor)
{
    if(P_Random() < 64)
    {
        P_MobjChangeState(actor, S_CTROLLWATER_DIVE1);
    }
    else
    {
        A_WaterTrollChase(actor);
    }
}

//
// Nemesis Chose Attack
//
void C_DECL A_NemesisDecide(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_CNEM_ATK1_1);
    }
    else
    {
        P_MobjChangeState(actor, S_CNEM_ATK2_1);
    }
}

//
// Muller Chose Attack
//
void C_DECL A_MullerDecide(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_MULLER_ATK1_1);
    }
    else
    {
        P_MobjChangeState(actor, S_MULLER_ATK2_1);
    }
}

//
// Angel Of Death 2 Chose Attack
//
void C_DECL A_Angel2Decide(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_ANG2_ATK2_1);
    }
    else
    {
        P_MobjChangeState(actor, S_ANG2_ATK3_1);
    }
}

//
// Angel 2 Triple Fireball Attack Decide
//
void C_DECL A_Angel2Attack1Continue1(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_ANG2_ATK1_6);
    }
    else
    {
        P_MobjChangeState(actor, S_ANG2_ATK2_5);
    }
}

void C_DECL A_Angel2Attack1Continue2(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_ANG2_ATK1_6);
    }
    else
    {
        P_MobjChangeState(actor, S_ANG2_ATK2_7);
    }
}

//
// PoopDeck Decide
//
void C_DECL A_PoopDecide(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_POOPDECK_ATK1_1);
    }
    else
    {
        P_MobjChangeState(actor, S_POOPDECK_ATK2_1);
    }
}

//
// Schabb's Demon Decide
//
void C_DECL A_SchabbsDDecide(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        P_MobjChangeState(actor, S_SCHABBSD_ATK1_1);
    }
    else
    {
        P_MobjChangeState(actor, S_SCHABBSD_ATK2_1);
    }
}

//
// Candelabra "random" animation
//
void C_DECL A_CandelabraDecide(mobj_t *actor)
{
    if(P_Random() < 96)
    {
        P_MobjChangeState(actor, S_OCANDELABRA1);
    }
    else
    {
        P_MobjChangeState(actor, S_OCANDELABRA3);
    }
}

//
// AMBIENT SOUND ACTIONS
//

//
// Lightning Decide
//
void C_DECL A_LightningDecide(mobj_t *mo)
{
    if(P_Random() < 128)
    {
        S_StartSound(SFX_ALIGH, mo);
    }
    else
    {
        S_StartSound(SFX_ALIGH2, mo);
    }
}

//
// Water Drop Decide
//
void C_DECL A_WaterDropDecide(mobj_t *mo)
{
    if(P_Random() < 128)
    {
        S_StartSound(SFX_ADROP, mo);
    }
    else
    {
        S_StartSound(SFX_ADROP2, mo);
    }
}

//
// Rocks Decide
//
void C_DECL A_RocksDecide(mobj_t *mo)
{
    if(P_Random() < 128)
    {
        S_StartSound(SFX_AROCK, mo);
    }
    else
    {
        S_StartSound(SFX_AROCK2, mo);
    }
}

//
// MISC ACTIONS
//

//
// Player Dead
//
void C_DECL A_PlayerDead(mobj_t *mo)
{
    // Default death sound.
    int     sound = SFX_PLYDTH;

    if((gamemode == commercial) && (mo->health < -50))
    {
        // IF THE PLAYER DIES
        // LESS THAN -50% WITHOUT GIBBING
        sound = SFX_PLYDTH;
    }

    S_StartSound(sound, mo);
}

//
// Gibbed/Slop
//
void C_DECL A_GScream(mobj_t *actor)
{
    S_StartSound(SFX_GIBBED, actor);
}

//
// Badguy Refire
//
void C_DECL A_BRefire(mobj_t *actor)
{
    // keep firing unless target got out of sight
    A_FaceTarget(actor);

    if(P_Random() < 10)
        return;

    if(!actor->target || actor->target->health <= 0 ||
       !P_CheckSight(actor, actor->target))
    {
        P_MobjChangeState(actor, actor->info->seeState);
    }
}

//
// Hitler Slop
//
void C_DECL A_HitlerSlop(mobj_t *mo)
{
    S_StartSound(SFX_HITSLP, mo);
}

//
// PACMAN CHARGE ATTACK
//

#define SKULLSPEED      (20*FRACUNIT)

void C_DECL A_PacmanAttack(mobj_t *actor)
{
    mobj_t *dest;
    angle_t an;
    int     dist;

    if(!actor->target)
        return;

    dest = actor->target;
    actor->flags |= MF_SKULLFLY;

    S_StartSound(actor->info->attackSound, actor);
    A_FaceTarget(actor);
    an = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = FixedMul(SKULLSPEED, finecosine[an]);
    actor->mom[MY] = FixedMul(SKULLSPEED, finesine[an]);
    dist = P_ApproxDistance(dest->pos[VX] - actor->pos[VX], dest->pos[VY] - actor->pos[VY]);
    dist = dist / SKULLSPEED;

    if(dist < 1)
        dist = 1;
    actor->mom[MZ] = (dest->pos[VZ] + FLT2FIX(dest->height /2) - actor->pos[VZ]) / dist;
}

//
// PACMAN PINK TRAIL 1
//
void C_DECL A_PinkPacmanBlur1(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_PACMANPINKBLUR1);
    mo->angle = actor->angle;
    mo->target = actor->target;
    A_Chase(actor);
}

//
// PACMAN RED TRAIL 1
//
void C_DECL A_RedPacmanBlur1(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_PACMANREDBLUR1);
    mo->angle = actor->angle;
    mo->target = actor->target;
    A_Chase(actor);
}

//
// PACMAN ORANGE TRAIL 1
//
void C_DECL A_OrangePacmanBlur1(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_PACMANORANGEBLUR1);
    mo->angle = actor->angle;
    mo->target = actor->target;
    A_Chase(actor);
}

//
// PACMAN BLUE TRAIL 1
//
void C_DECL A_BluePacmanBlur1(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_PACMANBLUEBLUR1);
    mo->angle = actor->angle;
    mo->target = actor->target;
    A_Chase(actor);
}

//
// PACMAN PINK TRAIL 2
//
void C_DECL A_PinkPacmanBlur2(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_PACMANPINKBLUR2);
    mo->angle = actor->angle;
    mo->target = actor->target;
    A_Chase(actor);
}

//
// PACMAN RED TRAIL 2
//
void C_DECL A_RedPacmanBlur2(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_PACMANREDBLUR2);
    mo->angle = actor->angle;
    mo->target = actor->target;
    A_Chase(actor);
}

//
// PACMAN ORANGE TRAIL 2
//
void C_DECL A_OrangePacmanBlur2(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_PACMANORANGEBLUR2);
    mo->angle = actor->angle;
    mo->target = actor->target;
    A_Chase(actor);
}

//
// PACMAN BLUE TRAIL 2
//
void C_DECL A_BluePacmanBlur2(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_PACMANBLUEBLUR2);
    mo->angle = actor->angle;
    mo->target = actor->target;
    A_Chase(actor);
}

//
// PACMAN PINK TRAIL ATTACK
//
void C_DECL A_PinkPacmanBlurA(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_PACMANPINKBLUR1);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// PACMAN RED TRAIL ATTACK
//
void C_DECL A_RedPacmanBlurA(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_PACMANREDBLUR1);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// PACMAN PINK TRAIL ATTACK 2
//
void C_DECL A_PinkPacmanBlurA2(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_PACMANPINKBLUR2);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// PACMAN RED TRAIL ATTACK 2
//
void C_DECL A_RedPacmanBlurA2(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_PACMANREDBLUR2);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// PACMAN "BRAIN"
//
void C_DECL A_PacmanSpawnerAwake(mobj_t *mo)
{

}

void C_DECL A_PacmanSwastika(mobj_t *mo)
{
    mobj_t *targ;
    mobj_t *newmobj;
    static int easy = 0;

    easy ^= 1;
    if(gameskill <= SM_EASY && (!easy))
        return;

    // shoot a cube at current target
    targ = brainTargets[brain.targeton++];
    brain.targeton %= numBrainTargets;

    // spawn brain missile
    newmobj = P_SpawnMissile(MT_PACMANSWASTIKA, mo, targ);
    if(newmobj)
    {
        newmobj->target = targ;
        newmobj->reactionTime =
            ((targ->pos[VY] - mo->pos[VY]) / newmobj->mom[MY]) / newmobj->state->tics;
    }
}

void C_DECL A_PacmanBJHead(mobj_t *mo)
{
    mobj_t     *targ;
    mobj_t     *newmobj;
    static int  easy = 0;

    easy ^= 1;
    if(gameskill <= SM_EASY && (!easy))
        return;

    // Shoot a cube at current target.
    targ = brainTargets[brain.targeton++];
    brain.targeton %= numBrainTargets;

    // Spawn brain missile.
    newmobj = P_SpawnMissile(MT_PACMANBJHEAD, mo, targ);
    if(newmobj)
    {
        newmobj->target = targ;
        newmobj->reactionTime =
            ((targ->pos[VY] - mo->pos[VY]) / newmobj->mom[MY]) / newmobj->state->tics;
    }
}

//
// ANGEL OF DEATH MISSILE TRAIL
//

//
// ANGEL OF DEATH MISSILE TRAIL FRAME 1
//
void C_DECL A_AngMissileBlur1(mobj_t *actor)
{
    mobj_t     *mo;

    mo = P_SpawnMobj3fv(MT_ANGMISSILEBLUR1, actor->pos);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// ANGEL OF DEATH MISSILE TRAIL FRAME 2
//
void C_DECL A_AngMissileBlur2(mobj_t *actor)
{
    mobj_t     *mo;

    mo = P_SpawnMobj3fv(MT_ANGMISSILEBLUR2, actor->pos);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// ANGEL OF DEATH MISSILE TRAIL FRAME 3
//
void C_DECL A_AngMissileBlur3(mobj_t *actor)
{
    mobj_t     *mo;

    mo = P_SpawnMobj3fv(MT_ANGMISSILEBLUR3, actor->pos);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// ANGEL OF DEATH MISSILE TRAIL FRAME 4
//
void C_DECL A_AngMissileBlur4(mobj_t *actor)
{
    mobj_t     *mo;

    mo = P_SpawnMobj3fv(MT_ANGMISSILEBLUR4, actor->pos);
    mo->angle = actor->angle;
    mo->target = actor->target;
}

//
// ANGEL OF DEATH FIRE ATTACK
//

//
// A_AngelFireStart
//
void C_DECL A_AngelFireStart(mobj_t *actor)
{
    S_StartSound(SFX_VILATK, actor);
}

//
// A_AngelFire
// Keep fire in front of player unless out of sight
//
void C_DECL A_AngelFire(mobj_t *actor)
{
    mobj_t     *dest;
    unsigned    an;

    dest = actor->tracer;
    if(!dest)
        return;

    // Don't move it if the vile lost sight.
    if(!P_CheckSight(actor->target, dest))
        return;

    an = dest->angle >> ANGLETOFINESHIFT;

    P_MobjUnsetPosition(actor);
    actor->pos[VX] = dest->pos[VX] + FixedMul(24 * FRACUNIT, finecosine[an]);
    actor->pos[VY] = dest->pos[VY] + FixedMul(24 * FRACUNIT, finesine[an]);
    actor->pos[VZ] = dest->pos[VZ];
    P_MobjSetPosition(actor);
}

void C_DECL A_AngelStartFire(mobj_t *actor)
{
    S_StartSound(SFX_FLAMST, actor);
    A_AngelFire(actor);
}

void C_DECL A_AngelFireCrackle(mobj_t *actor)
{
    S_StartSound(SFX_FLAME, actor);
    A_AngelFire(actor);
}

//
// A_AngelFireTarget
// Spawn the hellfire
//
void C_DECL A_AngelFireTarget(mobj_t *actor)
{
    mobj_t *mo;

    if(!actor->target)
        return;

    A_FaceTarget(actor);

    mo = P_SpawnMobj3fv(MT_ANGELFIRE, actor->target->pos);

    actor->tracer = mo;
    mo->target = actor;
    mo->tracer = actor->target;
    A_AngelFire(mo);
}

void C_DECL A_AngelFireAttack(mobj_t* actor)
{
    mobj_t*             fire;
    int                 an;

    if(!actor->target)
        return;

    A_FaceTarget(actor);

    if(!P_CheckSight(actor, actor->target))
        return;

    S_StartSound(SFX_BAREXP, actor);
    P_DamageMobj(actor->target, actor, actor, 20);
    actor->target->mom[MZ] =
        FIX2FLT(1000 * FRACUNIT / actor->target->info->mass);

    an = actor->angle >> ANGLETOFINESHIFT;

    fire = actor->tracer;

    if(!fire)
        return;

    // Move the fire between the Angel and the player.
    fire->pos[VX] = actor->target->pos[VX] - 24 * FIX2FLT(finecosine[an]);
    fire->pos[VY] = actor->target->pos[VY] - 24 * FIX2FLT(finesine[an]);
    P_RadiusAttack(fire, actor, 70, 69);
}

/**
 * Devil Incarnate: Missile attack (hellfire).
 */
void C_DECL A_DevilFireTarget(mobj_t *actor)
{
    mobj_t*             mo;

    if(!actor->target)
        return;

    A_FaceTarget(actor);

    mo = P_SpawnMobj3fv(MT_DEVILFIRE, actor->target->pos);

    actor->tracer = mo;
    mo->target = actor;
    mo->tracer = actor->target;
    A_AngelFire(mo);
}

//
// RAIN
//
void C_DECL A_SpawnRain(mobj_t *actor)
{
    P_SpawnMobj3fv(MT_RAINDROP, actor->pos);
}

//
// CATACOMB ZOMBIE FACE TARGET
//
void C_DECL A_ZombieFaceTarget(mobj_t *mo)
{
    A_FaceTarget(mo);

    if(P_Random() < 128)
    {
        S_StartSound(SFX_CZOMBA, mo);
    }
}

//
// CATACOMB EYE PROJECTILE
//
void C_DECL A_EyeProjectile(mobj_t *mo)
{
    S_StartSound(SFX_CEYEST, mo);
}

//
// CATACOMB WATER TROLL SPLASH RAISE
//
void C_DECL A_WaterTrollSplashR(mobj_t *mo)
{
    S_StartSound(SFX_CTRLSR, mo);
}

//
// CATACOMB WATER TROLL SPLASH LOWER
//
void C_DECL A_WaterTrollSplashL(mobj_t *mo)
{
    S_StartSound(SFX_CTRLSL, mo);
}

//
// CATACOMB CHESTS
//

//
// CHEST 1 (SMALL)
//
void C_DECL A_ChestOpenSmall(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        mobj_t *mo;
        mobj_t *mo2;

        actor->pos[VX] += 16 * FRACUNIT;  // So as not to spawn the things ontop of the chest
        actor->pos[VY] -= 4 * FRACUNIT;

        mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATACOMBFIREORBSMALL);
        mo->angle = actor->angle;
        mo->target = actor->target;
        actor->pos[VY] += 4 * FRACUNIT;

        mo2 = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATACOMBFIREORBSMALL);
        mo2->angle = actor->angle;
        mo2->target = actor->target;
        actor->pos[VX] -= 16 * FRACUNIT;  // return to original position
    }
    else
    {
        mobj_t *mo;
        mobj_t *mo2;
        mobj_t *mo3;

        actor->pos[VX] += 16 * FRACUNIT;  // So as not to spawn the things ontop of the chest
        actor->pos[VY] -= 4 * FRACUNIT;

        mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATACOMBFIREORBSMALL);
        mo->angle = actor->angle;
        mo->target = actor->target;
        actor->pos[VY] += 4 * FRACUNIT;

        mo2 = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATACOMBFIREORBSMALL);
        mo2->angle = actor->angle;
        mo2->target = actor->target;
        actor->pos[VY] += 4 * FRACUNIT;

        mo3 = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATACOMBFIREORBSMALL);
        mo3->angle = actor->angle;
        mo3->target = actor->target;
        actor->pos[VY] -= 4 * FRACUNIT;
        actor->pos[VX] -= 16 * FRACUNIT;  // return to original position
    }
}

//
// CHEST 2 (MED)
//
void C_DECL A_ChestOpenMed(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        mobj_t *mo;
        mobj_t *mo2;

        actor->pos[VX] += 16 * FRACUNIT;  // So as not to spawn the things ontop of the chest
        actor->pos[VY] -= 4 * FRACUNIT;

        mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATAHVIAL);
        mo->angle = actor->angle;
        mo->target = actor->target;
        actor->pos[VY] += 4 * FRACUNIT;

        mo2 = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATACOMBFIREORBLARGE);
        mo2->angle = actor->angle;
        mo2->target = actor->target;
        actor->pos[VX] -= 16 * FRACUNIT;  // return to original position
    }
    else
    {
        mobj_t *mo;
        mobj_t *mo2;
        mobj_t *mo3;

        actor->pos[VX] += 16 * FRACUNIT;  // So as not to spawn the things ontop of the chest
        actor->pos[VY] -= 4 * FRACUNIT;

        mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATACOMBFIREORBLARGE);
        mo->angle = actor->angle;
        mo->target = actor->target;
        actor->pos[VY] += 4 * FRACUNIT;

        mo2 = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATACOMBFIREORBSMALL);
        mo2->angle = actor->angle;
        mo2->target = actor->target;
        actor->pos[VY] += 4 * FRACUNIT;

        mo3 = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATACOMBFIREORBSMALL);
        mo3->angle = actor->angle;
        mo3->target = actor->target;
        actor->pos[VY] -= 4 * FRACUNIT;
        actor->pos[VX] -= 16 * FRACUNIT;  // return to original position
    }
}

//
// CHEST 3 (LARGE)
//
void C_DECL A_ChestOpenLarge(mobj_t *actor)
{
    if(P_Random() < 128)
    {
        mobj_t *mo;
        mobj_t *mo2;
        mobj_t *mo3;

        actor->pos[VX] += 16 * FRACUNIT;  // So as not to spawn the things ontop of the chest
        actor->pos[VY] -= 4 * FRACUNIT;

        mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATAHVIAL);
        mo->angle = actor->angle;
        mo->target = actor->target;
        actor->pos[VY] += 4 * FRACUNIT;

        mo2 = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATACOMBFIREORBLARGE);
        mo2->angle = actor->angle;
        mo2->target = actor->target;
        actor->pos[VY] += 4 * FRACUNIT;

        mo3 = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATACOMBFIREORBSMALL);
        mo3->angle = actor->angle;
        mo3->target = actor->target;
        actor->pos[VY] -= 4 * FRACUNIT;
        actor->pos[VX] -= 16 * FRACUNIT;  // return to original position
    }
    else
    {
        mobj_t *mo;
        mobj_t *mo2;
        mobj_t *mo3;

        actor->pos[VX] += 16 * FRACUNIT;  // So as not to spawn the things ontop of the chest
        actor->pos[VY] -= 4 * FRACUNIT;

        mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATAHVIAL);
        mo->angle = actor->angle;
        mo->target = actor->target;
        actor->pos[VY] += 4 * FRACUNIT;

        mo2 = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATAHVIAL);
        mo2->angle = actor->angle;
        mo2->target = actor->target;
        actor->pos[VY] += 4 * FRACUNIT;

        mo3 = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_CATACOMBFIREORBSMALL);
        mo3->angle = actor->angle;
        mo3->target = actor->target;
        actor->pos[VY] -= 4 * FRACUNIT;
        actor->pos[VX] -= 16 * FRACUNIT;  // return to original position.
    }
}

//
// Playing Dead Bad Guy "Alert"
//
void C_DECL A_PlayingDeadActive(mobj_t *actor)
{
    // Check for melee attack.
    if(actor->info->meleeState && P_CheckMeleeRange(actor))
    {
        if(actor->info->attackSound)
            S_StartSound(actor->info->attackSound, actor);

        P_MobjChangeState(actor, actor->info->meleeState);
        return;
    }
}

//
// Raven Refire
//
void C_DECL A_RavenRefire(mobj_t *actor)
{
    // Keep firing unless target got out of sight.
    A_FaceTarget(actor);

    if(P_Random() < 40)
        return;

    if(!actor->target || actor->target->health <= 0 ||
       !P_CheckSight(actor, actor->target))
    {
        P_MobjChangeState(actor, S_RAVEN_RUN1);
    }
}

//
// Dirt Devil Trail
//
void C_DECL A_DirtDevilChase(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_OMSDIRTDEVILTRAIL);
    mo->angle = actor->angle;
    mo->target = actor->target;
    A_Chase(actor);
}

//
// Drake Face Target
//
void C_DECL A_DrakeFaceTarget(mobj_t *mo)
{
    S_StartSound(SFX_OMOACT, mo);
    A_FaceTarget(mo);
}

//
// Drake Head
//
void C_DECL A_DrakeHead(mobj_t *actor)
{
    mobj_t *mo;
    actor->pos[VZ] += 160 * FRACUNIT; // Up
    mo = P_SpawnMobj3f(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_OMSFIREDRAKEHEAD);
    actor->pos[VZ] -= 160 * FRACUNIT; // Back Down
}

//
// Stalker Face Target
//
void C_DECL A_StalkerFaceTarget(mobj_t *mo)
{
    S_StartSound(SFX_SSPSUR, mo);
    A_FaceTarget(mo);
}

//
// Stalker Projectile Sound
//
void C_DECL A_StalkerProjectileSound(mobj_t *mo)
{
    S_StartSound(SFX_SPTFIR, mo);
}


//
// Uranus Green Blob Attack
//
void C_DECL A_FireGreenBlobMissile(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    P_SpawnMissile(MT_GREENBLOBMISSILE, actor, actor->target);
}

//
// Uranus Red Blob Attack
//
void C_DECL A_FireRedBlobMissile(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    P_SpawnMissile(MT_REDBLOBMISSILE, actor, actor->target);
}

//
// Mother Blob Attack
//
void C_DECL A_MotherBlobAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    P_SpawnMissile(MT_REDBLOBMISSILE, actor, actor->target);
    P_SpawnMissile(MT_REDBLOBMISSILE, actor, actor->target);
}

//
// URANUS GOLD KEYCARD
//
void C_DECL A_SpawnGoldKeyCard(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj3f(MT_UGOLDKEYCARD, actor->pos[VX], actor->pos[VY], actor->pos[VZ]);
}

//
// Shadow Nemesis Resurect Sound
//
void C_DECL A_ShadowNemesisResurectSound(mobj_t *mo)
{
    S_StartSound(SFX_CSNEMR, mo);
}
