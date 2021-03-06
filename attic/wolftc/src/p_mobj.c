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
 * p_mobj.c: Moving object handling. Spawn functions.
 */

#ifdef MSVC
#  pragma optimize("g", off)
#endif

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "wolftc.h"

#include "dmu_lib.h"
#include "hu_stuff.h"
#include "g_common.h"
#include "p_map.h"
#include "p_terraintype.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

#define VANISHTICS              (2*TICSPERSEC)

#define MAX_BOB_OFFSET      8

#define NOMOMENTUM_THRESHOLD    (0.000001f)
#define STOPSPEED               (1.0f/1.6/10)
#define STANDSPEED              (1.0f/2)

// TYPES -------------------------------------------------------------------

typedef struct spawnobj_s {
    float       pos[3];
    int         angle;
    int         type;
    int         thingflags;
} spawnobj_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    G_PlayerReborn(int player);
void    P_ApplyTorque(mobj_t *mo);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    P_SpawnMapThing(spawnspot_t *mthing);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float attackRange;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

spawnobj_t itemrespawnque[ITEMQUESIZE];
int     itemrespawntime[ITEMQUESIZE];
int     iquehead;
int     iquetail;

// CODE --------------------------------------------------------------------

const terraintype_t* P_MobjGetFloorTerrainType(mobj_t* mo)
{
    if(mo->floorMaterial && !IS_CLIENT)
    {
        return P_TerrainTypeForMaterial(mo->floorMaterial);
    }
    else
    {
        return P_GetPlaneMaterialType(P_GetPtrp(mo->subsector, DMU_SECTOR), PLN_FLOOR);
    }
}

/**
 * @return              @c true, if the mobj is still present.
 */
boolean P_MobjChangeState(mobj_t *mobj, statenum_t state)
{
    state_t            *st;

    do
    {
        if(state == S_NULL)
        {
            mobj->state = (state_t *) S_NULL;
            P_MobjRemove(mobj, false);
            return false;
        }

        P_MobjSetState(mobj, state);
        st = &states[state];

        mobj->turnTime = false; // $visangle-facetarget

        // Modified handling.
        // Call action functions when the state is set
        if(st->action)
            st->action(mobj);

        state = st->nextState;
    } while(!mobj->tics);

    return true;
}

void P_ExplodeMissile(mobj_t *mo)
{
    if(IS_CLIENT)
    {
        // Clients won't explode missiles.
        P_MobjChangeState(mo, S_NULL);
        return;
    }

    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;

    P_MobjChangeState(mo, mobjInfo[mo->type].deathState);

    mo->tics -= P_Random() & 3;

    if(mo->tics < 1)
        mo->tics = 1;

    if(mo->flags & MF_MISSILE)
    {
        mo->flags &= ~MF_MISSILE;
        mo->flags |= MF_VIEWALIGN;
        // Remove the brightshadow flag.
        if(mo->flags & MF_BRIGHTSHADOW)
            mo->flags &= ~MF_BRIGHTSHADOW;
        if(mo->flags & MF_BRIGHTEXPLODE)
            mo->flags |= MF_BRIGHTSHADOW;
    }

    if(mo->info->deathSound)
        S_StartSound(mo->info->deathSound, mo);
}

void P_FloorBounceMissile(mobj_t *mo)
{
    mo->mom[MZ] = -mo->mom[MZ];
    P_MobjChangeState(mo, mobjInfo[mo->type].deathState);
}

/**
 * @return               The ground friction factor for the mobj.
 */
float P_MobjGetFriction(mobj_t *mo)
{
    if((mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= mo->floorZ) && !mo->onMobj)
    {
        return FRICTION_FLY;
    }

    return XS_Friction(P_GetPtrp(mo->subsector, DMU_SECTOR));
}

