/**
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 *
 *\author Copyright © 2000-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © Raven Software, Corp.
 *\author Copyright © 1993-1996 by id Software, Inc.
 */

/* $Id$
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/**
 * p_user.c : Player related stuff.
 *
 * Bobbing POV/weapon, movement, pending weapon, artifact usage...
 * Compiles for jDoom, jHeretic and jHexen.
 */

/**
 * \bug  This file contains a fair amount of Raven licensed code
 *       however none is included when the build target is jDoom.
 *
 *       DJS - In the process of cleaning this up I will attempt to segregate
 *       offending code ready for rewrite or split.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#if __WOLFTC__
#  include "wolftc.h"
#  include "g_common.h"
#elif __JDOOM__
#  include "jdoom.h"
#  include "g_common.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#  include "g_common.h"
#elif __JHERETIC__
#  include "jheretic.h"
#  include "g_common.h"
#  include "r_common.h"
#  include "p_inventory.h"
#elif __JHEXEN__
#  include <math.h>
#  include "jhexen.h"
#  include "p_inventory.h"
#endif

#include "p_player.h"
#include "p_tick.h" // for P_IsPaused()
#include "p_view.h"
#include "d_net.h"
#include "p_player.h"
#include "p_map.h"
#include "g_common.h"

// MACROS ------------------------------------------------------------------

#define ANG5                (ANG90/18)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

#if __JHERETIC__ || __JHEXEN__
boolean     P_TestMobjLocation(mobj_t *mobj);
#endif

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean onground;

#if __JDOOM__ || __JDOOM64__ || __WOLFTC__
int maxHealth; // 100
int healthLimit; // 200
int godModeHealth; // 100
int soulSphereLimit; // 200
int megaSphereHealth; // 200
int soulSphereHealth; // 100
int armorPoints[4]; // Green, blue, IDFA and IDKFA points.
int armorClass[4]; // Green, blue, IDFA and IDKFA armor classes.
#endif

#if __JDOOM__ || __JDOOM64__ || __WOLFTC__
classinfo_t classInfo[NUM_PLAYER_CLASSES] = {
    {   // Player
        S_PLAY,
        S_PLAY_RUN1,
        S_PLAY_ATK1,
        S_PLAY_ATK2,
        20,
        0x3C,
        {0x19, 0x32},
        {0x18, 0x28},
        2048,
        {640, 1280, 320},
        24,
# if __WOLFTC__
        sfx_blockd
# else
        sfx_noway
# endif
    }
};
#elif __JHERETIC__
classinfo_t classInfo[NUM_PLAYER_CLASSES] = {
    {   // Player
        S_PLAY,
        S_PLAY_RUN1,
        S_PLAY_ATK1,
        S_PLAY_ATK2,
        20,
        0x3C,
        {0x19, 0x32},
        {0x18, 0x28},
        2048,
        {640, 1280, 320},
        24,
        sfx_None
    },
    {   // Chicken
        S_CHICPLAY,
        S_CHICPLAY_RUN1,
        S_CHICPLAY_ATK1,
        S_CHICPLAY_ATK1,
        20,
        0x3C,
        {0x19, 0x32},
        {0x18, 0x28},
        2500,
        {640, 1280, 320},
        24,
        sfx_None
    },
};
#elif __JHEXEN__
classinfo_t classInfo[NUM_PLAYER_CLASSES] = {
    {   // Fighter
        MT_PLAYER_FIGHTER,
        S_FPLAY,
        S_FPLAY_RUN1,
        S_FPLAY_ATK1,
        S_FPLAY_ATK2,
        20,
        15 * FRACUNIT,
        0x3C,
        {0x1D, 0x3C},
        {0x1B, 0x3B},
        2048,
        {640, 1280, 320},
        18,
        SFX_PLAYER_FIGHTER_FAILED_USE,
        {25 * FRACUNIT, 20 * FRACUNIT, 15 * FRACUNIT, 5 * FRACUNIT},
        {190, 225, 234}
    },
    {   // Cleric
        MT_PLAYER_CLERIC,
        S_CPLAY,
        S_CPLAY_RUN1,
        S_CPLAY_ATK1,
        S_CPLAY_ATK3,
        18,
        10 * FRACUNIT,
        0x32,
        {0x19, 0x32},
        {0x18, 0x28},
        2048,
        {640, 1280, 320},
        18,
        SFX_PLAYER_CLERIC_FAILED_USE,
        {10 * FRACUNIT, 25 * FRACUNIT, 5 * FRACUNIT, 20 * FRACUNIT},
        {190, 212, 225}
    },
    {   // Mage
        MT_PLAYER_MAGE,
        S_MPLAY,
        S_MPLAY_RUN1,
        S_MPLAY_ATK1,
        S_MPLAY_ATK2,
        16,
        5 * FRACUNIT,
        0x2D,
        {0x16, 0x2E},
        {0x15, 0x25},
        2048,
        {640, 1280, 320},
        18,
        SFX_PLAYER_MAGE_FAILED_USE,
        {5 * FRACUNIT, 15 * FRACUNIT, 10 * FRACUNIT, 25 * FRACUNIT},
        {190, 205, 224}
    },
    {   // Pig
        MT_PIGPLAYER,
        S_PIGPLAY,
        S_PIGPLAY_RUN1,
        S_PIGPLAY_ATK1,
        S_PIGPLAY_ATK1,
        1,
        0,
        0x31,
        {0x18, 0x31},
        {0x17, 0x27},
        2048,
        {640, 1280, 320},
        18,
        SFX_NONE,
        {0, 0, 0, 0},
        {0, 0, 0}
    },
};
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#if __JHERETIC__ || __JHEXEN__
static int newTorch[MAXPLAYERS]; // Used in the torch flicker effect.
static int newTorchDelta[MAXPLAYERS];
#endif

// CODE --------------------------------------------------------------------

/**
 * Moves the given origin along a given angle.
 */
void P_Thrust(player_t *player, angle_t angle, float move)
{
    mobj_t     *plrmo = player->plr->mo;
    uint        an = angle >> ANGLETOFINESHIFT;
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    sector_t   *sector = P_GetPtrp(plrmo->subsector, DMU_SECTOR);
#endif

    if(player->powers[PT_FLIGHT] && !(plrmo->pos[VZ] <= plrmo->floorZ))
    {
        /*float xmul=1, ymul=1;

           // How about Quake-flying? -- jk
           if(quakeFly)
           {
           float ang = LOOKDIR2RAD(player->plr->lookDir);
           xmul = ymul = cos(ang);
           mo->mom[MZ] += sin(ang) * move;
           } */

        plrmo->mom[MX] += move * FIX2FLT(finecosine[an]);
        plrmo->mom[MY] += move * FIX2FLT(finesine[an]);
    }
#if __JHERETIC__
    else if(P_ToXSector(sector)->special == 15)    // Friction_Low
    {
        plrmo->mom[MX] += (move / 4) * FIX2FLT(finecosine[an]);
        plrmo->mom[MY] += (move / 4) * FIX2FLT(finesine[an]);
    }
#elif __JHEXEN__
    else if(P_MobjGetFloorType(plrmo) == FLOOR_ICE)   // Friction_Low
    {
        plrmo->mom[MX] += (move / 2) * FIX2FLT(finecosine[an]);
        plrmo->mom[MY] += (move / 2) * FIX2FLT(finesine[an]);
    }
#endif
    else
    {
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
        float       mul = XS_ThrustMul(sector);

        if(mul != 1)
            move *= mul;
#endif
        plrmo->mom[MX] += move * FIX2FLT(finecosine[an]);
        plrmo->mom[MY] += move * FIX2FLT(finesine[an]);
    }
}

