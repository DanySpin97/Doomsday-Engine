/** @file mapdef.cpp  Resource manifest for a map.
 *
 * @authors Copyright © 2014-2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/resource/mapmanifest.h"
#include <de/NativePath>

using namespace de;

namespace res {

MapManifest::MapManifest(PathTree::NodeArgs const &args)
    : Node(args), Record(), _sourceFile(nullptr)
{}

String MapManifest::description(de::Uri::ComposeAsTextFlags uriCompositionFlags) const
{
    String info = String("%1").arg(composeUri().compose(uriCompositionFlags | de::Uri::DecodePath),
                                   ( uriCompositionFlags.testFlag(de::Uri::OmitScheme)? -14 : -22 ) );
    if (_sourceFile)
    {
        info += String(" " _E(C) "\"%1\"" _E(.)).arg(NativePath(sourceFile()->composePath()).pretty());
    }
    return info;
}

String MapManifest::composeUniqueId(Game const &currentGame) const
{
    return String("%1|%2|%3|%4")
              .arg(gets("id").fileNameWithoutExtension())
              .arg(sourceFile()->name().fileNameWithoutExtension())
              .arg(sourceFile()->hasCustom()? "pwad" : "iwad")
              .arg(currentGame.id())
              .toLower();
}

MapManifest &MapManifest::setSourceFile(File1 *newSourceFile)
{
    _sourceFile = newSourceFile;
    return *this;
}

File1 *MapManifest::sourceFile() const
{
    return _sourceFile;
}

MapManifest &MapManifest::setRecognizer(Id1MapRecognizer *newRecognizer)
{
    _recognized.reset(newRecognizer);
    return *this;
}

Id1MapRecognizer const &MapManifest::recognizer() const
{
    DENG2_ASSERT(bool(_recognized));
    return *_recognized;
}

}  // namespace res