void P_MobjMoveXY(mobj_t *mo)
{
    sector_t *backsector;
    fixed_t ptryx;
    fixed_t ptryy;
    player_t *player;
    fixed_t xmove;
    fixed_t ymove;
    boolean largeNegative;

    // $democam: cameramen have their own movement code
    if(P_CameraXYMovement(mo))
        return;

    xmove = MINMAX_OF(-FLT2FIX(MAXMOVE), mo->mom[MX], FLT2FIX(MAXMOVE));
    ymove = MINMAX_OF(-FLT2FIX(MAXMOVE), mo->mom[MY], FLT2FIX(MAXMOVE));
    mo->mom[MX] = xmove;
    mo->mom[MY] = ymove;

    if((xmove | ymove) == 0)
    {
        if(mo->flags & MF_SKULLFLY)
        {
            // the skull slammed into something
            mo->flags &= ~MF_SKULLFLY;
            mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;

            P_MobjChangeState(mo, mo->info->spawnState);
        }
        return;
    }

    player = mo->player;

    do
    {
        // killough 8/9/98: fix bug in original Doom source:
        // Large negative displacements were never considered.
        // This explains the tendency for Mancubus fireballs
        // to pass through walls.
        largeNegative = false;
        if(!cfg.moveBlock && (xmove < -FLT2FIX(MAXMOVE) / 2 || ymove < -FLT2FIX(MAXMOVE) / 2))
        {
            // Make an exception for "north-only wallrunning".
            if(!(cfg.wallRunNorthOnly && mo->wallRun))
                largeNegative = true;
        }

        if(xmove > FLT2FIX(MAXMOVE) / 2 || ymove > FLT2FIX(MAXMOVE) / 2 || largeNegative)
        {
            ptryx = mo->pos[VX] + (xmove >>= 1);
            ptryy = mo->pos[VY] + (ymove >>= 1);
        }
        else
        {
            ptryx = mo->pos[VX] + xmove;
            ptryy = mo->pos[VY] + ymove;
            xmove = ymove = 0;
        }

        // If mobj was wallrunning - stop.
        if(mo->wallRun)
            mo->wallRun = false;

        // killough $dropoff_fix
        if(!P_TryMove(mo, ptryx, ptryy, true, false))
        {
            // blocked move
            if(mo->flags2 & MF2_SLIDE)
            {                   // try to slide along it
                P_SlideMove(mo);
            }
            else if(mo->flags & MF_MISSILE)
            {
                sector_t*           backSec;

                //// kludge: Prevent missiles exploding against the sky.
                if(ceilingLine &&
                   (backSec = P_GetPtrp(ceilingLine, DMU_BACK_SECTOR)))
                {
                    if(P_GetIntp(backSec,
                                 DMU_CEILING_MATERIAL) == SKYMASKMATERIAL &&
                       mo->pos[VZ] > P_GetFloatp(backSec, DMU_CEILING_HEIGHT))
                    {
                        P_MobjRemove(mo, false);
                        return;
                    }
                }

                if(floorLine &&
                   (backSec = P_GetPtrp(floorLine, DMU_BACK_SECTOR)))
                {
                    if(P_GetIntp(backSec,
                                 DMU_FLOOR_MATERIAL) == SKYMASKMATERIAL &&
                       mo->pos[VZ] < P_GetFloatp(backSec, DMU_FLOOR_HEIGHT))
                    {
                        P_MobjRemove(mo, false);
                        return;
                    }
                }
                //// kludge end.

                P_ExplodeMissile(mo);
            }
            else
            {
                mo->mom[MX] = mo->mom[MY] = 0;
            }
        }
    } while(!INRANGE_OF(mom[MX], 0, NOMOMENTUM_THRESHOLD) ||
            !INRANGE_OF(mom[MY], 0, NOMOMENTUM_THRESHOLD));

    // slow down
    if(player && (P_GetPlayerCheats(player) & CF_NOMOMENTUM))
    {
        // debug option for no sliding at all
        mo->mom[MX] = mo->mom[MY] = 0;
        return;
    }

    if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
        return;                 // no friction for missiles ever

    if(mo->pos[VZ] > FLT2FIX(mo->floorZ) && !mo->onMobj && !(mo->flags2 & MF2_FLY))
        return;                 // no friction when falling

    if(cfg.slidingCorpses)
    {
        // killough $dropoff_fix: add objects falling off ledges
        // Does not apply to players! -jk
        if((mo->flags & MF_CORPSE || mo->intFlags & MIF_FALLING) &&
           !mo->player)
        {
            // do not stop sliding
            //  if halfway off a step with some momentum
            if(mo->mom[MX] > FRACUNIT / 4 || mo->mom[MX] < -FRACUNIT / 4 ||
               mo->mom[MY] > FRACUNIT / 4 || mo->mom[MY] < -FRACUNIT / 4)
            {
                if(mo->floorZ !=
                   P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
                    return;
            }
        }
    }

    // Stop player walking animation.
    if(player && !player->plr->cmd.forwardMove && !player->plr->cmd.sideMove &&
       mo->mom[MX] > -STANDSPEED && mo->mom[MX] < STANDSPEED &&
       mo->mom[MY] > -STANDSPEED && mo->mom[MY] < STANDSPEED)
    {
        // if in a walking frame, stop moving
        if((unsigned) ((player->plr->mo->state - states) - PCLASS_INFO(player->class)->runState) < 4)
            P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class)->normalState);
    }

    if((!player || (player->plr->cmd.forwardMove == 0 && player->plr->cmd.sideMove == 0)) &&
       mo->mom[MX] > -STOPSPEED && mo->mom[MX] < STOPSPEED &&
       mo->mom[MY] > -STOPSPEED && mo->mom[MY] < STOPSPEED)
    {
        mo->mom[MX] = 0;
        mo->mom[MY] = 0;
    }
    else
    {
        if((mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= FLT2FIX(mo->floorZ)) &&
           !mo->onMobj)
        {
            mo->mom[MX] *= FRICTION_FLY;
            mo->mom[MY] *= FRICTION_FLY;
        }
#if __JHERETIC__
        else if(P_ToXSector(P_GetPtrp(mo->subsector,
                                    DMU_SECTOR))->special == 15)
        {
            // Friction_Low
            mo->mom[MX] *= FRICTION_LOW;
            mo->mom[MY] *= FRICTION_LOW;
        }
#endif
        else
        {
            float       friction = P_MobjGetFriction(mo);

            mo->mom[MX] *= friction;
            mo->mom[MY] *= friction;
        }
    }
}

/*
static boolean PIT_Splash(sector_t *sector, void *data)
{
    mobj_t *mo = data;
    fixed_t floorheight;

    floorheight = P_GetFixedp(sector, DMU_FLOOR_HEIGHT);

    // Is the mobj touching the floor of this sector?
    if(mo->pos[VZ] < floorheight &&
       mo->pos[VZ] + mo->height / 2 > floorheight)
    {
        //// \todo Play a sound, spawn a generator, etc.
    }

    // Continue checking.
    return true;
}
*/

