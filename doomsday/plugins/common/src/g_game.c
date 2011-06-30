/**\file g_game.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 1999-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * Top-level (common) game routines.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "dmu_lib.h"
#include "fi_lib.h"
#include "hu_lib.h"
#include "p_saveg.h"
#include "g_controls.h"
#include "g_eventsequence.h"
#include "p_mapsetup.h"
#include "p_user.h"
#include "p_actor.h"
#include "p_tick.h"
#include "am_map.h"
#include "hu_stuff.h"
#include "hu_menu.h"
#include "hu_log.h"
#include "hu_chat.h"
#include "hu_msg.h"
#include "hu_pspr.h"
#include "g_common.h"
#include "g_update.h"
#include "g_eventsequence.h"
#include "d_net.h"
#include "x_hair.h"
#include "p_player.h"
#include "r_common.h"
#include "p_mapspec.h"
#include "p_start.h"
#include "p_inventory.h"
#if __JHERETIC__ || __JHEXEN__
# include "p_inventory.h"
# include "hu_inventory.h"
#endif

// MACROS ------------------------------------------------------------------

#define BODYQUEUESIZE       (32)

#define UNNAMEDMAP          "Unnamed"
#define NOTAMAPNAME         "N/A"
#define READONLYCVAR        CVF_READ_ONLY|CVF_NO_MAX|CVF_NO_MIN|CVF_NO_ARCHIVE

// TYPES -------------------------------------------------------------------

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
struct missileinfo_s {
    mobjtype_t  type;
    float       speed[2];
}
MonsterMissileInfo[] =
{
#if __JDOOM__ || __JDOOM64__
    {MT_BRUISERSHOT, {15, 20}},
    {MT_HEADSHOT, {10, 20}},
    {MT_TROOPSHOT, {10, 20}},
# if __JDOOM64__
    {MT_BRUISERSHOTRED, {15, 20}},
    {MT_NTROSHOT, {20, 40}},
# endif
#elif __JHERETIC__
    {MT_IMPBALL, {10, 20}},
    {MT_MUMMYFX1, {9, 18}},
    {MT_KNIGHTAXE, {9, 18}},
    {MT_REDAXE, {9, 18}},
    {MT_BEASTBALL, {12, 20}},
    {MT_WIZFX1, {18, 24}},
    {MT_SNAKEPRO_A, {14, 20}},
    {MT_SNAKEPRO_B, {14, 20}},
    {MT_HEADFX1, {13, 20}},
    {MT_HEADFX3, {10, 18}},
    {MT_MNTRFX1, {20, 26}},
    {MT_MNTRFX2, {14, 20}},
    {MT_SRCRFX1, {20, 28}},
    {MT_SOR2FX1, {20, 28}},
#endif
    {-1, {-1, -1}}                  // Terminator
};
#endif

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(CycleTextureGamma);
D_CMD(EndGame);
D_CMD(HelpScreen);
D_CMD(ListMaps);
D_CMD(LoadGame);
D_CMD(LoadGameName);
D_CMD(QuickLoadGame);
D_CMD(QuickSaveGame);
D_CMD(SaveGame);
D_CMD(SaveGameName);

void    G_PlayerReborn(int player);
void    G_InitNew(skillmode_t skill, uint episode, uint map);
void    G_DoInitNew(void);
void    G_DoReborn(int playernum);
void    G_DoLoadMap(void);
void    G_DoNewGame(void);
void    G_DoLoadGame(void);
void    G_DoPlayDemo(void);
void    G_DoMapCompleted(void);
void    G_DoVictory(void);
void    G_DoWorldDone(void);
void    G_DoSaveGame(void);
void    G_DoScreenShot(void);
boolean G_ValidateMap(uint *episode, uint *map);

#if __JHEXEN__
void    G_DoSingleReborn(void);
void    H2_PageTicker(void);
void    H2_AdvanceDemo(void);
#endif

void    G_StopDemo(void);

int Hook_DemoStop(int hookType, int val, void* paramaters);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

game_config_t cfg; // The global cfg.

int debugSound; // Debug flag for displaying sound info.

skillmode_t dSkill;

skillmode_t gameSkill;
uint gameEpisode;
uint gameMap;

uint nextMap;
#if __JHEXEN__
uint nextMapEntryPoint;
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
boolean secretExit;
#endif

#if __JHEXEN__
// Position indicator for cooperative net-play reborn
uint rebornPosition;
#endif
#if __JHEXEN__
uint mapHub = 0;
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
boolean respawnMonsters;
#endif
boolean monsterInfight;

boolean paused;
boolean sendPause; // Send a pause event next tic.
boolean userGame = false; // Ok to save / end game.
boolean deathmatch; // Only if started as net death.
player_t players[MAXPLAYERS];

int mapStartTic; // Game tic at map start.
int totalKills, totalItems, totalSecret; // For intermission.

boolean singledemo; // Quit after playing a demo from cmdline.
boolean briefDisabled = false;

boolean precache = true; // If @c true, load all graphics at start.
boolean customPal = false; // If @c true, a non-IWAD palette is in use.

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
wbstartstruct_t wmInfo; // Params for world map / intermission.
#endif

// Game Action Variables:
#define GA_SAVEGAME_NAME_LASTINDEX      (24)
#define GA_SAVEGAME_NAME_MAXLENGTH      (GA_SAVEGAME_NAME_LASTINDEX+1)

int gaSaveGameSlot = 0;
boolean gaSaveGameGenerateName = true;
char gaSaveGameName[GA_SAVEGAME_NAME_MAXLENGTH];
int gaLoadGameSlot = 0;

#if __JDOOM__ || __JDOOM64__
mobj_t *bodyQueue[BODYQUEUESIZE];
int bodyQueueSlot;
#endif

// vars used with game status cvars
int gsvInMap = 0;
int gsvCurrentMusic = 0;
int gsvMapMusic = -1;

int gsvArmor = 0;
int gsvHealth = 0;

#if !__JHEXEN__
int gsvKills = 0;
int gsvItems = 0;
int gsvSecrets = 0;
#endif

int gsvCurrentWeapon;
int gsvWeapons[NUM_WEAPON_TYPES];
int gsvKeys[NUM_KEY_TYPES];
int gsvAmmo[NUM_AMMO_TYPES];

char *gsvMapName = NOTAMAPNAME;

#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
int gsvInvItems[NUM_INVENTORYITEM_TYPES];
#endif

#if __JHEXEN__
int gsvWPieces[4];
#endif

static gamestate_t gameState = GS_STARTUP;

cvartemplate_t gamestatusCVars[] = {
   {"game-state", READONLYCVAR, CVT_INT, &gameState, 0, 0},
   {"game-state-map", READONLYCVAR, CVT_INT, &gsvInMap, 0, 0},
   {"game-paused", READONLYCVAR, CVT_INT, &paused, 0, 0},
   {"game-skill", READONLYCVAR, CVT_INT, &gameSkill, 0, 0},

   {"map-id", READONLYCVAR, CVT_INT, &gameMap, 0, 0},
   {"map-name", READONLYCVAR, CVT_CHARPTR, &gsvMapName, 0, 0},
   {"map-episode", READONLYCVAR, CVT_INT, &gameEpisode, 0, 0},
#if __JHEXEN__
   {"map-hub", READONLYCVAR, CVT_INT, &mapHub, 0, 0},
#endif
   {"game-music", READONLYCVAR, CVT_INT, &gsvCurrentMusic, 0, 0},
   {"map-music", READONLYCVAR, CVT_INT, &gsvMapMusic, 0, 0},
#if !__JHEXEN__
   {"game-stats-kills", READONLYCVAR, CVT_INT, &gsvKills, 0, 0},
   {"game-stats-items", READONLYCVAR, CVT_INT, &gsvItems, 0, 0},
   {"game-stats-secrets", READONLYCVAR, CVT_INT, &gsvSecrets, 0, 0},
#endif

   {"player-health", READONLYCVAR, CVT_INT, &gsvHealth, 0, 0},
   {"player-armor", READONLYCVAR, CVT_INT, &gsvArmor, 0, 0},
   {"player-weapon-current", READONLYCVAR, CVT_INT, &gsvCurrentWeapon, 0, 0},

#if __JDOOM__ || __JDOOM64__
   // Ammo
   {"player-ammo-bullets", READONLYCVAR, CVT_INT, &gsvAmmo[AT_CLIP], 0, 0},
   {"player-ammo-shells", READONLYCVAR, CVT_INT, &gsvAmmo[AT_SHELL], 0, 0},
   {"player-ammo-cells", READONLYCVAR, CVT_INT, &gsvAmmo[AT_CELL], 0, 0},
   {"player-ammo-missiles", READONLYCVAR, CVT_INT, &gsvAmmo[AT_MISSILE], 0, 0},
   // Weapons
   {"player-weapon-fist", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIRST], 0, 0},
   {"player-weapon-pistol", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SECOND], 0, 0},
   {"player-weapon-shotgun", READONLYCVAR, CVT_INT, &gsvWeapons[WT_THIRD], 0, 0},
   {"player-weapon-chaingun", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FOURTH], 0, 0},
   {"player-weapon-mlauncher", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIFTH], 0, 0},
   {"player-weapon-plasmarifle", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SIXTH], 0, 0},
   {"player-weapon-bfg", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SEVENTH], 0, 0},
   {"player-weapon-chainsaw", READONLYCVAR, CVT_INT, &gsvWeapons[WT_EIGHTH], 0, 0},
   {"player-weapon-sshotgun", READONLYCVAR, CVT_INT, &gsvWeapons[WT_NINETH], 0, 0},
   // Keys
   {"player-key-blue", READONLYCVAR, CVT_INT, &gsvKeys[KT_BLUECARD], 0, 0},
   {"player-key-yellow", READONLYCVAR, CVT_INT, &gsvKeys[KT_YELLOWCARD], 0, 0},
   {"player-key-red", READONLYCVAR, CVT_INT, &gsvKeys[KT_REDCARD], 0, 0},
   {"player-key-blueskull", READONLYCVAR, CVT_INT, &gsvKeys[KT_BLUESKULL], 0, 0},
   {"player-key-yellowskull", READONLYCVAR, CVT_INT, &gsvKeys[KT_YELLOWSKULL], 0, 0},
   {"player-key-redskull", READONLYCVAR, CVT_INT, &gsvKeys[KT_REDSKULL], 0, 0},
#elif __JHERETIC__
   // Ammo
   {"player-ammo-goldwand", READONLYCVAR, CVT_INT, &gsvAmmo[AT_CRYSTAL], 0, 0},
   {"player-ammo-crossbow", READONLYCVAR, CVT_INT, &gsvAmmo[AT_ARROW], 0, 0},
   {"player-ammo-dragonclaw", READONLYCVAR, CVT_INT, &gsvAmmo[AT_ORB], 0, 0},
   {"player-ammo-hellstaff", READONLYCVAR, CVT_INT, &gsvAmmo[AT_RUNE], 0, 0},
   {"player-ammo-phoenixrod", READONLYCVAR, CVT_INT, &gsvAmmo[AT_FIREORB], 0, 0},
   {"player-ammo-mace", READONLYCVAR, CVT_INT, &gsvAmmo[AT_MSPHERE], 0, 0},
    // Weapons
   {"player-weapon-staff", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIRST], 0, 0},
   {"player-weapon-goldwand", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SECOND], 0, 0},
   {"player-weapon-crossbow", READONLYCVAR, CVT_INT, &gsvWeapons[WT_THIRD], 0, 0},
   {"player-weapon-dragonclaw", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FOURTH], 0, 0},
   {"player-weapon-hellstaff", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIFTH], 0, 0},
   {"player-weapon-phoenixrod", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SIXTH], 0, 0},
   {"player-weapon-mace", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SEVENTH], 0, 0},
   {"player-weapon-gauntlets", READONLYCVAR, CVT_INT, &gsvWeapons[WT_EIGHTH], 0, 0},
   // Keys
   {"player-key-yellow", READONLYCVAR, CVT_INT, &gsvKeys[KT_YELLOW], 0, 0},
   {"player-key-green", READONLYCVAR, CVT_INT, &gsvKeys[KT_GREEN], 0, 0},
   {"player-key-blue", READONLYCVAR, CVT_INT, &gsvKeys[KT_BLUE], 0, 0},
   // Inventory items
   {"player-artifact-ring", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_INVULNERABILITY], 0, 0},
   {"player-artifact-shadowsphere", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_INVISIBILITY], 0, 0},
   {"player-artifact-crystalvial", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_HEALTH], 0, 0},
   {"player-artifact-mysticurn", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_SUPERHEALTH], 0, 0},
   {"player-artifact-tomeofpower", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TOMBOFPOWER], 0, 0},
   {"player-artifact-torch", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TORCH], 0, 0},
   {"player-artifact-firebomb", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_FIREBOMB], 0, 0},
   {"player-artifact-egg", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_EGG], 0, 0},
   {"player-artifact-wings", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_FLY], 0, 0},
   {"player-artifact-chaosdevice", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TELEPORT], 0, 0},
#elif __JHEXEN__
   // Mana
   {"player-mana-blue", READONLYCVAR, CVT_INT, &gsvAmmo[AT_BLUEMANA], 0, 0},
   {"player-mana-green", READONLYCVAR, CVT_INT, &gsvAmmo[AT_GREENMANA], 0, 0},
   // Keys
   {"player-key-steel", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY1], 0, 0},
   {"player-key-cave", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY2], 0, 0},
   {"player-key-axe", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY3], 0, 0},
   {"player-key-fire", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY4], 0, 0},
   {"player-key-emerald", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY5], 0, 0},
   {"player-key-dungeon", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY6], 0, 0},
   {"player-key-silver", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY7], 0, 0},
   {"player-key-rusted", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY8], 0, 0},
   {"player-key-horn", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY9], 0, 0},
   {"player-key-swamp", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEYA], 0, 0},
   {"player-key-castle", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEYB], 0, 0},
   // Weapons
   {"player-weapon-first", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIRST], 0, 0},
   {"player-weapon-second", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SECOND], 0, 0},
   {"player-weapon-third", READONLYCVAR, CVT_INT, &gsvWeapons[WT_THIRD], 0, 0},
   {"player-weapon-fourth", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FOURTH], 0, 0},
   // Weapon Pieces
   {"player-weapon-piece1", READONLYCVAR, CVT_INT, &gsvWPieces[0], 0, 0},
   {"player-weapon-piece2", READONLYCVAR, CVT_INT, &gsvWPieces[1], 0, 0},
   {"player-weapon-piece3", READONLYCVAR, CVT_INT, &gsvWPieces[2], 0, 0},
   {"player-weapon-allpieces", READONLYCVAR, CVT_INT, &gsvWPieces[3], 0, 0},
   // Inventory items
   {"player-artifact-defender", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_INVULNERABILITY], 0, 0},
   {"player-artifact-quartzflask", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_HEALTH], 0, 0},
   {"player-artifact-mysticurn", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_SUPERHEALTH], 0, 0},
   {"player-artifact-mysticambit", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_HEALINGRADIUS], 0, 0},
   {"player-artifact-darkservant", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_SUMMON], 0, 0},
   {"player-artifact-torch", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TORCH], 0, 0},
   {"player-artifact-porkalator", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_EGG], 0, 0},
   {"player-artifact-wings", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_FLY], 0, 0},
   {"player-artifact-repulsion", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_BLASTRADIUS], 0, 0},
   {"player-artifact-flechette", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_POISONBAG], 0, 0},
   {"player-artifact-banishment", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TELEPORTOTHER], 0, 0},
   {"player-artifact-speed", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_SPEED], 0, 0},
   {"player-artifact-might", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_BOOSTMANA], 0, 0},
   {"player-artifact-bracers", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_BOOSTARMOR], 0, 0},
   {"player-artifact-chaosdevice", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TELEPORT], 0, 0},
   {"player-artifact-skull", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZSKULL], 0, 0},
   {"player-artifact-heart", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMBIG], 0, 0},
   {"player-artifact-ruby", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMRED], 0, 0},
   {"player-artifact-emerald1", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMGREEN1], 0, 0},
   {"player-artifact-emerald2", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMGREEN2], 0, 0},
   {"player-artifact-sapphire1", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMBLUE1], 0, 0},
   {"player-artifact-sapphire2", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMBLUE2], 0, 0},
   {"player-artifact-daemoncodex", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZBOOK1], 0, 0},
   {"player-artifact-liberoscura", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZBOOK2], 0, 0},
   {"player-artifact-flamemask", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZSKULL2], 0, 0},
   {"player-artifact-glaiveseal", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZFWEAPON], 0, 0},
   {"player-artifact-holyrelic", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZCWEAPON], 0, 0},
   {"player-artifact-sigilmagus", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZMWEAPON], 0, 0},
   {"player-artifact-gear1", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEAR1], 0, 0},
   {"player-artifact-gear2", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEAR2], 0, 0},
   {"player-artifact-gear3", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEAR3], 0, 0},
   {"player-artifact-gear4", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEAR4], 0, 0},
#endif
   {NULL}
};

ccmdtemplate_t gameCmds[] = {
    { "endgame",        "",     CCmdEndGame },
    { "helpscreen",     "",     CCmdHelpScreen },
    { "listmaps",       "",     CCmdListMaps },
    { "loadgame",       "s",    CCmdLoadGameName },
    { "loadgame",       "",     CCmdLoadGame },
    { "quickload",      "",     CCmdQuickLoadGame },
    { "quicksave",      "",     CCmdQuickSaveGame },
    { "savegame",       "s*",   CCmdSaveGameName },
    { "savegame",       "",     CCmdSaveGame },
    { "togglegamma",    "",     CCmdCycleTextureGamma },
    { NULL }
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static uint dEpisode;
static uint dMap;

static gameaction_t gameAction;

// CODE --------------------------------------------------------------------

void G_Register(void)
{
    int i;

    for(i = 0; gamestatusCVars[i].name; ++i)
        Con_AddVariable(gamestatusCVars + i);

    for(i = 0; gameCmds[i].name; ++i)
        Con_AddCommand(gameCmds + i);
}

void G_SetGameAction(gameaction_t action)
{
    if(gameAction == GA_QUIT)
        return;

    if(gameAction != action)
        gameAction = action;
}

gameaction_t G_GetGameAction(void)
{
    return gameAction;
}

/**
 * Common Pre Game Initialization routine.
 * Game-specfic pre init actions should be placed in eg D_PreInit() (for jDoom)
 */