/**
 * Returns true if the player is currently standing on ground
 * or on top of another mobj.
 */
boolean P_IsPlayerOnGround(player_t *player)
{
    boolean onground =
        (player->plr->mo->pos[VZ] <= player->plr->mo->floorZ);

#if __JHEXEN__
    if((player->plr->mo->flags2 & MF2_ONMOBJ) && !onground)
    {
        //mobj_t *on = onmobj;

        onground = true; //(player->plr->mo->pos[VZ] <= on->pos[VZ] + on->height);
    }
#else
    if(player->plr->mo->onMobj && !onground && !(player->plr->mo->flags2 & MF2_FLY))
    {
        mobj_t *on = player->plr->mo->onMobj;

        onground = (player->plr->mo->pos[VZ] <= on->pos[VZ] + on->height);
    }
#endif
    return onground;
}

/**
 * Will make the player jump if the latest command so instructs,
 * providing that jumping is possible.
 */
void P_CheckPlayerJump(player_t *player)
{
    float       power = (IS_CLIENT ? netJumpPower : cfg.jumpPower);

    if(player->plr->flags & DDPF_CAMERA)
        return; // Cameras don't jump.

    // Check if we are allowed to jump.
    if(cfg.jumpEnabled && power > 0 && P_IsPlayerOnGround(player) &&
       player->brain.jump && player->jumpTics <= 0)
    {
        // Jump, then!
#if __JHEXEN__
        if(player->morphTics) // Pigs don't jump that high.
            player->plr->mo->mom[MZ] = (2 * power / 3);
        else
#endif
            player->plr->mo->mom[MZ] = power;

        player->jumpTics = PCLASS_INFO(player->class)->jumpTics;

#if __JHEXEN__
        player->plr->mo->flags2 &= ~MF2_ONMOBJ;
#endif
    }
}

void P_MovePlayer(player_t *player)
{
    ddplayer_t *dp = player->plr;
    mobj_t     *plrmo = player->plr->mo;
    //ticcmd_t   *cmd = &player->plr->cmd;
    playerbrain_t *brain = &player->brain;
    classinfo_t *pClassInfo = PCLASS_INFO(player->class);
    int         speed;
    float       forwardMove, sideMove;

    // Change the angle if possible.
    /* $unifiedangles */
   /*if(IS_SERVER && player != &players[0])
    {
        if(dp->fixCounter.angles == dp->fixAcked.angles)  // all acked?
        {
#ifdef _DEBUG
            VERBOSE2( Con_Message("Server accepts client %i angle from command (ang=%i).\n",
                                  player - players, cmd->angle) );
#endif
            // Accept the client's version of the angles.
            plrmo->angle = cmd->angle << 16;
            dp->lookDir = cmd->pitch / (float) DDMAXSHORT *110;
        }
    }*/

    // Slow > fast. Fast > slow.
    speed = brain->speed;
    if(cfg.alwaysRun)
        speed = !speed;

    // Do not let the player control movement if not onground.
    onground = P_IsPlayerOnGround(player);
    if(dp->flags & DDPF_CAMERA)    // $democam
    {
        static const fixed_t cameraSpeed[2] = {0x19, 0x31};

        // Cameramen have a 3D thrusters!
        P_Thrust3D(player, plrmo->angle, dp->lookDir,
                   brain->forwardMove * cameraSpeed[speed] * 2048,
                   brain->sideMove * cameraSpeed[speed] * 2048);
    }
    else
    {
        // 'Move while in air' hack (server doesn't know about this!!).
        // Movement while in air traditionally disabled.
        float           maxMove = FIX2FLT(pClassInfo->maxMove);
        int             movemul =
            (onground || plrmo->flags2 & MF2_FLY) ? pClassInfo->moveMul :
                (cfg.airborneMovement) ? cfg.airborneMovement * 64 : 0;

        forwardMove =
            FIX2FLT(pClassInfo->forwardMove[speed]) * brain->forwardMove;
        sideMove = FIX2FLT(pClassInfo->sideMove[speed]) * brain->sideMove;

        forwardMove *= turboMul;
        sideMove    *= turboMul;

#if __JHEXEN__
        if(player->powers[PT_SPEED] && !player->morphTics)
        {
            // Adjust for a player with a speed artifact
            forwardMove = (3 * forwardMove) / 2;
            sideMove = (3 * sideMove) / 2;
        }
#endif

        forwardMove = CLAMP(forwardMove, -maxMove, maxMove);
        sideMove    = CLAMP(sideMove,    -maxMove, maxMove);

        // Players can opt to reduce their maximum possible movement speed.
        if((int) cfg.playerMoveSpeed != 1)
        {   // A divsor has been specified, apply it.
            float           m = CLAMP(cfg.playerMoveSpeed, 0, 1);

            forwardMove *= m;
            sideMove    *= m;
        }

        if(forwardMove != 0 && movemul)
        {
            P_Thrust(player, plrmo->angle, forwardMove * movemul);
        }

        if(sideMove != 0 && movemul)
        {
            P_Thrust(player, plrmo->angle - ANG90, sideMove * movemul);
        }

        if((forwardMove != 0 || sideMove != 0) &&
           player->plr->mo->state == &states[pClassInfo->normalState])
        {
            P_MobjChangeState(player->plr->mo, pClassInfo->runState);
        }

        //P_CheckPlayerJump(player); // done in a different place
    }
#if __JHEXEN__
    // Look up/down using the delta.
    /*  if(cmd->lookdirdelta)
       {
       float fd = cmd->lookdirdelta / DELTAMUL;
       float delta = fd * fd;
       if(cmd->lookdirdelta < 0) delta = -delta;
       player->plr->lookDir += delta;
       } */

    // 110 corresponds 85 degrees.
    if(player->plr->lookDir > 110)
        player->plr->lookDir = 110;
    if(player->plr->lookDir < -110)
        player->plr->lookDir = -110;
#endif
}

/**
 * Fall on your ass when dying.
 * Decrease viewheight to floor height.
 */
