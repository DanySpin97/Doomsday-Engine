/**
 * @file rect.h
 * Rectangles. @ingroup math
 *
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_DATA_RECT_H
#define LIBDENG_DATA_RECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "point.h"
#include "size.h"

/**
 * RectRaw. Rectangle Raw (i)integer. Is intended as a handy POD
 * structure for easy manipulation of rectangles using integer precison.
 */
typedef struct RectRaw_s {
    Point2Raw origin;
    Size2Raw size;
} RectRaw;

/**
 * Rect. Class for manipulating rectangles using integer precision.
 */
struct rect_s; // The Rect instance (opaque).
typedef struct rect_s Rect;

Rect* Rect_New(void);
Rect* Rect_NewWithOriginSize(const Point2* origin, const Size2* size);
Rect* Rect_NewWithOriginSize2(int x, int y, int width, int height);
Rect* Rect_NewFromRaw(const RectRaw* rawRect);

void Rect_Delete(Rect* rect);

void Rect_Copy(Rect* rect, const Rect* other);

RectRaw* Rect_Raw(const Rect* rect, RectRaw* rawRect);

boolean Rect_IsNull(const Rect* rect);

const Point2* Rect_Origin(const Rect* rect);
int Rect_X(const Rect* rect);
int Rect_Y(const Rect* rect);

void Rect_SetOrigin(Rect* rect, const Point2* origin);
void Rect_SetX(Rect* rect, int x);
void Rect_SetY(Rect* rect, int y);
void Rect_SetXY(Rect* rect, int x, int y);

void Rect_Translate(Rect* r, const Point2Raw* delta);
void Rect_TranslateX(Rect* rect, int x);
void Rect_TranslateY(Rect* rect, int y);
void Rect_TranslateXY(Rect* rect, int x, int y);

const Size2* Rect_Size(const Rect* rect);
int Rect_Width(const Rect* rect);
int Rect_Height(const Rect* rect);

void Rect_SetSize(Rect* rect, const Size2* size);
void Rect_SetWidth(Rect* rect, int width);
void Rect_SetHeight(Rect* rect, int height);

void Rect_SetWidthHeight(Rect* rect, int width, int height);

Point2Raw* Rect_TopLeft(const Rect* rect, Point2Raw* point);
Point2Raw* Rect_TopRight(const Rect* rect, Point2Raw* point);
Point2Raw* Rect_BottomLeft(const Rect* rect, Point2Raw* point);
Point2Raw* Rect_BottomRight(const Rect* rect, Point2Raw* point);

Rect* Rect_Normalize(Rect* rect);
Rect* Rect_Unite(Rect* rect, const Rect* other);
Rect* Rect_UniteRaw(Rect* rect, const RectRaw* other);

boolean Rect_Equality(const Rect* rect, const Rect* other);

/**
 * Static non-members.
 */
RectRaw* Rect_Normalized(const Rect* rect, RectRaw* normalized);
RectRaw* Rect_United(const Rect* rect, const Rect* other, RectRaw* united);

/**
 * RectRawf. Rectangle Raw (f)loating point. Is intended as a handy
 * POD structure for easy manipulation of rectangles using floating
 * point precison.
 */
typedef struct RectRawf_s {
    Point2Rawf origin;
    Size2Rawf size;
} RectRawf;

/**
 * Rectf. Class for manipulating rectangles using floating point precision.
 */
struct rectf_s; // The Rectf instance (opaque).
typedef struct rectf_s Rectf;

Rectf* Rectf_New(void);
Rectf* Rectf_NewWithOriginSize(const Point2f* origin, const Size2f* size);
Rectf* Rectf_NewFromRaw(const RectRawf* rawRect);

void Rectf_Delete(Rectf* rect);

void Rectf_Copy(Rectf* rect, const Rectf* other);

RectRawf* Rectf_Raw(const Rectf* rect, RectRawf* rawRect);

boolean Rectf_IsNull(const Rectf* rect);

const Point2f* Rectf_Origin(const Rectf* rect);
int Rectf_X(const Rectf* rect);
int Rectf_Y(const Rectf* rect);

void Rectf_SetOrigin(Rectf* rect, const Point2f* origin);
void Rectf_SetX(Rectf* rect, double x);
void Rectf_SetY(Rectf* rect, double y);
void Rectf_SetXY(Rectf* rect, double x, double y);

void Rectf_TranslateX(Rectf* rect, double x);
void Rectf_TranslateY(Rectf* rect, double y);
void Rectf_TranslateXY(Rectf* rect, double x, double y);

const Size2f* Rectf_Size(const Rectf* rect);
double Rectf_Width(const Rectf* rect);
double Rectf_Height(const Rectf* rect);

void Rectf_SetSize(Rectf* rect, const Size2f* size);
void Rectf_SetWidth(Rectf* rect, double width);
void Rectf_SetHeight(Rectf* rect, double height);

void Rectf_SetWidthHeight(Rectf* rect, double width, double height);

Point2Rawf* Rectf_TopLeft(const Rectf* rect, Point2Rawf* point);
Point2Rawf* Rectf_TopRight(const Rectf* rect, Point2Rawf* point);
Point2Rawf* Rectf_BottomLeft(const Rectf* rect, Point2Rawf* point);
Point2Rawf* Rectf_BottomRight(const Rectf* rect, Point2Rawf* point);

Rectf* Rectf_Normalize(Rectf* rect);
Rectf* Rectf_Unite(Rectf* rect, const Rectf* other);
Rectf* Rectf_UniteRaw(Rectf* rect, const RectRawf* other);

boolean Rectf_Equality(const Rectf* rect, const Rectf* other);

/**
 * Static non-members:
 */
RectRawf* Rectf_Normalized(const Rectf* rect, RectRawf* normalized);
RectRawf* Rectf_United(const Rectf* rect, const Rectf* other, RectRawf* united);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_DATA_RECT_H */