void G_CommonPreInit(void)
{
    verbose = ArgExists("-verbose");

    // Register hooks.
    Plug_AddHook(HOOK_DEMO_STOP, Hook_DemoStop);

    // Setup the players.
    { int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        players[i].plr = DD_GetPlayer(i);
        players[i].plr->extraData = (void*) &players[i];
    }}

    G_RegisterBindClasses();
    G_RegisterPlayerControls();
    P_RegisterMapObjs();

    // Add our cvars and ccmds to the console databases.
    G_ConsoleRegistration();    // Main command list.
    D_NetConsoleRegistration(); // For network.
    G_Register();               // Read-only game status cvars (for playsim).
    G_ControlRegister();        // For controls/input.
    SV_Register();              // Game-save system.
    Hu_MenuRegister();          // For the menu.
    GUI_Register();             // For the UI library.
    Hu_MsgRegister();           // For the game messages.
    ST_Register();              // For the hud/statusbar.
    WI_Register();              // For the interlude/intermission.
    X_Register();               // For the crosshair.
    FI_StackRegister();         // For the InFine lib.
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    XG_Register();
#endif

    Con_SetString2("map-name", NOTAMAPNAME, SVF_WRITE_OVERRIDE);
}

#if __JHEXEN__
/**
 * \todo all this swapping colors around is rather silly, why not simply
 * reorder the translation tables at load time?
 */
void R_GetTranslation(int plrClass, int plrColor, int* tclass, int* tmap)
{
    *tclass = 1;

    if(plrColor == 0)
        *tmap = 1;
    else if(plrColor == 1)
        *tmap = 0;
    else
        *tmap = plrColor;

    // Fighter's colors are a bit different.
    if(plrClass == PCLASS_FIGHTER && *tmap > 1)
        *tclass = 0;
}

void R_SetTranslation(mobj_t* mo)
{
    if(!(mo->flags & MF_TRANSLATION))
    {   // No translation.
        mo->tmap = mo->tclass = 0;
    }
    else
    {
        int tclass, tmap;

        tmap = (mo->flags & MF_TRANSLATION) >> MF_TRANSSHIFT;

        if(mo->player)
        {
            tclass = 1;

            if(mo->player->class == PCLASS_FIGHTER)
            {   // Fighter's colors are a bit different.
                if(tmap == 0)
                    tmap = 2;
                else if(tmap == 2)
                    tmap = 0;
                else
                    tclass = 0;
            }

            mo->tclass = tclass;
        }
        else
            tclass = mo->special1;

        mo->tmap = tmap;
        mo->tclass = tclass;
    }
}
#endif

void R_LoadColorPalettes(void)
{
#define PALLUMPNAME         "PLAYPAL"
#define PALENTRIES          (256)
#define PALID               (0)

    lumpnum_t lumpNum = W_GetLumpNumForName(PALLUMPNAME);
    uint8_t data[PALENTRIES*3];

    // Record whether we are using a custom palette.
    customPal = !W_LumpIsFromIWAD(lumpNum);

    W_ReadLumpSection(lumpNum, (char*)data, 0 + PALID * (PALENTRIES * 3), PALENTRIES * 3);
    R_CreateColorPalette("R8G8B8", PALLUMPNAME, data, PALENTRIES);

    /**
     * Create the translation tables to map the green color ramp to gray,
     * brown, red.
     *
     * \note Assumes a given structure of the PLAYPAL. Could be read from a
     * lump instead?
     */
#if __JDOOM__ || __JDOOM64__
    {
    byte* translationtables = (byte*) DD_GetVariable(DD_TRANSLATIONTABLES_ADDRESS);
    int i;

    // Translate just the 16 green colors.
    for(i = 0; i < 256; ++i)
    {
        if(i >= 0x70 && i <= 0x7f)
        {
            // Map green ramp to gray, brown, red.
            translationtables[i] = 0x60 + (i & 0xf);
            translationtables[i + 256] = 0x40 + (i & 0xf);
            translationtables[i + 512] = 0x20 + (i & 0xf);
        }
        else
        {
            // Keep all other colors as is.
            translationtables[i] = translationtables[i + 256] =
                translationtables[i + 512] = i;
        }
    }
    }
#elif __JHERETIC__
    {
    int i;
    byte* translationtables = (byte*) DD_GetVariable(DD_TRANSLATIONTABLES_ADDRESS);

    // Fill out the translation tables.
    for(i = 0; i < 256; ++i)
    {
        if(i >= 225 && i <= 240)
        {
            translationtables[i] = 114 + (i - 225); // yellow
            translationtables[i + 256] = 145 + (i - 225); // red
            translationtables[i + 512] = 190 + (i - 225); // blue
        }
        else
        {
            translationtables[i] = translationtables[i + 256] =
                translationtables[i + 512] = i;
        }
    }
    }
#else // __JHEXEN__
    {
    int i;
    byte* translationtables = (byte*) DD_GetVariable(DD_TRANSLATIONTABLES_ADDRESS);

    for(i = 0; i < 3 * 7; ++i)
    {
        char name[9];
        lumpnum_t lumpNum;

        dd_snprintf(name, 9, "TRANTBL%X", i);

        if(-1 != (lumpNum = W_CheckLumpNumForName(name)))
        {
            W_ReadLumpSection(lumpNum, (char*)&translationtables[i * 256], 0, 256);
        }
    }
    }
#endif

#undef PALID
#undef PALENTRIES
#undef PALLUMPNAME
}

/**
 * \todo Read this information from a definition (ideally with more user
 * friendly mnemonics).
 */