void P_RipperBlood(mobj_t *mo)
{
    mobj_t         *th;
    float           pos[3];

    pos[VX] = mo->pos[VX];
    pos[VY] = mo->pos[VY];
    pos[VZ] = mo->pos[VZ];

    pos[VX] += FIX2FLT((P_Random() - P_Random()) << 12);
    pos[VY] += FIX2FLT((P_Random() - P_Random()) << 12);
    pos[VZ] += FIX2FLT((P_Random() - P_Random()) << 12);

    th = P_SpawnMobj3fv(MT_BLOOD, pos);
    th->flags |= MF_NOGRAVITY;
    th->mom[MX] = mo->mom[MX] / 2;
    th->mom[MY] = mo->mom[MY] / 2;
    th->tics += P_Random() & 3;
}

void P_HitFloor(mobj_t *mo)
{
    //P_MobjSectorsIterator(mo, PIT_Splash, mo);
}

void P_MobjMoveZ(mobj_t *mo)
{
    float       gravity;
    float       dist;
    float       delta;

    gravity = XS_Gravity(P_GetPtrp(mo->subsector, DMU_SECTOR));

    // $democam: cameramen get special z movement
    if(P_CameraZMovement(mo))
        return;

    // check for smooth step up
    if(mo->player && mo->pos[VZ] < mo->floorZ)
    {
        mo->dPlayer->viewHeight -= mo->floorZ - mo->pos[VZ];

        mo->dPlayer->viewHeightDelta =
            (cfg.plrViewHeight - mo->dPlayer->viewHeight) / 8;
    }

    // adjust height
    mo->pos[VZ] += mo->mom[MZ];
    if((mo->flags2 & MF2_FLY) &&
       mo->onMobj && mo->pos[VZ] > mo->onMobj->pos[VZ] + mo->onMobj->height)
        mo->onMobj = NULL; // We were on a mobj, we are NOT now.

    if((mo->flags & MF_FLOAT) && mo->target && !P_IsCamera(mo->target))
    {
        // float down towards target if too close
        if(!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
        {
            dist = P_ApproxDistance(mo->pos[VX] - mo->target->pos[VX],
                                    mo->pos[VY] - mo->target->pos[VY]);

            //delta = (mo->target->pos[VZ] + (mo->height / 2)) - mo->pos[VZ];
            delta = (mo->target->pos[VZ] + mo->target->height/2) -
                    (mo->pos[VZ] + mo->height /2);

            if(dist < mo->radius + mo->target->radius &&
               fabs(delta) < mo->height + mo->target->height)
            {
                // Don't go INTO the target.
                delta = 0;
            }

            if(delta < 0 && dist < -(delta * 3))
            {
                mo->pos[VZ] -= FLOATSPEED;
                P_MobjSetSRVOZ(mo, -FLOATSPEED);
            }
            else if(delta > 0 && dist < (delta * 3))
            {
                mo->pos[VZ] += FLOATSPEED;
                P_MobjSetSRVOZ(mo, FLOATSPEED);
            }
        }
    }

    // Do some fly-bobbing.
    if(mo->player && (mo->flags2 & MF2_FLY) && mo->pos[VZ] > mo->floorZ &&
       !mo->onMobj && (mapTime & 2))
    {
        mo->pos[VZ] += FIX2FLT(finesine[(FINEANGLES / 20 * mapTime >> 2) & FINEMASK]);
    }

    // Clip movement. Another thing?
    if(mo->onMobj && mo->pos[VZ] <= mo->onMobj->pos[VZ] + mo->onMobj->height)
    {
        if(mo->mom[MZ] < 0)
        {
            if(mo->player && mo->mom[MZ] < -gravity * 8 && !(mo->flags2 & MF2_FLY))
            {
                // Squat down.
                // Decrease viewheight for a moment
                // after hitting the ground (hard),
                // and utter appropriate sound.
                mo->dPlayer->viewHeightDelta = mo->mom[MZ] / 8;

                if(mo->player->health > 0)
                    S_StartSound(SFX_BLOCKD, mo);
            }
            mo->mom[MZ] = 0;
        }

        if(mo->mom[MZ] == 0)
            mo->pos[VZ] = mo->onMobj->pos[VZ] + mo->onMobj->height;

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            P_ExplodeMissile(mo);
            return;
        }
    }

    // The floor.
    if(mo->pos[VZ] <= mo->floorZ)
    {
        // hit the floor

        // Note (id):
        //  somebody left this after the setting mom[MZ] to 0,
        //  kinda useless there.
        //
        // cph - This was the a bug in the linuxdoom-1.10 source which
        //  caused it not to sync Doom 2 v1.9 demos. Someone
        //  added the above comment and moved up the following code. So
        //  demos would desync in close lost soul fights.
        // Note that this only applies to original Doom 1 or Doom2 demos - not
        //  Final Doom and Ultimate Doom.  So we test demo_compatibility *and*
        //  gamemission. (Note we assume that Doom1 is always Ult Doom, which
        //  seems to hold for most published demos.)
        //
        //  fraggle - cph got the logic here slightly wrong.  There are three
        //  versions of Doom 1.9:
        //
        //  * The version used in registered doom 1.9 + doom2 - no bounce
        //  * The version used in ultimate doom - has bounce
        //  * The version used in final doom - has bounce
        //
        // So we need to check that this is either retail or commercial
        // (but not doom2)
        int correct_lost_soul_bounce
                = (gamemode == retail || gamemode == commercial)
                  && gamemission != GM_DOOM2;

        if(correct_lost_soul_bounce && (mo->flags & MF_SKULLFLY))
        {
            // the skull slammed into something
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if(mo->mom[MZ] < 0)
        {
            if(mo->player && mo->mom[MZ] < -gravity * 8 && !(mo->flags2 & MF2_FLY))
            {
                // Squat down.
                // Decrease viewheight for a moment
                // after hitting the ground (hard),
                // and utter appropriate sound.
                mo->dPlayer->viewHeightDelta = mo->mom[MZ] / 8;

                // Fix DOOM bug - dead players grunting when hitting the ground
                // (e.g., after an archvile attack)
                if(mo->player->health > 0)
                    S_StartSound(SFX_BLOCKD, mo);
            }
            P_HitFloor(mo);
            mo->mom[MZ] = 0;
        }

        mo->pos[VZ] = mo->floorZ;

        // cph 2001/05/26 -
        // See lost soul bouncing comment above. We need this here for bug
        // compatibility with original Doom2 v1.9 - if a soul is charging and
        // hit by a raising floor this incorrectly reverses its Y momentum.
        //
        if(!correct_lost_soul_bounce && (mo->flags & MF_SKULLFLY))
            mo->mom[MZ] = -mo->mom[MZ];

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            if(mo->flags2 & MF2_FLOORBOUNCE)
            {
                P_FloorBounceMissile(mo);
                return;
            }
            else
            {
                P_ExplodeMissile(mo);
                return;
            }
        }
    }
    else if(mo->flags2 & MF2_LOGRAV)
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -(gravity / 8) * 2;
        else
            mo->mom[MZ] -= gravity / 8;
    }
    else if(!(mo->flags & MF_NOGRAVITY))
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -gravity * 2;
        else
            mo->mom[MZ] -= gravity;
    }

    if(mo->pos[VZ] + mo->height > mo->ceilingZ)
    {
        // hit the ceiling
        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;

        mo->pos[VZ] = mo->ceilingZ - mo->height;

        if(mo->flags & MF_SKULLFLY)
        {                       // the skull slammed into something
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            if(P_GetIntp(mo->subsector, DMU_CEILING_MATERIAL) == skyMaskMaterial)
            {
                // Don't explode against sky.
                {
                    P_MobjRemove(mo, false);
                }
                return;
            }
            P_ExplodeMissile(mo);
            return;
        }
    }
}

