/** @file dd_main.cpp
 *
 * Engine core.
 * @ingroup core
 *
 * @todo Much of this should be refactored and merged into the App classes.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#define DENG_NO_API_MACROS_BASE // functions defined here

#ifdef WIN32
#  define _WIN32_DCOM
#  include <objbase.h>
#endif

#include <QStringList>
#include <de/App>
#include <de/NativePath>
#include <de/binangle.h>

#ifdef __CLIENT__
#  include "clientapp.h"
#  include <de/DisplayMode>
#endif

#ifdef __SERVER__
#  include "serverapp.h"
#endif

#include "de_platform.h"

#ifdef UNIX
#  include <ctype.h>
#endif

#include <cstring>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "de_network.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_audio.h"
#include "de_ui.h"
#include "de_edit.h"
#include "de_filesys.h"
#include "de_resource.h"

#include "updater.h"
#include "m_misc.h"

#include "gl/svg.h"

#include "api_map.h"
#include "world/entitydef.h"
#include "world/p_players.h"
#ifdef __CLIENT__
#  include "Contact"
#endif
#include "world/world.h"

#include "ui/p_control.h"

#ifdef __CLIENT__
#  include "gl/gl_texmanager.h"

#  include "render/vlight.h"
#  include "ui/widgets/taskbarwidget.h"
#endif

#include <de/ArrayValue>
#include <de/DictionaryValue>

extern int renderTextures;

using namespace de;

class ZipFileType : public de::NativeFileType
{
public:
    ZipFileType() : NativeFileType("FT_ZIP", RC_PACKAGE)
    {}

    de::File1* interpret(de::FileHandle& hndl, String path, FileInfo const& info) const
    {
        if(Zip::recognise(hndl))
        {
            LOG_AS("ZipFileType");
            LOG_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\".");
            return new Zip(hndl, path, info);
        }
        return 0;
    }
};

class WadFileType : public de::NativeFileType
{
public:
    WadFileType() : NativeFileType("FT_WAD", RC_PACKAGE)
    {}

    de::File1* interpret(de::FileHandle& hndl, String path, FileInfo const& info) const
    {
        if(Wad::recognise(hndl))
        {
            LOG_AS("WadFileType");
            LOG_VERBOSE("Interpreted \"" + NativePath(path).pretty() + "\".");
            return new Wad(hndl, path, info);
        }
        return 0;
    }
};

typedef struct ddvalue_s {
    int*            readPtr;
    int*            writePtr;
} ddvalue_t;

static int DD_StartupWorker(void* parameters);
static int DD_DummyWorker(void* parameters);
static void DD_AutoLoad();
static void initPathMappings();

/**
 * @param path          Path to the file to be loaded. Either a "real" file in
 *                      the local file system, or a "virtual" file.
 * @param baseOffset    Offset from the start of the file in bytes to begin.
 *
 * @return  @c true iff the referenced file was loaded.
 */
static de::File1* tryLoadFile(de::Uri const& path, size_t baseOffset = 0);

static bool tryUnloadFile(de::Uri const& path);

filename_t ddBasePath = ""; // Doomsday root directory is at...?
filename_t ddRuntimePath, ddBinPath;

int isDedicated;

int verbose; // For debug messages (-verbose).

// List of file names, whitespace seperating (written to .cfg).
char* startupFiles = (char*) ""; // ignore warning

// Id of the currently running title finale if playing, else zero.
finaleid_t titleFinale;

int gameDataFormat; // Use a game-specifc data format where applicable.

static NullFileType nullFileType;

/// A symbolic name => file type map.
static FileTypes fileTypeMap;

// List of session data files (specified via the command line or in a cfg, or
// found using the default search algorithm (e.g., /auto and DOOMWADDIR)).
static ddstring_t** sessionResourceFileList;
static size_t numSessionResourceFileList;

#ifndef WIN32
extern GETGAMEAPI GetGameAPI;
#endif

#ifdef __CLIENT__

D_CMD(CheckForUpdates)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    Con_Message("Checking for available updates...");
    ClientApp::updater().checkNow(Updater::OnlyShowResultIfUpdateAvailable);
    return true;
}

D_CMD(CheckForUpdatesAndNotify)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    Con_Message("Checking for available updates...");
    ClientApp::updater().checkNow(Updater::AlwaysShowResult);
    return true;
}

D_CMD(LastUpdated)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    ClientApp::updater().printLastUpdated();
    return true;
}

D_CMD(ShowUpdateSettings)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    ClientApp::updater().showSettings();
    return true;
}

#endif // __CLIENT__

void DD_CreateFileTypes()
{
    FileType* ftype;

    /*
     * Packages types:
     */
    ResourceClass& packageClass = App_ResourceClass("RC_PACKAGE");

    ftype = new ZipFileType();
    ftype->addKnownExtension(".pk3");
    ftype->addKnownExtension(".zip");
    packageClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new WadFileType();
    ftype->addKnownExtension(".wad");
    packageClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new FileType("FT_LMP", RC_PACKAGE); ///< Treat lumps as packages so they are mapped to $App.DataPath.
    ftype->addKnownExtension(".lmp");
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    /*
     * Definition fileTypes:
     */
    ftype = new FileType("FT_DED", RC_DEFINITION);
    ftype->addKnownExtension(".ded");
    App_ResourceClass("RC_DEFINITION").addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    /*
     * Graphic fileTypes:
     */
    ResourceClass& graphicClass = App_ResourceClass("RC_GRAPHIC");

    ftype = new FileType("FT_PNG", RC_GRAPHIC);
    ftype->addKnownExtension(".png");
    graphicClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new FileType("FT_TGA", RC_GRAPHIC);
    ftype->addKnownExtension(".tga");
    graphicClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new FileType("FT_JPG", RC_GRAPHIC);
    ftype->addKnownExtension(".jpg");
    graphicClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new FileType("FT_PCX", RC_GRAPHIC);
    ftype->addKnownExtension(".pcx");
    graphicClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    /*
     * Model fileTypes:
     */
    ResourceClass& modelClass = App_ResourceClass("RC_MODEL");

    ftype = new FileType("FT_DMD", RC_MODEL);
    ftype->addKnownExtension(".dmd");
    modelClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new FileType("FT_MD2", RC_MODEL);
    ftype->addKnownExtension(".md2");
    modelClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    /*
     * Sound fileTypes:
     */
    ftype = new FileType("FT_WAV", RC_SOUND);
    ftype->addKnownExtension(".wav");
    App_ResourceClass("RC_SOUND").addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    /*
     * Music fileTypes:
     */
    ResourceClass& musicClass = App_ResourceClass("RC_MUSIC");

    ftype = new FileType("FT_OGG", RC_MUSIC);
    ftype->addKnownExtension(".ogg");
    musicClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new FileType("FT_MP3", RC_MUSIC);
    ftype->addKnownExtension(".mp3");
    musicClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new FileType("FT_MOD", RC_MUSIC);
    ftype->addKnownExtension(".mod");
    musicClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    ftype = new FileType("FT_MID", RC_MUSIC);
    ftype->addKnownExtension(".mid");
    musicClass.addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    /*
     * Font fileTypes:
     */
    ftype = new FileType("FT_DFN", RC_FONT);
    ftype->addKnownExtension(".dfn");
    App_ResourceClass("RC_FONT").addFileType(*ftype);
    fileTypeMap.insert(ftype->name().toLower(), ftype);

    /*
     * Misc fileTypes:
     */
    ftype = new FileType("FT_DEH", RC_PACKAGE); ///< Treat DeHackEd patches as packages so they are mapped to $App.DataPath.
    ftype->addKnownExtension(".deh");
    fileTypeMap.insert(ftype->name().toLower(), ftype);
}

FileType& DD_FileTypeByName(String name)
{
    if(!name.isEmpty())
    {
        FileTypes::iterator found = fileTypeMap.find(name.toLower());
        if(found != fileTypeMap.end()) return **found;
    }
    return nullFileType; // Not found.
}

FileType& DD_GuessFileTypeFromFileName(String path)
{
    if(!path.isEmpty())
    {
        DENG2_FOR_EACH_CONST(FileTypes, i, fileTypeMap)
        {
            FileType& ftype = **i;
            if(ftype.fileNameIsKnown(path))
                return ftype;
        }
    }
    return nullFileType;
}

FileTypes const& DD_FileTypes()
{
    return fileTypeMap;
}

static void createPackagesScheme()
{
    FS1::Scheme& scheme = App_FileSystem().createScheme("Packages");

    /*
     * Add default search paths.
     *
     * Note that the order here defines the order in which these paths are searched
     * thus paths must be added in priority order (newer paths have priority).
     */

#ifdef UNIX
    // There may be an iwaddir specified in a system-level config file.
    filename_t fn;
    if(UnixInfo_GetConfigValue("paths", "iwaddir", fn, FILENAME_T_MAXLEN))
    {
        NativePath path = de::App::app().commandLine().startupPath() / fn;
        scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), SearchPath::NoDescend));
        LOG_INFO("Using paths.iwaddir: %s") << path.pretty();
    }
#endif

    // Add the path from the DOOMWADDIR environment variable.
    if(!CommandLine_Check("-nodoomwaddir") && getenv("DOOMWADDIR"))
    {
        NativePath path = App::app().commandLine().startupPath() / getenv("DOOMWADDIR");
        scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), SearchPath::NoDescend));
        LOG_INFO("Using DOOMWADDIR: %s") << path.pretty();
    }

    // Add any paths from the DOOMWADPATH environment variable.
    if(!CommandLine_Check("-nodoomwadpath") && getenv("DOOMWADPATH"))
    {
#if WIN32
#  define SEP_CHAR      ';'
#else
#  define SEP_CHAR      ':'
#endif

        QStringList allPaths = QString(getenv("DOOMWADPATH")).split(SEP_CHAR, QString::SkipEmptyParts);
        for(int i = allPaths.count(); i--> 0; )
        {
            NativePath path = App::app().commandLine().startupPath() / allPaths[i];
            scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), SearchPath::NoDescend));
            LOG_INFO("Using DOOMWADPATH: %s") << path.pretty();
        }

#undef SEP_CHAR
    }

    scheme.addSearchPath(SearchPath(de::Uri("$(App.DataPath)/", RC_NULL), SearchPath::NoDescend));
    scheme.addSearchPath(SearchPath(de::Uri("$(App.DataPath)/$(GamePlugin.Name)/", RC_NULL), SearchPath::NoDescend));
}

