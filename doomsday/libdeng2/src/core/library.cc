/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/Library"
#include "de/Log"

using namespace de;

const char* Library::DEFAULT_TYPE = "library/generic";

Library::Library(const String& nativePath)
    : _library(0), _type(DEFAULT_TYPE)
{
    LOG_AS("Library::Library");
    LOG_VERBOSE("%s") << nativePath;
    
    _library = new QLibrary(nativePath);
    if(!_library->isLoaded())
    {
        delete _library;
        _library = 0;

        /// @throw LoadError Opening of the dynamic library failed.
        throw LoadError("Library::Library", _library->errorString());
    }

    if(address("deng_LibraryType")) 
    {
        // Query the type identifier.
        _type = SYMBOL(deng_LibraryType)();
    }
    
    if(_type.beginsWith("deng-plugin/") && address("deng_InitializePlugin"))
    {
        // Automatically call the initialization function, if one exists.
        SYMBOL(deng_InitializePlugin)();
    }
}

Library::~Library()
{
    if(_library)
    {
        if(_type.beginsWith("deng-plugin/") && address("deng_ShutdownPlugin")) 
        {
            // Automatically call the shutdown function, if one exists.
            SYMBOL(deng_ShutdownPlugin)();
        }

        delete _library;
    }
}

void* Library::address(const String& name)
{
    if(!_library)
    {
        return 0;
    }
    
    // Already looked up?
    Symbols::iterator found = _symbols.find(name);
    if(found != _symbols.end())
    {
        return found.value();
    }
    
    void* ptr = _library->resolve(name.toAscii().constData());

    if(!ptr)
    {
        /// @throw SymbolMissingError There is no symbol @a name in the library.
        throw SymbolMissingError("Library::symbol", "Symbol '" + name + "' was not found");
    }

    _symbols[name] = ptr;
    return ptr;
}
