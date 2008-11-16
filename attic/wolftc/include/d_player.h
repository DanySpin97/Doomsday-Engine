/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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

#ifndef __D_PLAYER__
#define __D_PLAYER__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

// The player data structure depends on a number
// of other structs: items (internal inventory),
// animation states (closely tied to the sprites
// used to represent them, unfortunately).
#include "d_items.h"
#include "p_pspr.h"

// In addition, the player is just a special
// case of the generic moving object/actor.
#include "p_mobj.h"

#include "g_controls.h"

#ifdef __GNUG__
#pragma interface
#endif

//
// Player states.
//
typedef enum {
    // Playing or camping.
    PST_LIVE,
    // Dead on the ground, view follows killer.
    PST_DEAD,
    // Ready to restart/respawn???
    PST_REBORN
} playerstate_t;

//
// Player internal flags, for cheats and debug.
//
typedef enum {
    // No clipping, walk through barriers.
    CF_NOCLIP = 1,
    // No damage, no health loss.
    CF_GODMODE = 2,
    // Not really a cheat, just a debug aid.
    CF_NOMOMENTUM = 4
} cheat_t;

typedef struct player_s {
    ddplayer_t     *plr; // Pointer to the engine's player data.
    playerstate_t   playerState;
    playerclass_t   class; // Player class type.
    playerbrain_t   brain;


    float           bob; // Bounded/scaled total momentum.

    // This is only used between levels,
    // mo->health is used during levels.
    int             health;
    int             armorPoints;
    // Armor type is 0-2.
    int             armorType;

    // Power ups. invinc and invis are tic counters.
    int             powers[NUM_POWER_TYPES];
    boolean         keys[NUM_KEY_TYPES];
    boolean         backpack;

    int             frags[MAXPLAYERS];
    weapontype_t    readyWeapon;

    // Is wp_nochange if not changing.
    weapontype_t    pendingWeapon;

    struct playerweapon_s {
        boolean         owned;
    } weapons[NUM_WEAPON_TYPES];
    struct playerammo_s {
        int             owned;
        int             max;
    } ammo[NUM_AMMO_TYPES];

    // True if button down last tic.
    int             attackDown;
    int             useDown;

    // Bit flags, for cheats and debug.
    // See cheat_t, above.
    int             cheats;

    // Refired shots are less accurate.
    int             refire;

    // For intermission stats.
    int             killCount;
    int             itemCount;
    int             secretCount;

    // For screen flashing (red or bright).
    int             damageCount;
    int             bonusCount;

    // Who did damage (NULL for floors/ceilings).
    mobj_t*         attacker;

    // Player skin colorshift,
    //  0-3 for which color to draw player.
    int             colorMap;

    // Overlay view sprites (gun, etc).
    pspdef_t        pSprites[NUMPSPRITES];

    // True if secret level has been done.
    boolean         didSecret;

    // The player's view pitch is centering back to zero.
    boolean         centering;

    // The player can jump if this counter is zero.
    int             jumpTics;

    int             update, startSpot;

    // Target view to a mobj (NULL=disabled).
    mobj_t*         viewLock;      // $democam
    int             lockFull;

    int             flyHeight;
} player_t;

typedef struct {
    boolean         inGame; // Whether the player is in game

    // Player stats, kills, collected items etc.
    int             kills;
    int             items;
    int             secret;
    int             time;
    int             frags[MAXPLAYERS];
    int             score; // Current score on entry, modified on return
} wbplayerstruct_t;

typedef struct {
    int             epsd; // Episode # (0-2)
    boolean         didSecret; // If true, splash the secret level.

    // Previous and next levels, origin 0.
    int             last;
    int             next;

    int             maxKills;
    int             maxItems;
    int             maxSecret;
    int             maxFrags;
    int             parTime;
    int             pNum; // Index of this player in game.

    wbplayerstruct_t plyr[MAXPLAYERS];
} wbstartstruct_t;

#endif
