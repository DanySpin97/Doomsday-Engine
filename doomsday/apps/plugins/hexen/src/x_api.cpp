/**\file x_api.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * Doomsday API exchange - jHexen specific
 */

#include <assert.h>
#include <string.h>
#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>
#include <gamefw/libgamefw.h>

#include "doomsday.h"

#include "jhexen.h"

#include "d_netsv.h"
#include "d_net.h"
#include "fi_lib.h"
#include "g_common.h"
#include "g_update.h"
#include "hu_menu.h"
#include "p_mapsetup.h"
#include "r_common.h"
#include "p_map.h"
#include "polyobjs.h"

using namespace de;

// Identifiers given to the games we register during startup.
static char const *gameIds[NUM_GAME_MODES] =
{
    "hexen-demo",
    "hexen",
    "hexen-dk",
    "hexen-betademo",
    "hexen-v10",
};

#define CONFIGDIR               "hexen"
#define STARTUPPK3              "libhexen.pk3"
#define LEGACYSAVEGAMENAMEEXP   "^(?:hex)[0-9]{1,1}(?:.hxs)"
#define LEGACYSAVEGAMESUBFOLDER "hexndata"

static void setCommonParameters(Game &game)
{
    //game.addResource(RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    game.addRequiredPackage("net.dengine.legacy.hexen_2");

    Record gameplayOptions;
    gameplayOptions.set("noMonsters", Record::withMembers("label", "No Monsters", "type", "boolean", "default", false));
    gameplayOptions.set("turbo", Record::withMembers("label", "Move Speed", "type", "number", "default", 1.0, "min", 0.1, "max", 4.0, "step", 0.1));
    game.objectNamespace().set(Game::DEF_OPTIONS, gameplayOptions);
}

/**
 * Register the game modes supported by this plugin.
 */
