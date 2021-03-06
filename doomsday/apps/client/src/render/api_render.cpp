/** @file api_render.cpp  Public API of the renderer.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#define DENG_NO_API_MACROS_RENDER

#ifndef __CLIENT__
#  error "api_render.cpp is for the client only"
#endif

#include "de_platform.h"
#include "api_render.h"

#include <de/Log>
#include <doomsday/console/exec.h>
#include <doomsday/defs/sprite.h>
#include <doomsday/res/Sprites>
#include <doomsday/world/Materials>

#include "dd_main.h"  // App_Resources
#include "def_main.h"
#include "sys_system.h"  // novideo
#include "gl/sys_opengl.h"

#include "render/r_main.h"
#include "render/billboard.h"  // Rend_SpriteMaterialSpec
#include "render/rend_model.h"

#include "resource/clientresources.h"
#include "MaterialVariantSpec"

using namespace de;

// m_misc.c
#undef M_ScreenShot
DENG_EXTERN_C dint M_ScreenShot(char const *name, dint flags);

#undef Models_CacheForState
DENG_EXTERN_C void Models_CacheForState(dint stateIndex)
{
    if (FrameModelDef *modelDef = App_Resources().modelDefForState(stateIndex))
    {
        App_Resources().cache(modelDef);
    }
}

// r_draw.cpp
#undef R_SetBorderGfx
DENG_EXTERN_C void R_SetBorderGfx(struct uri_s const *const *paths);

#undef Rend_CacheForMobjType
DENG_EXTERN_C void Rend_CacheForMobjType(dint num)
{
    LOG_AS("Rend.CacheForMobjType");

    if (::novideo) return;
    if (!((::useModels && ::precacheSkins) || ::precacheSprites)) return;
    if (num < 0 || num >= ::runtimeDefs.mobjInfo.size()) return;

    de::MaterialVariantSpec const &spec = Rend_SpriteMaterialSpec();

    /// @todo Optimize: Traverses the entire state list!
    for (dint i = 0; i < DED_Definitions()->states.size(); ++i)
    {
        if (::runtimeDefs.stateInfo[i].owner != &::runtimeDefs.mobjInfo[num])
            continue;

        Models_CacheForState(i);

        if (::precacheSprites)
        {
            if (state_t *state = Def_GetState(i))
            {
                App_Resources().cache(state->sprite, spec);
            }
        }
        /// @todo What about sounds?
    }
}

#undef R_RenderPlayerView
#undef R_SetViewOrigin
#undef R_SetViewAngle
#undef R_SetViewPitch
#undef R_ViewWindowGeometry
#undef R_ViewWindowOrigin
#undef R_ViewWindowSize
#undef R_SetViewWindowGeometry
#undef R_ViewPortGeometry
#undef R_ViewPortOrigin
#undef R_ViewPortSize
#undef R_SetViewPortPlayer

// r_main.cpp
DENG_EXTERN_C void R_RenderPlayerView(dint num);
DENG_EXTERN_C void R_SetViewOrigin(dint consoleNum, coord_t const origin[3]);
DENG_EXTERN_C void R_SetViewAngle(dint consoleNum, angle_t angle);
DENG_EXTERN_C void R_SetViewPitch(dint consoleNum, dfloat pitch);
DENG_EXTERN_C dint R_ViewWindowGeometry(dint consoleNum, RectRaw *geometry);
DENG_EXTERN_C dint R_ViewWindowOrigin(dint consoleNum, Point2Raw *origin);
DENG_EXTERN_C dint R_ViewWindowSize(dint consoleNum, Size2Raw *size);
DENG_EXTERN_C void R_SetViewWindowGeometry(dint consoleNum, RectRaw const *geometry, dd_bool interpolate);
DENG_EXTERN_C dint R_ViewPortGeometry(dint consoleNum, RectRaw *geometry);
DENG_EXTERN_C dint R_ViewPortOrigin(dint consoleNum, Point2Raw *origin);
DENG_EXTERN_C dint R_ViewPortSize(dint consoleNum, Size2Raw *size);
DENG_EXTERN_C void R_SetViewPortPlayer(dint consoleNum, dint viewPlayer);

// sky.cpp
#undef R_SkyParams
DENG_EXTERN_C void R_SkyParams(dint layer, dint param, void *data);

static inline MaterialVariantSpec const &pspriteMaterialSpec_GetSpriteInfo()
{
    return App_Resources().materialSpec(PSpriteContext, 0, 1, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                             0, -2, -1, false, true, true, false);
}

#undef R_GetSpriteInfo
DENG_EXTERN_C dd_bool R_GetSpriteInfo(dint id, dint frame, spriteinfo_t *info)
{
    if (!info) return false;

    de::zapPtr(info);

    auto const *sprDef = res::Sprites::get().spritePtr(id, frame);
    if (!sprDef)
    {
        LOG_RES_WARNING("Invalid sprite id:%i and/or frame:%i") << id << frame;
        return false;
    }

    defn::Sprite sprite(*sprDef);
    if (!sprite.hasView(0))
    {
        LOG_RES_WARNING("Sprite id:%i frame:%i has no front view")
            << id << frame;
        return false;
    }

    defn::Sprite::View const spriteView = sprite.view(0);
    info->material = world::Materials::get().materialPtr(*spriteView.material);
    info->flip     = spriteView.mirrorX;

    if (::novideo) return true;  // We can't prepare the material.

    /// @todo fixme: We should not be using the PSprite spec here. -ds
    MaterialAnimator &matAnimator = reinterpret_cast<world::Material *>(info->material)->as<ClientMaterial>()
            .getAnimator(pspriteMaterialSpec_GetSpriteInfo());
    matAnimator.prepare();  // Ensure we have up-to-date info.

    Vector2ui const &matDimensions = matAnimator.dimensions();
    TextureVariant *tex            = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture;
    Vector2i const &texDimensions  = tex->base().origin();
    dint const texBorder           = tex->spec().variant.border;

    info->geometry.origin.x    = -texDimensions.x + -texBorder;
    info->geometry.origin.y    = -texDimensions.y +  texBorder;
    info->geometry.size.width  = matDimensions.x + texBorder * 2;
    info->geometry.size.height = matDimensions.y + texBorder * 2;

    tex->glCoords(&info->texCoord[0], &info->texCoord[1]);

    return true;
}

#undef R_ChooseAlignModeAndScaleFactor
#undef R_ChooseScaleMode2
#undef R_ChooseScaleMode

// r_util.c
DENG_EXTERN_C dd_bool R_ChooseAlignModeAndScaleFactor(dfloat *scale, dint width, dint height, dint availWidth, dint availHeight, scalemode_t scaleMode);
DENG_EXTERN_C scalemode_t R_ChooseScaleMode2(dint width, dint height, dint availWidth, dint availHeight, scalemode_t overrideMode, dfloat stretchEpsilon);
DENG_EXTERN_C scalemode_t R_ChooseScaleMode(dint width, dint height, dint availWidth, dint availHeight, scalemode_t overrideMode);

#undef R_SetupFog
DENG_EXTERN_C void R_SetupFog(dfloat start, dfloat end, dfloat density, dfloat const *rgb)
{
    DENG2_ASSERT(rgb);
    Con_Execute(CMDS_DDAY, "fog on", true, false);
    Con_Executef(CMDS_DDAY, true, "fog start %f", start);
    Con_Executef(CMDS_DDAY, true, "fog end %f", end);
    Con_Executef(CMDS_DDAY, true, "fog density %f", density);
    Con_Executef(CMDS_DDAY, true, "fog color %.0f %.0f %.0f",
                 rgb[0] * 255, rgb[1] * 255, rgb[2] * 255);
}

#undef R_SetupFogDefaults
DENG_EXTERN_C void R_SetupFogDefaults()
{
    // Go with the defaults.
    Con_Execute(CMDS_DDAY, "fog off", true, false);
}

DENG_DECLARE_API(Rend) =
{
    { DE_API_RENDER },
    R_SetupFogDefaults,
    R_SetupFog,
    Rend_CacheForMobjType,
    Models_CacheForState,
    R_RenderPlayerView,
    R_SetViewOrigin,
    R_SetViewAngle,
    R_SetViewPitch,
    R_ViewWindowGeometry,
    R_ViewWindowOrigin,
    R_ViewWindowSize,
    R_SetViewWindowGeometry,
    R_SetBorderGfx,
    R_ViewPortGeometry,
    R_ViewPortOrigin,
    R_ViewPortSize,
    R_SetViewPortPlayer,
    R_ChooseAlignModeAndScaleFactor,
    R_ChooseScaleMode2,
    R_ChooseScaleMode,
    R_GetSpriteInfo,
    R_SkyParams,
    M_ScreenShot
};