void P_DeathThink(player_t *player)
{
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    angle_t angle;
#endif
    angle_t delta;
    int     lookDelta;

    P_MovePsprites(player);

    onground = (player->plr->mo->pos[VZ] <= player->plr->mo->floorZ);
#if __JDOOM__ || __JDOOM64__
    if(cfg.deathLookUp)
#elif __JHERETIC__
    if(player->plr->mo->type == MT_BLOODYSKULL)
#elif __JHEXEN__
    if(player->plr->mo->type == MT_BLOODYSKULL ||
       player->plr->mo->type == MT_ICECHUNK)
#endif
    {   // Flying bloody skull
        player->plr->viewHeight = 6;
        player->plr->viewHeightDelta = 0;

        if(onground)
        {
            if(player->plr->lookDir < 60)
            {
                lookDelta = (60 - player->plr->lookDir) / 8;
                if(lookDelta < 1 && (levelTime & 1))
                {
                    lookDelta = 1;
                }
                else if(lookDelta > 6)
                {
                    lookDelta = 6;
                }
                player->plr->lookDir += lookDelta;
                player->plr->flags |= DDPF_INTERPITCH;
            }
        }
    }
#if __JHEXEN__
    else if(!(player->plr->mo->flags2 & MF2_ICEDAMAGE)) // (if not frozen)
#else
    else // fall to the ground
#endif
    {
        if(player->plr->viewHeight > 6)
            player->plr->viewHeight -= 1;

        if(player->plr->viewHeight < 6)
            player->plr->viewHeight = 6;

        player->plr->viewHeightDelta = 0;

#if __JHERETIC__ || __JHEXEN__
        if(player->plr->lookDir > 0)
            player->plr->lookDir -= 6;
        else if(player->plr->lookDir < 0)
            player->plr->lookDir += 6;

        if(abs((int) player->plr->lookDir) < 6)
            player->plr->lookDir = 0;
#endif
        player->plr->flags |= DDPF_INTERPITCH;
    }

#if __JHEXEN__
    player->update |= PSF_VIEW_HEIGHT;
#endif

    P_CalcHeight(player);

    // In netgames we won't keep tracking the killer.
    if(
#if !__JHEXEN__
        !IS_NETGAME &&
#endif
        player->attacker && player->attacker != player->plr->mo)
    {
#if __JHEXEN__
        int dir = P_FaceMobj(player->plr->mo, player->attacker, &delta);
        if(delta < ANGLE_1 * 10)
        {   // Looking at killer, so fade damage and poison counters
            if(player->damageCount)
            {
                player->damageCount--;
            }
            if(player->poisonCount)
            {
                player->poisonCount--;
            }
        }
        delta = delta / 8;
        if(delta > ANGLE_1 * 5)
        {
            delta = ANGLE_1 * 5;
        }
        if(dir)
            player->plr->mo->angle += delta; // Turn clockwise
        else
            player->plr->mo->angle -= delta; // Turn counter clockwise
#else
        angle =
            R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                            player->attacker->pos[VX], player->attacker->pos[VY]);

        delta = angle - player->plr->mo->angle;

        if(delta < ANG5 || delta > (unsigned) -ANG5)
        {
            // Looking at killer, so fade damage flash down.
            player->plr->mo->angle = angle;
            if(player->damageCount)
                player->damageCount--;
        }
        else if(delta < ANG180)
            player->plr->mo->angle += ANG5; // Turn clockwise
        else
            player->plr->mo->angle -= ANG5; // Turn counter clockwise

        player->plr->flags |= DDPF_INTERYAW;
#endif
    }
    else
    {
        if(player->damageCount)
            player->damageCount--;

#if __JHEXEN__
        if(player->poisonCount)
            player->poisonCount--;
#endif
    }

    if(player->brain.use)
    {
        if(IS_CLIENT)
        {
            NetCl_PlayerActionRequest(player, GPA_USE);
        }
        else
        {
            P_RaiseDeadPlayer(player);
        }
    }
}

/**
 * Called when a dead player wishes to be reborn.
 *
 * @param player Player that wants to be reborn.
 */
void P_RaiseDeadPlayer(player_t *player)
{
    player->playerState = PST_REBORN;
#if __JHERETIC__ || __JHEXEN__
    P_InventoryResetCursor(player);

    if(player == &players[CONSOLEPLAYER])
    {
        R_SetFilter(0);
    }

    newTorch[player - players] = 0;
    newTorchDelta[player - players] = 0;
# if __JHEXEN__
    player->plr->mo->special1 = player->class;
    if(player->plr->mo->special1 > 2)
    {
        player->plr->mo->special1 = 0;
    }
# endif
    // Let the mobj know the player has entered the reborn state.  Some
    // mobjs need to know when it's ok to remove themselves.
    player->plr->mo->special2 = 666;
#endif
}

#if __JHERETIC__ || __JHEXEN__
void P_MorphThink(player_t *player)
{
    mobj_t *pmo;

#if __JHEXEN__
    if(player->morphTics & 15)
        return;

    pmo = player->plr->mo;
    //// \fixme: Replace equality to zero checks with mom in-range.
    if(pmo->mom[MX] == 0 && pmo->mom[MY] == 0 && P_Random() < 64)
    {   // Snout sniff
        P_SetPspriteNF(player, ps_weapon, S_SNOUTATK2);
        S_StartSound(SFX_PIG_ACTIVE1, pmo); // snort
        return;
    }

    if(P_Random() < 48)
    {
        if(P_Random() < 128)
            S_StartSound(SFX_PIG_ACTIVE1, pmo);
        else
            S_StartSound(SFX_PIG_ACTIVE2, pmo);
    }
# else
    if(player->health > 0)
        P_UpdateBeak(player, &player->pSprites[ps_weapon]); // Handle beak movement

    if(IS_CLIENT || player->morphTics & 15)
        return;

    pmo = player->plr->mo;
    //// \fixme: Replace equality to zero checks with mom in-range.
    if(pmo->mom[MX] == 0 && pmo->mom[MY] == 0 && P_Random() < 160)
    {   // Twitch view angle
        pmo->angle += (P_Random() - P_Random()) << 19;
    }

    if(pmo->pos[VZ] <= pmo->floorZ && (P_Random() < 32))
    {   // Jump and noise
        pmo->mom[MZ] += 1;
        P_MobjChangeState(pmo, S_CHICPLAY_PAIN);
        return;
    }

    if(P_Random() < 48)
    {   // Just noise.
        S_StartSound(sfx_chicact, pmo);
    }
# endif
}

/**
 * \todo Need to replace this as it comes straight from Hexen.
 */