void P_NightmareRespawn(mobj_t *mobj)
{
    float       pos[3];
    subsector_t *ss;
    mobj_t     *mo;

    pos[VX] = FIX2FLT(mobj->spawnSpot.pos[VX]);
    pos[VY] = FIX2FLT(mobj->spawnSpot.pos[VY]);
    pos[VZ] = FIX2FLT(mobj->spawnSpot.pos[VZ]);

    // somthing is occupying it's position?
    if(!P_CheckPosition2f(mobj, pos[VX], pos[VY]))
        return;                 // no respwan

    // spawn a teleport fog at old spot
    // because of removal of the body?
    mo = P_SpawnMobj3f(FLT2FIX(mobj->pos[VX]), FLT2FIX(mobj->pos[VY]),
                     P_GetFixedp(mobj->subsector, DMU_FLOOR_HEIGHT),
                     MT_TFOG);
    // initiate teleport sound
    S_StartSound(SFX_NMRRSP, mo);

    // spawn a teleport fog at the new spot
    ss = R_PointInSubsector(pos[VX], pos[VY]);
    mo = P_SpawnMobj3f(FLT2FIX(pos[VX]), FLT2FIX(pos[VY]),
                     P_GetFixedp(ss, DMU_FLOOR_HEIGHT),
                     MT_TFOG);

    S_StartSound(SFX_NMRRSP, mo);

    // spawn it
    if(mobj->info->flags & MF_SPAWNCEILING)
        pos[VZ] = ONCEILINGZ;
    else
        pos[VZ] = ONFLOORZ;

    // inherit attributes from deceased one
    mo = P_SpawnMobj3fv(mobj->type, pos);
    memcpy(mo->spawnSpot.pos, mobj->spawnSpot.pos, sizeof(mo->spawnSpot.pos));
    mo->spawnSpot.angle = mobj->spawnSpot.angle;
    mo->spawnSpot.type = mobj->spawnSpot.type;
    mo->spawnSpot.flags = mobj->spawnSpot.flags;
    mo->angle = mobj->spawnSpot.angle;

    if(mobj->spawnSpot.flags & MTF_AMBUSH)
        mo->flags |= MF_AMBUSH;

    mo->reactionTime = 18;

    // remove the old monster.
    P_MobjRemove(mobj, true);
}