void DD_CreateFileSystemSchemes()
{
    int const schemedef_max_searchpaths = 5;
    struct schemedef_s {
        char const* name;
        char const* optOverridePath;
        char const* optFallbackPath;
        FS1::Scheme::Flags flags;
        SearchPath::Flags searchPathFlags;
        /// Priority is right to left.
        char const* searchPaths[schemedef_max_searchpaths];
    } defs[] = {
        { "Defs",         NULL,           NULL,           FS1::Scheme::Flag(0), 0,
            { "$(App.DefsPath)/", "$(App.DefsPath)/$(GamePlugin.Name)/", "$(App.DefsPath)/$(GamePlugin.Name)/$(Game.IdentityKey)/" }
        },
        { "Graphics",     "-gfxdir2",     "-gfxdir",      FS1::Scheme::Flag(0), 0,
            { "$(App.DataPath)/graphics/" }
        },
        { "Models",       "-modeldir2",   "-modeldir",    FS1::Scheme::MappedInPackages, 0,
            { "$(App.DataPath)/$(GamePlugin.Name)/models/", "$(App.DataPath)/$(GamePlugin.Name)/models/$(Game.IdentityKey)/" }
        },
        { "Sfx",          "-sfxdir2",     "-sfxdir",      FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/$(GamePlugin.Name)/sfx/", "$(App.DataPath)/$(GamePlugin.Name)/sfx/$(Game.IdentityKey)/" }
        },
        { "Music",        "-musdir2",     "-musdir",      FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/$(GamePlugin.Name)/music/", "$(App.DataPath)/$(GamePlugin.Name)/music/$(Game.IdentityKey)/" }
        },
        { "Textures",     "-texdir2",     "-texdir",      FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/$(GamePlugin.Name)/textures/", "$(App.DataPath)/$(GamePlugin.Name)/textures/$(Game.IdentityKey)/" }
        },
        { "Flats",        "-flatdir2",    "-flatdir",     FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/$(GamePlugin.Name)/flats/", "$(App.DataPath)/$(GamePlugin.Name)/flats/$(Game.IdentityKey)/" }
        },
        { "Patches",      "-patdir2",     "-patdir",      FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/$(GamePlugin.Name)/patches/", "$(App.DataPath)/$(GamePlugin.Name)/patches/$(Game.IdentityKey)/" }
        },
        { "LightMaps",    "-lmdir2",      "-lmdir",       FS1::Scheme::MappedInPackages, 0,
            { "$(App.DataPath)/$(GamePlugin.Name)/lightmaps/" }
        },
        { "Fonts",        "-fontdir2",    "-fontdir",     FS1::Scheme::MappedInPackages, SearchPath::NoDescend,
            { "$(App.DataPath)/fonts/", "$(App.DataPath)/$(GamePlugin.Name)/fonts/", "$(App.DataPath)/$(GamePlugin.Name)/fonts/$(Game.IdentityKey)/" }
        },
        { 0, 0, 0, FS1::Scheme::Flag(0), 0, { 0 } }
    };

    createPackagesScheme();

    // Setup the rest...
    struct schemedef_s const* def = defs;
    for(int i = 0; defs[i].name; ++i, ++def)
    {
        FS1::Scheme& scheme = App_FileSystem().createScheme(def->name, def->flags);

        int searchPathCount = 0;
        while(def->searchPaths[searchPathCount] && ++searchPathCount < schemedef_max_searchpaths)
        {}

        for(int j = 0; j < searchPathCount; ++j)
        {
            scheme.addSearchPath(SearchPath(de::Uri(def->searchPaths[j], RC_NULL), def->searchPathFlags));
        }

        if(def->optOverridePath && CommandLine_CheckWith(def->optOverridePath, 1))
        {
            NativePath path = NativePath(CommandLine_NextAsPath());
            scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), def->searchPathFlags), FS1::OverridePaths);
            path = path / "$(Game.IdentityKey)";
            scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), def->searchPathFlags), FS1::OverridePaths);
        }

        if(def->optFallbackPath && CommandLine_CheckWith(def->optFallbackPath, 1))
        {
            NativePath path = NativePath(CommandLine_NextAsPath());
            scheme.addSearchPath(SearchPath(de::Uri::fromNativeDirPath(path), def->searchPathFlags), FS1::FallbackPaths);
        }
    }
}

ResourceSystem &App_ResourceSystem()
{
#ifdef __CLIENT__
    if(ClientApp::haveApp())
    {
        return ClientApp::resourceSystem();
    }
#endif

#ifdef __SERVER__
    if(ServerApp::haveApp())
    {
        return ServerApp::resourceSystem();
    }
#endif
    throw Error("App_ResourceSystem", "App not yet initialized");
}

de::ResourceClass &App_ResourceClass(String className)
{
    return App_ResourceSystem().resClass(className);
}

de::ResourceClass &App_ResourceClass(resourceclassid_t classId)
{
    return App_ResourceSystem().resClass(classId);
}

/**
 * Register the engine commands and variables.
 */
void DD_Register(void)
{
#ifdef __CLIENT__
    C_CMD("update",          "", CheckForUpdates);
    C_CMD("updateandnotify", "", CheckForUpdatesAndNotify);
    C_CMD("updatesettings",  "", ShowUpdateSettings);
    C_CMD("lastupdated",     "", LastUpdated);
#endif

    DD_RegisterLoop();
    F_Register();
    Con_Register();
    DH_Register();
    R_Register();
    S_Register();
#ifdef __CLIENT__
    B_Register(); // for control bindings
    DD_RegisterInput();
    SBE_Register(); // for bias editor
    Rend_Register();
    GL_Register();
    H_Register();
    UI_Register();
    Demo_Register();
    P_ControlRegister();
    I_Register();
#endif
    ResourceSystem::consoleRegister();
    Net_Register();
    World::consoleRegister();
    FI_Register();
}

static void addToPathList(ddstring_t*** list, size_t* listSize, const char* rawPath)
{
    ddstring_t* newPath = Str_New();
    DENG_ASSERT(list && listSize && rawPath && rawPath[0]);

    Str_Set(newPath, rawPath);
    F_FixSlashes(newPath, newPath);
    F_ExpandBasePath(newPath, newPath);

    *list = (ddstring_t**) M_Realloc(*list, sizeof(**list) * ++(*listSize));
    (*list)[(*listSize)-1] = newPath;
}

static void parseStartupFilePathsAndAddFiles(const char* pathString)
{
#define ATWSEPS                 ",; \t"

    char* token, *buffer;
    size_t len;

    if(!pathString || !pathString[0]) return;

    len = strlen(pathString);
    buffer = (char*)malloc(len + 1);
    if(!buffer) Con_Error("parseStartupFilePathsAndAddFiles: Failed on allocation of %lu bytes for parse buffer.", (unsigned long) (len+1));

    strcpy(buffer, pathString);
    token = strtok(buffer, ATWSEPS);
    while(token)
    {
        tryLoadFile(de::Uri(token, RC_NULL));
        token = strtok(NULL, ATWSEPS);
    }
    free(buffer);

#undef ATWSEPS
}

static void destroyPathList(ddstring_t*** list, size_t* listSize)
{
    DENG_ASSERT(list && listSize);
    if(*list)
    {
        size_t i;
        for(i = 0; i < *listSize; ++i)
            Str_Delete((*list)[i]);
        M_Free(*list); *list = 0;
    }
    *listSize = 0;
}

/**
 * Begin the Doomsday title animation sequence.
 */
void DD_StartTitle(void)
{
#ifdef __CLIENT__
    ddfinale_t fin;
    if(!Def_Get(DD_DEF_FINALE, "background", &fin)) return;

    ddstring_t setupCmds; Str_Init(&setupCmds);

    // Configure the predefined fonts (all normal, variable width).
    char const *fontName = R_ChooseVariableFont(FS_NORMAL, DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT);

    for(int i = 1; i <= FIPAGE_NUM_PREDEFINED_FONTS; ++i)
    {
        Str_Appendf(&setupCmds, "prefont %i System:%s\n", i, fontName);
    }

    // Configure the predefined colors.
    for(int i = 1; i <= MIN_OF(NUM_UI_COLORS, FIPAGE_NUM_PREDEFINED_FONTS); ++i)
    {
        ui_color_t* color = UI_Color(i - 1);
        Str_Appendf(&setupCmds, "precolor %i %f %f %f\n", i, color->red, color->green, color->blue);
    }

    titleFinale = FI_Execute2(fin.script, FF_LOCAL, Str_Text(&setupCmds));
    Str_Free(&setupCmds);
#endif
}

/**
 * Find all game data file paths in the auto directory with the extensions
 * wad, lmp, pk3, zip and deh.
 *
 * @param found         List of paths to be populated.
 *
 * @return  Number of paths added to @a found.
 */
static int findAllGameDataPaths(FS1::PathList &found)
{
    static String const extensions[] = {
        "wad", "lmp", "pk3", "zip", "deh",
#ifdef UNIX
        "WAD", "LMP", "PK3", "ZIP", "DEH", // upper case alternatives
#endif
        ""
    };
    int const numFoundSoFar = found.count();
    for(uint extIdx = 0; !extensions[extIdx].isEmpty(); ++extIdx)
    {
        Path pattern = Path("$(App.DataPath)/$(GamePlugin.Name)/auto/*." + extensions[extIdx]);
        App_FileSystem().findAllPaths(de::Uri(pattern).resolved(), 0, found);
    }
    return found.count() - numFoundSoFar;
}

/**
 * Find and try to load all game data file paths in auto directory.
 *
 * @return Number of new files that were loaded.
 */
static int loadFilesFromDataGameAuto()
{
    FS1::PathList found;
    findAllGameDataPaths(found);

    int numLoaded = 0;
    DENG2_FOR_EACH_CONST(FS1::PathList, i, found)
    {
        // Ignore directories.
        if(i->attrib & A_SUBDIR) continue;

        if(tryLoadFile(de::Uri(i->path, RC_NULL)))
        {
            numLoaded += 1;
        }
    }
    return numLoaded;
}

/**
 * Find and list all game data file paths in the auto directory.
 *
 * @return Number of new files that were added to the list.
 */