boolean P_UndoPlayerMorph(player_t *player)
{
    mobj_t     *fog = 0, *mo = 0, *pmo = 0;
    float       pos[3];
    angle_t     angle;
    int         playerNum;
    weapontype_t weapon;
    int         oldFlags, oldFlags2, oldBeast;

# if __JHEXEN__
    player->update |= PSF_MORPH_TIME | PSF_POWERS | PSF_HEALTH;
# endif

    pmo = player->plr->mo;
    memcpy(pos, pmo->pos, sizeof(pos));

    angle = pmo->angle;
    weapon = pmo->special1;
    oldFlags = pmo->flags;
    oldFlags2 = pmo->flags2;
# if __JHEXEN__
    oldBeast = pmo->type;
# else
    oldBeast = MT_CHICPLAYER;
# endif
    P_MobjChangeState(pmo, S_FREETARGMOBJ);

    playerNum = P_GetPlayerNum(player);
# if __JHEXEN__
    mo = P_SpawnMobj3fv(PCLASS_INFO(cfg.playerClass[playerNum])->mobjType, pos);
# else
    mo = P_SpawnMobj3fv(MT_PLAYER, pos);
# endif

    if(P_TestMobjLocation(mo) == false)
    {   // Didn't fit
        P_MobjRemove(mo);
        mo = P_SpawnMobj3fv(oldBeast, pos);

        mo->angle = angle;
        mo->health = player->health;
        mo->special1 = weapon;
        mo->player = player;
        mo->dPlayer = player->plr;
        mo->flags = oldFlags;
        mo->flags2 = oldFlags2;
        player->plr->mo = mo;
        player->morphTics = 2 * 35;
        return false;
    }

# if __JHEXEN__
    if(player->class == PCLASS_FIGHTER)
    {
        // The first type should be blue, and the third should be the
        // Fighter's original gold color
        if(playerNum == 0)
            mo->flags |= 2 << MF_TRANSSHIFT;
        else if(playerNum != 2)
            mo->flags |= playerNum << MF_TRANSSHIFT;
    }
    else
# endif
    if(playerNum != 0)
    {   // Set color translation bits for player sprites
        mo->flags |= playerNum << MF_TRANSSHIFT;
    }

    mo->angle = angle;
    mo->player = player;
    mo->dPlayer = player->plr;
    mo->reactionTime = 18;

    if(oldFlags2 & MF2_FLY)
    {
        mo->flags2 |= MF2_FLY;
        mo->flags |= MF_NOGRAVITY;
    }

    player->morphTics = 0;
# if __JHERETIC__
    player->powers[PT_WEAPONLEVEL2] = 0;
# endif
    player->health = mo->health = MAXHEALTH;
    player->plr->mo = mo;
# if __JHERETIC__
    player->class = PCLASS_PLAYER;
# else
    player->class = cfg.playerClass[playerNum];
# endif
    angle >>= ANGLETOFINESHIFT;
// REWRITE ME - I MATCH HEXEN UNTIL HERE

    fog =
        P_SpawnMobj3f(MT_TFOG,
                      pos[VX] + 20 * FIX2FLT(finecosine[angle]),
                      pos[VY] + 20 * FIX2FLT(finesine[angle]),
                      pos[VZ] + TELEFOGHEIGHT);
# if __JHERETIC__
    S_StartSound(sfx_telept, fog);
# else
    S_StartSound(SFX_TELEPORT, fog);
# endif
    P_PostMorphWeapon(player, weapon);

    player->update |= PSF_MORPH_TIME | PSF_HEALTH;
    player->plr->flags |= DDPF_FIXPOS | DDPF_FIXMOM;

    return true;
}
#endif

/**
 * Called once per tick by P_Ticker.
 * This routine does all the thinking for the console player during
 * netgames.
 *
 * FIXME: This should be removed in favor of the regular P_PlayerThink, which
 *        is supposed to handle all thinking regardless of whether it's a client
 *        or a server doing the thinking.
 */
void P_ClientSideThink(void)
{
    player_t *pl;
    ddplayer_t *dpl;
    mobj_t *mo;

    /*    int     i;
    ticcmd_t *cmd;
    int     fly;
*/
    if(!IS_CLIENT || !Get(DD_GAME_READY))
        return;

    pl = &players[CONSOLEPLAYER];
    dpl = pl->plr;
    mo = dpl->mo;

    // Applicable parts of the regular P_PlayerThink routine will be used.
    P_PlayerThink(pl, 1.0/TICSPERSEC);

    /*

    if(!mo)
        return;

    if(pl->playerState == PST_DEAD)
    {
        P_DeathThink(pl);
    }

    cmd = &pl->cmd; // The latest local command.
    P_CalcHeight(pl);

#if __JHEXEN__
    if(pl->morphTics > 0)
        pl->morphTics--;
#endif

    // Powers tic away.
    for(i = 0; i < NUM_POWER_TYPES; i++)
    {
        switch (i)
        {
#if __JDOOM__ || __JDOOM64__
        case PT_INVULNERABILITY:
        case PT_INVISIBILITY:
        case PT_IRONFEET:
        case PT_INFRARED:
        case PT_STRENGTH:
#elif __JHERETIC__
        case PT_INVULNERABILITY:
        case PT_WEAPONLEVEL2:
        case PT_INVISIBILITY:
        case PT_FLIGHT:
        case PT_INFRARED:
#elif __JHEXEN__
        case PT_INVULNERABILITY:
        case PT_INFRARED:
        case PT_FLIGHT:
        case PT_SPEED:
        case PT_MINOTAUR:
#endif
            if(pl->powers[i] > 0)
                pl->powers[i]--;
            else
                pl->powers[i] = 0;
            break;
        }
    }

    // Jumping.
    if(pl->jumpTics)
        pl->jumpTics--;

    P_CheckPlayerJump(pl);

    // Flying.
    fly = cmd->fly; //lookfly >> 4;
    if(fly && pl->powers[PT_FLIGHT])
    {
        if(fly != TOCENTER)
        {
            pl->flyHeight = fly * 2;
            if(!(mo->ddFlags & DDMF_FLY))
            {
                // Start flying.
                //      mo->ddFlags |= DDMF_FLY | DDMF_NOGRAVITY;
            }
        }
        else
        {
            //  mo->ddFlags &= ~(DDMF_FLY | DDMF_NOGRAVITY);
        }
    }
    // We are flying when the Fly flag is set.
    if(mo->ddFlags & DDMF_FLY)
    {
        // If we were on a mobj, we are NOT now.
        if(mo->onMobj)
            mo->onMobj = NULL;

        // Keep the fly flag in sync.
        mo->flags2 |= MF2_FLY;

        mo->mom[MZ] = (float) pl->flyHeight;
        if(pl->flyHeight)
            pl->flyHeight /= 2;
        // Do some fly-bobbing.
        if(mo->pos[VZ] > mo->floorZ && (mo->flags2 & MF2_FLY) &&
           !mo->onMobj && (levelTime & 2))
            mo->pos[VZ] += FIX2FLT(finesine[(FINEANGLES / 20 * levelTime >> 2) & FINEMASK]);
    }
#if __JHEXEN__
    else
    {
        // Clear the Fly flag.
        mo->flags2 &= ~MF2_FLY;
    }
#endif
*/

#if __JHEXEN__
/*    if(P_ToXSector(P_GetPtrp(mo->subsector, DMU_SECTOR))->special)
        P_PlayerInSpecialSector(pl);
*/
    // Set CONSOLEPLAYER thrust multiplier.
    if(mo->pos[VZ] > mo->floorZ)      // Airborne?
    {
        float       mul = (mo->ddFlags & DDMF_FLY) ? 1 : 0;
        DD_SetVariable(DD_CPLAYER_THRUST_MUL, &mul);
    }
    else
    {   float       mul =
            ((P_MobjGetFloorType(mo) == FLOOR_ICE) ? (1.0f / 2) : 1);
        DD_SetVariable(DD_CPLAYER_THRUST_MUL, &mul);
    }
#else
    // Set the proper thrust multiplier. XG gives this quite easily.
    // (The thrust multiplier is used by Cl_MovePlayer, the movement
    // "predictor"; almost all clientside movement is handled by that
    // routine, in fact.)
    {
    float       mul = XS_ThrustMul(P_GetPtrp(mo->subsector, DMU_SECTOR));
    DD_SetVariable(DD_CPLAYER_THRUST_MUL, &mul);
    }
#endif

    // Update view angles. The server fixes them if necessary.
    /*mo->angle = dpl->clAngle;
    dpl->lookDir = dpl->clLookDir;*/ /* $unifiedangles */
}