void P_MobjThinker(mobj_t *mobj)
{
    if(mobj->ddFlags & DDMF_REMOTE)
        return; // Remote mobjs are handled separately.

    // Spectres get selector = 1.
    if(mobj->type == MT_SHADOWS)
        mobj->selector = (mobj->selector & ~DDMOBJ_SELECTOR_MASK) | 1;


    // The first three bits of the selector special byte contain a
    // relative health level.
    P_UpdateHealthBits(mobj);

#if __JHERETIC__
    // Lightsources must stay where they're hooked.
    if(mobj->type == MT_LIGHTSOURCE)
    {
        if(mobj->moveDir > 0)
            mobj->pos[VZ] = P_GetFixedp(mobj->subsector, DMU_FLOOR_HEIGHT) + mobj->moveDir;
        else
            mobj->pos[VZ] = P_GetFixedp(mobj->subsector, DMU_CEILING_HEIGHT) + mobj->moveDir;
        return;
    }
#endif

    // Handle X and Y momentums
    if(mobj->mom[MX] || mobj->mom[MY] || (mobj->flags & MF_SKULLFLY))
    {
        P_MobjMoveXY(mobj);

        //// \fixme decent NOP/NULL/Nil function pointer please.
        if(mobj->thinker.function == NOPFUNC)
            return;             // mobj was removed
    }

    if(mobj->flags2 & MF2_FLOATBOB)
    {                           // Floating item bobbing motion
        // Keep it on the floor.
        mobj->pos[VZ] = FLT2FIX(mobj->floorZ);
#if __JHERETIC__
        // Negative floorclip raises the mobj off the floor.
        mobj->floorClip = -mobj->special1;
#elif __JDOOM__
        mobj->floorClip = 0;
#endif
        if(mobj->floorClip < -MAX_BOB_OFFSET)
        {
            // We don't want it going through the floor.
            mobj->floorClip = -MAX_BOB_OFFSET;
        }

        // Old floatbob used health as index, let's still increase it
        // as before (in case somebody wants to use it).
        mobj->health++;
    }
    else if(mobj->pos[VZ] != FLT2FIX(mobj->floorZ) || mobj->mom[MZ]) // GMJ 02/02/02
    {
        P_MobjMoveZ(mobj);
        if(mobj->thinker.function != P_MobjThinker) // cph - Must've been removed
            return;             // killough - mobj was removed
    }
    // non-sentient objects at rest
    else if(!(mobj->mom[MX] | mobj->mom[MY]) && !sentient(mobj) && !(mobj->player) &&
            !((mobj->flags & MF_CORPSE) && cfg.slidingCorpses))
    {
        // killough 9/12/98: objects fall off ledges if they are hanging off
        // slightly push off of ledge if hanging more than halfway off

        if(FIX2FLT(mobj->pos[VZ]) > mobj->dropOffZ && // Only objects contacting dropoff
           !(mobj->flags & MF_NOGRAVITY) && cfg.fallOff)
        {
            P_ApplyTorque(mobj);
        }
        else
        {
            mobj->intFlags &= ~MIF_FALLING;
            mobj->gear = 0;     // Reset torque
        }
    }

    if(cfg.slidingCorpses)
    {
        if((mobj->flags & MF_CORPSE ? FIX2FLT(mobj->pos[VZ]) > mobj->dropOffZ :
                                      FIX2FLT(mobj->pos[VZ]) - mobj->dropOffZ > 24) && // Only objects contacting drop off
           !(mobj->flags & MF_NOGRAVITY))    // Only objects which fall
        {
            P_ApplyTorque(mobj);    // Apply torque
        }
        else
        {
            mobj->intFlags &= ~MIF_FALLING;
            mobj->gear = 0;     // Reset torque
        }
    }

    // $vanish: dead monsters disappear after some time
    if(cfg.corpseTime && (mobj->flags & MF_CORPSE) && mobj->corpseTics != -1)
    {
        if(++mobj->corpseTics < cfg.corpseTime * TICSPERSEC)
        {
            mobj->translucency = 0; // Opaque.
        }
        else if(mobj->corpseTics < cfg.corpseTime * TICSPERSEC + VANISHTICS)
        {
            // Translucent during vanishing.
            mobj->translucency =
                ((mobj->corpseTics -
                  cfg.corpseTime * TICSPERSEC) * 255) / VANISHTICS;
        }
        else
        {
            // Too long; get rid of the corpse.
            mobj->corpseTics = -1;
            return;
        }
    }

    // cycle through states,
    // calling action functions at transitions
    if(mobj->tics != -1)
    {
        mobj->tics--;

        P_MobjAngleSRVOTicker(mobj);    // "angle-servo"; smooth actor turning

        // you can cycle through multiple states in a tic
        if(!mobj->tics)
        {
            P_MobjClearSRVO(mobj);
            if(!P_MobjChangeState(mobj, mobj->state->nextState))
                return;         // freed itself
        }
    }
    else if(!IS_CLIENT)
    {
        // check for nightmare respawn
        if(!(mobj->flags & MF_COUNTKILL))
            return;

        if(!respawnmonsters)
            return;

        mobj->moveCount++;

        if(mobj->moveCount < 12 * 35)
            return;

        if(mapTime & 31)
            return;

        if(P_Random() > 4)
            return;

        P_NightmareRespawn(mobj);
    }
}

/**
 * Spawns a mobj of "type" at the specified position.
 */
mobj_t *P_SpawnMobj3f(mobjtype_t type, float x, float y, float z)
{
    mobj_t         *mo;
    mobjinfo_t     *info = &mobjInfo[type];
    float           space;

    mo = P_MobjCreate(P_MobjThinker, x, y, z, 0, info->radius, info->height,
                      0);
    mo->type = type;
    mo->info = info;
    mo->flags = info->flags;
    mo->flags2 = info->flags2;
    mo->flags3 = info->flags3;
    mo->damage = info->damage;
    mo->health = info->spawnHealth
                   * (IS_NETGAME ? cfg.netMobHealthModifier : 1);
    mo->moveDir = DI_NODIR;

    P_SetDoomsdayFlags(mo);

    if(gameskill != SM_NIGHTMARE)
        mo->reactionTime = info->reactionTime;

    mo->lastLook = P_Random() % MAXPLAYERS;

    P_MobjSetState(mo, info->spawnState);

    // set subsector and/or block links
    P_MobjSetPosition(mo);

    //killough $dropoff_fix
    mo->floorZ   = P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT);
    mo->dropOffZ = mo->floorZ;
    mo->ceilingZ = P_GetFloatp(mo->subsector, DMU_CEILING_HEIGHT);

    if(mo->pos[VZ] == ONFLOORZ)
        mo->pos[VZ] = mo->floorZ;
    else if(mo->pos[VZ] == ONCEILINGZ)
        mo->pos[VZ] = mo->ceilingZ - mo->info->height;
    else if(mo->pos[VZ] == FLOATRANDZ)
    {
        space = mo->ceilingZ - mo->info->height - mo->floorZ;
        if(space > 48)
        {
            space -= 40;
            mo->pos[VZ] =
                ((space * P_Random()) / 256) + mo->floorZ + 40;
        }
        else
        {
            mo->pos[VZ] = mo->floorZ;
        }
    }

    mo->floorClip = 0;

    if((mo->flags2 & MF2_FLOORCLIP) &&
       mo->pos[VZ] == P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
    {
        const terraintype_t* tt = P_MobjGetFloorTerrainType(mo);

        if(tt->flags & TTF_FLOORCLIP)
        {
            mo->floorClip = 10;
        }
    }

    return mo;
}