static int listFilesFromDataGameAuto(ddstring_t*** list, size_t* listSize)
{
    if(!list || !listSize) return 0;

    FS1::PathList found;
    findAllGameDataPaths(found);

    int numFilesAdded = 0;
    DENG2_FOR_EACH_CONST(FS1::PathList, i, found)
    {
        // Ignore directories.
        if(i->attrib & A_SUBDIR) continue;

        QByteArray foundPath = i->path.toUtf8();
        addToPathList(list, listSize, foundPath.constData());

        numFilesAdded += 1;
    }
    return numFilesAdded;
}

boolean DD_ExchangeGamePluginEntryPoints(pluginid_t pluginId)
{
    if(pluginId != 0)
    {
        // Do the API transfer.
        GETGAMEAPI fptAdr;
        if(!(fptAdr = (GETGAMEAPI) DD_FindEntryPoint(pluginId, "GetGameAPI")))
            return false;
        app.GetGameAPI = fptAdr;
        DD_InitAPI();
        Def_GetGameClasses();
    }
    else
    {
        app.GetGameAPI = 0;
        DD_InitAPI();
        Def_GetGameClasses();
    }
    return true;
}

static void loadResource(ResourceManifest &manifest)
{
    DENG_ASSERT(manifest.resourceClass() == RC_PACKAGE);

    de::Uri path(manifest.resolvedPath(false/*do not locate resource*/), RC_NULL);
    if(path.isEmpty()) return;

    if(de::File1* file = tryLoadFile(path, 0/*base offset*/))
    {
        // Mark this as an original game resource.
        file->setCustom(false);

        // Print the 'CRC' number of IWADs, so they can be identified.
        if(Wad *wad = dynamic_cast<Wad*>(file))
        {
            Con_Message("  IWAD identification: %08x", wad->calculateCRC());
        }
    }
}

typedef struct {
    /// @c true iff caller (i.e., App_ChangeGame) initiated busy mode.
    boolean initiatedBusyMode;
} ddgamechange_paramaters_t;

static int DD_BeginGameChangeWorker(void* parameters)
{
    ddgamechange_paramaters_t* p = (ddgamechange_paramaters_t*)parameters;
    DENG_ASSERT(p);

    Map::initDummies();
    P_InitMapEntityDefs();

    if(p->initiatedBusyMode)
        Con_SetProgress(100);

    if(p->initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }
    return 0;
}

static int DD_LoadGameStartupResourcesWorker(void* parameters)
{
    ddgamechange_paramaters_t* p = (ddgamechange_paramaters_t*)parameters;
    DENG_ASSERT(p);

    // Reset file Ids so previously seen files can be processed again.
    App_FileSystem().resetFileIds();
    initPathMappings();
    App_FileSystem().resetAllSchemes();

    if(p->initiatedBusyMode)
        Con_SetProgress(50);

    if(App_GameLoaded())
    {
        // Create default Auto mappings in the runtime directory.

        // Data class resources.
        App_FileSystem().addPathMapping("auto/", de::Uri("$(App.DataPath)/$(GamePlugin.Name)/auto/", RC_NULL).resolved());

        // Definition class resources.
        App_FileSystem().addPathMapping("auto/", de::Uri("$(App.DefsPath)/$(GamePlugin.Name)/auto/", RC_NULL).resolved());
    }

    /**
     * Open all the files, load headers, count lumps, etc, etc...
     * @note  Duplicate processing of the same file is automatically guarded
     *        against by the virtual file system layer.
     */
    Con_Message("Loading game resources%s", verbose >= 1? ":" : "...");

    Game::Manifests const& gameManifests = App_CurrentGame().manifests();
    int const numPackages = gameManifests.count(RC_PACKAGE);
    int packageIdx = 0;
    for(Game::Manifests::const_iterator i = gameManifests.find(RC_PACKAGE);
        i != gameManifests.end() && i.key() == RC_PACKAGE; ++i, ++packageIdx)
    {
        loadResource(**i);

        // Update our progress.
        if(p->initiatedBusyMode)
        {
            Con_SetProgress((packageIdx + 1) * (200 - 50) / numPackages - 1);
        }
    }

    if(p->initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }
    return 0;
}

static int addListFiles(ddstring_t*** list, size_t* listSize, FileType const& ftype)
{
    size_t i;
    int count = 0;
    if(!list || !listSize) return 0;
    for(i = 0; i < *listSize; ++i)
    {
        if(&ftype != &DD_GuessFileTypeFromFileName(Str_Text((*list)[i]))) continue;
        if(tryLoadFile(de::Uri(Str_Text((*list)[i]), RC_NULL)))
        {
            count += 1;
        }
    }
    return count;
}

/**
 * (Re-)Initialize the VFS path mappings.
 */
static void initPathMappings()
{
    App_FileSystem().clearPathMappings();

    if(DD_IsShuttingDown()) return;

    // Create virtual directory mappings by processing all -vdmap options.
    int argC = CommandLine_Count();
    for(int i = 0; i < argC; ++i)
    {
        if(strnicmp("-vdmap", CommandLine_At(i), 6)) continue;

        if(i < argC - 1 && !CommandLine_IsOption(i + 1) && !CommandLine_IsOption(i + 2))
        {
            String source      = NativePath(CommandLine_PathAt(i + 1)).expand().withSeparators('/');
            String destination = NativePath(CommandLine_PathAt(i + 2)).expand().withSeparators('/');
            App_FileSystem().addPathMapping(source, destination);
            i += 2;
        }
    }
}

/// Skip all whitespace except newlines.
static inline char const* skipSpace(char const* ptr)
{
    DENG_ASSERT(ptr);
    while(*ptr && *ptr != '\n' && isspace(*ptr))
    { ptr++; }
    return ptr;
}

static bool parsePathLumpMapping(lumpname_t lumpName, ddstring_t* path, char const* buffer)
{
    DENG_ASSERT(lumpName && path);

    // Find the start of the lump name.
    char const* ptr = skipSpace(buffer);

    // Just whitespace?
    if(!*ptr || *ptr == '\n') return false;

    // Find the end of the lump name.
    char const* end = (char const*)M_FindWhite((char*)ptr);
    if(!*end || *end == '\n') return false;

    size_t len = end - ptr;
    // Invalid lump name?
    if(len > 8) return false;

    memset(lumpName, 0, LUMPNAME_T_MAXLEN);
    strncpy(lumpName, ptr, len);
    strupr(lumpName);

    // Find the start of the file path.
    ptr = skipSpace(end);
    if(!*ptr || *ptr == '\n') return false; // Missing file path.

    // We're at the file path.
    Str_Set(path, ptr);
    // Get rid of any extra whitespace on the end.
    Str_StripRight(path);
    F_FixSlashes(path, path);
    return true;
}

/**
 * <pre> LUMPNAM0 \\Path\\In\\The\\Base.ext
 * LUMPNAM1 Path\\In\\The\\RuntimeDir.ext
 *  :</pre>
 */
static bool parsePathLumpMappings(char const* buffer)
{
    DENG_ASSERT(buffer);

    bool successful = false;
    ddstring_t path; Str_Init(&path);
    ddstring_t line; Str_Init(&line);

    char const* ch = buffer;
    lumpname_t lumpName;
    do
    {
        ch = Str_GetLine(&line, ch);
        if(!parsePathLumpMapping(lumpName, &path, Str_Text(&line)))
        {
            // Failure parsing the mapping.
            // Ignore errors in individual mappings and continue parsing.
            //goto parseEnded;
        }
        else
        {
            String destination = NativePath(Str_Text(&path)).expand().withSeparators('/');
            App_FileSystem().addPathLumpMapping(lumpName, destination);
        }
    } while(*ch);

    // Success.
    successful = true;

//parseEnded:
    Str_Free(&line);
    Str_Free(&path);
    return successful;
}

/**
 * (Re-)Initialize the path => lump mappings.
 * @note Should be called after WADs have been processed.
 */
static void initPathLumpMappings()
{
    // Free old paths, if any.
    App_FileSystem().clearPathLumpMappings();

    if(DD_IsShuttingDown()) return;

    size_t bufSize = 0;
    uint8_t* buf = NULL;

    // Add the contents of all DD_DIREC lumps.
    DENG2_FOR_EACH_CONST(LumpIndex::Lumps, i, App_FileSystem().nameIndex().lumps())
    {
        de::File1& lump = **i;
        FileInfo const& lumpInfo = lump.info();

        if(!lump.name().beginsWith("DD_DIREC", Qt::CaseInsensitive)) continue;

        // Make a copy of it so we can ensure it ends in a null.
        if(bufSize < lumpInfo.size + 1)
        {
            bufSize = lumpInfo.size + 1;
            buf = (uint8_t*) M_Realloc(buf, bufSize);
            if(!buf) Con_Error("initPathLumpMappings: Failed on (re)allocation of %lu bytes for temporary read buffer.", (unsigned long) bufSize);
        }

        lump.read(buf, 0, lumpInfo.size);
        buf[lumpInfo.size] = 0;
        parsePathLumpMappings(reinterpret_cast<char const*>(buf));
    }

    if(buf) M_Free(buf);
}

static int DD_LoadAddonResourcesWorker(void* parameters)
{
    ddgamechange_paramaters_t* p = (ddgamechange_paramaters_t*)parameters;
    DENG_ASSERT(p);

    /**
     * Add additional game-startup files.
     * @note These must take precedence over Auto but not game-resource files.
     */
    if(startupFiles && startupFiles[0])
        parseStartupFilePathsAndAddFiles(startupFiles);

    if(p->initiatedBusyMode)
        Con_SetProgress(50);

    if(App_GameLoaded())
    {
        /**
         * Phase 3: Add real files from the Auto directory.
         * First ZIPs then WADs (they may contain WAD files).
         */
        listFilesFromDataGameAuto(&sessionResourceFileList, &numSessionResourceFileList);
        if(numSessionResourceFileList > 0)
        {
            addListFiles(&sessionResourceFileList, &numSessionResourceFileList, DD_FileTypeByName("FT_ZIP"));

            addListFiles(&sessionResourceFileList, &numSessionResourceFileList, DD_FileTypeByName("FT_WAD"));
        }

        // Final autoload round.
        DD_AutoLoad();
    }

    if(p->initiatedBusyMode)
        Con_SetProgress(180);

    initPathLumpMappings();

    // Re-initialize the resource locator as there are now new resources to be found
    // on existing search paths (probably that is).
    App_FileSystem().resetAllSchemes();

    if(p->initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }
    return 0;
}

