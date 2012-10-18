/**
 * @file game.cpp
 *
 * @ingroup core
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "updater/downloaddialog.h"
#include "abstractresource.h"

#include <de/Error>
#include <de/Log>
#include <de/memory.h>

#include "game.h"

namespace de {

typedef struct {
    struct AbstractResource_s** records;
    size_t numRecords;
} resourcerecordset_t;

struct Game::Instance
{
    /// Unique identifier of the plugin which registered this game.
    pluginid_t pluginId;

    /// Unique identifier string (e.g., "doom1-ultimate").
    ddstring_t identityKey;

    /// Formatted default title suitable for printing (e.g., "The Ultimate DOOM").
    ddstring_t title;

    /// Formatted default author suitable for printing (e.g., "id Software").
    ddstring_t author;

    /// The base directory for all data-class resources.
    ddstring_t dataPath;

    /// The base directory for all defs-class resources.
    ddstring_t defsPath;

    /// Name of the main config file (e.g., "configs/doom/game.cfg").
    ddstring_t mainConfig;

    /// Name of the file used for control bindings, set automatically at creation time.
    ddstring_t bindingConfig;

    /// Vector of records for required game resources (e.g., doomu.wad).
    resourcerecordset_t requiredResources[RESOURCECLASS_COUNT];

    Instance(char const* _identityKey, ddstring_t const* _dataPath,
             ddstring_t const* _defsPath, char const* configDir)
        : pluginId(0)
    {
        Str_Set(Str_InitStd(&identityKey), _identityKey);
        DENG_ASSERT(!Str_IsEmpty(&identityKey));

        Str_InitStd(&title);
        Str_InitStd(&author);

        Str_Set(Str_InitStd(&dataPath), Str_Text(_dataPath));
        Str_Set(Str_InitStd(&defsPath), Str_Text(_defsPath));

        Str_Appendf(Str_InitStd(&mainConfig), "configs/%s", configDir);
        Str_Strip(&mainConfig);
        F_FixSlashes(&mainConfig, &mainConfig);
        F_AppendMissingSlash(&mainConfig);
        Str_Append(&mainConfig, "game.cfg");

        Str_Appendf(Str_InitStd(&bindingConfig), "configs/%s", configDir);
        Str_Strip(&bindingConfig);
        F_FixSlashes(&bindingConfig, &bindingConfig);
        F_AppendMissingSlash(&bindingConfig);
        Str_Append(&bindingConfig, "player/bindings.cfg");

        memset(requiredResources, 0, sizeof requiredResources);
    }

    ~Instance()
    {
        Str_Free(&identityKey);
        Str_Free(&dataPath);
        Str_Free(&defsPath);
        Str_Free(&mainConfig);
        Str_Free(&bindingConfig);
        Str_Free(&title);
        Str_Free(&author);

        for(int i = 0; i < RESOURCECLASS_COUNT; ++i)
        {
            resourcerecordset_t* rset = &requiredResources[i];
            AbstractResource** rec;

            if(!rset || rset->numRecords == 0) continue;

            for(rec = rset->records; *rec; rec++)
            {
                AbstractResource_Delete(*rec);
            }
            M_Free(rset->records); rset->records = 0;
            rset->numRecords = 0;
        }
    }
};

Game::Game(char const* identityKey, ddstring_t const* dataPath,
    ddstring_t const* defsPath, char const* configDir, char const* title,
    char const* author)
{
    d = new Instance(identityKey, dataPath, defsPath, configDir);
    if(title)  Str_Set(&d->title, title);
    if(author) Str_Set(&d->author, author);
}

Game::~Game()
{
    delete d;
}

GameCollection& Game::collection() const
{
    return *reinterpret_cast<de::GameCollection*>(App_GameCollection());
}

Game& Game::addResource(resourceclass_t rclass, AbstractResource& record)
{
    if(!VALID_RESOURCE_CLASS(rclass))
        throw de::Error("Game::addResource", QString("Invalid resource class %1").arg(rclass));

    resourcerecordset_t* rset = &d->requiredResources[rclass];
    rset->records = (AbstractResource**) M_Realloc(rset->records, sizeof(*rset->records) * (rset->numRecords+2));
    rset->records[rset->numRecords] = &record;
    rset->records[rset->numRecords+1] = 0; // Terminate.
    rset->numRecords++;
    return *this;
}

bool Game::isRequiredResource(char const* absolutePath)
{
    AbstractResource* const* records = resources(RC_PACKAGE, 0);
    if(records)
    {
        // Is this resource from a container?
        try
        {
            FileInfo const& info = App_FileSystem()->zipFileInfo(absolutePath);
            // Yes; use the container's path instead.
            absolutePath = Str_Text(info.container->path());
        }
        catch(FS1::NotFoundError const&)
        {} // Ignore this error.

        for(AbstractResource* const* i = records; *i; i++)
        {
            AbstractResource* record = *i;
            if(AbstractResource_ResourceFlags(record) & RF_STARTUP)
            {
                ddstring_t const* resolvedPath = AbstractResource_ResolvedPath(record, true);
                if(resolvedPath && !Str_CompareIgnoreCase(resolvedPath, absolutePath))
                {
                    return true;
                }
            }
        }
    }
    // Not found, so no.
    return false;
}

bool Game::allStartupResourcesFound() const
{
    for(uint i = 0; i < RESOURCECLASS_COUNT; ++i)
    {
        AbstractResource* const* records = resources(resourceclass_t(i), 0);
        if(!records) continue;

        for(AbstractResource* const* i = records; *i; i++)
        {
            AbstractResource* record = *i;
            int const flags = AbstractResource_ResourceFlags(record);

            if((flags & RF_STARTUP) && !(flags & RF_FOUND))
                return false;
        }
    }
    return true;
}

Game& Game::setPluginId(pluginid_t newId)
{
    d->pluginId = newId;
    return *this;
}

pluginid_t Game::pluginId() const
{
    return d->pluginId;
}

ddstring_t const& Game::identityKey() const
{
    return d->identityKey;
}

ddstring_t const& Game::dataPath() const
{
    return d->dataPath;
}

ddstring_t const& Game::defsPath() const
{
    return d->defsPath;
}

ddstring_t const& Game::mainConfig() const
{
    return d->mainConfig;
}

ddstring_t const& Game::bindingConfig() const
{
    return d->bindingConfig;
}

ddstring_t const& Game::title() const
{
    return d->title;
}

ddstring_t const& Game::author() const
{
    return d->author;
}

AbstractResource* const* Game::resources(resourceclass_t rclass, int* count) const
{
    if(!VALID_RESOURCE_CLASS(rclass))
    {
        if(count) *count = 0;
        return 0;
    }

    if(count) *count = (int)d->requiredResources[rclass].numRecords;
    return d->requiredResources[rclass].records? d->requiredResources[rclass].records : 0;
}

Game* Game::fromDef(GameDef const& def)
{
    AutoStr* dataPath = AutoStr_NewStd();
    Str_Set(dataPath, def.dataPath);
    Str_Strip(dataPath);
    F_FixSlashes(dataPath, dataPath);
    F_ExpandBasePath(dataPath, dataPath);
    F_AppendMissingSlash(dataPath);

    AutoStr* defsPath = AutoStr_NewStd();
    Str_Set(defsPath, def.defsPath);
    Str_Strip(defsPath);
    F_FixSlashes(defsPath, defsPath);
    F_ExpandBasePath(defsPath, defsPath);
    F_AppendMissingSlash(defsPath);

    return new Game(def.identityKey, dataPath, defsPath, def.configDir,
                    def.defaultTitle, def.defaultAuthor);
}

void Game::printBanner(Game const& game)
{
    Con_PrintRuler();
    Con_FPrintf(CPF_WHITE | CPF_CENTER, "%s\n", Str_Text(&game.title()));
    Con_PrintRuler();
}

void Game::printResources(Game const& game, bool printStatus, int rflags)
{
    size_t count = 0;
    for(uint i = 0; i < RESOURCECLASS_COUNT; ++i)
    {
        AbstractResource* const* records = game.resources((resourceclass_t)i, 0);
        if(!records) continue;

        for(AbstractResource* const* recordIt = records; *recordIt; recordIt++)
        {
            AbstractResource* rec = *recordIt;

            if(rflags >= 0 && (rflags & AbstractResource_ResourceFlags(rec)))
            {
                AbstractResource_Print(rec, printStatus);
                count += 1;
            }
        }
    }

    if(count == 0)
        Con_Printf(" None\n");
}

void Game::print(Game const& game, int flags)
{
    if(isNullGame(game))
        flags &= ~PGF_BANNER;

#if _DEBUG
    Con_Printf("pluginid:%i data:\"%s\" defs:\"%s\"\n", int(game.pluginId()),
               F_PrettyPath(Str_Text(&game.dataPath())),
               F_PrettyPath(Str_Text(&game.defsPath())));
#endif

    if(flags & PGF_BANNER)
        printBanner(game);

    if(!(flags & PGF_BANNER))
        Con_Printf("Game: %s - ", Str_Text(&game.title()));
    else
        Con_Printf("Author: ");
    Con_Printf("%s\n", Str_Text(&game.author()));
    Con_Printf("IdentityKey: %s\n", Str_Text(&game.identityKey()));

    if(flags & PGF_LIST_STARTUP_RESOURCES)
    {
        Con_Printf("Startup resources:\n");
        printResources(game, (flags & PGF_STATUS) != 0, RF_STARTUP);
    }

    if(flags & PGF_LIST_OTHER_RESOURCES)
    {
        Con_Printf("Other resources:\n");
        Con_Printf("   ");
        printResources(game, /*(flags & PGF_STATUS) != 0*/false, 0);
    }

    if(flags & PGF_STATUS)
        Con_Printf("Status: %s\n",
                   game.collection().isCurrentGame(game)? "Loaded" :
                         game.allStartupResourcesFound()? "Complete/Playable" :
                                                          "Incomplete/Not playable");
}