mobj_t *P_SpawnMobj3fv(mobjtype_t type, float pos[3])
{
    return P_SpawnMobj3f(type, pos[VX], pos[VY], pos[VZ]);
}

/**
 * Queue up a spawn from the specified spot.
 */
void P_RespawnEnqueue(spawnspot_t *spot)
{
    spawnspot_t *spawnobj = &itemrespawnque[iquehead];

    memcpy(spawnobj, spot, sizeof(*spawnobj));

    itemrespawntime[iquehead] = mapTime;
    iquehead = (iquehead + 1) & (ITEMQUESIZE - 1);

    // lose one off the end?
    if(iquehead == iquetail)
        iquetail = (iquetail + 1) & (ITEMQUESIZE - 1);
}

void P_CheckRespawnQueue(void)
{
    float       pos[3];
    subsector_t *ss;
    mobj_t     *mo;
    spawnobj_t *sobj;
    int         i;

    // Only respawn items in deathmatch 2 and optionally in coop.
    if(deathmatch != 2 && (!cfg.coopRespawnItems || !IS_NETGAME || deathmatch))
        return;

    // Nothing left to respawn?
    if(iquehead == iquetail)
        return;

    // Wait at least 30 seconds
    if(mapTime - itemrespawntime[iquetail] < 30 * 35)
        return;

    // Get the attributes of the mobj from spawn parameters.
    sobj = &itemrespawnque[iquetail];

    memcpy(pos, sobj->pos, sizeof(pos));
    ss = R_PointInSubsector(pos[VX], pos[VY]);
    pos[VZ] = P_GetFloatp(ss, DMU_FLOOR_HEIGHT);

    // Spawn a teleport fog at the new spot.
    mo = P_SpawnMobj3fv(MT_IFOG, pos);
    S_StartSound(SFX_NMRRSP, mo);

    // Find which type to spawn.
    for(i = 0; i < Get(DD_NUMMOBJTYPES); ++i)
    {
        if(sobj->type == mobjInfo[i].doomedNum)
            break;
    }

    if(mobjInfo[i].flags & MF_SPAWNCEILING)
        pos[VZ] = ONCEILINGZ;
    else
        pos[VZ] = ONFLOORZ;

    mo = P_SpawnMobj3fv(i, pos);
    mo->angle = sobj->angle;

    mo->floorClip = 0;

    if((mo->flags2 & MF2_FLOORCLIP) &&
       mo->pos[VZ] == P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
    {
        const terraintype_t* tt = P_MobjGetFloorTerrainType(mo);

        if(tt->flags & TTF_FLOORCLIP)
        {
            mo->floorClip = 10;
        }
    }

    // Copy spawn attributes to the new mobj.
    memcpy(mo->spawnSpot.pos, sobj->pos, sizeof(mo->spawnSpot.pos));
    mo->spawnSpot.angle = sobj->angle;
    mo->spawnSpot.type = sobj->type;
    mo->spawnSpot.flags = sobj->thingflags;

    // Pull it from the que.
    iquetail = (iquetail + 1) & (ITEMQUESIZE - 1);
}

/**
 * Called when a player is spawned on the level.
 * Most of the player structure stays unchanged between levels.
 */
void P_SpawnPlayer(spawnspot_t *spot, int pnum)
{
    player_t   *p;
    float       pos[3];
    mobj_t     *mobj;
    int         i;

    if(pnum < 0)
        pnum = 0;
    if(pnum >= MAXPLAYERS - 1)
        pnum = MAXPLAYERS - 1;

    // Not playing?
    if(!players[pnum].plr->inGame)
        return;

    p = &players[pnum];

    if(p->playerState == PST_REBORN)
        G_PlayerReborn(pnum);

    if(spot)
    {
        pos[VX] = spot->pos[VX];
        pos[VY] = spot->pos[VY];
        pos[VZ] = ONFLOORZ;
    }
    else
    {
        pos[VX] = pos[VY] = pos[VZ] = 0;
    }

    mobj = P_SpawnMobj3fv(MT_PLAYER, pos);

    // With clients all player mobjs are remote, even the CONSOLEPLAYER.
    if(IS_CLIENT)
    {
        mobj->flags &= ~MF_SOLID;
        mobj->ddFlags = DDMF_REMOTE | DDMF_DONTDRAW;
        // The real flags are received from the server later on.
    }

    // Set color translations for player sprites.
    i = cfg.playerColor[pnum];
    if(i > 0)
        mobj->flags |= i << MF_TRANSSHIFT;

    mobj->angle = (spot? spot->angle : 0); /* $unifiedangles */
    p->plr->lookDir = 0; /* $unifiedangles */
    p->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
    mobj->player = p;
    mobj->dPlayer = p->plr;
    mobj->health = p->health;

    p->plr->mo = mobj;
    p->playerState = PST_LIVE;
    p->refire = 0;
    p->damageCount = 0;
    p->bonusCount = 0;
    p->plr->extraLight = 0;
    p->plr->fixedColorMap = 0;
    p->plr->lookDir = 0;

    if(!spot)
        p->plr->flags |= DDPF_CAMERA;

    if(p->plr->flags & DDPF_CAMERA)
    {
        p->plr->mo->pos[VZ] += (float) cfg.plrViewHeight;
        p->plr->viewHeight = 0;
    }
    else
        p->plr->viewHeight = (float) cfg.plrViewHeight;

    p->class = PCLASS_PLAYER;

    // Setup gun psprite.
    P_SetupPsprites(p);

    // Give all cards in death match mode.
    if(deathmatch)
        for(i = 0; i < NUM_KEY_TYPES; i++)
            p->keys[i] = true;

    if(pnum == CONSOLEPLAYER)
    {
        // Wake up the status bar.
        ST_Start();
        // Wake up the heads up text.
        HU_Start();
    }
}

/*
 * Spawns the passed thing into the world.
 */
void P_SpawnMapThing(spawnspot_t *th)
{
    int         i, bit;
    mobj_t     *mobj;
    float       pos[3];

    //Con_Message("x = %i, y = %i, height = %i, angle = %i, type = %i, options = %i\n",
    //            th->x, th->y, th->height, th->angle, th->type, th->options);

    // Count deathmatch start positions.
    if(th->type == 11)
    {
        if(deathmatchP < &deathmatchStarts[MAX_DM_STARTS])
        {
            memcpy(deathmatchP, th, sizeof(*th));
            deathmatchP++;
        }
        return;
    }

    // Check for players specially.
    if(th->type >= 1 && th->type <= 4)
    {
        // Register this player start.
        P_RegisterPlayerStart(th);
        return;
    }

    // Don't spawn things flagged for Multiplayer if we're not in a netgame.
    if(!IS_NETGAME && (th->flags & MTF_NOTSINGLE))
        return;

    // Don't spawn things flagged for Not Deathmatch if we're deathmatching.
    if(deathmatch && (th->flags & MTF_NOTDM))
        return;

    // Don't spawn things flagged for Not Coop if we're coop'in.
    if(IS_NETGAME && !deathmatch && (th->flags & MTF_NOTCOOP))
        return;

    // Check for apropriate skill level.
    if(gameskill == SM_BABY)
        bit = 1;
    else if(gameskill == SM_NIGHTMARE)
        bit = 4;
    else
        bit = 1 << (gameskill - 1);

    if(!(th->flags & bit))
        return;

    // Find which type to spawn.
    for(i = 0; i < Get(DD_NUMMOBJTYPES); ++i)
    {
        if(th->type == mobjInfo[i].doomedNum)
            break;
    }

    // Clients only spawn local objects.
    if(IS_CLIENT)
    {
        if(!(mobjInfo[i].flags & MF_LOCAL))
            return;
    }

    if(i == Get(DD_NUMMOBJTYPES))
        return;

    // Don't spawn keycards in deathmatch.
    if(deathmatch && mobjInfo[i].flags & MF_NOTDMATCH)
        return;

    // Check for specific disabled objects.
    if(IS_NETGAME && (th->flags & MTF_NOTSINGLE)) // Multiplayer flag.
    {
        if(cfg.noCoopWeapons && !deathmatch && i >= MT_CLIP
           && i <= MT_SUPERSHOTGUN)
            return;

        // Don't spawn any special objects in coop?
        if(cfg.noCoopAnything && !deathmatch)
            return;

        // BFG disabled in netgames?
        if(cfg.noNetBFG && i == MT_MISC25)
            return;
    }

    // Don't spawn any monsters if -nomonsters.
    if(nomonsters && (i == MT_SKULL || (mobjInfo[i].flags & MF_COUNTKILL)))
    {
        return;
    }

    pos[VX] = (float) th->pos[VX];
    pos[VY] = (float) th->pos[VY];

    if(mobjInfo[i].flags & MF_SPAWNCEILING)
        pos[VZ] = ONCEILINGZ;
    else if(mobjInfo[i].flags2 & MF2_SPAWNFLOAT)
        pos[VZ] = FLOATRANDZ;
    else
        pos[VZ] = ONFLOORZ;

    mobj = P_SpawnMobj3fv(i, pos);
    if(mobj->flags2 & MF2_FLOATBOB)
    {   // Seed random starting index for bobbing motion.
        mobj->health = P_Random();
    }

    mobj->angle = ANG45 * (th->angle / 45);
    if(mobj->tics > 0)
        mobj->tics = 1 + (P_Random() % mobj->tics);
    if(mobj->flags & MF_COUNTKILL)
        totalKills++;
    if(mobj->flags & MF_COUNTITEM)
        totalItems++;

    mobj->visAngle = mobj->angle >> 16; // "angle-servo"; smooth actor turning
    if(th->flags & MTF_AMBUSH)
        mobj->flags |= MF_AMBUSH;

    // Set the spawn info for this mobj
    memcpy(mobj->spawnSpot.pos, pos, sizeof(mobj->spawnSpot.pos));
    mobj->spawnSpot.angle = mobj->angle;
    mobj->spawnSpot.type = mobjInfo[i].doomedNum;
    mobj->spawnSpot.flags = th->flags;
}

mobj_t *P_SpawnCustomPuff(mobjtype_t type, float x, float y, float z)
{
    mobj_t *th;

    // Clients do not spawn puffs.
    if(IS_CLIENT)
        return NULL;

    z += FIX2FLT((P_Random() - P_Random()) << 10);

    th = P_SpawnMobj3f(type, x, y, z);
    th->mom[MZ] = 1;
    th->tics -= P_Random() & 3;

    // Make it last at least one tic.
    if(th->tics < 1)
        th->tics = 1;

    return th;
}

void P_SpawnPuff(float x, float y, float z)
{
    mobj_t     *th = P_SpawnCustomPuff(MT_PUFF, x, y, z);

    // Don't make punches spark on the wall.
    if(th && attackRange == MELEERANGE)
        P_MobjChangeState(th, S_PUFF3);
}

void P_SpawnBlood(float x, float y, float z, int damage)
{
    mobj_t     *th;

    z += FIX2FLT((P_Random() - P_Random()) << 10);
    th = P_SpawnMobj3f(MT_BLOOD, x, y, z);
    th->mom[MZ] = 2;
    th->tics -= P_Random() & 3;

    if(th->tics < 1)
        th->tics = 1;

    if(damage <= 12 && damage >= 9)
        P_MobjChangeState(th, S_BLOOD2);
    else if(damage < 9)
        P_MobjChangeState(th, S_BLOOD3);
}

/**
 * Moves the missile forward a bit and possibly explodes it right there.
 * @param   th  The missile to be checked.
 * @return      @c true, if the missile is at a valid location
 *              else @c false,.
 */
boolean P_CheckMissileSpawn(mobj_t *th)
{
    th->tics -= P_Random() & 3;
    if(th->tics < 1)
        th->tics = 1;

    // move a little forward so an angle can
    // be computed if it immediately explodes
    th->pos[VX] += (th->mom[MX] >> 1);
    th->pos[VY] += (th->mom[MY] >> 1);
    th->pos[VZ] += (th->mom[MZ] >> 1);

    if(!P_TryMove(th, th->pos[VX], th->pos[VY], false, false))
    {
        P_ExplodeMissile(th);
        return false;
    }

    return true;
}

/**
 * Tries to aim at a nearby monster if source is a player. Else aim is
 * taken at dest.
 *
 * @param   source      The mobj doing the shooting.
 * @param   dest        The mobj being shot at. Can be @c NULL if source
 *                      is a player.
 * @param   type        The type of mobj to be shot.
 * @return              Pointer to the newly spawned missile.
 */
mobj_t *P_SpawnMissile(mobjtype_t type, mobj_t *source, mobj_t *dest)
{
    float       pos[3];
    mobj_t     *th;
    angle_t     an;
    float       dist;
    float       slope;
    float       spawnZOff = 0;

    memcpy(pos, source->pos, sizeof(pos));

    if(source->player)
    {
        // see which target is to be aimed at
        an = source->angle;
        slope = P_AimLineAttack(source, an, 16 * 64);
        if(!cfg.noAutoAim)
            if(!lineTarget)
            {
                an += 1 << 26;
                slope = P_AimLineAttack(source, an, 16 * 64);

                if(!lineTarget)
                {
                    an -= 2 << 26;
                    slope = P_AimLineAttack(source, an, 16 * 64);
                }

                if(!lineTarget)
                {
                    an = source->angle;
                    slope =
                        tan(LOOKDIR2RAD(source->dPlayer->lookDir)) / 1.2f;
                }
            }

        if(!(source->player->plr->flags & DDPF_CAMERA))
            spawnZOff = (cfg.plrViewHeight - 9) +
                        (source->player->plr->lookDir) / 173;
    }
    else
    {
        // Type specific offset to spawn height z.
        switch(type)
        {
        case MT_TRACER:            // Revenant Tracer Missile.
            spawnZOff = 16 + 32;
            break;

        default:
            spawnZOff = 32;
            break;
        }
    }

    pos[VZ] += spawnZOff;
    pos[VZ] -= source->floorClip;

    th = P_SpawnMobj3f(FLT2FIX(pos[VX]), FLT2FIX(pos[VY]), FLT2FIX(pos[VZ]), type);

    if(th->info->seeSound)
        S_StartSound(th->info->seeSound, th);

    if(!source->player)
    {
        an = R_PointToAngle2(pos[VX], pos[VY],
                             dest->pos[VX], dest->pos[VY]);
        // fuzzy player
        if(dest->flags & MF_SHADOW)
            an += (P_Random() - P_Random()) << 20;
    }

    th->target = source;        // where it came from
    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->mom[MX] = FIX2FLT(FixedMul(th->info->speed, finecosine[an]));
    th->mom[MY] = FIX2FLT(FixedMul(th->info->speed, finesine[an]));

    if(source->player)
    {   // Allow free-aim with the BFG in deathmatch?
        if(deathmatch && cfg.netBFGFreeLook == 0 && type == MT_BFG)
            th->mom[MZ] = 0;
        else
            th->mom[MZ] = FIX2FLT(th->info->speed) * slope;
    }
    else
    {
        dist = P_ApproxDistance(dest->pos[VX] - pos[VX],
                                dest->pos[VY] - pos[VY]);
        dist /= FIX2FLT(th->info->speed);
        if(dist < 1)
            dist = 1;
        th->mom[MZ] = (dest->pos[VZ] - source->pos[VZ]) / dist;
    }

    // Make sure the speed is right (in 3D).
    dist = P_ApproxDistance(P_ApproxDistance(th->mom[MX], th->mom[MY]), th->mom[MZ]);
    if(!dist)
        dist = 1;
    dist = FIX2FLT(th->info->speed) / dist;

    th->mom[MX] *= dist;
    th->mom[MY] *= dist;
    th->mom[MZ] *= dist;

    if(P_CheckMissileSpawn(th))
        return th;

    return NULL;
}
