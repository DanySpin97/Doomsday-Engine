/*
 * The Doomsday Engine Project -- dengcl
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "localserver.h"

#include <de/core.h>
#include <de/net.h>
#include <de/data.h>

using namespace de;

LocalServer::LocalServer(de::duint16 listenOnPort) : listenOnPort_(listenOnPort)
{
    CommandLine svArgs = App::app().commandLine();
    
#if defined(WIN32)
    svArgs.insert(0, "dengsv.exe");
#elif defined(MACOSX)
    svArgs.insert(0, svArgs.at(0).fileNamePath().concatenatePath("../Resources/dengsv"));
#else
    svArgs.insert(0, "./dengsv");
#endif

    svArgs.append("--port");
    svArgs.append(NumberValue(listenOnPort_).asText());

    svArgs.remove(1);
    extern char** environ;
    svArgs.execute(environ);
}

LocalServer::~LocalServer()
{
    try
    {
        std::cout << "Stopping local server...\n";

        Link(Address("localhost", listenOnPort_)) << CommandPacket("quit");
    }
    catch(const Socket::ConnectionError&)
    {
        std::cout << "Couldn't connect to local server!\n";
    }
}