void P_PlayerThinkState(player_t *player)
{
#if __JHEXEN__
    player->worldTimer++;
#endif

    if(player->plr->mo)
    {
        mobj_t             *plrmo = player->plr->mo;

        // jDoom
        // Selector 0 = Generic (used by default)
        // Selector 1 = Fist
        // Selector 2 = Pistol
        // Selector 3 = Shotgun
        // Selector 4 = Fist
        // Selector 5 = Chaingun
        // Selector 6 = Missile
        // Selector 7 = Plasma
        // Selector 8 = BFG
        // Selector 9 = Chainsaw
        // Selector 10 = Super shotgun

        // jHexen
        // Selector 0 = Generic (used by default)
        // Selector 1..4 = Weapon 1..4
        plrmo->selector =
            (plrmo->selector & ~DDMOBJ_SELECTOR_MASK) | (player->readyWeapon + 1);

        // Reactiontime is used to prevent movement for a bit after a teleport.
        if(plrmo->reactionTime > 0)
        {
            plrmo->reactionTime--;
        }
        else
        {
            plrmo->reactionTime = 0;
        }
    }

    if(player->playerState != PST_DEAD)
    {
        // Clear the view angle interpolation flags by default.
        player->plr->flags &= ~(DDPF_INTERYAW | DDPF_INTERPITCH);
    }
}

void P_PlayerThinkCheat(player_t *player)
{
    if(player->plr->mo)
    {
        mobj_t             *plrmo = player->plr->mo;

        // fixme: do this in the cheat code
        if(P_GetPlayerCheats(player) & CF_NOCLIP)
            plrmo->flags |= MF_NOCLIP;
        else
            plrmo->flags &= ~MF_NOCLIP;
    }
}

void P_PlayerThinkAttackLunge(player_t *player)
{
    mobj_t     *plrmo = player->plr->mo;

    if(plrmo && (plrmo->flags & MF_JUSTATTACKED))
    {
        ticcmd_t   *cmd = &player->plr->cmd;

        cmd->angle = plrmo->angle >> 16;    // Don't turn.
                                            // The client must know of this.
        player->plr->flags |= DDPF_FIXANGLES;
        cmd->forwardMove = 0xc800 / 512;
        cmd->sideMove = 0;
        plrmo->flags &= ~MF_JUSTATTACKED;
    }
}

/**
 * @return              @c true, if thinking should be stopped. Otherwise,
 *                      @c false.
 */
boolean P_PlayerThinkDeath(player_t *player)
{
    if(player->playerState == PST_DEAD)
    {
        P_DeathThink(player);
        return true; // stop!
    }
    return false; // don't stop
}

void P_PlayerThinkMorph(player_t *player)
{
#if __JHERETIC__ || __JHEXEN__
    if(player->morphTics)
    {
        P_MorphThink(player);
        if(!--player->morphTics)
        {   // Attempt to undo the pig/chicken.
            P_UndoPlayerMorph(player);
        }
    }
#endif
}

/**
 * \todo Need to replace this as it comes straight from Hexen.
 */
void P_PlayerThinkMove(player_t *player)
{
    mobj_t             *plrmo = player->plr->mo;

    // Move around.
    // Reactiontime is used to prevent movement for a bit after a teleport.
    if(plrmo && !plrmo->reactionTime)
    {
        P_MovePlayer(player);

#if __JHEXEN__
        plrmo = player->plr->mo;
        if(player->powers[PT_SPEED] && !(levelTime & 1) &&
           P_ApproxDistance(plrmo->mom[MX], plrmo->mom[MY]) > 12)
        {
            mobj_t     *speedMo;
            int         playerNum;

            speedMo = P_SpawnMobj3fv(MT_PLAYER_SPEED, plrmo->pos);
            if(speedMo)
            {
                speedMo->angle = plrmo->angle;
                playerNum = P_GetPlayerNum(player);

                if(player->class == PCLASS_FIGHTER)
                {
                    // The first type should be blue, and the
                    // third should be the Fighter's original gold color.
                    if(playerNum == 0)
                    {
                        speedMo->flags |= 2 << MF_TRANSSHIFT;
                    }
                    else if(playerNum != 2)
                    {
                        speedMo->flags |= playerNum << MF_TRANSSHIFT;
                    }
                }
                else if(playerNum)
                {   // Set color translation bits for player sprites.
                    speedMo->flags |= playerNum << MF_TRANSSHIFT;
                }

                speedMo->target = plrmo;
                speedMo->special1 = player->class;
                if(speedMo->special1 > 2)
                {
                    speedMo->special1 = 0;
                }

                speedMo->sprite = plrmo->sprite;
                speedMo->floorClip = plrmo->floorClip;
                if(player == &players[CONSOLEPLAYER])
                {
                    speedMo->flags2 |= MF2_DONTDRAW;
                }
            }
        }
#endif
    }
}

void P_PlayerThinkFly(player_t *player)
{
    mobj_t             *plrmo = player->plr->mo;

    // Reactiontime is used to prevent movement for a bit after a teleport.
    if(plrmo->reactionTime)
        return;

    // Is flying allowed?
    if(player->plr->flags & DDPF_CAMERA)
        return;

    if(player->brain.fallDown)
    {
        plrmo->flags2 &= ~MF2_FLY;
        plrmo->flags &= ~MF_NOGRAVITY;
    }
    else if(player->brain.upMove != 0 && player->powers[PT_FLIGHT])
    {
        player->flyHeight = player->brain.upMove * 10;
        if(!(plrmo->flags2 & MF2_FLY))
        {
            plrmo->flags2 |= MF2_FLY;
            plrmo->flags |= MF_NOGRAVITY;
#if __JHEXEN__
            if(plrmo->mom[MZ] <= -39)
            {   // Stop falling scream.
                S_StopSound(0, plrmo);
            }
#endif
        }
    }

    // Apply Z momentum based on flight speed.
    if(plrmo->flags2 & MF2_FLY)
    {
        plrmo->mom[MZ] = (float) player->flyHeight;
        if(player->flyHeight)
        {
            player->flyHeight /= 2;
        }
    }
}

void P_PlayerThinkJump(player_t *player)
{
    if(player->plr->mo->reactionTime)
        return; // Not yet.

    // Jumping.
    if(player->jumpTics)
        player->jumpTics--;

    P_CheckPlayerJump(player);
}

void P_PlayerThinkView(player_t *player)
{
    P_CalcHeight(player);
}

void P_PlayerThinkSpecial(player_t *player)
{
#if __JHEXEN__
    int                 floorType;
#endif

    if(P_ToXSector(P_GetPtrp(player->plr->mo->subsector, DMU_SECTOR))->special)
        P_PlayerInSpecialSector(player);

#if __JHEXEN__
    if((floorType = P_MobjGetFloorType(player->plr->mo)) != FLOOR_SOLID)
    {
        P_PlayerOnSpecialFlat(player, floorType);
    }
#endif
}

/**
 * \todo Need to replace this as it comes straight from Hexen.
 */