void R_LoadVectorGraphics(void)
{
#define R (1.0f)
    const vgline_t keysquare[] = {
        { {0, 0}, {R / 4, -R / 2} },
        { {R / 4, -R / 2}, {R / 2, -R / 2} },
        { {R / 2, -R / 2}, {R / 2, R / 2} },
        { {R / 2, R / 2}, {R / 4, R / 2} },
        { {R / 4, R / 2}, {0, 0} }, // Handle part type thing.
        { {0, 0}, {-R, 0} }, // Stem.
        { {-R, 0}, {-R, -R / 2} }, // End lockpick part.
        { {-3 * R / 4, 0}, {-3 * R / 4, -R / 4} }
    };
    const vgline_t thintriangle_guy[] = {
        { {-R / 2, R - R / 2}, {R, 0} }, // >
        { {R, 0}, {-R / 2, -R + R / 2} },
        { {-R / 2, -R + R / 2}, {-R / 2, R - R / 2} } // |>
    };
#if __JDOOM__ || __JDOOM64__
    const vgline_t player_arrow[] = {
        { {-R + R / 8, 0}, {R, 0} }, // -----
        { {R, 0}, {R - R / 2, R / 4} }, // ----->
        { {R, 0}, {R - R / 2, -R / 4} },
        { {-R + R / 8, 0}, {-R - R / 8, R / 4} }, // >---->
        { {-R + R / 8, 0}, {-R - R / 8, -R / 4} },
        { {-R + 3 * R / 8, 0}, {-R + R / 8, R / 4} }, // >>--->
        { {-R + 3 * R / 8, 0}, {-R + R / 8, -R / 4} }
    };
    const vgline_t cheat_player_arrow[] = {
        { {-R + R / 8, 0}, {R, 0} }, // -----
        { {R, 0}, {R - R / 2, R / 6} }, // ----->
        { {R, 0}, {R - R / 2, -R / 6} },
        { {-R + R / 8, 0}, {-R - R / 8, R / 6} }, // >----->
        { {-R + R / 8, 0}, {-R - R / 8, -R / 6} },
        { {-R + 3 * R / 8, 0}, {-R + R / 8, R / 6} }, // >>----->
        { {-R + 3 * R / 8, 0}, {-R + R / 8, -R / 6} },
        { {-R / 2, 0}, {-R / 2, -R / 6} }, // >>-d--->
        { {-R / 2, -R / 6}, {-R / 2 + R / 6, -R / 6} },
        { {-R / 2 + R / 6, -R / 6}, {-R / 2 + R / 6, R / 4} },
        { {-R / 6, 0}, {-R / 6, -R / 6} }, // >>-dd-->
        { {-R / 6, -R / 6}, {0, -R / 6} },
        { {0, -R / 6}, {0, R / 4} },
        { {R / 6, R / 4}, {R / 6, -R / 7} }, // >>-ddt->
        { {R / 6, -R / 7}, {R / 6 + R / 32, -R / 7 - R / 32} },
        { {R / 6 + R / 32, -R / 7 - R / 32}, {R / 6 + R / 10, -R / 7} }
    };
#elif __JHERETIC__
    const vgline_t player_arrow[] = {
        { {-R + R / 4, 0}, {0, 0} }, // center line.
        { {-R + R / 4, R / 8}, {R, 0} }, // blade
        { {-R + R / 4, -R / 8}, {R, 0} },
        { {-R + R / 4, -R / 4}, {-R + R / 4, R / 4} }, // crosspiece
        { {-R + R / 8, -R / 4}, {-R + R / 8, R / 4} },
        { {-R + R / 8, -R / 4}, {-R + R / 4, -R / 4} }, //crosspiece connectors
        { {-R + R / 8, R / 4}, {-R + R / 4, R / 4} },
        { {-R - R / 4, R / 8}, {-R - R / 4, -R / 8} }, // pommel
        { {-R - R / 4, R / 8}, {-R + R / 8, R / 8} },
        { {-R - R / 4, -R / 8}, {-R + R / 8, -R / 8} }
    };
    const vgline_t cheat_player_arrow[] = {
        { {-R + R / 8, 0}, {R, 0} }, // -----
        { {R, 0}, {R - R / 2, R / 6} }, // ----->
        { {R, 0}, {R - R / 2, -R / 6} },
        { {-R + R / 8, 0}, {-R - R / 8, R / 6} }, // >----->
        { {-R + R / 8, 0}, {-R - R / 8, -R / 6} },
        { {-R + 3 * R / 8, 0}, {-R + R / 8, R / 6} }, // >>----->
        { {-R + 3 * R / 8, 0}, {-R + R / 8, -R / 6} },
        { {-R / 2, 0}, {-R / 2, -R / 6} }, // >>-d--->
        { {-R / 2, -R / 6}, {-R / 2 + R / 6, -R / 6} },
        { {-R / 2 + R / 6, -R / 6}, {-R / 2 + R / 6, R / 4} },
        { {-R / 6, 0}, {-R / 6, -R / 6} }, // >>-dd-->
        { {-R / 6, -R / 6}, {0, -R / 6} },
        { {0, -R / 6}, {0, R / 4} },
        { {R / 6, R / 4}, {R / 6, -R / 7} }, // >>-ddt->
        { {R / 6, -R / 7}, {R / 6 + R / 32, -R / 7 - R / 32} },
        { {R / 6 + R / 32, -R / 7 - R / 32}, {R / 6 + R / 10, -R / 7} }
    };
#elif __JHEXEN__
    const vgline_t player_arrow[] = {
        { {-R + R / 4, 0}, {0, 0} }, // center line.
        { {-R + R / 4, R / 8}, {R, 0} }, // blade
        { {-R + R / 4, -R / 8}, {R, 0} },
        { {-R + R / 4, -R / 4}, {-R + R / 4, R / 4} }, // crosspiece
        { {-R + R / 8, -R / 4}, {-R + R / 8, R / 4} },
        { {-R + R / 8, -R / 4}, {-R + R / 4, -R / 4} }, // crosspiece connectors
        { {-R + R / 8, R / 4}, {-R + R / 4, R / 4} },
        { {-R - R / 4, R / 8}, {-R - R / 4, -R / 8} }, // pommel
        { {-R - R / 4, R / 8}, {-R + R / 8, R / 8} },
        { {-R - R / 4, -R / 8}, {-R + R / 8, -R / 8} }
    };
#endif
#undef R
    const vgline_t crossHair1[] = { // + (open center)
        { {-1,  0}, {-.4f, 0} },
        { { 0, -1}, { 0,  -.4f} },
        { { 1,  0}, { .4f, 0} },
        { { 0,  1}, { 0,   .4f} }
    };
    const vgline_t crossHair2[] = { // > <
        { {-1, -.714f}, {-.286f, 0} },
        { {-1,  .714f}, {-.286f, 0} },
        { { 1, -.714f}, { .286f, 0} },
        { { 1,  .714f}, { .286f, 0} }
    };
    const vgline_t crossHair3[] = { // square
        { {-1, -1}, {-1,  1} },
        { {-1,  1}, { 1,  1} },
        { { 1,  1}, { 1, -1} },
        { { 1, -1}, {-1, -1} }
    };
    const vgline_t crossHair4[] = { // square (open center)
        { {-1, -1}, {-1, -.5f} },
        { {-1, .5f}, {-1, 1} },
        { {-1, 1}, {-.5f, 1} },
        { {.5f, 1}, {1, 1} },
        { { 1, 1}, {1, .5f} },
        { { 1, -.5f}, {1, -1} },
        { { 1, -1}, {.5f, -1} },
        { {-.5f, -1}, {-1, -1} }
    };
    const vgline_t crossHair5[] = { // diamond
        { { 0, -1}, { 1,  0} },
        { { 1,  0}, { 0,  1} },
        { { 0,  1}, {-1,  0} },
        { {-1,  0}, { 0, -1} }
    };
    const vgline_t crossHair6[] = { // ^
        { {-1, -1}, { 0,  0} },
        { { 0,  0}, { 1, -1} }
    };

    R_NewVectorGraphic(VG_KEYSQUARE, keysquare, sizeof(keysquare) / sizeof(keysquare[0]));
    R_NewVectorGraphic(VG_TRIANGLE, thintriangle_guy, sizeof(thintriangle_guy) / sizeof(thintriangle_guy[0]));
    R_NewVectorGraphic(VG_ARROW, player_arrow, sizeof(player_arrow) / sizeof(player_arrow[0]));
#if !__JHEXEN__
    R_NewVectorGraphic(VG_CHEATARROW, cheat_player_arrow, sizeof(cheat_player_arrow) / sizeof(cheat_player_arrow[0]));
#endif
    R_NewVectorGraphic(VG_XHAIR1, crossHair1, sizeof(crossHair1) / sizeof(crossHair1[0]));
    R_NewVectorGraphic(VG_XHAIR2, crossHair2, sizeof(crossHair2) / sizeof(crossHair2[0]));
    R_NewVectorGraphic(VG_XHAIR3, crossHair3, sizeof(crossHair3) / sizeof(crossHair3[0]));
    R_NewVectorGraphic(VG_XHAIR4, crossHair4, sizeof(crossHair4) / sizeof(crossHair4[0]));
    R_NewVectorGraphic(VG_XHAIR5, crossHair5, sizeof(crossHair5) / sizeof(crossHair5[0]));
    R_NewVectorGraphic(VG_XHAIR6, crossHair6, sizeof(crossHair6) / sizeof(crossHair6[0]));
}

/**
 * @param name  Name of the font to lookup.
 * @return  Unique id of the found font.
 */
fontid_t R_MustFindFontForName(const char* name)
{
    fontid_t id = Fonts_IdForName(name);
    if(id == 0)
        Con_Error("Failed loading font \"%s\".", name);
    return id;
}

void R_InitRefresh(void)
{
    VERBOSE( Con_Message("R_InitRefresh: Loading data for referesh.\n") );

    R_LoadColorPalettes();
    R_LoadVectorGraphics();
 
    // Setup the view border.
    cfg.screenBlocks = cfg.setBlocks;
    { dduri_t* paths[9];
    uint i;
    for(i = 0; i < 9; ++i)
        paths[i] = ((borderGraphics[i] && borderGraphics[i][0])? Uri_Construct2(borderGraphics[i], RC_NULL) : 0);
    R_SetBorderGfx(paths);
    for(i = 0; i < 9; ++i)
        if(paths[i])
            Uri_Destruct(paths[i]);
    }
    R_ResizeViewWindow(RWF_FORCE|RWF_NO_LERP);

    // Locate our fonts.
    fonts[GF_FONTA]   = R_MustFindFontForName("a");
    fonts[GF_FONTB]   = R_MustFindFontForName("b");
    fonts[GF_STATUS]  = R_MustFindFontForName("status");
#if __JDOOM__
    fonts[GF_INDEX]   = R_MustFindFontForName("index");
#endif
#if __JDOOM__ || __JDOOM64__
    fonts[GF_SMALL]   = R_MustFindFontForName("small");
#endif
#if __JHERETIC__ || __JHEXEN__
    fonts[GF_SMALLIN] = R_MustFindFontForName("smallin");
#endif

    { float mul = 1.4f;
    DD_SetVariable(DD_PSPRITE_LIGHTLEVEL_MULTIPLIER, &mul);
    }
}

void R_InitHud(void)
{
    Hu_LoadData();

#if __JHERETIC__ || __JHEXEN__
    VERBOSE( Con_Message("Initializing inventory...\n") )
    Hu_InventoryInit();
#endif

    VERBOSE2( Con_Message("Initializing statusbar...\n") )
    ST_Init();

    VERBOSE2( Con_Message("Initializing menu...\n") )
    Hu_MenuInit();

    VERBOSE2( Con_Message("Initializing status-message/question system...\n") )
    Hu_MsgInit();
}

/**
 * Common Post Game Initialization routine.
 * Game-specific post init actions should be placed in eg D_PostInit()
 * (for jDoom) and NOT here.
 */
void G_CommonPostInit(void)
{
    R_InitRefresh();
    FI_StackInit();
    GUI_Init();

    // Init the save system and create the game save directory.
    SV_Init();

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    XG_ReadTypes();
#endif

    VERBOSE( Con_Message("Initializing playsim...\n") )
    P_Init();

    VERBOSE( Con_Message("Initializing head-up displays...\n") )
    R_InitHud();

    G_InitEventSequences();
#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
    Cht_Init();
#endif

    // From this point on, the shortcuts are always active.
    DD_Execute(true, "activatebcontext shortcut");

    // Display a breakdown of the available maps.
    DD_Execute(true, "listmaps");
}

/**
 * Common game shutdown routine.
 * \note Game-specific actions should be placed in G_Shutdown rather than here.
 */
void G_CommonShutdown(void)
{
    Plug_RemoveHook(HOOK_DEMO_STOP, Hook_DemoStop);

    Hu_MsgShutdown();
    Hu_UnloadData();

    SV_Shutdown();
    P_Shutdown();
    G_ShutdownEventSequences();

    ST_Shutdown();
    GUI_Shutdown();
    FI_StackShutdown();
}

/**
 * Retrieve the current game state.
 *
 * @return              The current game state.
 */
gamestate_t G_GetGameState(void)
{
    return gameState;
}

#if _DEBUG
static const char* getGameStateStr(gamestate_t state)
{
    struct statename_s {
        gamestate_t     state;
        const char*     name;
    } stateNames[] =
    {
        {GS_MAP, "GS_MAP"},
        {GS_INTERMISSION, "GS_INTERMISSION"},
        {GS_FINALE, "GS_FINALE"},
        {GS_STARTUP, "GS_STARTUP"},
        {GS_WAITING, "GS_WAITING"},
        {GS_INFINE, "GS_INFINE"},
        {-1, NULL}
    };
    uint                i;

    for(i = 0; stateNames[i].name; ++i)
        if(stateNames[i].state == state)
            return stateNames[i].name;

    return NULL;
}
#endif

/**
 * Called when the gameui binding context is active. Triggers the menu.
 */
int G_UIResponder(event_t* ev)
{
    // Handle "Press any key to continue" messages.
    if(Hu_MsgResponder(ev))
        return true;

    if(ev->state != EVS_DOWN)
        return false;
    if(!(ev->type == EV_KEY || ev->type == EV_MOUSE_BUTTON || ev->type == EV_JOY_BUTTON))
        return false;

    if(!Hu_MenuIsActive())
    {
        // Any key/button down pops up menu if in demos.
        if((G_GetGameAction() == GA_NONE && !singledemo && Get(DD_PLAYBACK)) ||
           (G_GetGameState() == GS_INFINE && FI_IsMenuTrigger()))
        {
            Hu_MenuCommand(MCMD_OPEN);
            return true;
        }
    }

    return false;
}

