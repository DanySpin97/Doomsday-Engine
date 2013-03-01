/** @file materialmanifest.cpp Description of a logical material resource.
 *
 * @authors Copyright © 2011-2013 Daniel Swanson <danij@dengine.net>
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
#include "Materials"

#include "resource/materialmanifest.h"

using namespace de;

DENG2_PIMPL(MaterialManifest)
{
    /// Material associated with this.
    Material *material;

    /// Unique identifier.
    materialid_t id;

    /// Classification flags.
    MaterialManifest::Flags flags;

    Instance(Public *i) : Base(i),
        material(0),
        id(0)
    {}
};

MaterialManifest::MaterialManifest(PathTree::NodeArgs const &args)
    : Node(args), d(new Instance(this))
{}

MaterialManifest::~MaterialManifest()
{
    delete d;
}

Materials &MaterialManifest::materials()
{
    return App_Materials();
}

Material *MaterialManifest::derive()
{
    if(!hasMaterial())
    {
        // Instantiate and associate the new material with this.
        setMaterial(new Material(*this));

        // Include the material in the app's scheme-agnostic list of instances.
        /// @todo de::Materials should observe instead.
        materials().addMaterial(*d->material);
    }
    return &material();
}

materialid_t MaterialManifest::id() const
{
    return d->id;
}

void MaterialManifest::setId(materialid_t id)
{
    d->id = id;
}

MaterialScheme &MaterialManifest::scheme() const
{
    LOG_AS("MaterialManifest::scheme");
    /// @todo Optimize: MaterialManifest should contain a link to the owning MaterialScheme.
    foreach(MaterialScheme *scheme, materials().allSchemes())
    {
        if(&scheme->index() == &tree()) return *scheme;
    }
    /// @throw Error Failed to determine the scheme of the manifest (should never happen...).
    throw Error("MaterialManifest::scheme", String("Failed to determine scheme for manifest [%1]").arg(de::dintptr(this)));
}

String MaterialManifest::description(de::Uri::ComposeAsTextFlags uriCompositionFlags) const
{
    String info = String("%1 %2")
                      .arg(composeUri().compose(uriCompositionFlags | de::Uri::DecodePath),
                           ( uriCompositionFlags.testFlag(de::Uri::OmitScheme)? -14 : -22 ) )
                      .arg(sourceDescription(), -7);
#ifdef __CLIENT__
    info += String("x%1").arg(!hasMaterial()? 0 : material().variantCount());
#endif
    return info;
}

String MaterialManifest::sourceDescription() const
{
    if(!isCustom()) return "game";
    if(isAutoGenerated()) return "add-on"; // Unintuitive but correct.
    return "def";
}

MaterialManifest::Flags MaterialManifest::flags() const
{
    return d->flags;
}

void MaterialManifest::setFlags(MaterialManifest::Flags flagsToChange, bool set)
{
    if(set)
    {
        d->flags |= flagsToChange;
    }
    else
    {
        d->flags &= ~flagsToChange;
    }
}

bool MaterialManifest::hasMaterial() const
{
    return !!d->material;
}

Material &MaterialManifest::material() const
{
    if(!d->material)
    {
        /// @throw MissingMaterialError  The manifest is not presently associated with a material.
        throw MissingMaterialError("MaterialManifest::material", "Missing required material");
    }
    return *d->material;
}

void MaterialManifest::setMaterial(Material *newMaterial)
{
    if(d->material == newMaterial) return;
    d->material = newMaterial;
}