void P_PlayerThinkSounds(player_t *player)
{
#if __JHEXEN__
    mobj_t             *plrmo = player->plr->mo;

    switch(player->class)
    {
        case PCLASS_FIGHTER:
            if(plrmo->mom[MZ] <= -35 &&
               plrmo->mom[MZ] >= -40 && !player->morphTics &&
               !S_IsPlaying(SFX_PLAYER_FIGHTER_FALLING_SCREAM, plrmo))
            {
                S_StartSound(SFX_PLAYER_FIGHTER_FALLING_SCREAM, plrmo);
            }
            break;

        case PCLASS_CLERIC:
            if(plrmo->mom[MZ] <= -35 &&
               plrmo->mom[MZ] >= -40 && !player->morphTics &&
               !S_IsPlaying(SFX_PLAYER_CLERIC_FALLING_SCREAM, plrmo))
            {
                S_StartSound(SFX_PLAYER_CLERIC_FALLING_SCREAM, plrmo);
            }
            break;

        case PCLASS_MAGE:
            if(plrmo->mom[MZ] <= -35 &&
               plrmo->mom[MZ] >= -40 && !player->morphTics &&
               !S_IsPlaying(SFX_PLAYER_MAGE_FALLING_SCREAM, plrmo))
            {
                S_StartSound(SFX_PLAYER_MAGE_FALLING_SCREAM, plrmo);
            }
            break;

        default:
            break;
    }
#endif
}

void P_PlayerThinkItems(player_t *player)
{
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    int                 arti = 0; // What to use?
    int                 pnum = player - players;

    // Moving in the inventory is handled by "invleft" and "invright" commands.
    // \fixme Switch to using impulses for consistency.

    // Check the "Use Artifact" impulse.
    if(P_GetImpulseControlState(pnum, CTL_USE_ARTIFACT))
    {
        if(player->brain.speed && artiSkipParm)
        {
            if(player->inventory[player->invPtr].type != arti_none)
            {
                arti = 0xff;
            }
        }
        else
        {
            // If the inventory is visible, just close it (depending on cfg.chooseAndUse).
            if(ST_IsInventoryVisible())
            {
                player->readyArtifact = player->inventory[player->invPtr].type;

                ST_Inventory(false); // close the inventory

                if(cfg.chooseAndUse)
                    arti = player->inventory[player->invPtr].type;
                else
                    arti = 0;
            }
            else
            {
                arti = player->inventory[player->invPtr].type;
            }
        }
    }

    // Artifact hot keys.
#if __JHERETIC__
    // Check Tome of Power and other artifact hotkeys.
    if(!arti && P_GetImpulseControlState(pnum, CTL_TOME_OF_POWER) &&
       !player->powers[PT_WEAPONLEVEL2])
    {
        arti = arti_tomeofpower;
    }
#endif
#if __JHEXEN__
    if(!arti && P_GetImpulseControlState(pnum, CTL_HEALTH) &&
       (player->plr->mo->health < MAXHEALTH))
    {
        arti = arti_health;
    }
    if(!arti && P_GetImpulseControlState(pnum, CTL_INVULNERABILITY) &&
       !player->powers[PT_INVULNERABILITY])
    {
        arti = arti_invulnerability;
    }
#endif

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    {
        // Check for other artifact keys.
        struct {
            int control;
            int artifact;
        } controlArtiMap[] = {
#if __JHERETIC__
            { CTL_INVULNERABILITY, arti_invulnerability },
            { CTL_INVISIBILITY, arti_invisibility },
            { CTL_HEALTH, arti_health },
            { CTL_SUPER_HEALTH, arti_superhealth },
            { CTL_TORCH, arti_torch },
            { CTL_FIREBOMB, arti_firebomb },
            { CTL_EGG, arti_egg },
            { CTL_FLY, arti_fly },
            { CTL_TELEPORT, arti_teleport },
            { CTL_PANIC, NUMARTIFACTS },
#endif
#if __JHEXEN__ || __JSTRIFE__
            { CTL_INVULNERABILITY, arti_invulnerability },
            { CTL_BLAST_RADIUS, arti_blastradius },
            { CTL_MYSTIC_URN, arti_superhealth },
            { CTL_TORCH, arti_torch },
            { CTL_KRATER, arti_boostmana },
            { CTL_SPEED_BOOTS, arti_speed },
            { CTL_POISONBAG, arti_poisonbag },
            { CTL_EGG, arti_egg },
            { CTL_TELEPORT, arti_teleport },
            { CTL_DARK_SERVANT, arti_summon },
            { CTL_TELEPORT_OTHER, arti_teleportother },
            { CTL_PANIC, NUMARTIFACTS },
#endif
            { 0, arti_none }              // Terminator.
        };
        int i;
        for(i = 0; controlArtiMap[i].artifact != arti_none && !arti; i++)
        {
            if(P_GetImpulseControlState(pnum, controlArtiMap[i].control))
            {
                arti = controlArtiMap[i].artifact;
                break;
            }
        }
    }
#endif

    if(arti)
    {   // Use an artifact
        if(arti == NUMARTIFACTS)
        {   // Use one of each artifact (except puzzle artifacts).
            int     i;
# if __JHEXEN__ || __JSTRIFE__
            for(i = arti_none + 1; i < arti_firstpuzzitem; ++i)
# else
            for(i = arti_none + 1; i < NUMARTIFACTS; ++i)
# endif
            {
                P_InventoryUseArtifact(player, i);
            }
        }
        else if(arti == 0xff)
        {
            P_InventoryNextArtifact(player);
        }
        else
        {
            P_InventoryUseArtifact(player, arti);
        }
    }
#endif

#if __JHERETIC__ || __JHEXEN__
    if(player->brain.upMove > 0 && !player->powers[PT_FLIGHT])
    {
        // Start flying automatically.
        P_InventoryUseArtifact(player, arti_fly);
    }
#endif
}

void P_PlayerThinkWeapons(player_t *player)
{
    playerbrain_t *brain = &player->brain;
    weapontype_t oldweapon = player->pendingWeapon;
    weapontype_t newweapon;

    if(brain->cycleWeapon)
    {
        player->pendingWeapon = P_PlayerFindWeapon(player, brain->cycleWeapon > 0);
    }

    // Check for weapon change.
#if __JHERETIC__ || __JHEXEN__
    if(brain->changeWeapon != WT_NOCHANGE && !player->morphTics)
#else
    if(brain->changeWeapon != WT_NOCHANGE)
#endif
    {
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
#  define HAS_WEAPON(x) (player->weaponOwned[x])
#  define CUR_WEAPON(x)  (palyer->readyWeapon == x)
#endif

        // The actual changing of the weapon is done when the weapon psprite
        // can do it (read: not in the middle of an attack).
        newweapon = brain->changeWeapon;
#if __JDOOM__ || __JDOOM64__
        if(newweapon == player->readyWeapon)
        {
            // Swapping between fists and chainaw.
            if(newweapon == WT_FIRST)
                newweapon = WT_EIGHTH;
            else if(newweapon == WT_EIGHTH)
                newweapon = WT_FIRST;

            // Swapping between shotgun and super-shotgun.
            if(gameMode == commercial)
            {
                if(newweapon == WT_THIRD)
                    newweapon = WT_NINETH;
                else if(newweapon == WT_NINETH)
                    newweapon = WT_THIRD;
            }
        }

# if !__JDOOM64__
        if(gameMode != commercial && newweapon == WT_NINETH)
        {
            // In non-Doom II, supershotgun is the same as normal shotgun.
            newweapon = WT_THIRD;
        }
# endif
#endif // __JDOOM__ || __JDOOM64__

#if __JHERETIC__
        // Swapping between staff and gauntlets.
        if(newweapon == player->readyWeapon)
        {
            if(newweapon == WT_FIRST)
                newweapon = WT_EIGHTH;
            else if(newweapon == WT_EIGHTH)
                newweapon = WT_FIRST;
        }
#endif // __JHERETIC__

        if(player->weaponOwned[newweapon] && newweapon != player->readyWeapon)
        {
            if(weaponInfo[newweapon][player->class].mode[0].gameModeBits
               & gameModeBits)
            {
                player->pendingWeapon = newweapon;
            }
        }
    }

    if(player->pendingWeapon != oldweapon)
    {
#if __JDOOM__ || __JDOOM64__
        player->update |= PSF_PENDING_WEAPON | PSF_READY_WEAPON;
#elif __JHEXEN__
        player->update |= PSF_PENDING_WEAPON;
#endif
    }
}