/**
 * Change the game's state.
 *
 * @param state         The state to change to.
 */
void G_ChangeGameState(gamestate_t state)
{
    boolean gameUIActive = false;
    boolean gameActive = true;

    if(G_GetGameAction() == GA_QUIT)
        return;

    if(state < 0 || state >= NUM_GAME_STATES)
        Con_Error("G_ChangeGameState: Invalid state %i.\n", (int) state);

    if(gameState != state)
    {
#if _DEBUG
// Log gamestate changes in debug builds, with verbose.
VERBOSE(Con_Message("G_ChangeGameState: New state %s.\n",
                    getGameStateStr(state)));
#endif

        gameState = state;
    }

    // Update the state of the gameui binding context.
    switch(gameState)
    {
    case GS_FINALE:
    case GS_STARTUP:
    case GS_WAITING:
    case GS_INFINE:
        gameActive = false;
    case GS_INTERMISSION:
        gameUIActive = true;
        break;
    default:
        break;
    }

    if(gameUIActive)
    {
        DD_Execute(true, "activatebcontext gameui");
        B_SetContextFallback("gameui", G_UIResponder);
    }

    DD_Executef(true, "%sactivatebcontext game", gameActive? "" : "de");
}

boolean G_StartFinale(const char* script, int flags, finale_mode_t mode)
{
    assert(script && script[0]);
    { uint i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        // Clear the message queue for all local players.
        ST_LogEmpty(i);

        // Close the automap for all local players.
        ST_AutomapOpen(i, false, true);

#if __JHERETIC__ || __JHEXEN__
        Hu_InventoryOpen(i, false);
#endif
    }}
    FI_StackExecute(script, flags, mode);
    return true;
}

/**
 * Begin the titlescreen animation sequence.
 */
void G_StartTitle(void)
{
    ddfinale_t fin;

    G_StopDemo();
    userGame = false;

    // The title script must always be defined.
    if(!Def_Get(DD_DEF_FINALE, "title", &fin))
        Con_Error("G_StartTitle: A title script must be defined.");

    G_StartFinale(fin.script, FF_LOCAL, FIMODE_NORMAL);
}

/**
 * Begin the helpscreen animation sequence.
 */
void G_StartHelp(void)
{
    ddfinale_t fin;

    if(GA_QUIT == G_GetGameAction())
    {
        return;
    }

    if(Def_Get(DD_DEF_FINALE, "help", &fin))
    {
        Hu_MenuCommand(MCMD_CLOSEFAST);
        G_StartFinale(fin.script, FF_LOCAL, FIMODE_NORMAL);
        return;
    }
    Con_Message("G_GetFinaleScript: Warning, script \"help\" not defined.\n");    
}

int G_EndGameResponse(msgresponse_t response, void* context)
{
    if(response == MSG_YES)
    {
        G_StartTitle();
    }
    return true;
}

void G_EndGame(void)
{
    if(GA_QUIT == G_GetGameAction())
    {
        return;
    }

    if(!userGame)
    {
        Hu_MsgStart(MSG_ANYKEY, ENDNOGAME, NULL, NULL);
        return;
    }

    if(IS_NETGAME)
    {
        Hu_MsgStart(MSG_ANYKEY, NETEND, NULL, NULL);
        return;
    }

    Hu_MsgStart(MSG_YESNO, ENDGAME, G_EndGameResponse, NULL);
}

void G_DoLoadMap(void)
{
    char* lname, *ptr;
    ddfinale_t fin;
    boolean hasBrief;
    int i;

#if __JHEXEN__
    static int firstFragReset = 1;
#endif

    mapStartTic = (int) GAMETIC; // Fr time calculation.

    // If we're the server, let clients know the map will change.
    NetSv_SendGameState(GSF_CHANGE_MAP, DDSP_ALL_PLAYERS);

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];

        if(plr->plr->inGame && plr->playerState == PST_DEAD)
            plr->playerState = PST_REBORN;

#if __JHEXEN__
        if(!IS_NETGAME || (IS_NETGAME != 0 && deathmatch != 0) ||
            firstFragReset == 1)
        {
            memset(plr->frags, 0, sizeof(plr->frags));
            firstFragReset = 0;
        }
#else
        memset(plr->frags, 0, sizeof(plr->frags));
#endif
    }

#if __JHEXEN__
    SN_StopAllSequences();
#endif

    // Set all player mobjs to NULL, clear control state toggles etc.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        players[i].plr->mo = NULL;
        G_ResetLookOffset(i);
    }

    // Determine whether there is a briefing to run before the map starts
    // (played after the map has been loaded).
    hasBrief = G_BriefingEnabled(gameEpisode, gameMap, &fin);
    if(!hasBrief)
    {
#if __JHEXEN__
        /**
         * \kludge Due to the way music is managed with Hexen, unless we
         * explicitly stop the current playing track the engine will not
         * change tracks. This is due to the use of the runtime-updated
         * "currentmap" definition (the engine thinks music has not changed
         * because the current Music definition is the same).
         *
         * The only reason it worked previously was because the
         * waiting-for-map-load song was started prior to load.
         *
         * \todo Rethink the Music definition stuff with regard to Hexen.
         * Why not create definitions during startup by parsing MAPINFO?
         */
        S_StopMusic();
        //S_StartMusic("chess", true); // Waiting-for-map-load song
#endif
        S_MapMusic(gameEpisode, gameMap);
        S_PauseMusic(true);
    }

    P_SetupMap(gameEpisode, gameMap, 0, gameSkill);
    Set(DD_DISPLAYPLAYER, CONSOLEPLAYER); // View the guy you are playing.
    G_SetGameAction(GA_NONE);
    nextMap = 0;

    Z_CheckHeap();

    // Clear cmd building stuff.
    G_ResetMousePos();

    sendPause = paused = false;

    G_ControlReset(-1); // Clear all controls for all local players.

    // Set the game status cvar for map name.
    lname = (char *) DD_GetVariable(DD_MAP_NAME);
    if(lname)
    {
        ptr = strchr(lname, ':'); // Skip the E#M# or Map #.
        if(ptr)
        {
            lname = ptr + 1;
            while(*lname && isspace(*lname))
                lname++;
        }
    }

#if __JHEXEN__
    // In jHexen we can look in the MAPINFO for the map name
    if(!lname)
        lname = P_GetMapName(gameMap);
#endif

    // Set the map name
    // If still no name, call it unnamed.
    if(!lname)
    {
        Con_SetString2("map-name", UNNAMEDMAP, SVF_WRITE_OVERRIDE);
    }
    else
    {
        Con_SetString2("map-name", lname, SVF_WRITE_OVERRIDE);
    }

    // Start a briefing, if there is one.
    if(hasBrief)
    {
        G_StartFinale(fin.script, 0, FIMODE_BEFORE);
    }
    else // No briefing, start the map.
    {
        G_ChangeGameState(GS_MAP);
        S_PauseMusic(false);
        R_ResizeViewWindow(RWF_FORCE|RWF_NO_LERP);
    }
}

int G_Responder(event_t* ev)
{
    assert(NULL != ev);

    // Eat all events once shutdown has begun.
    if(GA_QUIT == G_GetGameAction())
        return true; 

    // With the menu active, none of these should respond to input events.
    if(G_GetGameState() == GS_MAP && !Hu_MenuIsActive() && !Hu_IsMessageActive())
    {
        if(ST_Responder(ev))
            return true;

        if(G_EventSequenceResponder(ev))
            return true;
    }

    if(Hu_MenuResponder(ev))
        return true;

    return false; // Not eaten.
}

int G_PrivilegedResponder(event_t* ev)
{
    // Ignore all events once shutdown has begun.
    if(GA_QUIT == G_GetGameAction())
        return false;

    if(Hu_MenuPrivilegedResponder(ev))
        return true;

    // Process the screen shot key right away.
    if(devParm && ev->type == EV_KEY && ev->data1 == DDKEY_F1)
    {
        if(ev->state == EVS_DOWN)
            G_ScreenShot();
        return true; // All F1 events are eaten.
    }

    return false; // Not eaten.
}

/**
 * Updates the game status cvars based on game and player data.
 * Called each tick by G_Ticker().
 */
void G_UpdateGSVarsForPlayer(player_t* pl)
{
    int                 i, plrnum;
    gamestate_t         gameState;

    if(!pl)
        return;

    plrnum = pl - players;
    gameState = G_GetGameState();

    gsvHealth = pl->health;
#if !__JHEXEN__
    // Map stats
    gsvKills = pl->killCount;
    gsvItems = pl->itemCount;
    gsvSecrets = pl->secretCount;
#endif
        // armor
#if __JHEXEN__
    gsvArmor = FixedDiv(PCLASS_INFO(pl->class)->autoArmorSave +
                        pl->armorPoints[ARMOR_ARMOR] +
                        pl->armorPoints[ARMOR_SHIELD] +
                        pl->armorPoints[ARMOR_HELMET] +
                        pl->armorPoints[ARMOR_AMULET], 5 * FRACUNIT) >> FRACBITS;
#else
    gsvArmor = pl->armorPoints;
#endif
    // Owned keys
    for(i = 0; i < NUM_KEY_TYPES; ++i)
#if __JHEXEN__
        gsvKeys[i] = (pl->keys & (1 << i))? 1 : 0;
#else
        gsvKeys[i] = pl->keys[i];
#endif
    // current weapon
    gsvCurrentWeapon = pl->readyWeapon;

    // owned weapons
    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
        gsvWeapons[i] = pl->weapons[i].owned;

#if __JHEXEN__
    // weapon pieces
    gsvWPieces[0] = (pl->pieces & WPIECE1)? 1 : 0;
    gsvWPieces[1] = (pl->pieces & WPIECE2)? 1 : 0;
    gsvWPieces[2] = (pl->pieces & WPIECE3)? 1 : 0;
    gsvWPieces[3] = (pl->pieces == 7)? 1 : 0;
#endif
    // Current ammo amounts.
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
        gsvAmmo[i] = pl->ammo[i].owned;

#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
    // Inventory items.
    for(i = 0; i < NUM_INVENTORYITEM_TYPES; ++i)
    {
        if(pl->plr->inGame && gameState == GS_MAP)
            gsvInvItems[i] = P_InventoryCount(plrnum, IIT_FIRST + i);
        else
            gsvInvItems[i] = 0;
    }
#endif
}

static void runGameAction(void)
{
    if(G_GetGameAction() == GA_QUIT)
    {
#define QUITWAIT_MILLISECONDS 1500

        static uint quitTime = 0;

        if(quitTime == 0)
        {
            quitTime = Sys_GetRealTime();

            Hu_MenuCommand(MCMD_CLOSEFAST);

            if(!IS_NETGAME)
            {
#if __JDOOM__ || __JDOOM64__
                // Play an exit sound if it is enabled.
                if(cfg.menuQuitSound)
                {
# if __JDOOM64__
                    static int quitsounds[8] = {
                        SFX_VILACT,
                        SFX_GETPOW,
                        SFX_PEPAIN,
                        SFX_SLOP,
                        SFX_SKESWG,
                        SFX_KNTDTH,
                        SFX_BSPACT,
                        SFX_SGTATK
                    };
# else
                    static int quitsounds[8] = {
                        SFX_PLDETH,
                        SFX_DMPAIN,
                        SFX_POPAIN,
                        SFX_SLOP,
                        SFX_TELEPT,
                        SFX_POSIT1,
                        SFX_POSIT3,
                        SFX_SGTATK
                    };
                    static int quitsounds2[8] = {
                        SFX_VILACT,
                        SFX_GETPOW,
                        SFX_BOSCUB,
                        SFX_SLOP,
                        SFX_SKESWG,
                        SFX_KNTDTH,
                        SFX_BSPACT,
                        SFX_SGTATK
                    };

                    if(gameModeBits & GM_ANY_DOOM2)
                        S_LocalSound(quitsounds2[P_Random() & 7], 0);
                    else
# endif
                        S_LocalSound(quitsounds[P_Random() & 7], 0);
                }
#endif
                DD_Executef(true, "activatebcontext deui");
            }
        }

        if(Sys_GetRealTime() > quitTime + QUITWAIT_MILLISECONDS)
        {
            Sys_Quit();
        }
        else
        {
            float t = (Sys_GetRealTime() - quitTime) / (float) QUITWAIT_MILLISECONDS;
            quitDarkenOpacity = t*t*t;
        }

        // No game state changes occur once we have begun to quit.
        return;

#undef QUITWAIT_MILLISECONDS
    }

    // Do things to change the game state.
    {gameaction_t currentAction;
    while((currentAction = G_GetGameAction()) != GA_NONE)
    {
        switch(currentAction)
        {
#if __JHEXEN__
        case GA_INITNEW:
            G_DoInitNew();
            break;

        case GA_SINGLEREBORN:
            G_DoSingleReborn();
            break;
#endif

        case GA_LEAVEMAP:
            G_DoWorldDone();
            break;

        case GA_LOADMAP:
            G_DoLoadMap();
            break;

        case GA_NEWGAME:
            G_DoNewGame();
            break;

        case GA_LOADGAME:
            G_DoLoadGame();
            break;

        case GA_SAVEGAME:
            G_DoSaveGame();
            break;

        case GA_MAPCOMPLETED:
            G_DoMapCompleted();
            break;

        case GA_VICTORY:
            G_SetGameAction(GA_NONE);
            break;

        case GA_SCREENSHOT:
            G_DoScreenShot();
            G_SetGameAction(GA_NONE);
            break;

        case GA_NONE:
        default:
            break;
        }
    }}
}