static int DD_ActivateGameWorker(void *parameters)
{
    ddgamechange_paramaters_t *p = (ddgamechange_paramaters_t*)parameters;
    uint i;
    DENG_ASSERT(p);

    // Texture resources are located now, prior to initializing the game.
    App_ResourceSystem().initCompositeTextures();
    App_ResourceSystem().initFlatTextures();
    App_ResourceSystem().initSpriteTextures();
    App_ResourceSystem().textureScheme("Lightmaps").clear();
    App_ResourceSystem().textureScheme("Flaremaps").clear();

    if(p->initiatedBusyMode)
        Con_SetProgress(50);

    // Now that resources have been located we can begin to initialize the game.
    if(App_GameLoaded() && gx.PreInit)
    {
        DENG_ASSERT(App_CurrentGame().pluginId() != 0);

        DD_SetActivePluginId(App_CurrentGame().pluginId());
        gx.PreInit(App_Games().id(App_CurrentGame()));
        DD_SetActivePluginId(0);
    }

    if(p->initiatedBusyMode)
        Con_SetProgress(100);

    /**
     * Parse the game's main config file.
     * If a custom top-level config is specified; let it override.
     */
    { ddstring_t const* configFileName = 0;
    ddstring_t tmp;
    if(CommandLine_CheckWith("-config", 1))
    {
        Str_Init(&tmp); Str_Set(&tmp, CommandLine_Next());
        F_FixSlashes(&tmp, &tmp);
        configFileName = &tmp;
    }
    else
    {
        configFileName = App_CurrentGame().mainConfig();
    }

    Con_Message("Parsing primary config \"%s\"...", F_PrettyPath(Str_Text(configFileName)));
    Con_ParseCommands2(Str_Text(configFileName), CPCF_SET_DEFAULT | CPCF_ALLOW_SAVE_STATE);
    if(configFileName == &tmp)
        Str_Free(&tmp);
    }

#ifdef __CLIENT__
    if(App_GameLoaded())
    {
        // Apply default control bindings for this game.
        B_BindGameDefaults();

        // Read bindings for this game and merge with the working set.
        Con_ParseCommands2(Str_Text(App_CurrentGame().bindingConfig()), CPCF_ALLOW_SAVE_BINDINGS);
    }
#endif

    if(p->initiatedBusyMode)
        Con_SetProgress(120);

    Def_Read();

    if(p->initiatedBusyMode)
        Con_SetProgress(130);

    App_ResourceSystem().initSprites(); // Fully initialize sprites.
#ifdef __CLIENT__
    App_ResourceSystem().initModels();
#endif

    Def_PostInit();

    DD_ReadGameHelp();

    // Re-init to update the title, background etc.
    //Rend_ConsoleInit();

    // Reset the tictimer so than any fractional accumulation is not added to
    // the tic/game timer of the newly-loaded game.
    gameTime = 0;
    DD_ResetTimer();

    // Make sure that the next frame does not use a filtered viewer.
    R_ResetViewer();

    // Invalidate old cmds and init player values.
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t* plr = &ddPlayers[i];

        plr->extraLight = plr->targetExtraLight = 0;
        plr->extraLightCounter = 0;
    }

    if(gx.PostInit)
    {
        DD_SetActivePluginId(App_CurrentGame().pluginId());
        gx.PostInit();
        DD_SetActivePluginId(0);
    }

    if(p->initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }

    return 0;
}

de::Games &App_Games()
{
#ifdef __CLIENT__
    if(ClientApp::haveApp())
    {
        return ClientApp::games();
    }
#endif

#ifdef __SERVER__
    if(ServerApp::haveApp())
    {
        return ServerApp::games();
    }
#endif
    throw Error("App_Games", "App not yet initialized");
}

boolean App_GameLoaded()
{
#ifdef __CLIENT__
    if(!ClientApp::haveApp()) return false;
#endif
#ifdef __SERVER__
    if(!ServerApp::haveApp()) return false;
#endif
    return !App_CurrentGame().isNull();
}

void DD_DestroyGames()
{
    destroyPathList(&sessionResourceFileList, &numSessionResourceFileList);
    App_Games().clear();
    App::app().setGame(App_Games().nullGame());
}

static void populateGameInfo(GameInfo& info, de::Game& game)
{
    info.identityKey = Str_Text(game.identityKey());
    info.title       = Str_Text(game.title());
    info.author      = Str_Text(game.author());
}

/// @note Part of the Doomsday public API.
#undef DD_GameInfo
boolean DD_GameInfo(GameInfo *info)
{
    LOG_AS("DD_GameInfo");
    if(!info)
    {
        LOG_WARNING("Received invalid info (=NULL), ignoring.");
        return false;
    }

    zapPtr(info);

    if(App_GameLoaded())
    {
        populateGameInfo(*info, App_CurrentGame());
        return true;
    }

    LOG_WARNING("No game currently loaded, returning false.");
    return false;
}

#undef DD_AddGameResource
void DD_AddGameResource(gameid_t gameId, resourceclassid_t classId, int rflags,
    char const *names, void *params)
{
    if(!VALID_RESOURCECLASSID(classId))
        Con_Error("DD_AddGameResource: Unknown resource class %i.", (int)classId);

    if(!names || !names[0])
        Con_Error("DD_AddGameResource: Invalid name argument.");

    // Construct and attach the new resource record.
    Game &game = App_Games().byId(gameId);
    ResourceManifest *manifest = new ResourceManifest(classId, rflags);
    game.addManifest(*manifest);

    // Add the name list to the resource record.
    QStringList nameList = String(names).split(";", QString::SkipEmptyParts);
    foreach(QString const &nameRef, nameList)
    {
        manifest->addName(nameRef);
    }

    if(params && classId == RC_PACKAGE)
    {
        // Add the identityKey list to the resource record.
        QStringList idKeys = String((char const *) params).split(";", QString::SkipEmptyParts);
        foreach(QString const &idKeyRef, idKeys)
        {
            manifest->addIdentityKey(idKeyRef);
        }
    }
}

#undef DD_DefineGame
gameid_t DD_DefineGame(GameDef const *def)
{
    LOG_AS("DD_DefineGame");
    if(!def)
    {
        LOG_WARNING("Received invalid GameDef (=NULL), ignoring.");
        return 0; // Invalid id.
    }

    // Game mode identity keys must be unique. Ensure that is the case.
    try
    {
        /*Game &game =*/ App_Games().byIdentityKey(def->identityKey);
        LOG_WARNING("Failed adding game \"%s\", identity key '%s' already in use, ignoring.")
                << def->defaultTitle << def->identityKey;
        return 0; // Invalid id.
    }
    catch(Games::NotFoundError const &)
    {} // Ignore the error.

    Game *game = Game::fromDef(*def);
    if(!game) return 0; // Invalid def.

    // Add this game to our records.
    game->setPluginId(DD_ActivePluginId());
    App_Games().add(*game);
    return App_Games().id(*game);
}

#undef DD_GameIdForKey
gameid_t DD_GameIdForKey(char const *identityKey)
{
    try
    {
        return App_Games().id(App_Games().byIdentityKey(identityKey));
    }
    catch(Games::NotFoundError const &)
    {
        LOG_AS("DD_GameIdForKey");
        LOG_WARNING("Game \"%s\" is not defined, returning 0.") << identityKey;
    }
    return 0; // Invalid id.
}

de::Game &App_CurrentGame()
{
    return App::game().as<de::Game>();
}