void P_PlayerThinkUse(player_t *player)
{
    if(IS_NETGAME && IS_SERVER && player != &players[CONSOLEPLAYER])
    {
        // Clients send use requests instead.
        return;
    }

    // Check for use.
    if(player->brain.use)
    {
        if(!player->useDown)
        {
            P_UseLines(player);
            player->useDown = true;
        }
    }
    else
    {
        player->useDown = false;
    }
}

void P_PlayerThinkPsprites(player_t *player)
{
    // Cycle psprites.
    P_MovePsprites(player);
}

void P_PlayerThinkPowers(player_t *player)
{
    // Counters, time dependend power ups.

#if __JDOOM__ || __JDOOM64__
    // Strength counts up to diminish fade.
    if(player->powers[PT_STRENGTH])
        player->powers[PT_STRENGTH]++;

    if(player->powers[PT_IRONFEET])
        player->powers[PT_IRONFEET]--;
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    if(player->powers[PT_INVULNERABILITY])
        player->powers[PT_INVULNERABILITY]--;

    if(player->powers[PT_INVISIBILITY])
    {
        if(!--player->powers[PT_INVISIBILITY])
            player->plr->mo->flags &= ~MF_SHADOW;
    }
#endif

#if __JDOOM__ || __JDOOM64__ || __JHEXEN__
    if(player->powers[PT_INFRARED])
        player->powers[PT_INFRARED]--;
#endif

    if(player->damageCount)
        player->damageCount--;

    if(player->bonusCount)
        player->bonusCount--;

#if __JHERETIC__ || __JHEXEN__
# if __JHERETIC__
    if(player->powers[PT_FLIGHT])
# elif __JHEXEN__
    if(player->powers[PT_FLIGHT] && IS_NETGAME)
# endif
    {
        if(!--player->powers[PT_FLIGHT])
        {
            if(player->plr->mo->pos[VZ] != player->plr->mo->floorZ && cfg.lookSpring)
            {
                player->centering = true;
            }

            player->plr->mo->flags2 &= ~MF2_FLY;
            player->plr->mo->flags &= ~MF_NOGRAVITY;
        }
    }
#endif

#if __JHERETIC__
    if(player->powers[PT_WEAPONLEVEL2])
    {
        if(!--player->powers[PT_WEAPONLEVEL2])
        {
            if((player->readyWeapon == WT_SIXTH) &&
               (player->pSprites[ps_weapon].state != &states[S_PHOENIXREADY])
               && (player->pSprites[ps_weapon].state != &states[S_PHOENIXUP]))
            {
                P_SetPsprite(player, ps_weapon, S_PHOENIXREADY);
                player->ammo[AT_FIREORB] -= USE_PHRD_AMMO_2;
                player->refire = 0;
                player->update |= PSF_AMMO;
            }
            else if((player->readyWeapon == WT_EIGHTH) ||
                    (player->readyWeapon == WT_FIRST))
            {
                player->pendingWeapon = player->readyWeapon;
                player->update |= PSF_PENDING_WEAPON;
            }
        }
    }
#endif

    // Colormaps
#if __JHERETIC__ || __JHEXEN__
    if(player->powers[PT_INFRARED])
    {
        if(player->powers[PT_INFRARED] <= BLINKTHRESHOLD)
        {
            if(player->powers[PT_INFRARED] & 8)
            {
                player->plr->fixedColorMap = 0;
            }
            else
            {
                player->plr->fixedColorMap = 1;
            }
        }
        else if(!(levelTime & 16))  /* && player == &players[CONSOLEPLAYER]) */
        {
            ddplayer_t *dp = player->plr;
            int     playerNumber = player - players;

            if(newTorch[playerNumber])
            {
                if(dp->fixedColorMap + newTorchDelta[playerNumber] > 7 ||
                   dp->fixedColorMap + newTorchDelta[playerNumber] < 1 ||
                   newTorch[playerNumber] == dp->fixedColorMap)
                {
                    newTorch[playerNumber] = 0;
                }
                else
                {
                    dp->fixedColorMap += newTorchDelta[playerNumber];
                }
            }
            else
            {
                newTorch[playerNumber] = (M_Random() & 7) + 1;
                newTorchDelta[playerNumber] =
                    (newTorch[playerNumber] ==
                     dp->fixedColorMap) ? 0 : ((newTorch[playerNumber] >
                                                dp->fixedColorMap) ? 1 : -1);
            }
        }
    }
    else
    {
        player->plr->fixedColorMap = 0;
    }
#endif

#if __JHEXEN__
    if(player->powers[PT_INVULNERABILITY])
    {
        if(player->class == PCLASS_CLERIC)
        {
            if(!(levelTime & 7) && (player->plr->mo->flags & MF_SHADOW) &&
               !(player->plr->mo->flags2 & MF2_DONTDRAW))
            {
                player->plr->mo->flags &= ~MF_SHADOW;
                if(!(player->plr->mo->flags & MF_ALTSHADOW))
                {
                    player->plr->mo->flags2 |= MF2_DONTDRAW | MF2_NONSHOOTABLE;
                }
            }
            if(!(levelTime & 31))
            {
                if(player->plr->mo->flags2 & MF2_DONTDRAW)
                {
                    if(!(player->plr->mo->flags & MF_SHADOW))
                    {
                        player->plr->mo->flags |= MF_SHADOW | MF_ALTSHADOW;
                    }
                    else
                    {
                        player->plr->mo->flags2 &=
                        ~(MF2_DONTDRAW | MF2_NONSHOOTABLE);
                    }
                }
                else
                {
                    player->plr->mo->flags |= MF_SHADOW;
                    player->plr->mo->flags &= ~MF_ALTSHADOW;
                }
            }
        }
        if(!(--player->powers[PT_INVULNERABILITY]))
        {
            player->plr->mo->flags2 &= ~(MF2_INVULNERABLE | MF2_REFLECTIVE);
            if(player->class == PCLASS_CLERIC)
            {
                player->plr->mo->flags2 &= ~(MF2_DONTDRAW | MF2_NONSHOOTABLE);
                player->plr->mo->flags &= ~(MF_SHADOW | MF_ALTSHADOW);
            }
        }
    }
    if(player->powers[PT_MINOTAUR])
    {
        player->powers[PT_MINOTAUR]--;
    }

    if(player->powers[PT_SPEED])
    {
        player->powers[PT_SPEED]--;
    }

    if(player->poisonCount && !(levelTime & 15))
    {
        player->poisonCount -= 5;
        if(player->poisonCount < 0)
        {
            player->poisonCount = 0;
        }
        P_PoisonDamage(player, player->poisoner, 1, true);
    }
#endif // __JHEXEN__
}