/**
 * The core of the timing loop. Game state, game actions etc occur here.
 *
 * @param ticLength     How long this tick is, in seconds.
 */
void G_Ticker(timespan_t ticLength)
{
    static gamestate_t oldGameState = -1;
    static trigger_t fixed = {1.0 / TICSPERSEC};

    int i;

    // Always tic:
    Hu_FogEffectTicker(ticLength);
    Hu_MenuTicker(ticLength);
    Hu_MsgTicker(ticLength);

    if(IS_CLIENT && !Get(DD_GAME_READY))
        return;

    // Do player reborns if needed.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];

        if(plr->plr->inGame && plr->playerState == PST_REBORN &&
           !P_MobjIsCamera(plr->plr->mo))
            G_DoReborn(i);

        // Player has left?
        if(plr->playerState == PST_GONE)
        {
            plr->playerState = PST_REBORN;
            if(plr->plr->mo)
            {
                if(!IS_CLIENT)
                {
                    P_SpawnTeleFog(plr->plr->mo->pos[VX],
                                   plr->plr->mo->pos[VY],
                                   plr->plr->mo->angle + ANG180);
                }

                // Let's get rid of the mobj.
#ifdef _DEBUG
Con_Message("G_Ticker: Removing player %i's mobj.\n", i);
#endif
                P_MobjRemove(plr->plr->mo, true);
                plr->plr->mo = NULL;
            }
        }
    }

    runGameAction();

    if(G_GetGameAction() != GA_QUIT)
    {
        // Update the viewer's look angle
        //G_LookAround(CONSOLEPLAYER);

        if(!IS_CLIENT)
        {
            // Enable/disable sending of frames (delta sets) to clients.
            Set(DD_ALLOW_FRAMES, G_GetGameState() == GS_MAP);

            // Tell Doomsday when the game is paused (clients can't pause
            // the game.)
            Set(DD_CLIENT_PAUSED, P_IsPaused());
        }

        // Must be called on every tick.
        P_RunPlayers(ticLength);
    }
    else
    {
        if(!IS_CLIENT)
        {
            // Disable sending of frames (delta sets) to clients.
            Set(DD_ALLOW_FRAMES, false);
        }
    }

    if(G_GetGameState() == GS_MAP && !IS_DEDICATED)
    {
        ST_Ticker(ticLength);
    }

    // Track view window changes.
    R_ResizeViewWindow(0);

    // The following is restricted to fixed 35 Hz ticks.
    if(M_RunTrigger(&fixed, ticLength))
    {
        // Do main actions.
        switch(G_GetGameState())
        {
        case GS_MAP:
            // Update in-map game status cvar.
            if(oldGameState != GS_MAP)
                gsvInMap = 1;

            P_DoTick();
            HU_UpdatePsprites();

            // Active briefings once again (they were disabled when loading
            // a saved game).
            briefDisabled = false;

            if(IS_DEDICATED)
                break;

            Hu_Ticker();
            break;

        case GS_INTERMISSION:
#if __JDOOM__ || __JDOOM64__
            WI_Ticker();
#else
            IN_Ticker();
#endif
            break;

        default:
            if(oldGameState != G_GetGameState())
            {
                // Update game status cvars.
                gsvInMap = 0;
                Con_SetString2("map-name", NOTAMAPNAME, SVF_WRITE_OVERRIDE);
                gsvMapMusic = -1;
            }
            break;
        }

        // Update the game status cvars for player data.
        G_UpdateGSVarsForPlayer(&players[CONSOLEPLAYER]);

        // Servers will have to update player information and do such stuff.
        if(!IS_CLIENT)
            NetSv_Ticker();
    }

    oldGameState = gameState;
}

/**
 * Called when a player leaves a map.
 *
 * Jobs include; striping keys, inventory and powers from the player
 * and configuring other player-specific properties ready for the next
 * map.
 *
 * @param player        Id of the player to configure.
 */
void G_PlayerLeaveMap(int player)
{
#if __JHERETIC__ || __JHEXEN__
    uint i;
    int flightPower;
#endif
    player_t* p = &players[player];
    boolean newCluster;

#if __JHEXEN__
    newCluster = (P_GetMapCluster(gameMap) != P_GetMapCluster(nextMap));
#else
    newCluster = true;
#endif

#if __JHERETIC__ || __JHEXEN__
    // Remember if flying.
    flightPower = p->powers[PT_FLIGHT];
#endif

#if __JHERETIC__
    // Empty the inventory of excess items
    for(i = 0; i < NUM_INVENTORYITEM_TYPES; ++i)
    {
        inventoryitemtype_t type = IIT_FIRST + i;
        uint count = P_InventoryCount(player, type);

        if(count)
        {
            uint j;

            if(type != IIT_FLY)
                count--;

            for(j = 0; j < count; ++j)
                P_InventoryTake(player, type, true);
        }
    }
#endif

#if __JHEXEN__
    if(newCluster)
    {
        uint count = P_InventoryCount(player, IIT_FLY);

        for(i = 0; i < count; ++i)
            P_InventoryTake(player, IIT_FLY, true);
    }
#endif

    // Remove their powers.
    p->update |= PSF_POWERS;
    memset(p->powers, 0, sizeof(p->powers));

#if __JHEXEN__
    if(!newCluster && !deathmatch)
        p->powers[PT_FLIGHT] = flightPower; // Restore flight.
#endif

    // Remove their keys.
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    p->update |= PSF_KEYS;
    memset(p->keys, 0, sizeof(p->keys));
#else
    if(!deathmatch && newCluster)
        p->keys = 0;
#endif

    // Misc
#if __JHERETIC__
    p->rain1 = NULL;
    p->rain2 = NULL;
#endif

    // Un-morph?
#if __JHERETIC__ || __JHEXEN__
    p->update |= PSF_MORPH_TIME;
    if(p->morphTics)
    {
        p->readyWeapon = p->plr->mo->special1; // Restore weapon.
        p->morphTics = 0;
    }
#endif

    p->plr->lookDir = 0;
    p->plr->mo->flags &= ~MF_SHADOW; // Cancel invisibility.
    p->plr->extraLight = 0; // Cancel gun flashes.
    p->plr->fixedColorMap = 0; // Cancel IR goggles.

    // Clear filter.
    p->plr->flags &= ~DDPF_VIEW_FILTER;
    p->plr->flags |= DDPF_FILTER; // Server: Send the change to the client.
    p->damageCount = 0; // No palette changes.
    p->bonusCount = 0;

#if __JHEXEN__
    p->poisonCount = 0;
#endif

    ST_LogEmpty(p - players);
}

/**
 * Safely clears the player data structures.
 */
void ClearPlayer(player_t *p)
{
    ddplayer_t *ddplayer = p->plr;
    int         playeringame = ddplayer->inGame;
    int         flags = ddplayer->flags;
    int         start = p->startSpot;
    fixcounters_t counter, acked;

    // Restore counters.
    counter = ddplayer->fixCounter;
    acked = ddplayer->fixAcked;

    memset(p, 0, sizeof(*p));
    // Restore the pointer to ddplayer.
    p->plr = ddplayer;
#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
    P_InventoryEmpty(p - players);
    P_InventorySetReadyItem(p - players, IIT_NONE);
#endif
    // Also clear ddplayer.
    memset(ddplayer, 0, sizeof(*ddplayer));
    // Restore the pointer to this player.
    ddplayer->extraData = p;
    // Restore the playeringame data.
    ddplayer->inGame = playeringame;
    ddplayer->flags = flags & ~(DDPF_INTERYAW | DDPF_INTERPITCH);
    // Don't clear the start spot.
    p->startSpot = start;
    // Restore counters.
    ddplayer->fixCounter = counter;
    ddplayer->fixAcked = acked;

    ddplayer->fixCounter.angles++;
    ddplayer->fixCounter.pos++;
    ddplayer->fixCounter.mom++;

/*    ddplayer->fixAcked.angles =
        ddplayer->fixAcked.pos =
        ddplayer->fixAcked.mom = -1;
#ifdef _DEBUG
    Con_Message("ClearPlayer: fixacked set to -1 (counts:%i, %i, %i)\n",
                ddplayer->fixCounter.angles,
                ddplayer->fixCounter.pos,
                ddplayer->fixCounter.mom);
#endif*/
}

/**
 * Called after a player dies (almost everything is cleared and then
 * re-initialized).
 */
void G_PlayerReborn(int player)
{
    player_t       *p;
    int             frags[MAXPLAYERS];
    int             killcount, itemcount, secretcount;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    int             i;
#endif
#if __JHERETIC__
    boolean         secret = false;
    int             spot;
#elif __JHEXEN__
    uint            worldTimer;
#endif

    if(player < 0 || player >= MAXPLAYERS)
        return; // Wha?

    p = &players[player];

    memcpy(frags, p->frags, sizeof(frags));
    killcount = p->killCount;
    itemcount = p->itemCount;
    secretcount = p->secretCount;
#if __JHEXEN__
    worldTimer = p->worldTimer;
#endif

#if __JHERETIC__
    if(p->didSecret)
        secret = true;
    spot = p->startSpot;
#endif

    // Clears (almost) everything.
    ClearPlayer(p);

#if __JHERETIC__
    p->startSpot = spot;
#endif

    memcpy(p->frags, frags, sizeof(p->frags));
    p->killCount = killcount;
    p->itemCount = itemcount;
    p->secretCount = secretcount;
#if __JHEXEN__
    p->worldTimer = worldTimer;
    p->colorMap = cfg.playerColor[player];
#endif
#if __JHEXEN__
    p->class = cfg.playerClass[player];
#endif
    p->useDown = p->attackDown = true; // Don't do anything immediately.
    p->playerState = PST_LIVE;
    p->health = maxHealth;

#if __JDOOM__ || __JDOOM64__
    p->readyWeapon = p->pendingWeapon = WT_SECOND;
    p->weapons[WT_FIRST].owned = true;
    p->weapons[WT_SECOND].owned = true;

    // Initalize the player's ammo counts.
    memset(p->ammo, 0, sizeof(p->ammo));
    p->ammo[AT_CLIP].owned = 50;

    // See if the Values specify anything.
    P_InitPlayerValues(p);

#elif __JHERETIC__
    p->readyWeapon = p->pendingWeapon = WT_SECOND;
    p->weapons[WT_FIRST].owned = true;
    p->weapons[WT_SECOND].owned = true;
    p->ammo[AT_CRYSTAL].owned = 50;

    if(gameMap == 8 || secret)
    {
        p->didSecret = true;
    }

#else
    p->readyWeapon = p->pendingWeapon = WT_FIRST;
    p->weapons[WT_FIRST].owned = true;
    localQuakeHappening[player] = false;
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    // Reset maxammo.
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
        p->ammo[i].max = maxAmmo[i];
#endif

    // We'll need to update almost everything.
#if __JHERETIC__
    p->update |=
        PSF_STATE | PSF_HEALTH | PSF_ARMOR_TYPE | PSF_ARMOR_POINTS |
        PSF_INVENTORY | PSF_POWERS | PSF_KEYS | PSF_OWNED_WEAPONS | PSF_AMMO |
        PSF_MAX_AMMO | PSF_PENDING_WEAPON | PSF_READY_WEAPON;
#else
    p->update |= PSF_REBORN;
#endif

    p->plr->flags &= ~DDPF_DEAD;
}

#if __JDOOM__ || __JDOOM64__
void G_QueueBody(mobj_t* mo)
{
    if(!mo)
        return;

    // Flush an old corpse if needed.
    if(bodyQueueSlot >= BODYQUEUESIZE)
        P_MobjRemove(bodyQueue[bodyQueueSlot % BODYQUEUESIZE], false);

    bodyQueue[bodyQueueSlot % BODYQUEUESIZE] = mo;
    bodyQueueSlot++;
}
#endif