bool App_ChangeGame(Game &game, bool allowReload)
{
    //LOG_AS("App_ChangeGame");

    bool isReload = false;

    // Ignore attempts to re-load the current game?
    if(&App_CurrentGame() == &game)
    {
        if(!allowReload)
        {
            if(App_GameLoaded())
            {
                LOG_MSG("%s (%s) - already loaded.")
                        << Str_Text(game.title()) << Str_Text(game.identityKey());
            }
            return true;
        }
        // We are re-loading.
        isReload = true;
    }

    // The current game will be gone very soon.
    DENG2_FOR_EACH_OBSERVER(App::GameUnloadAudience, i, App::app().audienceForGameUnload)
    {
        i->aboutToUnloadGame(App::game());
    }

    // Quit netGame if one is in progress.
#ifdef __SERVER__
    if(netGame && isServer)
    {
        N_ServerClose();
    }
#else
    if(netGame)
    {
        Con_Execute(CMDS_DDAY, "net disconnect", true, false);
    }
#endif

    S_Reset();

#ifdef __CLIENT__
    Demo_StopPlayback();

    GL_PurgeDeferredTasks();
    App_ResourceSystem().releaseAllGLTextures();
    App_ResourceSystem().pruneUnusedTextureSpecs();
    GL_LoadLightingSystemTextures();
    GL_LoadFlareTextures();
    Rend_ParticleLoadSystemTextures();
    GL_SetFilter(false);

    if(!game.isNull())
    {
        ClientWindow &mainWin = ClientWindow::main();
        mainWin.taskBar().close();

        // Trap the mouse automatically when loading a game in fullscreen.
        if(mainWin.isFullScreen())
        {
            mainWin.canvas().trapMouse();
        }
    }
#endif

    // If a game is presently loaded; unload it.
    if(App_GameLoaded())
    {
        if(gx.Shutdown)
            gx.Shutdown();
        Con_SaveDefaults();

#ifdef __CLIENT__
        R_ClearViewData();
        R_DestroyContactLists();
        P_ControlShutdown();

        Con_Execute(CMDS_DDAY, "clearbindings", true, false);
        B_BindDefaults();
        B_InitialContextActivations();
#endif
        // Reset the world back to it's initial state (unload the map, reset players, etc...).
        App_World().reset();

        Z_FreeTags(PU_GAMESTATIC, PU_PURGELEVEL - 1);

        P_ShutdownMapEntityDefs();

        R_ShutdownSvgs();

        App_ResourceSystem().clearAllRuntimeResources();
        App_ResourceSystem().clearAllAnimGroups();
        App_ResourceSystem().clearAllColorPalettes();

        Sfx_InitLogical();

        Con_ClearDatabases();

        { // Tell the plugin it is being unloaded.
            void *unloader = DD_FindEntryPoint(App_CurrentGame().pluginId(), "DP_Unload");
            LOG_DEBUG("Calling DP_Unload (%p)") << de::dintptr(unloader);
            DD_SetActivePluginId(App_CurrentGame().pluginId());
            if(unloader) ((pluginfunc_t)unloader)();
            DD_SetActivePluginId(0);
        }

        // The current game is now the special "null-game".
        App::app().setGame(App_Games().nullGame());

        Con_InitDatabases();
        DD_Register();

#ifdef __CLIENT__
        I_InitVirtualInputDevices();
#endif

        R_InitSvgs();

#ifdef __CLIENT__
        R_InitViewWindow();
#endif

        App_FileSystem().unloadAllNonStartupFiles();

        // Reset file IDs so previously seen files can be processed again.
        /// @todo this releases the IDs of startup files too but given the
        /// only startup file is doomsday.pk3 which we never attempt to load
        /// again post engine startup, this isn't an immediate problem.
        App_FileSystem().resetFileIds();

        // Update the dir/WAD translations.
        initPathLumpMappings();
        initPathMappings();

        App_FileSystem().resetAllSchemes();
    }

    FI_Shutdown();
    titleFinale = 0; // If the title finale was in progress it isn't now.

    if(!game.isNull())
    {
        LOG_VERBOSE("Selecting game '%s'...") << Str_Text(game.identityKey());
    }
    else if(!isReload)
    {
        LOG_VERBOSE("Unloaded game.");
    }

    Library_ReleaseGames();

#ifdef __CLIENT__
    char buf[256];
    DD_ComposeMainWindowTitle(buf);
    ClientWindow::main().setWindowTitle(buf);
#endif

    if(!DD_IsShuttingDown())
    {
        // Re-initialize subsystems needed even when in ringzero.
        if(!DD_ExchangeGamePluginEntryPoints(game.pluginId()))
        {
            LOG_WARNING("Failed exchanging entrypoints with plugin %i, aborting...") << int(game.pluginId());
            return false;
        }

        FI_Init();
    }

    // This is now the current game.
    App::app().setGame(game);

#ifdef __CLIENT__
    DD_ComposeMainWindowTitle(buf);
    ClientWindow::main().setWindowTitle(buf);
#endif

    /**
     * If we aren't shutting down then we are either loading a game or switching
     * to ringzero (the current game will have already been unloaded).
     */
    if(!DD_IsShuttingDown())
    {
        /**
         * The bulk of this we can do in busy mode unless we are already busy
         * (which can happen if a fatal error occurs during game load and we must
         * shutdown immediately; Sys_Shutdown will call back to load the special
         * "null-game" game).
         */
        int const busyMode = BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0);
        ddgamechange_paramaters_t p;
        BusyTask gameChangeTasks[] = {
            // Phase 1: Initialization.
            { DD_BeginGameChangeWorker,          &p, busyMode, "Loading game...",   200, 0.0f, 0.1f, 0 },

            // Phase 2: Loading "startup" resources.
            { DD_LoadGameStartupResourcesWorker, &p, busyMode, NULL,                200, 0.1f, 0.3f, 0 },

            // Phase 3: Loading "addon" resources.
            { DD_LoadAddonResourcesWorker,       &p, busyMode, "Loading add-ons...", 200, 0.3f, 0.7f, 0 },

            // Phase 4: Game activation.
            { DD_ActivateGameWorker,             &p, busyMode, "Starting game...",  200, 0.7f, 1.0f, 0 }
        };

        p.initiatedBusyMode = !BusyMode_Active();

        if(App_GameLoaded())
        {
            // Tell the plugin it is being loaded.
            /// @todo Must this be done in the main thread?
            void *loader = DD_FindEntryPoint(App_CurrentGame().pluginId(), "DP_Load");
            LOG_DEBUG("Calling DP_Load (%p)") << de::dintptr(loader);
            DD_SetActivePluginId(App_CurrentGame().pluginId());
            if(loader) ((pluginfunc_t)loader)();
            DD_SetActivePluginId(0);
        }

        /// @todo Kludge: Use more appropriate task names when unloading a game.
        if(game.isNull())
        {
            gameChangeTasks[0].name = "Unloading game...";
            gameChangeTasks[3].name = "Switching to ringzero...";
        }
        // kludge end

        BusyMode_RunTasks(gameChangeTasks, sizeof(gameChangeTasks)/sizeof(gameChangeTasks[0]));

#ifdef __CLIENT__
        // Process any GL-related tasks we couldn't while Busy.
        Rend_ParticleLoadExtraTextures();
#endif

        if(App_GameLoaded())
        {
            Game::printBanner(App_CurrentGame());
        }
        else
        {
            // Lets play a nice title animation.
            DD_StartTitle();
        }
    }

    DENG_ASSERT(DD_ActivePluginId() == 0);

#ifdef __CLIENT__
    /**
     * Clear any input events we may have accumulated during this process.
     * @note Only necessary here because we might not have been able to use
     *       busy mode (which would normally do this for us on end).
     */
    DD_ClearEvents();

    if(!App_GameLoaded())
    {
        ClientWindow::main().taskBar().open();
    }
    else
    {
        ClientWindow::main().console().clearLog();
    }
#endif

    // Game change is complete.
    DENG2_FOR_EACH_OBSERVER(App::GameChangeAudience, i, App::app().audienceForGameChange)
    {
        i->currentGameChanged(App::game());
    }

    return true;
}

World &App_World()
{
#ifdef __CLIENT__
    return ClientApp::world();
#endif

#ifdef __SERVER__
    return ServerApp::world();
#endif
}

boolean DD_IsShuttingDown()
{
    return Sys_IsShuttingDown();
}

/**
 * Looks for new files to autoload from the auto-load data directory.
 */
static void DD_AutoLoad()
{
    /**
     * Keep loading files if any are found because virtual files may now
     * exist in the auto-load directory.
     */
    int numNewFiles;
    while((numNewFiles = loadFilesFromDataGameAuto()) > 0)
    {
        LOG_VERBOSE("Autoload round completed with %i new files.") << numNewFiles;
    }
}

/**
 * Attempt to determine which game is to be played.
 *
 * @todo Logic here could be much more elaborate but is it necessary?
 */
Game *DD_AutoselectGame()
{
    if(CommandLine_CheckWith("-game", 1))
    {
        char const *identityKey = CommandLine_Next();
        try
        {
            Game &game = App_Games().byIdentityKey(identityKey);
            if(game.allStartupFilesFound())
            {
                return &game;
            }
        }
        catch(Games::NotFoundError const &)
        {} // Ignore the error.
    }

    // If but one lonely game; select it.
    if(App_Games().numPlayable() == 1)
    {
        return App_Games().firstPlayable();
    }

    // We don't know what to do.
    return 0;
}

int DD_EarlyInit()
{
    // Determine the requested degree of verbosity.
    verbose = CommandLine_Exists("-verbose");

#ifdef __SERVER__
    isDedicated = true;
#else
    isDedicated = false;
#endif

    // Bring the console online as soon as we can.
    DD_ConsoleInit();

    Con_InitDatabases();

    // Register the engine's console commands and variables.
    DD_Register();

    return true;
}

/**
 * This gets called when the main window is ready for GL init. The application
 * event loop is already running.
 */
void DD_FinishInitializationAfterWindowReady()
{
    LOG_DEBUG("Window is ready, finishing initialization");

#ifdef __CLIENT__
# ifdef WIN32
    // Now we can get the color transfer table as the window is available.
    DisplayMode_SaveOriginalColorTransfer();
# endif
    if(!Sys_GLInitialize())
    {
        Con_Error("Error initializing OpenGL.\n");
    }
    else
    {
        char buf[256];
        DD_ComposeMainWindowTitle(buf);
        ClientWindow::main().setWindowTitle(buf);
    }
#endif

    // Initialize engine subsystems and initial state.
    if(!DD_Init())
    {
        exit(2); // Cannot continue...
        return;
    }

    /// @todo This notification should be done from the app.
    DENG2_FOR_EACH_OBSERVER(App::StartupCompleteAudience, i, App::app().audienceForStartupComplete)
    {
        i->appStartupCompleted();
    }
}

/**
 * Engine initialization. After completed, the game loop is ready to be started.
 * Called from the app entrypoint function.
 *
 * @return  @c true on success, @c false if an error occurred.
 */