NullGame::NullGame(ddstring_t const* dataPath, ddstring_t const* defsPath)
    : Game("null-game", dataPath, defsPath, "doomsday", "null-game", "null-game")
{}

} // namespace de

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    reinterpret_cast<de::Game*>(inst)

#define TOINTERNAL_CONST(inst) \
    reinterpret_cast<de::Game const*>(inst)

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::Game* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    de::Game const* self = TOINTERNAL_CONST(inst)

struct game_s* Game_New(char const* identityKey, ddstring_t const* dataPath, ddstring_t const* defsPath,
                        char const* configDir, char const* title, char const* author)
{
    return reinterpret_cast<struct game_s*>(new de::Game(identityKey, dataPath, defsPath, configDir, title, author));
}

void Game_Delete(struct game_s* game)
{
    if(game)
    {
        SELF(game);
        delete self;
    }
}

boolean Game_IsNullObject(Game const* game)
{
    if(!game) return false;
    return de::isNullGame(*reinterpret_cast<de::Game const*>(game));
}

struct game_s* Game_AddResource(struct game_s* game, resourceclass_t rclass, struct AbstractResource_s* record)
{
    SELF(game);
    DENG_ASSERT(record);
    self->addResource(rclass, *record);
    return game;
}

boolean Game_IsRequiredResource(struct game_s* game, char const* absolutePath)
{
    SELF(game);
    return self->isRequiredResource(absolutePath);
}

