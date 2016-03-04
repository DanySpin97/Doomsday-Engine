/** @file profiles.cpp  Abstract set of persistent profiles.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/Profiles"
#include "de/String"
#include "de/File"
#include "de/App"

#include <QTextStream>

namespace de {

DENG2_PIMPL(Profiles)
{
    typedef QMap<String, AbstractProfile *> Profiles;
    Profiles profiles;
    String persistentName;

    Instance(Public *i) : Base(i)
    {}

    ~Instance()
    {
        clear();
    }

    void add(AbstractProfile *profile)
    {
        if(profiles.contains(profile->name()))
        {
            delete profiles[profile->name()];
        }
        profiles.insert(profile->name(), profile);
        profile->setOwner(thisPublic);
    }

    void clear()
    {
        qDeleteAll(profiles.values());
        profiles.clear();
    }

    /**
     * For persistent profiles, determines the file name of the Info file
     * where all the profile contents are written to and read from.
     */
    String fileName() const
    {
        if(persistentName.isEmpty()) return "";
        return String("/home/configs/%1.dei").arg(persistentName);
    }

    void loadProfilesFromInfo(File const &file, bool markReadOnly)
    {
        try
        {
            LOG_RES_VERBOSE("Reading profiles from %s") << file.description();

            Block raw;
            file >> raw;

            de::Info info;
            info.parse(String::fromUtf8(raw));

            foreach(de::Info::Element const *elem, info.root().contentsInOrder())
            {
                if(!elem->isBlock()) continue;

                // There may be multiple profiles in the file.
                de::Info::BlockElement const &profBlock = elem->as<de::Info::BlockElement>();
                if(profBlock.blockType() == "group" &&
                   profBlock.name()      == "profile")
                {
                    String profileName = profBlock.keyValue("name").text;
                    if(profileName.isEmpty()) continue; // Name is required.

                    LOG_VERBOSE("Reading profile '%s'") << profileName;

                    auto *prof = self.profileFromInfoBlock(profBlock);
                    prof->setName(profileName);
                    prof->setReadOnly(markReadOnly);
                    add(prof);
                }
            }
        }
        catch(Error const &er)
        {
            LOG_RES_WARNING("Failed to load profiles from %s:\n%s")
                    << file.description() << er.asText();
        }
    }
};

Profiles::Profiles()
    : d(new Instance(this))
{}

StringList Profiles::profiles() const
{
    return d->profiles.keys();
}

int Profiles::count() const
{
    return d->profiles.size();
}

Profiles::AbstractProfile *Profiles::tryFind(String const &name) const
{
    auto found = d->profiles.constFind(name);
    if(found != d->profiles.constEnd())
    {
        return found.value();
    }
    return nullptr;
}

Profiles::AbstractProfile &Profiles::find(String const &name) const
{
    if(auto *p = tryFind(name))
    {
        return *p;
    }
    throw NotFoundError("Profiles::find", "Profile '" + name + "' not found");
}

void Profiles::setPersistentName(String const &name)
{
    d->persistentName = name;
}

String Profiles::persistentName() const
{
    return d->persistentName;
}

bool Profiles::isPersistent() const
{
    return !d->persistentName.isEmpty();
}

LoopResult Profiles::forAll(std::function<LoopResult (AbstractProfile &)> func)
{
    foreach(AbstractProfile *prof, d->profiles.values())
    {
        if(auto result = func(*prof))
        {
            return result;
        }
    }
    return LoopContinue;
}

void Profiles::clear()
{
    d->clear();
}

void Profiles::add(AbstractProfile *profile)
{
    d->add(profile);
}

void Profiles::remove(AbstractProfile &profile)
{
    DENG2_ASSERT(&profile.owner() == this);

    d->profiles.remove(profile.name());
    profile.setOwner(nullptr);
}

void Profiles::serialize() const
{
    if(!isPersistent()) return;

    LOG_AS("Profiles");
    LOGDEV_VERBOSE("Serializing %s profiles") << d->persistentName;

    // We will write one Info file with all the profiles.
    String info;
    QTextStream os(&info);
    os.setCodec("UTF-8");

    os << "# Autogenerated Info file based on " << d->persistentName
       << " profiles\n";

    // Write /home/configs/(persistentName).dei with all non-readonly profiles.
    int count = 0;
    for(auto *prof : d->profiles)
    {
        if(prof->isReadOnly()) continue;

        os << "\nprofile {\n"
              "    name: " << prof->name() << "\n";
        for(auto line : prof->toInfoSource().split('\n'))
        {
            os << "    " << line << "\n";
        }
        os << "}\n";
        ++count;
    }

    // Create the pack and update the file system.
    File &outFile = App::rootFolder().replaceFile(d->fileName());
    outFile << info.toUtf8();
    outFile.flush(); // we're done

    LOG_VERBOSE("Wrote \"%s\" with %i profile%s")
            << d->fileName() << count << (count != 1? "s" : "");
}

void Profiles::deserialize()
{
    if(!isPersistent()) return;

    LOG_AS("Profiles");
    LOGDEV_VERBOSE("Deserializing %s profiles") << d->persistentName;

    clear();

    // Read all fixed profiles from */profiles/(persistentName)/
    FS::FoundFiles folders;
    App::fileSystem().findAll("profiles" / d->persistentName, folders);
    DENG2_FOR_EACH(FS::FoundFiles, i, folders)
    {
        if(Folder const *folder = (*i)->maybeAs<Folder>())
        {
            // Let's see if it contains any .dei files.
            DENG2_FOR_EACH_CONST(Folder::Contents, k, folder->contents())
            {
                if(k->first.fileNameExtension() == ".dei")
                {
                    // Load this profile.
                    d->loadProfilesFromInfo(*k->second, true /* read-only */);
                }
            }
        }
    }

    // Read /home/configs/(persistentName).dei
    if(File const *file = App::rootFolder().tryLocate<File const>(d->fileName()))
    {
        d->loadProfilesFromInfo(*file, false /* modifiable */);
    }
}

// Profiles::AbstractProfile --------------------------------------------------

DENG2_PIMPL(Profiles::AbstractProfile)
{
    Profiles *owner = nullptr;
    String name;
    bool readOnly = false;

    Instance(Public *i) : Base(i) {}

    ~Instance() {}
};

Profiles::AbstractProfile::AbstractProfile()
    : d(new Instance(this))
{}

Profiles::AbstractProfile::~AbstractProfile()
{}

void Profiles::AbstractProfile::setOwner(Profiles *owner)
{
    DENG2_ASSERT(d->owner == nullptr);
    d->owner = owner;
}

Profiles &Profiles::AbstractProfile::owner()
{
    DENG2_ASSERT(d->owner);
    return *d->owner;
}

Profiles const &Profiles::AbstractProfile::owner() const
{
    DENG2_ASSERT(d->owner);
    return *d->owner;
}

String Profiles::AbstractProfile::name() const
{
    return d->name;
}

bool Profiles::AbstractProfile::setName(String const &newName)
{
    if(newName.isEmpty()) return false;

    Profiles *owner = d->owner;
    if(owner)
    {
        if(owner->tryFind(newName))
        {
            // This name is already in use.
            return false;
        }
        owner->remove(*this);
    }

    d->name = newName;

    if(owner)
    {
        owner->add(this);
    }
    return true;
}

bool Profiles::AbstractProfile::isReadOnly() const
{
    return d->readOnly;
}

void Profiles::AbstractProfile::setReadOnly(bool readOnly)
{
    d->readOnly = readOnly;
}

} // namespace de