boolean DD_Init(void)
{
#ifdef _DEBUG
    // Type size check.
    {
        void *ptr = 0;
        int32_t int32 = 0;
        int16_t int16 = 0;
        float float32 = 0;

        DENG_UNUSED(ptr);
        DENG_UNUSED(int32);
        DENG_UNUSED(int16);
        DENG_UNUSED(float32);

        ASSERT_32BIT(int32);
        ASSERT_16BIT(int16);
        ASSERT_32BIT(float32);
#ifdef __64BIT__
        ASSERT_64BIT(ptr);
        ASSERT_64BIT(int64_t);
#else
        ASSERT_NOT_64BIT(ptr);
#endif
    }
#endif

#ifdef __CLIENT__
    if(!GL_EarlyInit())
    {
        Sys_CriticalMessage("GL_EarlyInit() failed.");
        return false;
    }
#endif

    // Initialize the subsystems needed prior to entering busy mode for the first time.
    Sys_Init();
    DD_CreateFileTypes();
    F_Init();
    DD_CreateFileSystemSchemes();

    FR_Init();

#ifdef __CLIENT__
    // Enter busy mode until startup complete.
    Con_InitProgress2(200, 0, .25f); // First half.
#endif
    BusyMode_RunNewTaskWithName(BUSYF_NO_UPLOADS | BUSYF_STARTUP | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                DD_StartupWorker, 0, "Starting up...");

    // Engine initialization is complete. Now finish up with the GL.
#ifdef __CLIENT__
    GL_Init();
    GL_InitRefresh();
#endif

#ifdef __CLIENT__
    // Do deferred uploads.
    Con_InitProgress2(200, .25f, .25f); // Stop here for a while.
#endif
    BusyMode_RunNewTaskWithName(BUSYF_STARTUP | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                DD_DummyWorker, 0, "Buffering...");

    // Add resource paths specified using -iwad on the command line.
    FS1::Scheme &scheme = App_FileSystem().scheme(App_ResourceClass("RC_PACKAGE").defaultScheme());
    for(int p = 0; p < CommandLine_Count(); ++p)
    {
        if(!CommandLine_IsMatchingAlias("-iwad", CommandLine_At(p)))
        {
            continue;
        }

        while(++p != CommandLine_Count() && !CommandLine_IsOption(p))
        {
            /// @todo Do not add these as search paths, publish them directly to
            ///       the "Packages" scheme.

            // CommandLine_PathAt() always returns an absolute path.
            directory_t *dir = Dir_FromText(CommandLine_PathAt(p));
            de::Uri uri = de::Uri::fromNativeDirPath(Dir_Path(dir), RC_PACKAGE);

            LOG_DEBUG("User supplied IWAD path: \"%s\"") << Dir_Path(dir);

            scheme.addSearchPath(SearchPath(uri, SearchPath::NoDescend));

            Dir_Delete(dir);
        }

        p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
    }

    // Try to locate all required data files for all registered games.
#ifdef __CLIENT__
    Con_InitProgress2(200, .25f, 1); // Second half.
#endif
    App_Games().locateAllResources();

    // Attempt automatic game selection.
    if(!CommandLine_Exists("-noautoselect") || isDedicated)
    {
        if(de::Game *game = DD_AutoselectGame())
        {
            // An implicit game session has been defined.
            // Add all resources specified using -file options on the command line
            // to the list for this session.
            for(int p = 0; p < CommandLine_Count(); ++p)
            {
                if(!CommandLine_IsMatchingAlias("-file", CommandLine_At(p)))
                {
                    continue;
                }

                while(++p != CommandLine_Count() && !CommandLine_IsOption(p))
                {
                    addToPathList(&sessionResourceFileList, &numSessionResourceFileList, CommandLine_PathAt(p));
                }

                p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
            }

            // Begin the game session.
            App_ChangeGame(*game);

            // We do not want to load these resources again on next game change.
            destroyPathList(&sessionResourceFileList, &numSessionResourceFileList);
        }
#ifdef __SERVER__
        else
        {
            // A server is presently useless without a game, as shell
            // connections can only be made after a game is loaded and the
            // server mode started.
            /// @todo Allow shell connections in ringzero mode, too.
            Con_Error("No playable games available.");
        }
#endif
    }

    initPathLumpMappings();

    // Re-initialize the filesystem subspace schemess as there are now new
    // resources to be found on existing search paths (probably that is).
    App_FileSystem().resetAllSchemes();

    // One-time execution of various command line features available during startup.
    if(CommandLine_CheckWith("-dumplump", 1))
    {
        String name = CommandLine_Next();
        lumpnum_t lumpNum = App_FileSystem().lumpNumForName(name);
        if(lumpNum >= 0)
        {
            F_DumpLump(lumpNum);
        }
        else
        {
            LOG_WARNING("Cannot dump unknown lump \"%s\", ignoring.") << name;
        }
    }

    if(CommandLine_Check("-dumpwaddir"))
    {
        Con_Executef(CMDS_CMDLINE, false, "listlumps");
    }

    // Try to load the autoexec file. This is done here to make sure everything is
    // initialized: the user can do here anything that s/he'd be able to do in-game
    // provided a game was loaded during startup.
    Con_ParseCommands("autoexec.cfg");

    // Read additional config files that should be processed post engine init.
    if(CommandLine_CheckWith("-parse", 1))
    {
        LOG_MSG("Parsing additional (pre-init) config files:");
        Time begunAt;
        forever
        {
            char const *arg = CommandLine_Next();
            if(!arg || arg[0] == '-')
            {
                break;
            }

            LOG_MSG("  Processing \"%s\"...") << F_PrettyPath(arg);
            Con_ParseCommands(arg);
        }
        LOG_INFO(String("  Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
    }

    // A console command on the command line?
    for(int p = 1; p < CommandLine_Count() - 1; p++)
    {
        if(stricmp(CommandLine_At(p), "-command") &&
           stricmp(CommandLine_At(p), "-cmd"))
        {
            continue;
        }

        for(++p; p < CommandLine_Count(); p++)
        {
            char const *arg = CommandLine_At(p);

            if(arg[0] == '-')
            {
                p--;
                break;
            }
            Con_Execute(CMDS_CMDLINE, arg, false, false);
        }
    }

    /*
     * One-time execution of network commands on the command line.
     * Commands are only executed if we have loaded a game during startup.
     */
    if(App_GameLoaded())
    {
        // Client connection command.
        if(CommandLine_CheckWith("-connect", 1))
        {
            Con_Executef(CMDS_CMDLINE, false, "connect %s", CommandLine_Next());
        }

        // Incoming TCP port.
        if(CommandLine_CheckWith("-port", 1))
        {
            Con_Executef(CMDS_CMDLINE, false, "net-ip-port %s", CommandLine_Next());
        }

#ifdef __SERVER__
        // Automatically start the server.
        N_ServerOpen();
#endif
    }
    else
    {
        // No game loaded.
        // Lets get most of everything else initialized.
        // Reset file IDs so previously seen files can be processed again.
        App_FileSystem().resetFileIds();
        initPathLumpMappings();
        initPathMappings();
        App_FileSystem().resetAllSchemes();

        App_ResourceSystem().initCompositeTextures();
        App_ResourceSystem().initFlatTextures();
        App_ResourceSystem().initSpriteTextures();
        App_ResourceSystem().textureScheme("Lightmaps").clear();
        App_ResourceSystem().textureScheme("Flaremaps").clear();

        Def_Read();

        App_ResourceSystem().initSprites();
#ifdef __CLIENT__
        App_ResourceSystem().initModels();
#endif

        Def_PostInit();

        // Lets play a nice title animation.
        DD_StartTitle();

        // We'll open the console and print a list of the known games too.
        //Con_Execute(CMDS_DDAY, "conopen", true, false);
        if(!CommandLine_Exists("-noautoselect"))
        {
            LOG_INFO("Automatic game selection failed");
        }
        /// @todo Whether or not to list the games depends on whether the app
        /// has a graphical interface. The graphical client will display the
        /// GameSelection widget where as the server will not.
        //Con_Execute(CMDS_DDAY, "listgames", false, false);
    }

    return true;
}

static int DD_StartupWorker(void * /*context*/)
{
#ifdef WIN32
    // Initialize COM for this thread (needed for DirectInput).
    CoInitialize(NULL);
#endif
    Con_SetProgress(10);

    // Any startup hooks?
    DD_CallHooks(HOOK_STARTUP, 0, 0);
    Con_SetProgress(20);

    // Was the change to userdir OK?
    if(CommandLine_CheckWith("-userdir", 1) && !app.usingUserDir)
    {
        LOG_WARNING("User directory not found (check -userdir).");
    }

    initPathMappings();
    App_FileSystem().resetAllSchemes();

    // Initialize the definition databases.
    Def_Init();

    Con_SetProgress(40);

    Net_Init();
    // Now we can hide the mouse cursor for good.
    Sys_HideMouse();

    // Read config files that should be read BEFORE engine init.
    if(CommandLine_CheckWith("-cparse", 1))
    {
        Time begunAt;

        LOG_MSG("Parsing additional (pre-init) config files:");
        for(;;)
        {
            char const *arg = CommandLine_NextAsPath();
            if(!arg || arg[0] == '-') break;

            LOG_MSG("  Processing \"%s\"...") << F_PrettyPath(arg);
            Con_ParseCommands(arg);
        }
        LOG_INFO(String("  Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
    }

    /*
     * Add required engine resource files.
     */
    String foundPath = App_FileSystem().findPath(de::Uri("doomsday.pk3", RC_PACKAGE),
                                                 RLF_DEFAULT, App_ResourceClass(RC_PACKAGE));
    foundPath = App_BasePath() / foundPath; // Ensure the path is absolute.
    de::File1 *loadedFile = tryLoadFile(de::Uri(foundPath, RC_NULL));
    DENG2_ASSERT(loadedFile != 0);
    DENG2_UNUSED(loadedFile);

    /*
     * No more lumps/packages will be loaded in startup mode after this point.
     */
    F_EndStartup();

    // Load engine help resources.
    DD_InitHelp();
    Con_SetProgress(60);

    // Execute the startup script (Startup.cfg).
    Con_ParseCommands("startup.cfg");
    Con_SetProgress(90);

    R_BuildTexGammaLut();
    R_Init();
    Con_SetProgress(165);

    Net_InitGame();
#ifdef __CLIENT__
    Demo_Init();
#endif

    LOG_MSG("Initializing InFine subsystem...");
    FI_Init();

    LOG_MSG("Initializing UI subsystem...");
    UI_Init();
    Con_SetProgress(190);

    // In dedicated mode the console must be opened, so all input events
    // will be handled by it.
    if(isDedicated)
    {
        Con_Open(true);
    }
    Con_SetProgress(199);

    DD_CallHooks(HOOK_INIT, 0, 0); // Any initialization hooks?
    Con_SetProgress(200);

#ifdef WIN32
    // This thread has finished using COM.
    CoUninitialize();
#endif

    BusyMode_WorkerEnd();
    return 0;
}

/**
 * This only exists so we have something to call while the deferred uploads of the
 * startup are processed.
 */
static int DD_DummyWorker(void * /*context*/)
{
    Con_SetProgress(200);
    BusyMode_WorkerEnd();
    return 0;
}

void DD_CheckTimeDemo(void)
{
    static boolean checked = false;

    if(!checked)
    {
        checked = true;
        if(CommandLine_CheckWith("-timedemo", 1) || // Timedemo mode.
           CommandLine_CheckWith("-playdemo", 1)) // Play-once mode.
        {
            char buf[200]; sprintf(buf, "playdemo %s", CommandLine_Next());

            Con_Execute(CMDS_CMDLINE, buf, false, false);
        }
    }
}

static int DD_UpdateEngineStateWorker(void *context)
{
    DENG2_ASSERT(context != 0);
    bool const initiatedBusyMode = *static_cast<bool *>(context);

#ifdef __CLIENT__
    if(!novideo)
    {
        GL_InitRefresh();
    }
#endif

    if(initiatedBusyMode)
    {
        Con_SetProgress(50);
    }

    R_Update();

    if(initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }
    return 0;
}

void DD_UpdateEngineState()
{
    LOG_MSG("Updating engine state...");

    // Stop playing sounds and music.
    S_Reset();

#ifdef __CLIENT__
    GL_SetFilter(false);
    Demo_StopPlayback();
#endif

    //App_FileSystem().resetFileIds();

    // Update the dir/WAD translations.
    initPathLumpMappings();
    initPathMappings();
    // Re-build the filesystem subspace schemes as there may be new resources to be found.
    App_FileSystem().resetAllSchemes();

    App_ResourceSystem().initCompositeTextures();
    App_ResourceSystem().initFlatTextures();
    App_ResourceSystem().initSpriteTextures();

    if(App_GameLoaded() && gx.UpdateState)
    {
        gx.UpdateState(DD_PRE);
    }

#ifdef __CLIENT__
    boolean hadFog = usingFog;

    GL_TotalReset();
    GL_TotalRestore(); // Bring GL back online.

    // Make sure the fog is enabled, if necessary.
    if(hadFog)
    {
        GL_UseFog(true);
    }
#endif

    /*
     * The bulk of this we can do in busy mode unless we are already busy
     * (which can happen during a runtime game change).
     */
    bool initiatedBusyMode = !BusyMode_Active();
    if(initiatedBusyMode)
    {
#ifdef __CLIENT__
        Con_InitProgress(200);
#endif
        BusyMode_RunNewTaskWithName(BUSYF_ACTIVITY | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                    DD_UpdateEngineStateWorker, &initiatedBusyMode,
                                    "Updating engine state...");
    }
    else
    {
        /// @todo Update the current task name and push progress.
        DD_UpdateEngineStateWorker(&initiatedBusyMode);
    }

    if(App_GameLoaded() && gx.UpdateState)
    {
        gx.UpdateState(DD_POST);
    }

#ifdef __CLIENT__
    App_ResourceSystem().restartAllMaterialAnimations();
#endif
}

ddvalue_t ddValues[DD_LAST_VALUE - DD_FIRST_VALUE - 1] = {
    {&netGame, 0},
    {&isServer, 0},                         // An *open* server?
    {&isClient, 0},
#ifdef __SERVER__
    {&allowFrames, &allowFrames},
#else
    {0, 0},
#endif
    {&consolePlayer, &consolePlayer},
    {&displayPlayer, 0 /*&displayPlayer*/}, // use R_SetViewPortPlayer() instead
#ifdef __CLIENT__
    {&mipmapping, 0},
    {&filterUI, 0},
    {&defResX, &defResX},
    {&defResY, &defResY},
#else
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
#endif
    {0, 0},
    {0, 0}, //{&mouseInverseY, &mouseInverseY},
    {&levelFullBright, &levelFullBright},
    {&CmdReturnValue, 0},
#ifdef __CLIENT__
    {&gameReady, &gameReady},
#else
    {0, 0},
#endif
    {&isDedicated, 0},
    {&novideo, 0},
    {&defs.count.mobjs.num, 0},
    {&gotFrame, 0},
#ifdef __CLIENT__
    {&playback, 0},
#else
    {0, 0},
#endif
    {&defs.count.sounds.num, 0},
    {&defs.count.music.num, 0},
    {0, 0},
#ifdef __CLIENT__
    {&clientPaused, &clientPaused},
#else
    {0, 0},
#endif
    {&weaponOffsetScaleY, &weaponOffsetScaleY},
    {&gameDataFormat, &gameDataFormat},
#ifdef __CLIENT__
    {&gameDrawHUD, 0},
    {&symbolicEchoMode, &symbolicEchoMode},
    {&numTexUnits, 0},
    {&rendLightAttenuateFixedColormap, &rendLightAttenuateFixedColormap}
#else
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0}
#endif
};

/**
 * Get a 32-bit signed integer value.
 */
#undef DD_GetInteger
int DD_GetInteger(int ddvalue)
{
    switch(ddvalue)
    {
#ifdef __CLIENT__
    case DD_SHIFT_DOWN:
        return I_ShiftDown();

    case DD_WINDOW_WIDTH:
        return DENG_GAMEVIEW_WIDTH;

    case DD_WINDOW_HEIGHT:
        return DENG_GAMEVIEW_HEIGHT;

    case DD_CURRENT_CLIENT_FINALE_ID:
        return Cl_CurrentFinale();

    case DD_DYNLIGHT_TEXTURE:
        return (int) GL_PrepareLSTexture(LST_DYNAMIC);
#endif

    case DD_NUMLUMPS:
        return F_LumpCount();

    case DD_MAP_MUSIC: {
        if(App_World().hasMap())
        {
            de::Uri mapUri = App_World().map().uri();
            if(ded_mapinfo_t *mapInfo = Def_GetMapInfo(reinterpret_cast<uri_s *>(&mapUri)))
            {
                return Def_GetMusicNum(mapInfo->music);
            }
        }
        return -1; }

    default: break;
    }

    if(ddvalue >= DD_LAST_VALUE || ddvalue <= DD_FIRST_VALUE)
        return 0;
    if(ddValues[ddvalue].readPtr == 0)
        return 0;
    return *ddValues[ddvalue].readPtr;
}

/**
 * Set a 32-bit signed integer value.
 */
#undef DD_SetInteger
void DD_SetInteger(int ddvalue, int parm)
{
    if(ddvalue <= DD_FIRST_VALUE || ddvalue >= DD_LAST_VALUE)
        return;
    if(ddValues[ddvalue].writePtr)
        *ddValues[ddvalue].writePtr = parm;
}

/**
 * Get a pointer to the value of a variable. Not all variables support
 * this. Added for 64-bit support.
 */
#undef DD_GetVariable
void *DD_GetVariable(int ddvalue)
{
    static int value;
    static float valueF;
    static double valueD;

    DENG_UNUSED(valueF);

    switch(ddvalue)
    {
    case DD_GAME_EXPORTS:
        return &gx;

    case DD_POLYOBJ_COUNT:
        value = App_World().hasMap()? App_World().map().polyobjCount() : 0;
        return &value;

    case DD_TRANSLATIONTABLES_ADDRESS:
        return translationTables;

    case DD_MAP_NAME:
        if(App_World().hasMap())
        {
            de::Uri mapUri = App_World().map().uri();
            ded_mapinfo_t *mapInfo = Def_GetMapInfo(reinterpret_cast<uri_s *>(&mapUri));
            if(mapInfo && mapInfo->name[0])
            {
                int id = Def_Get(DD_DEF_TEXT, mapInfo->name, NULL);
                if(id != -1)
                {
                    return defs.text[id].text;
                }
                return mapInfo->name;
            }
        }
        return NULL;

    case DD_MAP_AUTHOR:
        if(App_World().hasMap())
        {
            de::Uri mapUri = App_World().map().uri();
            ded_mapinfo_t* mapInfo = Def_GetMapInfo(reinterpret_cast<uri_s *>(&mapUri));
            if(mapInfo && mapInfo->author[0])
            {
                return mapInfo->author;
            }
        }
        return NULL;

    case DD_MAP_MIN_X:
        valueD = App_World().hasMap()? App_World().map().bounds().minX : 0;
        return &valueD;

    case DD_MAP_MIN_Y:
        valueD = App_World().hasMap()? App_World().map().bounds().minY : 0;
        return &valueD;

    case DD_MAP_MAX_X:
        valueD = App_World().hasMap()? App_World().map().bounds().maxX : 0;
        return &valueD;

    case DD_MAP_MAX_Y:
        valueD = App_World().hasMap()? App_World().map().bounds().maxY : 0;
        return &valueD;

    case DD_PSPRITE_OFFSET_X:
        return &pspOffset[VX];

    case DD_PSPRITE_OFFSET_Y:
        return &pspOffset[VY];

    case DD_PSPRITE_LIGHTLEVEL_MULTIPLIER:
        return &pspLightLevelMultiplier;

    /*case DD_CPLAYER_THRUST_MUL:
        return &cplrThrustMul;*/

    case DD_GRAVITY:
        valueD = App_World().hasMap()? App_World().map().gravity() : 0;
        return &valueD;

#ifdef __CLIENT__
    case DD_TORCH_RED:
        return &torchColor.x;

    case DD_TORCH_GREEN:
        return &torchColor.y;

    case DD_TORCH_BLUE:
        return &torchColor.z;

    case DD_TORCH_ADDITIVE:
        return &torchAdditive;

# ifdef WIN32
    case DD_WINDOW_HANDLE:
        return ClientWindow::main().nativeHandle();
# endif
#endif

    // We have to separately calculate the 35 Hz ticks.
    case DD_GAMETIC: {
        static timespan_t       fracTic;
        fracTic = gameTime * TICSPERSEC;
        return &fracTic; }

    case DD_NUMLUMPS: {
        static int count;
        count = F_LumpCount();
        return &count; }

    default: break;
    }

    if(ddvalue >= DD_LAST_VALUE || ddvalue <= DD_FIRST_VALUE)
        return 0;

    // Other values not supported.
    return ddValues[ddvalue].writePtr;
}

/**
 * Set the value of a variable. The pointer can point to any data, its
 * interpretation depends on the variable. Added for 64-bit support.
 */
#undef DD_SetVariable
void DD_SetVariable(int ddvalue, void *parm)
{
    if(ddvalue <= DD_FIRST_VALUE || ddvalue >= DD_LAST_VALUE)
    {
        switch(ddvalue)
        {
        /*case DD_CPLAYER_THRUST_MUL:
            cplrThrustMul = *(float*) parm;
            return;*/

        case DD_GRAVITY:
            if(App_World().hasMap())
                App_World().map().setGravity(*(coord_t*) parm);
            return;

        case DD_PSPRITE_OFFSET_X:
            pspOffset[VX] = *(float *) parm;
            return;

        case DD_PSPRITE_OFFSET_Y:
            pspOffset[VY] = *(float *) parm;
            return;

        case DD_PSPRITE_LIGHTLEVEL_MULTIPLIER:
            pspLightLevelMultiplier = *(float *) parm;
            return;

#ifdef __CLIENT__
        case DD_TORCH_RED:
            torchColor.x = de::clamp(0.f, *((float*) parm), 1.f);
            return;

        case DD_TORCH_GREEN:
            torchColor.y = de::clamp(0.f, *((float*) parm), 1.f);
            return;

        case DD_TORCH_BLUE:
            torchColor.z = de::clamp(0.f, *((float*) parm), 1.f);
            return;

        case DD_TORCH_ADDITIVE:
            torchAdditive = (*(int*) parm)? true : false;
            break;
#endif

        default:
            break;
        }
    }
}

/// @note Part of the Doomsday public API.
fontschemeid_t DD_ParseFontSchemeName(char const *str)
{
#ifdef __CLIENT__
    try
    {
        FontScheme &scheme = App_ResourceSystem().fontScheme(str);
        if(!scheme.name().compareWithoutCase("System"))
        {
            return FS_SYSTEM;
        }
        if(!scheme.name().compareWithoutCase("Game"))
        {
            return FS_GAME;
        }
    }
    catch(ResourceSystem::UnknownSchemeError const &)
    {}
#endif
    qDebug() << "Unknown font scheme:" << String(str) << ", returning 'FS_INVALID'";
    return FS_INVALID;
}

String DD_MaterialSchemeNameForTextureScheme(String textureSchemeName)
{
    if(!textureSchemeName.compareWithoutCase("Textures"))
    {
        return "Textures";
    }
    if(!textureSchemeName.compareWithoutCase("Flats"))
    {
        return "Flats";
    }
    if(!textureSchemeName.compareWithoutCase("Sprites"))
    {
        return "Sprites";
    }
    if(!textureSchemeName.compareWithoutCase("System"))
    {
        return "System";
    }

    return "";
}

AutoStr *DD_MaterialSchemeNameForTextureScheme(ddstring_t const *textureSchemeName)
{
    if(!textureSchemeName)
    {
        return AutoStr_FromTextStd("");
    }
    else
    {
        QByteArray schemeNameUtf8 = DD_MaterialSchemeNameForTextureScheme(String(Str_Text(textureSchemeName))).toUtf8();
        return AutoStr_FromTextStd(schemeNameUtf8.constData());
    }
}

/**
 * Convert propertyType enum constant into a string for error/debug messages.
 */
const char* value_Str(int val)
{
    static char valStr[40];
    struct val_s {
        int                 val;
        const char*         str;
    } valuetypes[] = {
        { DDVT_BOOL, "DDVT_BOOL" },
        { DDVT_BYTE, "DDVT_BYTE" },
        { DDVT_SHORT, "DDVT_SHORT" },
        { DDVT_INT, "DDVT_INT" },
        { DDVT_UINT, "DDVT_UINT" },
        { DDVT_FIXED, "DDVT_FIXED" },
        { DDVT_ANGLE, "DDVT_ANGLE" },
        { DDVT_FLOAT, "DDVT_FLOAT" },
        { DDVT_DOUBLE, "DDVT_DOUBLE" },
        { DDVT_LONG, "DDVT_LONG" },
        { DDVT_ULONG, "DDVT_ULONG" },
        { DDVT_PTR, "DDVT_PTR" },
        { DDVT_BLENDMODE, "DDVT_BLENDMODE" },
        { 0, NULL }
    };
    uint i;

    for(i = 0; valuetypes[i].str; ++i)
        if(valuetypes[i].val == val)
            return valuetypes[i].str;

    sprintf(valStr, "(unnamed %i)", val);
    return valStr;
}

D_CMD(Load)
{
    DENG_UNUSED(src);

    bool didLoadGame = false, didLoadResource = false;
    int arg = 1;

    AutoStr *searchPath = AutoStr_NewStd();
    Str_Set(searchPath, argv[arg]);
    Str_Strip(searchPath);
    if(Str_IsEmpty(searchPath)) return false;

    F_FixSlashes(searchPath, searchPath);

    // Ignore attempts to load directories.
    if(Str_RAt(searchPath, 0) == '/')
    {
        Con_Message("Directories cannot be \"loaded\" (only files and/or known games).");
        return true;
    }

    // Are we loading a game?
    try
    {
        Game &game = App_Games().byIdentityKey(Str_Text(searchPath));
        if(!game.allStartupFilesFound())
        {
            Con_Message("Failed to locate all required startup resources:");
            Game::printFiles(game, FF_STARTUP);
            Con_Message("%s (%s) cannot be loaded.", Str_Text(game.title()), Str_Text(game.identityKey()));
            return true;
        }

        BusyMode_FreezeGameForBusyMode();

        if(!App_ChangeGame(game)) return false;

        didLoadGame = true;
        ++arg;
    }
    catch(Games::NotFoundError const &)
    {} // Ignore the error.

    // Try the resource locator.
    for(; arg < argc; ++arg)
    {
        try
        {
            String foundPath = App_FileSystem().findPath(de::Uri::fromNativePath(argv[arg], RC_PACKAGE),
                                                          RLF_MATCH_EXTENSION, App_ResourceClass(RC_PACKAGE));
            foundPath = App_BasePath() / foundPath; // Ensure the path is absolute.

            if(tryLoadFile(de::Uri(foundPath, RC_NULL)))
            {
                didLoadResource = true;
            }
        }
        catch(FS1::NotFoundError const&)
        {} // Ignore this error.
    }

    if(didLoadResource)
    {
        DD_UpdateEngineState();
    }

    return didLoadGame || didLoadResource;
}

static de::File1* tryLoadFile(de::Uri const& search, size_t baseOffset)
{
    try
    {
        de::FileHandle& hndl = App_FileSystem().openFile(search.path(), "rb", baseOffset, false /* no duplicates */);

        de::Uri foundFileUri = hndl.file().composeUri();
        VERBOSE( Con_Message("Loading \"%s\"...", NativePath(foundFileUri.asText()).pretty().toUtf8().constData()) )

        App_FileSystem().index(hndl.file());

        return &hndl.file();
    }
    catch(FS1::NotFoundError const&)
    {
        if(App_FileSystem().accessFile(search))
        {
            // Must already be loaded.
            LOG_DEBUG("\"%s\" already loaded.") << NativePath(search.asText()).pretty();
        }
    }
    return 0;
}

static bool tryUnloadFile(de::Uri const& search)
{
    try
    {
        de::File1 &file = App_FileSystem().find(search);
        de::Uri foundFileUri = file.composeUri();
        NativePath nativePath(foundFileUri.asText());

        // Do not attempt to unload a resource required by the current game.
        if(App_CurrentGame().isRequiredFile(file))
        {
            LOG_MSG("\"%s\" is required by the current game."
                    " Required game files cannot be unloaded in isolation.")
                    << nativePath.pretty();
            return false;
        }

        LOG_VERBOSE("Unloading \"%s\"...") << nativePath.pretty();

        App_FileSystem().deindex(file);
        delete &file;

        return true;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore.
    return false;
}

D_CMD(Unload)
{
    DENG_UNUSED(src);

    BusyMode_FreezeGameForBusyMode();

    // No arguments; unload the current game if loaded.
    if(argc == 1)
    {
        if(!App_GameLoaded())
        {
            Con_Message("There is no game currently loaded.");
            return true;
        }
        return App_ChangeGame(App_Games().nullGame());
    }

    AutoStr *searchPath = AutoStr_NewStd();
    Str_Set(searchPath, argv[1]);
    Str_Strip(searchPath);
    if(Str_IsEmpty(searchPath)) return false;

    F_FixSlashes(searchPath, searchPath);

    // Ignore attempts to unload directories.
    if(Str_RAt(searchPath, 0) == '/')
    {
        Con_Message("Directories cannot be \"unloaded\" (only files and/or known games).");
        return true;
    }

    // Unload the current game if specified.
    if(argc == 2)
    {
        try
        {
            Game &game = App_Games().byIdentityKey(Str_Text(searchPath));
            if(App_GameLoaded())
            {
                return App_ChangeGame(App_Games().nullGame());
            }

            Con_Message("%s is not currently loaded.", Str_Text(game.identityKey()));
            return true;
        }
        catch(Games::NotFoundError const &)
        {} // Ignore the error.
    }

    // Try the resource locator.
    bool didUnloadFiles = false;
    for(int i = 1; i < argc; ++i)
    {
        try
        {
            String foundPath = App_FileSystem().findPath(de::Uri::fromNativePath(argv[1], RC_PACKAGE),
                                                         RLF_MATCH_EXTENSION, App_ResourceClass(RC_PACKAGE));
            foundPath = App_BasePath() / foundPath; // Ensure the path is absolute.

            if(tryUnloadFile(de::Uri(foundPath, RC_NULL)))
            {
                didUnloadFiles = true;
            }
        }
        catch(FS1::NotFoundError const&)
        {} // Ignore this error.
    }

    if(didUnloadFiles)
    {
        // A changed file list may alter the main lump directory.
        DD_UpdateEngineState();
    }

    return didUnloadFiles;
}

D_CMD(Reset)
{
    DENG2_UNUSED3(src, argc, argv);

    DD_UpdateEngineState();
    return true;
}

D_CMD(ReloadGame)
{
    DENG2_UNUSED3(src, argc, argv);

    if(!App_GameLoaded())
    {
        Con_Message("No game is presently loaded.");
        return true;
    }
    App_ChangeGame(App_CurrentGame(), true/* allow reload */);
    return true;
}

// dd_loop.c
DENG_EXTERN_C boolean DD_IsSharpTick(void);

// net_main.c
DENG_EXTERN_C void Net_SendPacket(int to_player, int type, const void* data, size_t length);

#undef R_SetupMap
DENG_EXTERN_C void R_SetupMap(int mode, int flags)
{
    DENG2_UNUSED2(mode, flags);

    if(!App_World().hasMap()) return; // Huh?

    // Perform map setup again. Its possible that after loading we now
    // have more HOMs to fix, etc..
    Map &map = App_World().map();

#ifdef __CLIENT__
    map.initSkyFix();
#endif

#ifdef __CLIENT__
    // Update all sectors.
    /// @todo Refactor away.
    foreach(Sector *sector, map.sectors())
    foreach(LineSide *side, sector->sides())
    {
        side->fixMissingMaterials();
    }
#endif

    // Re-initialize polyobjs.
    /// @todo Still necessary?
    map.initPolyobjs();

    // Reset the timer so that it will appear that no time has passed.
    DD_ResetTimer();
}

// sys_system.c
DENG_EXTERN_C void Sys_Quit(void);

DENG_DECLARE_API(Base) =
{
    { DE_API_BASE },

    Sys_Quit,
    DD_GetInteger,
    DD_SetInteger,
    DD_GetVariable,
    DD_SetVariable,
    DD_DefineGame,
    DD_GameIdForKey,
    DD_AddGameResource,
    DD_GameInfo,
    DD_IsSharpTick,
    Net_SendPacket,
    R_SetupMap
};