/**
 * Handles the updating of the player's view angles depending on the game
 * input controllers. Control states are queried from the engine. Note
 * that this is done as often as possible (i.e., on every frame) so that
 * changes will be smooth and lag-free.
 *
 * @param player        Player doing the thinking.
 * @param ticLength     Time to think, in seconds. Use as a multiplier.
 *                      Note that original game logic was always using a
 *                      tick duration of 1/35 seconds.
 */
void P_PlayerThinkLookAround(player_t *player, timespan_t ticLength)
{
    int                 playerNum = player - players;
    ddplayer_t         *plr = player->plr;
    int                 turn = 0;
    boolean             strafe = false;
    float               vel, off;
    int                 turnSpeed;
    float               offsetSensitivity = 100; // \fixme Should be done engine-side, mouse sensitivity!
    classinfo_t        *pClassInfo = PCLASS_INFO(player->class);

    if(!plr->mo || player->playerState == PST_DEAD || player->viewLock)
        return; // Nothing to control.

    turnSpeed = pClassInfo->turnSpeed[0] * TICRATE;

    // Check for extra speed.
    P_GetControlState(playerNum, CTL_SPEED, &vel, NULL);
    if(vel != 0)
    {
        // Hurry, good man!
        turnSpeed = pClassInfo->turnSpeed[1] * TICRATE;
    }

    // Check for strafe.
    P_GetControlState(playerNum, CTL_STRAFE, &vel, 0);
    strafe = (vel != 0);

    if(!strafe)
    {
        // Yaw.
        P_GetControlState(playerNum, CTL_TURN, &vel, &off);
        plr->mo->angle -= (angle_t) (FLT2FIX(turnSpeed * vel * ticLength) +
                                     (offsetSensitivity * off) / 180 * ANGLE_180);
    }

    // Look center requested?
    if(P_GetImpulseControlState(playerNum, CTL_LOOK_CENTER))
        player->centering = true;

    P_GetControlState(playerNum, CTL_LOOK, &vel, &off);
    if(player->centering)
    {
        // Automatic vertical look centering.
        float step = 8 * ticLength * TICRATE;

        if(plr->lookDir > step)
        {
            plr->lookDir -= step;
        }
        else if(plr->lookDir < -step)
        {
            plr->lookDir += step;
        }
        else
        {
            plr->lookDir = 0;
            player->centering = false;
        }
    }
    else
    {
        // Pitch as controlled by CTL_LOOK.
        plr->lookDir += 110.f/85.f * (turnSpeed/65535.f*360 * vel * ticLength +
                                      offsetSensitivity * off);
        if(plr->lookDir < -110)
            plr->lookDir = -110;
        else if(plr->lookDir > 110)
            plr->lookDir = 110;
    }
}

void P_PlayerThinkUpdateControls(player_t* player)
{
    int                 playerNum = player - players;
    classinfo_t        *pClassInfo = PCLASS_INFO(player->class);
    float               vel, off;
    int                 i;
    boolean             strafe = false;
    playerbrain_t      *brain = &player->brain;

    // Check for speed.
    P_GetControlState(playerNum, CTL_SPEED, &vel, 0);
    brain->speed = (vel != 0);

    // Check for strafe.
    P_GetControlState(playerNum, CTL_STRAFE, &vel, 0);
    strafe = (vel != 0);

    // Move status.
    P_GetControlState(playerNum, CTL_WALK, &vel, &off);
    brain->forwardMove = off + vel;
    P_GetControlState(playerNum, strafe? CTL_TURN : CTL_SIDESTEP, &vel, &off);
    if(strafe)
    {
        // Saturate.
        vel = (vel > 0? 1 : vel < 0? -1 : 0);
    }
    brain->sideMove = off + vel;

    // Flight.
    P_GetControlState(playerNum, CTL_ZFLY, &vel, &off);
    brain->upMove = off + vel;
    if(P_GetImpulseControlState(playerNum, CTL_FALL_DOWN))
    {
        brain->fallDown = true;
    }
    else
    {
        brain->fallDown = false;
    }

    // Check for look centering based on lookSpring.
    if(cfg.lookSpring &&
       (fabs(brain->forwardMove) > .333f || fabs(brain->sideMove > .333f)))
    {
        // Center view when mlook released w/lookspring, or when moving.
        player->centering = true;
    }

    // Jump.
    brain->jump = (P_GetImpulseControlState(playerNum, CTL_JUMP) != 0);

    // Use.
    brain->use = (P_GetImpulseControlState(playerNum, CTL_USE) != 0);

    // Fire.
    P_GetControlState(playerNum, CTL_ATTACK, &vel, &off);
    brain->attack = (vel + off != 0);

    // Weapons.
    brain->changeWeapon = WT_NOCHANGE;
    for(i = 0; i < NUM_WEAPON_TYPES && (CTL_WEAPON1 + i <= CTL_WEAPON0); i++)
        if(P_GetImpulseControlState(playerNum, CTL_WEAPON1 + i))
        {
            brain->changeWeapon = i;
        }

    // Weapon cycling.
    if(P_GetImpulseControlState(playerNum, CTL_NEXT_WEAPON))
    {
        brain->cycleWeapon = +1;
    }
    else if(P_GetImpulseControlState(playerNum, CTL_PREV_WEAPON))
    {
        brain->cycleWeapon = -1;
    }
    else
    {
        brain->cycleWeapon = 0;
    }
}

/**
 * Main thinker function for players. Handles both single player and
 * multiplayer games, as well as all the different types of players
 * (normal/camera).
 *
 * Functionality is divided to various other functions whose name begins
 * with "P_PlayerThink".
 *
 * @param player        Player that is doing the thinking.
 * @param ticLength     How much time has passed in the game world, in
 *                      seconds. For instance, to be used as a multiplier
 *                      on turning.
 */
void P_PlayerThink(player_t *player, timespan_t ticLength)
{
    if(P_IsPaused())
        return;

    if(G_GetGameState() != GS_LEVEL)
    {
        // Just check the controls in case some UI stuff is relying on them
        // (like intermission).
        P_PlayerThinkUpdateControls(player);
        return;
    }

    P_PlayerThinkState(player);

    // Adjust turn angles and look direction. This is done in fractional time.
    P_PlayerThinkLookAround(player, ticLength);

    if(!M_CheckTrigger(DD_GetVariable(DD_SHARED_FIXED_TRIGGER), ticLength))
        return; // It's too soon.

    P_PlayerThinkUpdateControls(player);

    if(!IS_CLIENT) // Locally only.
    {
        P_PlayerThinkCamera(player); // $democam
        P_PlayerThinkCheat(player);
        P_PlayerThinkAttackLunge(player);
    }

    if(P_PlayerThinkDeath(player))
        return; // I'm dead!

    if(!IS_CLIENT) // Locally only.
    {
        P_PlayerThinkMorph(player);
        P_PlayerThinkMove(player);
    }

    P_PlayerThinkFly(player);
    P_PlayerThinkJump(player);
    P_PlayerThinkView(player);
    P_PlayerThinkSpecial(player);

    if(!IS_CLIENT) // Locally only.
    {
        P_PlayerThinkSounds(player);
        P_PlayerThinkItems(player);
    }

    P_PlayerThinkUse(player);
    P_PlayerThinkWeapons(player);
    P_PlayerThinkPsprites(player);
    P_PlayerThinkPowers(player);
}