void G_DoReborn(int plrNum)
{
    if(plrNum < 0 || plrNum >= MAXPLAYERS)
        return; // Wha?

    // Clear the currently playing script, if any.
    FI_StackClear();

    if(!IS_NETGAME)
    {
        // We've just died, don't do a briefing now.
        briefDisabled = true;

#if __JHEXEN__
        if(SV_HxRebornSlotAvailable())
        {   // Use the reborn code if the slot is available
            G_SetGameAction(GA_SINGLEREBORN);
        }
        else
        {   // Start a new game if there's no reborn info
            G_SetGameAction(GA_NEWGAME);
        }
#else
        // Reload the map from scratch.
        G_SetGameAction(GA_LOADMAP);
#endif
    }
    else
    {   // In a net game.
        P_RebornPlayer(plrNum);
    }
}

#if __JHEXEN__
void G_StartNewInit(void)
{
    SV_HxInitBaseSlot();
    SV_HxClearRebornSlot();

    P_ACSInitNewGame();

    // Default the player start spot group to 0
    rebornPosition = 0;
}

void G_StartNewGame(skillmode_t skill)
{
    G_StartNewInit();
    G_InitNew(skill, 0, P_TranslateMap(0));
}
#endif

/**
 * Leave the current map and start intermission routine.
 * (if __JHEXEN__ the intermission will only be displayed when exiting a
 * hub and in DeathMatch games)
 *
 * @param newMap        ID of the map we are entering.
 * @param _entryPoint   Entry point on the new map.
 * @param secretExit
 */
void G_LeaveMap(uint newMap, uint _entryPoint, boolean _secretExit)
{
    if(cyclingMaps && mapCycleNoExit)
        return;

#if __JHEXEN__
    if(gameMode == hexen_demo && newMap != DDMAXINT && newMap > 3)
    {   // Not possible in the 4-map demo.
        P_SetMessage(&players[CONSOLEPLAYER], "PORTAL INACTIVE -- DEMO", false);
        return;
    }
#endif

#if __JHEXEN__
    nextMap = newMap;
    nextMapEntryPoint = _entryPoint;
#else
    secretExit = _secretExit;
# if __JDOOM__
      // If no Wolf3D maps, no secret exit!
      if(secretExit && (gameModeBits & GM_ANY_DOOM2) && !P_MapExists(0, 30))
          secretExit = false;
# endif
#endif

    G_SetGameAction(GA_MAPCOMPLETED);
}

/**
 * @return              @c true, if the game has been completed.
 */
boolean G_IfVictory(void)
{
#if __JDOOM64__
    if(gameMap == 27)
    {
        return true;
    }
#elif __JDOOM__
    if(gameMode == doom_chex)
    {
        if(gameMap == 4)
            return true;
    }
    else if((gameModeBits & GM_ANY_DOOM) && gameMap == 7)
        return true;
#elif __JHERETIC__
    if(gameMap == 7)
    {
        return true;
    }

#elif __JHEXEN__
    if(nextMap == DDMAXINT && nextMapEntryPoint == DDMAXINT)
    {
        return true;
    }
#endif
    return false;
}

static int prepareIntermission(void* paramaters)
{
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    wmInfo.episode = gameEpisode;
    wmInfo.currentMap = gameMap;
    wmInfo.nextMap = nextMap;
    wmInfo.didSecret = players[CONSOLEPLAYER].didSecret;

# if __JDOOM__ || __JDOOM64__
    wmInfo.maxKills = totalKills;
    wmInfo.maxItems = totalItems;
    wmInfo.maxSecret = totalSecret;

    G_PrepareWIData();
# endif
#endif

#if __JDOOM__ || __JDOOM64__
    WI_Init(&wmInfo);
#elif __JHERETIC__
    IN_Init(&wmInfo);
#else /* __JHEXEN__ */
    IN_Init();
#endif
    G_ChangeGameState(GS_INTERMISSION);

    Con_BusyWorkerEnd();
    return 0;
}

void G_DoMapCompleted(void)
{
    int i;

    G_SetGameAction(GA_NONE);

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
        {
            ST_AutomapOpen(i, false, true);

#if __JHERETIC__ || __JHEXEN__
            Hu_InventoryOpen(i, false);
#endif

            G_PlayerLeaveMap(i); // take away cards and stuff

            // Update this client's stats.
            NetSv_SendPlayerState(i, DDSP_ALL_PLAYERS,
                                  PSF_FRAGS | PSF_COUNTERS, true);
        }
    }

    GL_SetFilter(false);

#if __JHEXEN__
    SN_StopAllSequences();
#endif

    // Go to an intermission?
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    {
    ddmapinfo_t minfo;
    char levid[8];

    P_MapId(gameEpisode, gameMap, levid);

    if(Def_Get(DD_DEF_MAP_INFO, levid, &minfo) && (minfo.flags & MIF_NO_INTERMISSION))
    {
        G_WorldDone();
        return;
    }
    }
#elif __JHEXEN__
    if(!deathmatch)
    {
        G_WorldDone();
        return;
    }
#endif

    // Has the player completed the game?
    if(G_IfVictory())
    {   // Victorious!
        G_SetGameAction(GA_VICTORY);
        return;
    }

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
# if __JDOOM__
    if((gameModeBits & (GM_DOOM|GM_DOOM_SHAREWARE|GM_DOOM_ULTIMATE)) && gameMap == 8)
    {
        int i;
        for(i = 0; i < MAXPLAYERS; ++i)
            players[i].didSecret = true;
    }
# endif

    // Determine the next map.
    nextMap = G_GetNextMap(gameEpisode, gameMap, secretExit);
#endif

    // Time for an intermission.
#if __JDOOM64__
    S_StartMusic("dm2int", true);
#elif __JDOOM__
    S_StartMusic((gameModeBits & GM_ANY_DOOM2)? "dm2int" : "inter", true);
#elif __JHERETIC__
    S_StartMusic("intr", true);
#elif __JHEXEN__
    S_StartMusic("hub", true);
#endif
    S_PauseMusic(true);

    Con_Busy(BUSYF_TRANSITION, NULL, prepareIntermission, NULL);

#if __JHERETIC__
    // @fixme is this necessary at this time?
    NetSv_SendGameState(0, DDSP_ALL_PLAYERS);
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    NetSv_Intermission(IMF_BEGIN, 0, 0);
#else /* __JHEXEN__ */
    NetSv_Intermission(IMF_BEGIN, (int) nextMap, (int) nextMapEntryPoint);
#endif

    S_PauseMusic(false);
}

#if __JDOOM__ || __JDOOM64__
void G_PrepareWIData(void)
{
    int             i;
    ddmapinfo_t     minfo;
    char            levid[8];
    wbstartstruct_t *info = &wmInfo;

    info->maxFrags = 0;

    P_MapId(gameEpisode, gameMap, levid);

    // See if there is a par time definition.
    if(Def_Get(DD_DEF_MAP_INFO, levid, &minfo) && minfo.parTime > 0)
        info->parTime = TICRATE * (int) minfo.parTime;
    else
        info->parTime = -1; // Unknown.

    info->pNum = CONSOLEPLAYER;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t       *p = &players[i];
        wbplayerstruct_t *pStats = &info->plyr[i];

        pStats->inGame = p->plr->inGame;
        pStats->kills = p->killCount;
        pStats->items = p->itemCount;
        pStats->secret = p->secretCount;
        pStats->time = mapTime;
        memcpy(pStats->frags, p->frags, sizeof(pStats->frags));
    }
}
#endif

void G_WorldDone(void)
{
    ddfinale_t fin;

#if __JDOOM__ || __JDOOM64__
    if(secretExit)
        players[CONSOLEPLAYER].didSecret = true;
#endif

    // Clear the currently playing script, if any.
    FI_StackClear();

    if(G_DebriefingEnabled(gameEpisode, gameMap, &fin) &&
       G_StartFinale(fin.script, 0, FIMODE_AFTER))
    {
        return;
    }

    // We have either just returned from a debriefing or there wasn't one.
    briefDisabled = false;

    G_SetGameAction(GA_LEAVEMAP);
}

void G_DoWorldDone(void)
{
#if __JHEXEN__
    SV_MapTeleport(nextMap, nextMapEntryPoint);
    rebornPosition = nextMapEntryPoint;
#else
# if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    gameMap = nextMap;
# endif

    G_DoLoadMap();
#endif

    G_SetGameAction(GA_NONE);
}

#if __JHEXEN__
/**
 * Called by G_Ticker based on gameaction.  Loads a game from the reborn
 * save slot.
 */
void G_DoSingleReborn(void)
{
    G_SetGameAction(GA_NONE);
    SV_LoadGame(SV_HxGetRebornSlot());
}
#endif

boolean G_IsLoadGamePossible(void)
{
    return !(IS_CLIENT && !Get(DD_PLAYBACK));
}

boolean G_LoadGame(int slot)
{
    if(!G_IsLoadGamePossible()) return false;

    // Check whether this slot is in use. We do this here also because we
    // need to provide our caller with instant feedback. Naturally this is
    // no guarantee that the game-save will be acessible come load time.

    // First ensure we have up-to-date info.
    SV_UpdateGameSaveInfo();
    if(!SV_IsGameSaveSlotUsed(slot))
    {
        Con_Message("Warning:G_LoadGame: Save slot #%i is not in use, aborting load.\n", slot);
        return false;
    }

    // Everything appears to be in order - schedule the game-save load!
    gaLoadGameSlot = slot;
    G_SetGameAction(GA_LOADGAME);
    return true;
}

/**
 * Called by G_Ticker based on gameaction.
 */
void G_DoLoadGame(void)
{
    G_SetGameAction(GA_NONE);
    if(SV_LoadGame(gaLoadGameSlot))
    {
#if __JHEXEN__
        if(!IS_NETGAME)
        {
            // Copy the base slot to the reborn slot.
            SV_HxUpdateRebornSlot();
        }
#endif
    }
}

boolean G_IsSaveGamePossible(void)
{
    player_t* player;

    if(IS_CLIENT || Get(DD_PLAYBACK)) return false;
    if(GS_MAP != G_GetGameState()) return false;

    player = &players[CONSOLEPLAYER];
    if(PST_DEAD == player->playerState) return false;

    return true;
}

boolean G_SaveGame2(int slot, const char* name)
{
    player_t* player = &players[CONSOLEPLAYER];

    if(0 > slot || slot >= NUMSAVESLOTS) return false;
    if(!G_IsSaveGamePossible()) return false;

    gaSaveGameSlot = slot;
    if(NULL != name && name[0])
    {
        // A new name.
        strncpy(gaSaveGameName, name, GA_SAVEGAME_NAME_LASTINDEX);
        gaSaveGameName[GA_SAVEGAME_NAME_LASTINDEX] = '\0';
    }
    else
    {
        // Reusing the current name or generating a new one.
        gaSaveGameGenerateName = (NULL != name && !name[0]);
        memset(gaSaveGameName, 0, GA_SAVEGAME_NAME_LASTINDEX);
    }
    G_SetGameAction(GA_SAVEGAME);
    return true;
}

boolean G_SaveGame(int slot)
{
    return G_SaveGame2(slot, NULL);
}

ddstring_t* G_GenerateSaveGameName(void)
{
    ddstring_t* str = Str_New();
    int time = mapTime / TICRATE, hours, seconds, minutes;
    const char* baseName = NULL, *mapName = P_GetMapNiceName();
    lumpname_t mapIdentifier;
    char baseNameBuf[256];

    hours   = time / 3600; time -= hours * 3600;
    minutes = time / 60;   time -= minutes * 60;
    seconds = time;

#if __JHEXEN__
    // No map name? Try MAPINFO.
    if(NULL == mapName)
    {
        mapName = P_GetMapName(gameMap);
    }
#endif
    // Still no map name? Use the identifier.
    // Some tricksy modders provide us with an empty map name...
    // \todo Move this logic engine-side.
    if(NULL == mapName || !mapName[0] || mapName[0] == ' ')
    {
        P_MapId(gameEpisode, gameMap, mapIdentifier);
        mapName = mapIdentifier;
    }

    if(!P_IsMapFromIWAD(gameEpisode, gameMap))
    {
        F_ExtractFileBase(baseNameBuf, P_MapSourceFile(gameEpisode, gameMap), 256);
        baseName = baseNameBuf;
    }

    Str_Appendf(str, "%s%s%s %02i:%02i:%02i", (NULL != baseName? baseName : ""),
        (NULL != baseName? ":" : ""), mapName, hours, minutes, seconds);

    return str;
}

/**
 * Called by G_Ticker based on gameaction.
 */
