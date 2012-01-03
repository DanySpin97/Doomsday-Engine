/**\file d_main.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * Game initialization (jDoom-specific).
 */

// HEADER FILES ------------------------------------------------------------

#include <assert.h>

#include "jdoom.h"

#include "d_netsv.h"
#include "m_argv.h"
#include "p_map.h"
#include "p_saveg.h"
#include "am_map.h"
#include "g_defs.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int verbose;

boolean devParm; // checkparm of -devparm
boolean noMonstersParm; // checkparm of -nomonsters
boolean respawnParm; // checkparm of -respawn
boolean fastParm; // checkparm of -fast
boolean turboParm; // checkparm of -turbo
//boolean randomClassParm; // checkparm of -randclass

float turboMul; // Multiplier for turbo.

gamemode_t gameMode;
int gameModeBits;

// Default font colors.
float defFontRGB[3];
float defFontRGB2[3];
float defFontRGB3[3];

// The patches used in drawing the view border.
char* borderGraphics[] = {
    "Flats:FLOOR7_2", // Background.
    "BRDR_T", // Top.
    "BRDR_R", // Right.
    "BRDR_B", // Bottom.
    "BRDR_L", // Left.
    "BRDR_TL", // Top left.
    "BRDR_TR", // Top right.
    "BRDR_BR", // Bottom right.
    "BRDR_BL" // Bottom left.
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static skillmode_t startSkill;
static uint startEpisode;
static uint startMap;
static boolean autoStart;

// CODE --------------------------------------------------------------------

/**
 * Get a 32-bit integer value.
 */
int D_GetInteger(int id)
{
    switch(id)
    {
    case DD_DMU_VERSION:
        return DMUAPI_VER;

    default:
        break;
    }
    return 0;
}

/**
 * Get a pointer to the value of a named variable/constant.
 */
void* D_GetVariable(int id)
{
    static float bob[2];

    switch(id)
    {
    case DD_PLUGIN_NAME:
        return PLUGIN_NAMETEXT;

    case DD_PLUGIN_NICENAME:
        return PLUGIN_NICENAME;

    case DD_PLUGIN_VERSION_SHORT:
        return PLUGIN_VERSION_TEXT;

    case DD_PLUGIN_VERSION_LONG:
        return PLUGIN_VERSION_TEXTLONG "\n" PLUGIN_DETAILS;

    case DD_PLUGIN_HOMEURL:
        return PLUGIN_HOMEURL;

    case DD_PLUGIN_DOCSURL:
        return PLUGIN_DOCSURL;

    case DD_GAME_CONFIG:
        return gameConfigString;

    case DD_ACTION_LINK:
        return actionlinks;

    case DD_XGFUNC_LINK:
        return xgClasses;

    case DD_PSPRITE_BOB_X:
        R_GetWeaponBob(DISPLAYPLAYER, &bob[0], 0);
        return &bob[0];

    case DD_PSPRITE_BOB_Y:
        R_GetWeaponBob(DISPLAYPLAYER, 0, &bob[1]);
        return &bob[1];

    case DD_TM_FLOOR_Z:
        return (void*) &tmFloorZ;

    case DD_TM_CEILING_Z:
        return (void*) &tmCeilingZ;

    default:
        break;
    }
    return 0;
}

/**
 * Pre Game Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void D_PreInit(void)
{
    int i;

    // Configure default colors:
    switch(gameMode)
    {
    case doom2_hacx:
        defFontRGB[CR] = .85f;
        defFontRGB[CG] = 0;
        defFontRGB[CB] = 0;

        defFontRGB2[CR] = .2f;
        defFontRGB2[CG] = .9f;
        defFontRGB2[CB] = .2f;

        defFontRGB3[CR] = .2f;
        defFontRGB3[CG] = .9f;
        defFontRGB3[CB] = .2f;
        break;

    case doom_chex:
        defFontRGB[CR] = 1;
        defFontRGB[CG] = 1;
        defFontRGB[CB] = 1;

        defFontRGB2[CR] = .85f;
        defFontRGB2[CG] = 0;
        defFontRGB2[CB] = 0;

        defFontRGB3[CR] = .2f;
        defFontRGB3[CG] = .2f;
        defFontRGB3[CB] = .9f;
        break;

    default:
        defFontRGB[CR] = 1;
        defFontRGB[CG] = 1;
        defFontRGB[CB] = 1;

        defFontRGB2[CR] = .85f;
        defFontRGB2[CG] = 0;
        defFontRGB2[CB] = 0;

        defFontRGB3[CR] = 1;
        defFontRGB3[CG] = .9f;
        defFontRGB3[CB] = .4f;
        break;
    }

    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&cfg, 0, sizeof(cfg));
    cfg.playerMoveSpeed = 1;
    cfg.dclickUse = false;
    cfg.povLookAround = true;
    cfg.screenBlocks = cfg.setBlocks = 10;
    cfg.echoMsg = true;
    cfg.lookSpeed = 3;
    cfg.turnSpeed = 1;

    cfg.menuPatchReplaceMode = PRM_ALLOW_TEXT;
    cfg.menuScale = .9f;
    cfg.menuTextGlitter = .5f;
    cfg.menuShadow = 0.33f;
    cfg.menuQuitSound = true;
    cfg.menuEffectFlags = MEF_TEXT_TYPEIN|MEF_TEXT_SHADOW|MEF_TEXT_GLITTER;
    cfg.menuTextFlashColor[0] = .7f;
    cfg.menuTextFlashColor[1] = .9f;
    cfg.menuTextFlashColor[2] = 1;
    cfg.menuTextFlashSpeed = 4;
    cfg.menuCursorRotate = true;

    cfg.inludePatchReplaceMode = PRM_ALLOW_TEXT;

    cfg.hudPatchReplaceMode = PRM_ALLOW_TEXT;
    cfg.hudKeysCombine = false;
    cfg.hudShown[HUD_HEALTH] = true;
    cfg.hudShown[HUD_ARMOR] = true;
    cfg.hudShown[HUD_AMMO] = true;
    cfg.hudShown[HUD_KEYS] = true;
    cfg.hudShown[HUD_FRAGS] = true;
    cfg.hudShown[HUD_FACE] = false;
    cfg.hudShown[HUD_LOG] = true;
    for(i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // when the hud/statusbar unhides.
    {
        cfg.hudUnHide[i] = 1;
    }
    cfg.hudScale = .6f;

    memcpy(cfg.hudColor, defFontRGB2, sizeof(cfg.hudColor));
    cfg.hudColor[CA] = 1;

    cfg.hudFog = 1;
    cfg.hudIconAlpha = 1;
    cfg.xhairSize = .5f;
    cfg.xhairVitality = false;
    cfg.xhairColor[0] = 1;
    cfg.xhairColor[1] = 1;
    cfg.xhairColor[2] = 1;
    cfg.xhairColor[3] = 1;

    cfg.filterStrength = .8f;
    cfg.moveCheckZ = true;
    cfg.jumpPower = 9;
    cfg.airborneMovement = 1;
    cfg.weaponAutoSwitch = 1; // if better
    cfg.noWeaponAutoSwitchIfFiring = false;
    cfg.ammoAutoSwitch = 0; // never
    cfg.secretMsg = true;
    cfg.slidingCorpses = false;
    cfg.fastMonsters = false;
    cfg.netJumping = true;
    cfg.netEpisode = 0;
    cfg.netMap = 0;
    cfg.netSkill = SM_MEDIUM;
    cfg.netColor = 4;
    cfg.netBFGFreeLook = 0;    // allow free-aim 0=none 1=not BFG 2=All
    cfg.netMobDamageModifier = 1;
    cfg.netMobHealthModifier = 1;
    cfg.netGravity = -1;        // use map default
    cfg.plrViewHeight = DEFAULT_PLAYER_VIEWHEIGHT;
    cfg.mapTitle = true;
    cfg.hideIWADAuthor = true;

    if(gameMode == doom2_hacx)
    {
        cfg.menuTextColors[0][CR] = cfg.menuTextColors[0][CG] = cfg.menuTextColors[0][CB] = 1;
        memcpy(cfg.menuTextColors[1], defFontRGB, sizeof(cfg.menuTextColors[1]));
        cfg.menuTextColors[2][CR] = .2f;
        cfg.menuTextColors[2][CG] = .2f;
        cfg.menuTextColors[2][CB] = .9f;
        memcpy(cfg.menuTextColors[3], defFontRGB2, sizeof(cfg.menuTextColors[3]));
    }
    else
    {
        memcpy(cfg.menuTextColors[0], defFontRGB2, sizeof(cfg.menuTextColors[0]));
        cfg.menuTextColors[1][CR] = 1.f;
        cfg.menuTextColors[1][CG] = .7f;
        cfg.menuTextColors[1][CB] = .3f;
        memcpy(cfg.menuTextColors[2], defFontRGB,  sizeof(cfg.menuTextColors[2]));
        memcpy(cfg.menuTextColors[3], defFontRGB2, sizeof(cfg.menuTextColors[3]));
    }

    cfg.menuSlam = false;
    cfg.menuShortcutsEnabled = true;
    cfg.menuGameSaveSuggestName = true;

    cfg.confirmQuickGameSave = true;
    cfg.maxSkulls = true;
    cfg.allowSkullsInWalls = false;
    cfg.anyBossDeath = false;
    cfg.monstersStuckInDoors = false;
    cfg.avoidDropoffs = true;
    cfg.moveBlock = false;
    cfg.fallOff = true;
    cfg.fixOuchFace = true;
    cfg.fixStatusbarOwnedWeapons = true;

    cfg.statusbarScale = 1;
    cfg.statusbarOpacity = 1;
    cfg.statusbarCounterAlpha = 1;

    cfg.automapCustomColors = 0; // Never.
    cfg.automapL0[0] = .4f; // Unseen areas
    cfg.automapL0[1] = .4f;
    cfg.automapL0[2] = .4f;

    cfg.automapL1[0] = 1.f; // onesided lines
    cfg.automapL1[1] = 0.f;
    cfg.automapL1[2] = 0.f;

    cfg.automapL2[0] = .77f; // floor height change lines
    cfg.automapL2[1] = .6f;
    cfg.automapL2[2] = .325f;

    cfg.automapL3[0] = 1.f; // ceiling change lines
    cfg.automapL3[1] = .95f;
    cfg.automapL3[2] = 0.f;

    cfg.automapMobj[0] = 0.f;
    cfg.automapMobj[1] = 1.f;
    cfg.automapMobj[2] = 0.f;

    cfg.automapBack[0] = 0.f;
    cfg.automapBack[1] = 0.f;
    cfg.automapBack[2] = 0.f;
    cfg.automapOpacity = .7f;
    cfg.automapLineAlpha = .7f;
    cfg.automapShowDoors = true;
    cfg.automapDoorGlow = 8;
    cfg.automapHudDisplay = 2;
    cfg.automapRotate = true;
    cfg.automapBabyKeys = false;
    cfg.automapZoomSpeed = .1f;
    cfg.automapPanSpeed = .5f;
    cfg.automapPanResetOnOpen = true;
    cfg.automapOpenSeconds = AUTOMAP_OPEN_SECONDS;

    cfg.hudCheatCounterScale = .7f;

    cfg.msgCount = 4;
    cfg.msgScale = .8f;
    cfg.msgUptime = 5;
    cfg.msgAlign = 0; // Left.
    cfg.msgBlink = 5;

    if(gameMode == doom2_hacx)
    {
        cfg.msgColor[CR] = .2f;
        cfg.msgColor[CG] = .2f;
        cfg.msgColor[CB] = .9f;
    }
    else
    {
        memcpy(cfg.msgColor, defFontRGB2, sizeof(cfg.msgColor));
    }

    cfg.chatBeep = true;

    cfg.killMessages = true;
    cfg.bobWeapon = 1;
    cfg.bobView = 1;
    cfg.bobWeaponLower = true;
    cfg.cameraNoClip = true;
    cfg.respawnMonstersNightmare = true;

    cfg.weaponOrder[0] = WT_SIXTH;
    cfg.weaponOrder[1] = WT_NINETH;
    cfg.weaponOrder[2] = WT_FOURTH;
    cfg.weaponOrder[3] = WT_THIRD;
    cfg.weaponOrder[4] = WT_SECOND;
    cfg.weaponOrder[5] = WT_EIGHTH;
    cfg.weaponOrder[6] = WT_FIFTH;
    cfg.weaponOrder[7] = WT_SEVENTH;
    cfg.weaponOrder[8] = WT_FIRST;

    cfg.weaponCycleSequential = true;
    cfg.berserkAutoSwitch = true;

    // Use the DOOM transition by default.
    Con_SetInteger("con-transition", 1);

    // Do the common pre init routine;
    G_CommonPreInit();
}

/**
 * Post Game Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void D_PostInit(void)
{
    ddstring_t* path;
    Uri* uri;
    int p;

    /// \kludge Border background is different in DOOM2.
    /// \fixme Do this properly!
    if(gameModeBits & GM_ANY_DOOM2)
        borderGraphics[0] = "Flats:GRNROCK";
    else
        borderGraphics[0] = "Flats:FLOOR7_2";

    // Common post init routine
    G_CommonPostInit();

    // Initialize ammo info.
    P_InitAmmoInfo();

    // Initialize weapon info.
    P_InitWeaponInfo();

    // Game parameters.
    monsterInfight = GetDefInt("AI|Infight", 0);

    // Get skill / episode / map from parms.
    gameSkill = startSkill = SM_NOITEMS;
    startEpisode = 0;
    startMap = 0;
    autoStart = false;

    // Command line options.
    noMonstersParm = ArgCheck("-nomonsters")? true : false;
    respawnParm = ArgCheck("-respawn")? true : false;
    fastParm = ArgCheck("-fast")? true : false;
    devParm = ArgCheck("-devparm")? true : false;

    if(ArgCheck("-altdeath"))
        cfg.netDeathmatch = 2;
    else if(ArgCheck("-deathmatch"))
        cfg.netDeathmatch = 1;

    p = ArgCheck("-timer");
    if(p && p < myargc - 1 && deathmatch)
    {
        int time = atoi(Argv(p + 1));
        Con_Message("Maps will end after %d minute", time);
        if(time > 1)
            Con_Message("s");
        Con_Message(".\n");
    }

    // Turbo option.
    p = ArgCheck("-turbo");
    turboMul = 1.0f;
    if(p)
    {
        int scale = 200;

        turboParm = true;
        if(p < myargc - 1)
            scale = atoi(Argv(p + 1));
        if(scale < 10)
            scale = 10;
        if(scale > 400)
            scale = 400;

        Con_Message("turbo scale: %i%%\n", scale);
        turboMul = scale / 100.f;
    }

    // Load a saved game?
    p = ArgCheck("-loadgame");
    if(p && p < myargc - 1)
    {
        const int saveSlot = Argv(p + 1)[0] - '0';
        if(G_LoadGame(saveSlot))
        {
            // No further initialization is to be done.
            return;
        }
    }

    p = ArgCheck("-skill");
    if(p && p < myargc - 1)
    {
        startSkill = Argv(p + 1)[0] - '1';
        autoStart = true;
    }

    p = ArgCheck("-episode");
    if(p && p < myargc - 1)
    {
        startEpisode = Argv(p + 1)[0] - '1';
        startMap = 0;
        autoStart = true;
    }

    p = ArgCheck("-warp");
    if(p && p < myargc - 1)
    {
        if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
        {
            startMap = atoi(Argv(p + 1)) - 1;
            autoStart = true;
        }
        else if(p < myargc - 2)
        {
            startEpisode = Argv(p + 1)[0] - '1';
            startMap = Argv(p + 2)[0] - '1';
            autoStart = true;
        }
    }

    // Are we autostarting?
    if(autoStart)
    {
        if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
            Con_Message("Warp to Map %d, Skill %d\n", startMap+1, startSkill + 1);
        else
            Con_Message("Warp to Episode %d, Map %d, Skill %d\n", startEpisode+1, startMap+1, startSkill + 1);
    }

    // Validate episode and map.
    uri = G_ComposeMapUri((gameModeBits & (GM_DOOM|GM_DOOM_SHAREWARE|GM_DOOM_ULTIMATE))? startEpisode : 0, startMap);
    path = Uri_ComposePath(uri);
    if((autoStart || IS_NETGAME) && !P_MapExists(Str_Text(path)))
    {
        startEpisode = 0;
        startMap = 0;
    }
    Str_Delete(path);
    Uri_Delete(uri);

    if(autoStart || IS_NETGAME)
    {
        G_DeferedInitNew(startSkill, startEpisode, startMap);
    }
    else
    {
        G_StartTitle(); // Start up intro loop.
    }
}

void D_Shutdown(void)
{
    WI_Shutdown();
    G_CommonShutdown();
}
