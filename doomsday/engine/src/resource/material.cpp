/**
 * @file material.cpp
 * Logical material. @ingroup resource
 *
 * @authors Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
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

#include <cstring>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_resource.h"
#include "m_misc.h"

#include "audio/s_environ.h"
#include "resource/materialsnapshot.h"
#include "render/rend_main.h"

#include <de/memory.h>

using namespace de;

typedef struct material_variantlist_node_s {
    struct material_variantlist_node_s *next;
    MaterialVariant *variant;
} material_variantlist_node_t;

static void destroyVariants(material_t *mat)
{
    DENG2_ASSERT(mat);
    while(mat->_variants)
    {
        material_variantlist_node_t *next = mat->_variants->next;
        delete mat->_variants->variant;
        M_Free(mat->_variants);
        mat->_variants = next;
    }
    mat->_prepared = 0;
}

void Material_Initialize(material_t *mat)
{
    DENG2_ASSERT(mat);
    std::memset(mat, 0, sizeof *mat);
    mat->header.type = DMU_MATERIAL;
    mat->_envClass = MEC_UNKNOWN;
    mat->_size = Size2_New();
}

void Material_Destroy(material_t *mat)
{
    DENG2_ASSERT(mat);
    Material_DestroyVariants(mat);
    Size2_Delete(mat->_size);
    mat->_size = 0;
}

void Material_Ticker(material_t *mat, timespan_t time)
{
    DENG2_ASSERT(mat);
    for(material_variantlist_node_t *node = mat->_variants; node; node = node->next)
    {
        node->variant->ticker(time);
    }
}

ded_material_t *Material_Definition(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->_def;
}

void Material_SetDefinition(material_t *mat, struct ded_material_s *def)
{
    DENG2_ASSERT(mat);
    if(mat->_def != def)
    {
        mat->_def = def;

        // Textures are updated automatically at prepare-time, so just clear them.
        Material_SetDetailTexture(mat, NULL);
        Material_SetShinyTexture(mat, NULL);
        Material_SetShinyMaskTexture(mat, NULL);
    }

    if(!mat->_def) return;

    mat->_flags = mat->_def->flags;

    Size2Raw size(def->width, def->height);
    Material_SetSize(mat, &size);

    Material_SetEnvironmentClass(mat, S_MaterialEnvClassForUri(def->uri));

    // Update custom status.
    /// @todo This should take into account the whole definition, not just whether
    ///       the primary layer's first texture is custom or not.
    mat->_isCustom = false;
    if(def->layers[0].stageCount.num > 0 && def->layers[0].stages[0].texture)
    {
        de::Uri *texUri = reinterpret_cast<de::Uri *>(def->layers[0].stages[0].texture);
        try
        {
            TextureManifest &manifest = App_Textures()->find(*texUri);
            if(Texture *tex = manifest.texture())
            {
                mat->_isCustom = tex->flags().testFlag(Texture::Custom);
            }
        }
        catch(Textures::NotFoundError const &)
        {} // Ignore this error.
    }
}

Size2 const *Material_Size(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->_size;
}

void Material_SetSize(material_t* mat, const Size2Raw* newSize)
{
    DENG2_ASSERT(mat);
    if(!newSize) return;

    Size2 *size = Size2_NewFromRaw(newSize);
    if(!Size2_Equality(mat->_size, size))
    {
        Size2_SetWidthHeight(mat->_size, newSize->width, newSize->height);
        R_UpdateMapSurfacesOnMaterialChange(mat);
    }
    Size2_Delete(size);
}

int Material_Width(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return Size2_Width(mat->_size);
}

void Material_SetWidth(material_t *mat, int width)
{
    DENG2_ASSERT(mat);
    if(Size2_Width(mat->_size) == width) return;
    Size2_SetWidth(mat->_size, width);
    R_UpdateMapSurfacesOnMaterialChange(mat);
}

int Material_Height(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return Size2_Height(mat->_size);
}

void Material_SetHeight(material_t *mat, int height)
{
    DENG2_ASSERT(mat);
    if(Size2_Height(mat->_size) == height) return;
    Size2_SetHeight(mat->_size, height);
    R_UpdateMapSurfacesOnMaterialChange(mat);
}

short Material_Flags(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->_flags;
}

void Material_SetFlags(material_t *mat, short flags)
{
    DENG2_ASSERT(mat);
    mat->_flags = flags;
}

boolean Material_IsCustom(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->_isCustom;
}

boolean Material_IsGroupAnimated(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->_inAnimGroup;
}

boolean Material_IsSkyMasked(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return 0 != (mat->_flags & MATF_SKYMASK);
}

boolean Material_IsDrawable(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return 0 == (mat->_flags & MATF_NO_DRAW);
}

boolean Material_HasGlow(material_t *mat)
{
    if(novideo) return false;

    /// @todo We should not need to prepare to determine this.
    MaterialSnapshot const &ms = reinterpret_cast<MaterialSnapshot const &>(
        *App_Materials()->prepare(*mat, *Rend_MapSurfaceDiffuseMaterialSpec(), true));

    return (ms.glowStrength() > .0001f);
}

boolean Material_HasTranslation(material_t const *mat)
{
    DENG2_ASSERT(mat);
    /// @todo Separate meanings.
    return Material_IsGroupAnimated(mat);
}

int Material_LayerCount(material_t const *mat)
{
    DENG2_ASSERT(mat);
    DENG2_UNUSED(mat);
    return 1;
}

void Material_SetGroupAnimated(material_t *mat, boolean yes)
{
    DENG2_ASSERT(mat);
    mat->_inAnimGroup = yes;
}

byte Material_Prepared(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->_prepared;
}

void Material_SetPrepared(material_t *mat, byte state)
{
    DENG2_ASSERT(mat && state <= 2);
    mat->_prepared = state;
}

materialid_t Material_PrimaryBind(material_t const *mat)
{
    DENG2_ASSERT(mat);
    return mat->_primaryBind;
}

void Material_SetPrimaryBind(material_t *mat, materialid_t bindId)
{
    DENG2_ASSERT(mat);
    mat->_primaryBind = bindId;
}

material_env_class_t Material_EnvironmentClass(material_t const *mat)
{
    DENG2_ASSERT(mat);
    if(!Material_IsDrawable(mat))
        return MEC_UNKNOWN;
    return mat->_envClass;
}

void Material_SetEnvironmentClass(material_t *mat, material_env_class_t envClass)
{
    DENG2_ASSERT(mat);
    mat->_envClass = envClass;
}

struct texture_s *Material_DetailTexture(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->_detailTex;
}

void Material_SetDetailTexture(material_t *mat, struct texture_s *tex)
{
    DENG2_ASSERT(mat);
    mat->_detailTex = tex;
}

float Material_DetailStrength(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->_detailStrength;
}

void Material_SetDetailStrength(material_t *mat, float strength)
{
    DENG2_ASSERT(mat);
    mat->_detailStrength = MINMAX_OF(0, strength, 1);
}

float Material_DetailScale(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->_detailScale;
}

void Material_SetDetailScale(material_t *mat, float scale)
{
    DENG2_ASSERT(mat);
    mat->_detailScale = MINMAX_OF(0, scale, 1);
}

struct texture_s *Material_ShinyTexture(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->_shinyTex;
}

void Material_SetShinyTexture(material_t *mat, struct texture_s *tex)
{
    DENG2_ASSERT(mat);
    mat->_shinyTex = tex;
}

blendmode_t Material_ShinyBlendmode(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->_shinyBlendmode;
}

void Material_SetShinyBlendmode(material_t *mat, blendmode_t blendmode)
{
    DENG2_ASSERT(mat && VALID_BLENDMODE(blendmode));
    mat->_shinyBlendmode = blendmode;
}

float const *Material_ShinyMinColor(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->_shinyMinColor;
}

void Material_SetShinyMinColor(material_t *mat, float const colorRGB[3])
{
    DENG2_ASSERT(mat && colorRGB);
    mat->_shinyMinColor[CR] = MINMAX_OF(0, colorRGB[CR], 1);
    mat->_shinyMinColor[CG] = MINMAX_OF(0, colorRGB[CG], 1);
    mat->_shinyMinColor[CB] = MINMAX_OF(0, colorRGB[CB], 1);
}

float Material_ShinyStrength(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->_shinyStrength;
}

void Material_SetShinyStrength(material_t *mat, float strength)
{
    DENG2_ASSERT(mat);
    mat->_shinyStrength = MINMAX_OF(0, strength, 1);
}

struct texture_s *Material_ShinyMaskTexture(material_t *mat)
{
    DENG2_ASSERT(mat);
    return mat->_shinyMaskTex;
}

void Material_SetShinyMaskTexture(material_t *mat, struct texture_s *tex)
{
    DENG2_ASSERT(mat);
    mat->_shinyMaskTex = tex;
}

struct materialvariant_s *Material_AddVariant(material_t *mat, struct materialvariant_s *variant)
{
    DENG2_ASSERT(mat);
    if(!variant)
    {
#if _DEBUG
        Con_Error("Material_AddVariant: Argument variant==NULL, ignoring.");
#endif
        return variant;
    }

    material_variantlist_node_t *node = static_cast<material_variantlist_node_t *>(M_Malloc(sizeof *node));
    if(!node) Con_Error("Material_AddVariant: Failed on allocation of %lu bytes for new node.", (unsigned long) sizeof *node);

    node->variant = reinterpret_cast<MaterialVariant *>(variant);
    node->next = mat->_variants;
    mat->_variants = node;
    return variant;
}

int Material_IterateVariants(material_t *mat,
    int (*callback)(struct materialvariant_s *variant, void *parameters), void *parameters)
{
    DENG2_ASSERT(mat);
    int result = 0;
    if(callback)
    {
        material_variantlist_node_t *node = mat->_variants;
        while(node)
        {
            material_variantlist_node_t *next = node->next;
            result = callback(reinterpret_cast<struct materialvariant_s *>(node->variant), parameters);
            if(result) break;
            node = next;
        }
    }
    return result;
}

int Material_VariantCount(material_t const *mat)
{
    DENG_ASSERT(mat);
    int count = 0;
    for(material_variantlist_node_t *node = mat->_variants; node; node = node->next)
    {
        ++count;
    }
    return count;
}

void Material_DestroyVariants(material_t *mat)
{
    destroyVariants(mat);
}