void G_DoSaveGame(void)
{
    boolean mustFreeNameStr = false;
    const ddstring_t* nameStr = NULL;
    const char* name;

    if(0 != strlen(gaSaveGameName))
    {
        name = gaSaveGameName;
    }
    else
    {
        // No name specified.
        const gamesaveinfo_t* info = SV_GetGameSaveInfoForSlot(gaSaveGameSlot);
        if(!gaSaveGameGenerateName && !Str_IsEmpty(&info->name))
        {
            // Slot already in use; reuse the existing name.
            nameStr = &info->name;
        }
        else
        {
            nameStr = G_GenerateSaveGameName();
            mustFreeNameStr = true;
        }
        name = Str_Text(nameStr);
    }

    // Try to make a new game-save.
    if(SV_SaveGame(gaSaveGameSlot, name))
    {
        //Hu_MenuUpdateGameSaveWidgets();
        P_SetMessage(&players[CONSOLEPLAYER], TXT_GAMESAVED, false);
    }
    G_SetGameAction(GA_NONE);

    if(mustFreeNameStr)
        Str_Delete((ddstring_t*)nameStr);
}

#if __JHEXEN__
void G_DeferredNewGame(skillmode_t skill)
{
    dSkill = skill;
    G_SetGameAction(GA_NEWGAME);
}

void G_DoInitNew(void)
{
    SV_HxInitBaseSlot();
    G_InitNew(dSkill, dEpisode, dMap);
    G_SetGameAction(GA_NONE);
}
#endif

/**
 * Can be called by the startup code or the menu task, CONSOLEPLAYER,
 * DISPLAYPLAYER, playeringame[] should be set.
 */
void G_DeferedInitNew(skillmode_t skill, uint episode, uint map)
{
    dSkill = skill;
    dEpisode = episode;
    dMap = map;

#if __JHEXEN__
    G_SetGameAction(GA_INITNEW);
#else
    G_SetGameAction(GA_NEWGAME);
#endif
}

void G_DoNewGame(void)
{
    G_StopDemo();
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    if(!IS_NETGAME)
    {
        deathmatch = false;
        respawnMonsters = false;
        noMonstersParm = ArgExists("-nomonsters")? true : false;
    }
    G_InitNew(dSkill, dEpisode, dMap);
#else
    G_StartNewGame(dSkill);
#endif
    G_SetGameAction(GA_NONE);
}

/**
 * Start a new game.
 */
void G_InitNew(skillmode_t skill, uint episode, uint map)
{
    int i;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    int speed;
#endif

    // Close any open automaps.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;
        ST_AutomapOpen(i, false, true);
#if __JHERETIC__ || __JHEXEN__
        Hu_InventoryOpen(i, false);
#endif
    }

    // If there are any InFine scripts running, they must be stopped.
    FI_StackClear();

    if(paused)
    {
        paused = false;
    }

    if(skill < SM_BABY)
        skill = SM_BABY;
    if(skill > NUM_SKILL_MODES - 1)
        skill = NUM_SKILL_MODES - 1;

    // Make sure that the episode and map numbers are good.
    G_ValidateMap(&episode, &map);

    M_ResetRandom();

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__ || __JSTRIFE__
    respawnMonsters = respawnParm;
#endif

#if __JDOOM__ || __JHERETIC__
    // Is respawning enabled at all in nightmare skill?
    if(skill == SM_NIGHTMARE)
        respawnMonsters = cfg.respawnMonstersNightmare;
#endif

//// \kludge Doom/Heretic Fast Monters/Missiles
#if __JDOOM__ || __JDOOM64__
    // Fast monsters?
    if(fastParm
# if __JDOOM__
        || (skill == SM_NIGHTMARE && gameSkill != SM_NIGHTMARE)
# endif
        )
    {
        for(i = S_SARG_RUN1; i <= S_SARG_RUN8; ++i)
            STATES[i].tics = 1;
        for(i = S_SARG_ATK1; i <= S_SARG_ATK3; ++i)
            STATES[i].tics = 4;
        for(i = S_SARG_PAIN; i <= S_SARG_PAIN2; ++i)
            STATES[i].tics = 1;
    }
    else
    {
        for(i = S_SARG_RUN1; i <= S_SARG_RUN8; ++i)
            STATES[i].tics = 2;
        for(i = S_SARG_ATK1; i <= S_SARG_ATK3; ++i)
            STATES[i].tics = 8;
        for(i = S_SARG_PAIN; i <= S_SARG_PAIN2; ++i)
            STATES[i].tics = 2;
    }
#endif

    // Fast missiles?
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
# if __JDOOM64__
    speed = fastParm;
# elif __JDOOM__
    speed = (fastParm || (skill == SM_NIGHTMARE && gameSkill != SM_NIGHTMARE));
# else
    speed = skill == SM_NIGHTMARE;
# endif

    for(i = 0; MonsterMissileInfo[i].type != -1; ++i)
    {
        MOBJINFO[MonsterMissileInfo[i].type].speed =
            MonsterMissileInfo[i].speed[speed];
    }
#endif
// <-- KLUDGE

    if(!IS_CLIENT)
    {
        // Force players to be initialized upon first map load.
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            player_t           *plr = &players[i];

            plr->playerState = PST_REBORN;
#if __JHEXEN__
            plr->worldTimer = 0;
#else
            plr->didSecret = false;
#endif
        }
    }

    userGame = true; // Will be set false if a demo.
    paused = false;
    gameEpisode = episode;
    gameMap = map;
    gameSkill = skill;

    NetSv_UpdateGameConfig();

    G_DoLoadMap();
}

int G_QuitGameResponse(msgresponse_t response, void* context)
{
    if(response == MSG_YES)
    {
        G_SetGameAction(GA_QUIT);
    }
    return true;
}

void G_QuitGame(void)
{
    const char* endString;

    if(G_GetGameAction() == GA_QUIT)
        return; // Already in progress.

#if __JDOOM__ || __JDOOM64__
    endString = endmsg[((int) GAMETIC % (NUM_QUITMESSAGES + 1))];
#else
    endString = GET_TXT(TXT_QUITMSG);
#endif

#if __JDOOM__ || __JDOOM64__
    S_LocalSound(SFX_SWTCHN, NULL);
#elif __JHERETIC__
    S_LocalSound(SFX_SWITCH, NULL);
#elif __JHEXEN__
    S_LocalSound(SFX_PICKUP_KEY, NULL);
#endif

    Con_Open(false);
    Hu_MsgStart(MSG_YESNO, endString, G_QuitGameResponse, NULL);
}

/**
 * Return the index of this map.
 */
uint G_GetMapNumber(uint episode, uint map)
{
#if __JHEXEN__
    return P_TranslateMap(map);
#elif __JDOOM64__
    return map;
#else
# if __JDOOM__
    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
        return map;
    else
# endif
    {
        return map + episode * 9; // maps per episode.
    }
#endif
}

/**
 * Compose the name of the map lump identifier.
 */
void P_MapId(uint episode, uint map, char* lumpName)
{
#if __JDOOM64__
    sprintf(lumpName, "MAP%02u", map+1);
#elif __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
        sprintf(lumpName, "MAP%02u", map+1);
    else
        sprintf(lumpName, "E%uM%u", episode+1, map+1);
#elif  __JHERETIC__
    sprintf(lumpName, "E%uM%u", episode+1, map+1);
#else
    sprintf(lumpName, "MAP%02u", map+1);
#endif
}

/**
 * return               @c true if the specified map is present.
 */
boolean P_MapExists(uint episode, uint map)
{
    char buf[9];
    P_MapId(episode, map, buf);
    return W_CheckLumpNumForName2(buf, true) >= 0;
}

/**
 * return               Name of the source file containing the map if present, else 0.
 */
const char* P_MapSourceFile(uint episode, uint map)
{
    lumpnum_t lumpNum;
    char buf[9];
    P_MapId(episode, map, buf);
    if((lumpNum = W_CheckLumpNumForName2(buf, true)) >= 0)
        return W_LumpSourceFile(lumpNum);
    return 0;
}

/**
 * Returns true if the specified (episode, map) pair can be used.
 * Otherwise the values are adjusted so they are valid.
 */
boolean G_ValidateMap(uint* episode, uint* map)
{
    boolean ok = true;

#if __JDOOM64__
    if(*map > 98)
    {
        *map = 98;
        ok = false;
    }
#elif __JDOOM__
    if(gameModeBits & (GM_DOOM_SHAREWARE|GM_DOOM_CHEX))
    {
        if(*episode != 0)
        {
            *episode = 0;
            ok = false;
        }
    }
    else
    {
        if(*episode > 8)
        {
            *episode = 8;
            ok = false;
        }
    }

    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
    {
        if(*map > 98)
        {
            *map = 98;
            ok = false;
        }
    }
    else
    {
        if(*map > 8)
        {
            *map = 8;
            ok = false;
        }
    }

#elif __JHERETIC__
    //  Allow episodes 0-8.
    if(*episode > 8)
    {
        *episode = 8;
        ok = false;
    }

    if(*map > 8)
    {
        *map = 8;
        ok = false;
    }

    if(gameMode == heretic_shareware)
    {
        if(*episode != 0)
        {
            *episode = 0;
            ok = false;
        }
    }
    else if(gameMode == heretic_extended)
    {
        if(*episode == 5)
        {
            if(*map > 2)
            {
                *map = 2;
                ok = false;
            }
        }
        else if(*episode > 4)
        {
            *episode = 4;
            ok = false;
        }
    }
    else // Registered version checks
    {
        if(*episode == 3)
        {
            if(*map != 0)
            {
                *map = 0;
                ok = false;
            }
        }
        else if(*episode > 2)
        {
            *episode = 2;
            ok = false;
        }
    }
#elif __JHEXEN__
    if(*map > 98)
    {
        *map = 98;
        ok = false;
    }
#endif

    // Check that the map truly exists.
    if(!P_MapExists(*episode, *map))
    {
        // (0,0) should exist always?
        *episode = 0;
        *map = 0;
        ok = false;
    }

    return ok;
}

/**
 * Return the next map according to the default map progression.
 *
 * @param episode       Current episode.
 * @param map           Current map.
 * @param secretExit
 * @return              The next map.
 */
uint G_GetNextMap(uint episode, uint map, boolean secretExit)
{
#if __JHEXEN__
    return G_GetMapNumber(episode, P_GetMapNextMap(map));
#elif __JDOOM64__
    if(secretExit)
    {
        switch(map)
        {
        case 0: return 31;
        case 3: return 28;
        case 11: return 29;
        case 17: return 30;
        case 31: return 0;
        default:
            Con_Message("G_NextMap: Warning - No secret exit on map %u!", map+1);
            break;
        }
    }

    switch(map)
    {
    case 23: return 27;
    case 31: return 0;
    case 28: return 4;
    case 29: return 12;
    case 30: return 18;
    case 24: return 0;
    case 25: return 0;
    case 26: return 0;
    default:
        return map + 1;
    }
#elif __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
    {
        if(secretExit)
        {
            switch(map)
            {
            case 14: return 30;
            case 30: return 31;
            default:
               Con_Message("G_NextMap: Warning - No secret exit on map %u!", map+1);
               break;
            }
        }

        switch(map)
        {
        case 30:
        case 31: return 15;
        default:
            return map + 1;
        }
    }
    else if(gameMode == doom_chex)
    {
        return map + 1; // Go to next map.
    }
    else
    {
        if(secretExit && map != 8)
            return 8; // Go to secret map.

        switch(map)
        {
        case 8: // Returning from secret map.
            switch(episode)
            {
            case 0: return 3;
            case 1: return 5;
            case 2: return 6;
            case 3: return 2;
            default:
                Con_Error("G_NextMap: Invalid episode num #%u!", episode);
            }
            return 0; // Unreachable
        default:
            return map + 1; // Go to next map.
        }
    }
#elif __JHERETIC__
    if(secretExit && map != 8)
        return 8; // Go to secret map.

    switch(map)
    {
    case 8: // Returning from secret map.
        switch(episode)
        {
        case 0: return 6;
        case 1: return 4;
        case 2: return 4;
        case 3: return 4;
        case 4: return 3;
        default:
            Con_Error("G_NextMap: Invalid episode num #%u!", episode);
        }
        return 0; // Unreachable
    default:
        return map + 1; // Go to next map.
    }
#endif
}

#if __JHERETIC__
const char* P_GetShortMapName(uint episode, uint map)
{
    const char* name = P_GetMapName(episode, map);
    const char* ptr;

    // Skip over the "ExMx:" from the beginning.
    ptr = strchr(name, ':');
    if(!ptr)
        return name;

    name = ptr + 1;
    while(*name && isspace(*name))
        name++; // Skip any number of spaces.

    return name;
}

const char* P_GetMapName(uint episode, uint map)
{
    char                id[10], *ptr;
    ddmapinfo_t         info;

    // Compose the map identifier.
    P_MapId(episode, map, id);

    // Get the map info definition.
    if(!Def_Get(DD_DEF_MAP_INFO, id, &info))
    {
        // There is no map information for this map...
        return "";
    }

    if(Def_Get(DD_DEF_TEXT, info.name, &ptr) != -1)
        return ptr;

    return info.name;
}
#endif