boolean Game_AllStartupResourcesFound(struct game_s const* game)
{
    SELF_CONST(game);
    return self->allStartupResourcesFound();
}

struct game_s* Game_SetPluginId(struct game_s* game, pluginid_t pluginId)
{
    SELF(game);
    return reinterpret_cast<struct game_s*>(&self->setPluginId(pluginId));
}

pluginid_t Game_PluginId(struct game_s const* game)
{
    SELF_CONST(game);
    return self->pluginId();
}

ddstring_t const* Game_IdentityKey(struct game_s const* game)
{
    SELF_CONST(game);
    return &self->identityKey();
}

ddstring_t const* Game_Title(struct game_s const* game)
{
    SELF_CONST(game);
    return &self->title();
}

ddstring_t const* Game_Author(struct game_s const* game)
{
    SELF_CONST(game);
    return &self->author();
}

ddstring_t const* Game_MainConfig(struct game_s const* game)
{
    SELF_CONST(game);
    return &self->mainConfig();
}

ddstring_t const* Game_BindingConfig(struct game_s const* game)
{
    SELF_CONST(game);
    return &self->bindingConfig();
}

struct AbstractResource_s* const* Game_Resources(struct game_s const* game, resourceclass_t rclass, int* count)
{
    SELF_CONST(game);
    return self->resources(rclass, count);
}

ddstring_t const* Game_DataPath(struct game_s const* game)
{
    SELF_CONST(game);
    return &self->dataPath();
}

ddstring_t const* Game_DefsPath(struct game_s const* game)
{
    SELF_CONST(game);
    return &self->defsPath();
}

struct game_s* Game_FromDef(GameDef const* def)
{
    if(!def) return 0;
    return reinterpret_cast<struct game_s*>(de::Game::fromDef(*def));
}

void Game_PrintBanner(Game const* game)
{
    if(!game) return;
    de::Game::printBanner(*reinterpret_cast<de::Game const*>(game));
}

void Game_PrintResources(Game const* game, boolean printStatus, int rflags)
{
    if(!game) return;
    de::Game::printResources(*reinterpret_cast<de::Game const*>(game), CPP_BOOL(printStatus), rflags);
}

void Game_Print(Game const* game, int flags)
{
    if(!game) return;
    de::Game::print(*reinterpret_cast<de::Game const*>(game), flags);
}

/// @todo Do this really belong here? Semantically, this appears misplaced. -ds
void Game_Notify(int notification, void* param)
{
    DENG_UNUSED(param);

    switch(notification)
    {
    case DD_NOTIFY_GAME_SAVED:
        // If an update has been downloaded and is ready to go, we should
        // re-show the dialog now that the user has saved the game as
        // prompted.
        DEBUG_Message(("Game_Notify: Game saved.\n"));
        Updater_RaiseCompletedDownloadDialog();
        break;
    }
}
