/** @file shard.cpp  3D map geometry shard.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#include "render/shard.h"

using namespace de;

Shard::Shard(DrawList::Spec const &listSpec, blendmode_t blendmode, GLuint modTex,
    Vector3f const &modColor, bool hasDynlights)
    : listSpec    (listSpec)
    , blendmode   (blendmode)
    , modTex      (modTex)
    , modColor    (modColor)
    , hasDynlights(hasDynlights)
{}