/**
 * Print a list of maps and the WAD files where they are from.
 */
void G_PrintFormattedMapList(uint episode, const char** files, uint count)
{
    const char* current = NULL;
    uint i, k, rangeStart = 0, len;
    char mapId[9];

    for(i = 0; i < count; ++i)
    {
        if(!current && files[i])
        {
            current = files[i];
            rangeStart = i;
        }
        else if(current && (!files[i] || stricmp(current, files[i])))
        {
            // Print a range.
            len = i - rangeStart;
            Con_Printf("  "); // Indentation.
            if(len <= 2)
            {
                for(k = rangeStart; k < i; ++k)
                {
                    P_MapId(episode, k, mapId);
                    Con_Printf("%s%s", mapId, (k != i-1) ? "," : "");
                }
            }
            else
            {
                P_MapId(episode, rangeStart, mapId);
                Con_Printf("%s-", mapId);
                P_MapId(episode, i-1, mapId);
                Con_Printf("%s", mapId);
            }
            Con_Printf(": %s\n", F_PrettyPath(current));

            // Moving on to a different file.
            current = files[i];
            rangeStart = i;
        }
    }
}

/**
 * Print a list of loaded maps and which WAD files are they located in.
 * The maps are identified using the "ExMy" and "MAPnn" markers.
 */
void G_PrintMapList(void)
{
    uint episode, map, numEpisodes, maxMapsPerEpisode;
    const char* sourceList[100];

#if __JDOOM__
    if(gameMode == doom_ultimate)
    {
        numEpisodes = 4;
        maxMapsPerEpisode = 9;
    }
    else if(gameMode == doom)
    {
        numEpisodes = 3;
        maxMapsPerEpisode = 9;
    }
    else
    {
        numEpisodes = 1;
        maxMapsPerEpisode = gameMode == doom_chex? 5 : 99;
    }
#elif __JHERETIC__
    if(gameMode == heretic_extended)
        numEpisodes = 6;
    else if(gameMode == heretic)
        numEpisodes = 3;
    else
        numEpisodes = 1;
    maxMapsPerEpisode = 9;
#else
    numEpisodes = 1;
    maxMapsPerEpisode = 99;
#endif

    for(episode = 0; episode < numEpisodes; ++episode)
    {
        memset((void*) sourceList, 0, sizeof(sourceList));

        // Find the name of each map (not all may exist).
        for(map = 0; map < maxMapsPerEpisode; ++map)
        {
            sourceList[map] = P_MapSourceFile(episode, map);
        }
        G_PrintFormattedMapList(episode, sourceList, 99);
    }
}

/**
 * Check if there is a finale before the map.
 * Returns true if a finale was found.
 */
int G_BriefingEnabled(uint episode, uint map, ddfinale_t* fin)
{
    char mid[20];

    // If we're already in the INFINE state, don't start a finale.
    if(briefDisabled || G_GetGameState() == GS_INFINE || IS_CLIENT || Get(DD_PLAYBACK))
        return false;

    // Is there such a finale definition?
    P_MapId(episode, map, mid);

    return Def_Get(DD_DEF_FINALE_BEFORE, mid, fin);
}

/**
 * Check if there is a finale after the map.
 * Returns true if a finale was found.
 */
int G_DebriefingEnabled(uint episode, uint map, ddfinale_t* fin)
{
    char mid[20];

    // If we're already in the INFINE state, don't start a finale.
    if(briefDisabled)
        return false;
#if __JHEXEN__
    if(cfg.overrideHubMsg && G_GetGameState() == GS_MAP &&
       !(nextMap == DDMAXINT && nextMapEntryPoint == DDMAXINT) &&
       P_GetMapCluster(map) != P_GetMapCluster(nextMap))
        return false;
#endif
    if(G_GetGameState() == GS_INFINE || IS_CLIENT || Get(DD_PLAYBACK))
        return false;

    // Is there such a finale definition?
    P_MapId(episode, map, mid);
    return Def_Get(DD_DEF_FINALE_AFTER, mid, fin);
}

/**
 * Stops both playback and a recording. Called at critical points like
 * starting a new game, or ending the game in the menu.
 */
void G_StopDemo(void)
{
    DD_Execute(true, "stopdemo");
}

int Hook_DemoStop(int hookType, int val, void* paramaters)
{
    boolean aborted = val != 0;

    G_ChangeGameState(GS_WAITING);

    if(!aborted && singledemo)
    {   // Playback ended normally.
        G_SetGameAction(GA_QUIT);
        return true;
    }

    G_SetGameAction(GA_NONE);

    if(IS_NETGAME && IS_CLIENT)
    {
        // Restore normal game state?
        deathmatch = false;
        noMonstersParm = false;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        respawnMonsters = false;
#endif
#if __JHEXEN__
        randomClassParm = false;
#endif
    }

    {int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        ST_AutomapOpen(i, false, true);
#if __JHERETIC__ || __JHEXEN__
        Hu_InventoryOpen(i, false);
#endif
    }}
    return true;
}

void G_ScreenShot(void)
{
    G_SetGameAction(GA_SCREENSHOT);
}

/**
 * Find an unused screenshot file name. Uses the game's identity key as the
 * file name base.
 * @return  Composed file name. Must be released with Str_Delete().
 */
static ddstring_t* composeScreenshotFileName(void)
{
    ddgameinfo_t gameInfo;
    ddstring_t* name;
    int numPos;

    if(!DD_GetGameInfo(&gameInfo))
    {
        Con_Error("composeScreenshotFileName: Failed retrieving GameInfo.");
        return NULL; // Unreachable.
    }
   
    name = Str_Appendf(Str_New(), "%s-", gameInfo.identityKey);
    numPos = Str_Length(name);
    { int i;
    for(i = 0; i < 1e6; ++i) // Stop eventually...
    {
        Str_Appendf(name, "%03i.tga", i);
        if(!F_FileExists(Str_Text(name)))
            break;
        Str_Truncate(name, numPos);
    }}
    return name;
}

void G_DoScreenShot(void)
{
    ddstring_t* name = composeScreenshotFileName();
    if(NULL == name)
    {
        Con_Message("G_DoScreenShot: Failed composing file name, screenshot not saved.\n");
        return;
    }

    if(0 != M_ScreenShot(Str_Text(name), 24))
    {
        Con_Message("Wrote screenshot \"%s\"\n", F_PrettyPath(Str_Text(name)));
    }
    Str_Delete(name);
}

static void openLoadMenu(void)
{
    Hu_MenuCommand(MCMD_OPEN);
    /// \fixme This should be called automatically when opening the page
    /// thus making this function redundant.
    Hu_MenuUpdateGameSaveWidgets();
    Hu_MenuSetActivePage(&LoadMenu);
}

static void openSaveMenu(void)
{
    Hu_MenuCommand(MCMD_OPEN);
    /// \fixme This should be called automatically when opening the page
    /// thus making this function redundant.
    Hu_MenuUpdateGameSaveWidgets();
    Hu_MenuSetActivePage(&SaveMenu);
}

int G_QuickLoadGameResponse(msgresponse_t response, void* context)
{
    if(response == MSG_YES)
    {
        const int slot = Con_GetInteger("game-save-quick-slot");
        G_LoadGame(slot);
    }
    return true;
}

void G_QuickLoadGame(void)
{
    const int slot = Con_GetInteger("game-save-quick-slot");
    const gamesaveinfo_t* info;
    char buf[80];

    if(GA_QUIT == G_GetGameAction())
    {
        return;
    }

    if(IS_NETGAME)
    {
        S_LocalSound(SFX_QUICKLOAD_PROMPT, NULL);
        Hu_MsgStart(MSG_ANYKEY, QLOADNET, NULL, NULL);
        return;
    }

    if(0 > slot || !SV_IsGameSaveSlotUsed(slot))
    {
        S_LocalSound(SFX_QUICKLOAD_PROMPT, NULL);
        Hu_MsgStart(MSG_ANYKEY, QSAVESPOT, NULL, NULL);
        return;
    }

    if(!cfg.confirmQuickGameSave)
    {
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        G_LoadGame(slot);
        return;
    }

    info = SV_GetGameSaveInfoForSlot(slot);
    dd_snprintf(buf, 80, QLPROMPT, Str_Text(&info->name));

    S_LocalSound(SFX_QUICKLOAD_PROMPT, NULL);
    Hu_MsgStart(MSG_YESNO, buf, G_QuickLoadGameResponse, NULL);
}

int G_QuickSaveGameResponse(msgresponse_t response, void* context)
{
    if(response == MSG_YES)
    {
        const int slot = Con_GetInteger("game-save-quick-slot");
        G_SaveGame(slot);
    }
    return true;
}

void G_QuickSaveGame(void)
{
    player_t* player = &players[CONSOLEPLAYER];
    const int slot = Con_GetInteger("game-save-quick-slot");
    boolean slotIsUsed;
    char buf[80];

    if(GA_QUIT == G_GetGameAction())
    {
        return;
    }

    if(player->playerState == PST_DEAD || Get(DD_PLAYBACK))
    {
        S_LocalSound(SFX_QUICKSAVE_PROMPT, NULL);
        Hu_MsgStart(MSG_ANYKEY, SAVEDEAD, NULL, NULL);
        return;
    }

    if(G_GetGameState() != GS_MAP)
    {
        S_LocalSound(SFX_QUICKSAVE_PROMPT, NULL);
        Hu_MsgStart(MSG_ANYKEY, SAVEOUTMAP, NULL, NULL);
        return;
    }

    // If no quick-save slot has been nominated - allow doing so now.
    if(0 > slot)
    {
        Hu_MenuCommand(MCMD_OPEN);
        Hu_MenuUpdateGameSaveWidgets();
        Hu_MenuSetActivePage(&SaveMenu);
        menuNominatingQuickSaveSlot = true;
        return;
    }

    slotIsUsed = SV_IsGameSaveSlotUsed(slot);
    if(!slotIsUsed || !cfg.confirmQuickGameSave)
    {
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        G_SaveGame(slot);
        return;
    }

    if(slotIsUsed)
    {
        const gamesaveinfo_t* info = SV_GetGameSaveInfoForSlot(slot);
        sprintf(buf, QSPROMPT, Str_Text(&info->name));
    }
    else
    {
        char identifier[11];
        dd_snprintf(identifier, 10, "#%10.i", slot);
        dd_snprintf(buf, 80, QLPROMPT, identifier);
    }

    S_LocalSound(SFX_QUICKSAVE_PROMPT, NULL);
    Hu_MsgStart(MSG_YESNO, buf, G_QuickSaveGameResponse, NULL);
}

D_CMD(LoadGameName)
{
    int slot;
    if(!G_IsLoadGamePossible()) return false;

    slot = SV_ParseGameSaveSlot(argv[1]);
    if(slot >= 0)
    {
        // A known slot identifier. Try to schedule a GA_LOADGAME action.
        return G_LoadGame(slot);
    }

    // Clearly the caller needs some assistance...
    Con_Message("Failed to determine game-save slot from \"%s\"\n", argv[1]);

    // We'll open the load menu if caller is the console.
    // Reasoning: User attempted to load a named game-save however the name
    // specified didn't match anything known. Opening the load menu allows
    // the user to see the names of the known game-saves.
    if(CMDS_CONSOLE == src)
    {
        Con_Message("Opening game-save load menu...\n");
        openLoadMenu();
        return true;
    }

    // No action means the command failed.
    return false;
}

D_CMD(LoadGame)
{
    if(!G_IsLoadGamePossible()) return false;
    openLoadMenu();
    return true;
}

D_CMD(SaveGameName)
{
    int slot;
    if(!G_IsSaveGamePossible()) return false;
    
    slot = SV_ParseGameSaveSlot(argv[1]);
    if(slot >= 0)
    {
        // A known slot identifier. Try to schedule a GA_SAVEGAME action.
        // We do not care if there is a save already present in this slot.
        return G_SaveGame2(slot, argc > 2? argv[2] : NULL);
    }

    // Clearly the caller needs some assistance...
    Con_Message("Failed to determine game-save slot from \"%s\"\n", argv[1]);
    // No action means the command failed.
    return false;
}

D_CMD(SaveGame)
{
    if(!G_IsSaveGamePossible()) return false;
    openSaveMenu();
    return true;
}

D_CMD(QuickLoadGame)
{
    G_QuickLoadGame();
    return true;
}

D_CMD(QuickSaveGame)
{
    G_QuickSaveGame();
    return true;
}

D_CMD(HelpScreen)
{
    G_StartHelp();
    return true;
}

D_CMD(EndGame)
{
    G_EndGame();
    return true;
}

D_CMD(CycleTextureGamma)
{
    R_CycleGammaLevel();
    return true;
}

D_CMD(ListMaps)
{
    Con_Message("Available maps:\n");
    G_PrintMapList();
    return true;
}
