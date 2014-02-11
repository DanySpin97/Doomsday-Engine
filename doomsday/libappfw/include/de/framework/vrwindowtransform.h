/** @file vrwindowtransform.h  Window content transformation for virtual reality.
 *
 * @authors Copyright (c) 2013 Christopher Bruns <cmbruns@rotatingpenguin.com>
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_VRWINDOWTRANSFORM_H
#define LIBAPPFW_VRWINDOWTRANSFORM_H

#include "../WindowTransform"

namespace de {

/**
 * Window content transformation for virtual reality.
 */
class VRWindowTransform : public WindowTransform
{
public:
    VRWindowTransform(BaseWindow &window);

    void glInit();
    void glDeinit();

    Vector2ui logicalRootSize(Vector2ui const &physicalCanvasSize) const;
    Vector2f windowToLogicalCoords(Vector2i const &pos) const;

    void drawTransformed();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_VRWINDOWTRANSFORM_H
