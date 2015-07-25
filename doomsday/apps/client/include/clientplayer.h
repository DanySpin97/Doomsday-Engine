/** @file clientplayer.h  Client-side player state.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef CLIENT_CLIENTPLAYER_H
#define CLIENT_CLIENTPLAYER_H

#include <doomsday/player.h>
#include "render/viewports.h"

struct ConsoleEffectStack;

/**
 * Information about a client player.
 *
 * @todo This is probably partially obsolete? Rename/revamp. -jk
 */
typedef struct clplayerstate_s {
    thid_t clMobjId;
    float forwardMove;
    float sideMove;
    int angle;
    angle_t turnDelta;
    int friction;
    int pendingFixes;
    thid_t pendingFixTargetClMobjId;
    angle_t pendingAngleFix;
    float pendingLookDirFix;
    coord_t pendingOriginFix[3];
    coord_t pendingMomFix[3];
} clplayerstate_t;

/**
 * Client-side player state.
 */
class ClientPlayer : public Player
{
public:
    ClientPlayer();

    viewdata_t &viewport();
    viewdata_t const &viewport() const;

    clplayerstate_t &clPlayerState();
    clplayerstate_t const &clPlayerState() const;

    ConsoleEffectStack &fxStack();
    ConsoleEffectStack const &fxStack() const;
    
private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_CLIENTPLAYER_H