static int G_RegisterGames(int, int, void *)
{
    Games &games = DoomsdayApp::games();

    /* Hexen (Death Kings) */
    Game &deathkings = games.defineGame(gameIds[hexen_deathkings],
        Record::withMembers(Game::DEF_CONFIG_DIR, CONFIGDIR,
                            Game::DEF_TITLE, "Hexen: Deathkings of the Dark Citadel",
                            Game::DEF_AUTHOR, "Raven Software",
                            Game::DEF_RELEASE_DATE, "1996-01-01",
                            Game::DEF_TAGS, "hexen deathkings",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/hexen-dk.mapinfo"));
    deathkings.addRequiredPackage("com.ravensoftware.hexen com.ravensoftware.hexen.mac");
    deathkings.addRequiredPackage("com.ravensoftware.hexen.deathkings");
    setCommonParameters(deathkings);
    //deathkings.addResource(RC_PACKAGE, FF_STARTUP, "hexdd.wad", "MAP59;MAP60");
    //deathkings.addResource(RC_PACKAGE, FF_STARTUP, "hexen.wad", "MAP08;MAP22;TINTTAB;FOGMAP;TRANTBLA;DARTA1;ARTIPORK;SKYFOG;TALLYTOP;GROVER");
    deathkings.addResource(RC_DEFINITION, 0, "hexen-dk.ded", 0);

    /* Hexen */
    Game &hxn = games.defineGame(gameIds[hexen],
        Record::withMembers(Game::DEF_CONFIG_DIR, CONFIGDIR,
                            Game::DEF_TITLE, "Hexen",
                            Game::DEF_AUTHOR, "Raven Software",
                            Game::DEF_RELEASE_DATE, "1995-12-01",
                            Game::DEF_TAGS, "hexen",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/hexen.mapinfo"));
    hxn.addRequiredPackage("com.ravensoftware.hexen_1.1 com.ravensoftware.hexen.mac_1.1");
    setCommonParameters(hxn);
    //hxn.addResource(RC_PACKAGE, FF_STARTUP, "hexen.wad", "MAP08;MAP22;TINTTAB;FOGMAP;TRANTBLA;DARTA1;ARTIPORK;SKYFOG;TALLYTOP;GROVER");
    hxn.addResource(RC_DEFINITION, 0, "hexen.ded", 0);

    /* Hexen (v1.0) */
    Game &hexen10 = games.defineGame(gameIds[hexen_v10],
        Record::withMembers(Game::DEF_CONFIG_DIR, CONFIGDIR,
                            Game::DEF_TITLE, "Hexen v1.0",
                            Game::DEF_AUTHOR, "Raven Software",
                            Game::DEF_RELEASE_DATE, "1995-10-30",
                            Game::DEF_TAGS, "hexen",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/hexen.mapinfo"));
    hexen10.addRequiredPackage("com.ravensoftware.hexen_1.0");
    setCommonParameters(hexen10);
    //hexen10.addResource(RC_PACKAGE, FF_STARTUP, "hexen.wad", "MAP08;MAP22;MAP41;TINTTAB;FOGMAP;DARTA1;ARTIPORK;SKYFOG;GROVER");
    hexen10.addResource(RC_DEFINITION, 0, "hexen-v10.ded", 0);

    /* Hexen (Demo) */
    Game &demo = games.defineGame(gameIds[hexen_demo],
        Record::withMembers(Game::DEF_CONFIG_DIR, CONFIGDIR,
                            Game::DEF_TITLE, "Hexen 4-map Demo",
                            Game::DEF_AUTHOR, "Raven Software",
                            Game::DEF_RELEASE_DATE, "1995-10-18",
                            Game::DEF_TAGS, "hexen demo",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/hexen.mapinfo"));
    demo.addRequiredPackage("com.ravensoftware.hexen.demo "
                            "com.ravensoftware.hexen.macdemo");
    setCommonParameters(demo);
    //demo.addResource(RC_PACKAGE, FF_STARTUP, "hexendemo.wad;machexendemo.wad;hexen.wad", "MAP01;MAP04;TINTTAB;FOGMAP;DARTA1;ARTIPORK;DEMO3==18150");
    demo.addResource(RC_DEFINITION, 0, "hexen-demo.ded", 0);

    /* Hexen (Beta Demo) */
    Game &beta = games.defineGame(gameIds[hexen_betademo],
        Record::withMembers(Game::DEF_CONFIG_DIR, CONFIGDIR,
                            Game::DEF_TITLE, "Hexen 4-map Beta Demo",
                            Game::DEF_AUTHOR, "Raven Software",
                            Game::DEF_RELEASE_DATE, "1995-10-01",
                            Game::DEF_TAGS, "hexen demo",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/hexen.mapinfo"));
    beta.addRequiredPackage("com.ravensoftware.hexen.beta");
    setCommonParameters(beta);
    //beta.addResource(RC_PACKAGE, FF_STARTUP, "hexendemo.wad;machexendemo.wad;hexenbeta.wad;hexen.wad", "MAP01;MAP04;TINTTAB;FOGMAP;DARTA1;ARTIPORK;AFLYA0;DEMO3==13866");
    beta.addResource(RC_DEFINITION, 0, "hexen-demo.ded", 0);

    return true;
}

#undef STARTUPPK3
#undef CONFIGDIR

/**
 * Called right after the game plugin is selected into use.
 */
DENG_ENTRYPOINT void DP_Load(void)
{
    Plug_AddHook(HOOK_VIEWPORT_RESHAPE, R_UpdateViewport);
    gfw_SetCurrentGame(GFW_HEXEN);
}

/**
 * Called when the game plugin is freed from memory.
 */
DENG_ENTRYPOINT void DP_Unload(void)
{
    Plug_RemoveHook(HOOK_VIEWPORT_RESHAPE, R_UpdateViewport);
}

void G_PreInit(char const *gameId)
{
    /// \todo Refactor me away.
    { size_t i;
    for(i = 0; i < NUM_GAME_MODES; ++i)
        if(!strcmp(gameIds[i], gameId))
        {
            gameMode = (gamemode_t) i;
            gameModeBits = 1 << gameMode;
            break;
        }
    if(i == NUM_GAME_MODES)
        Con_Error("Failed gamemode lookup for id %i.", gameId);
    }

    X_PreInit();
}

/**
 * Called by the engine to initiate a soft-shutdown request.
 */
dd_bool G_TryShutdown(void)
{
    G_QuitGame();
    return true;
}

DENG_ENTRYPOINT void *GetGameAPI(char const *name)
{
    if (auto *ptr = Common_GetGameAPI(name))
    {
        return ptr;
    }

    #define HASH_ENTRY(Name, Func) std::make_pair(QByteArray(Name), de::function_cast<void *>(Func))
    static QHash<QByteArray, void *> const funcs(
    {
        HASH_ENTRY("DrawWindow",    X_DrawWindow),
        HASH_ENTRY("EndFrame",      X_EndFrame),
        HASH_ENTRY("GetInteger",    X_GetInteger),
        HASH_ENTRY("GetPointer",    X_GetVariable),
        HASH_ENTRY("PostInit",      X_PostInit),
        HASH_ENTRY("PreInit",       G_PreInit),
        HASH_ENTRY("Shutdown",      X_Shutdown),
        HASH_ENTRY("TryShutdown",   G_TryShutdown),
    });
    #undef HASH_ENTRY

    auto found = funcs.find(name);
    if (found != funcs.end()) return found.value();
    return nullptr;
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
DENG_ENTRYPOINT void DP_Initialize(void)
{
    Plug_AddHook(HOOK_STARTUP, G_RegisterGames);
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
DENG_ENTRYPOINT char const *deng_LibraryType(void)
{
    return "deng-plugin/game";
}

#if defined (DENG_STATIC_LINK)

DENG_EXTERN_C void *staticlib_hexen_symbol(char const *name)
{
    DENG_SYMBOL_PTR(name, deng_LibraryType)
    DENG_SYMBOL_PTR(name, DP_Initialize);
    DENG_SYMBOL_PTR(name, DP_Load);
    DENG_SYMBOL_PTR(name, DP_Unload);
    DENG_SYMBOL_PTR(name, GetGameAPI);
    qWarning() << name << "not found in hexen";
    return nullptr;
}

#else

DENG_DECLARE_API(Base);
DENG_DECLARE_API(B);
DENG_DECLARE_API(Busy);
DENG_DECLARE_API(Client);
DENG_DECLARE_API(Con);
DENG_DECLARE_API(Def);
DENG_DECLARE_API(F);
DENG_DECLARE_API(FR);
DENG_DECLARE_API(GL);
DENG_DECLARE_API(Infine);
DENG_DECLARE_API(InternalData);
DENG_DECLARE_API(Material);
DENG_DECLARE_API(Map);
DENG_DECLARE_API(MPE);
DENG_DECLARE_API(Player);
DENG_DECLARE_API(R);
DENG_DECLARE_API(Rend);
DENG_DECLARE_API(S);
DENG_DECLARE_API(Server);
DENG_DECLARE_API(Svg);
DENG_DECLARE_API(Thinker);
DENG_DECLARE_API(Uri);

DENG_API_EXCHANGE(
    DENG_GET_API(DE_API_BASE, Base);
    DENG_GET_API(DE_API_BINDING, B);
    DENG_GET_API(DE_API_BUSY, Busy);
    DENG_GET_API(DE_API_CLIENT, Client);
    DENG_GET_API(DE_API_CONSOLE, Con);
    DENG_GET_API(DE_API_DEFINITIONS, Def);
    DENG_GET_API(DE_API_FILE_SYSTEM, F);
    DENG_GET_API(DE_API_FONT_RENDER, FR);
    DENG_GET_API(DE_API_GL, GL);
    DENG_GET_API(DE_API_INFINE, Infine);
    DENG_GET_API(DE_API_INTERNAL_DATA, InternalData);
    DENG_GET_API(DE_API_MATERIALS, Material);
    DENG_GET_API(DE_API_MAP, Map);
    DENG_GET_API(DE_API_MAP_EDIT, MPE);
    DENG_GET_API(DE_API_PLAYER, Player);
    DENG_GET_API(DE_API_RESOURCE, R);
    DENG_GET_API(DE_API_RENDER, Rend);
    DENG_GET_API(DE_API_SOUND, S);
    DENG_GET_API(DE_API_SERVER, Server);
    DENG_GET_API(DE_API_SVG, Svg);
    DENG_GET_API(DE_API_THINKER, Thinker);
    DENG_GET_API(DE_API_URI, Uri);
)

#endif
