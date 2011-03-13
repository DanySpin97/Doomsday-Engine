/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

/**
 * dd_world.h: Public API to World Data
 *
 * World data comprises the map and all the objects in it. The public API
 * includes accessing and modifying map data objects via DMU.
 */

#ifndef __DOOMSDAY_WORLD_H__
#define __DOOMSDAY_WORLD_H__

#include "dd_share.h"

#ifdef __cplusplus
extern "C" {
#endif

unsigned int    P_ToIndex(const void* ptr);
void*           P_ToPtr(int type, uint index);
int             P_Callback(int type, uint index, void* context,
                           int (*callback)(void* p, void* ctx));
int             P_Callbackp(int type, void* ptr, void* context,
                            int (*callback)(void* p, void* ctx));
int             P_Iteratep(void *ptr, uint prop, void* context,
                           int (*callback) (void* p, void* ctx));

/* dummy functions */
void*           P_AllocDummy(int type, void* extraData);
void            P_FreeDummy(void* dummy);
int             P_DummyType(void* dummy);
boolean         P_IsDummy(void* dummy);
void*           P_DummyExtraData(void* dummy);

/* index-based write functions */
void            P_SetBool(int type, uint index, uint prop, boolean param);
void            P_SetByte(int type, uint index, uint prop, byte param);
void            P_SetInt(int type, uint index, uint prop, int param);
void            P_SetFixed(int type, uint index, uint prop, fixed_t param);
void            P_SetAngle(int type, uint index, uint prop, angle_t param);
void            P_SetFloat(int type, uint index, uint prop, float param);
void            P_SetPtr(int type, uint index, uint prop, void* param);

void            P_SetBoolv(int type, uint index, uint prop, boolean* params);
void            P_SetBytev(int type, uint index, uint prop, byte* params);
void            P_SetIntv(int type, uint index, uint prop, int* params);
void            P_SetFixedv(int type, uint index, uint prop, fixed_t* params);
void            P_SetAnglev(int type, uint index, uint prop, angle_t* params);
void            P_SetFloatv(int type, uint index, uint prop, float* params);
void            P_SetPtrv(int type, uint index, uint prop, void* params);

/* pointer-based write functions */
void            P_SetBoolp(void* ptr, uint prop, boolean param);
void            P_SetBytep(void* ptr, uint prop, byte param);
void            P_SetIntp(void* ptr, uint prop, int param);
void            P_SetFixedp(void* ptr, uint prop, fixed_t param);
void            P_SetAnglep(void* ptr, uint prop, angle_t param);
void            P_SetFloatp(void* ptr, uint prop, float param);
void            P_SetPtrp(void* ptr, uint prop, void* param);

void            P_SetBoolpv(void* ptr, uint prop, boolean* params);
void            P_SetBytepv(void* ptr, uint prop, byte* params);
void            P_SetIntpv(void* ptr, uint prop, int* params);
void            P_SetFixedpv(void* ptr, uint prop, fixed_t* params);
void            P_SetAnglepv(void* ptr, uint prop, angle_t* params);
void            P_SetFloatpv(void* ptr, uint prop, float* params);
void            P_SetPtrpv(void* ptr, uint prop, void* params);

/* index-based read functions */
boolean         P_GetBool(int type, uint index, uint prop);
byte            P_GetByte(int type, uint index, uint prop);
int             P_GetInt(int type, uint index, uint prop);
fixed_t         P_GetFixed(int type, uint index, uint prop);
angle_t         P_GetAngle(int type, uint index, uint prop);
float           P_GetFloat(int type, uint index, uint prop);
void*           P_GetPtr(int type, uint index, uint prop);

void            P_GetBoolv(int type, uint index, uint prop, boolean* params);
void            P_GetBytev(int type, uint index, uint prop, byte* params);
void            P_GetIntv(int type, uint index, uint prop, int* params);
void            P_GetFixedv(int type, uint index, uint prop, fixed_t* params);
void            P_GetAnglev(int type, uint index, uint prop, angle_t* params);
void            P_GetFloatv(int type, uint index, uint prop, float* params);
void            P_GetPtrv(int type, uint index, uint prop, void* params);

/* pointer-based read functions */
boolean         P_GetBoolp(void* ptr, uint prop);
byte            P_GetBytep(void* ptr, uint prop);
int             P_GetIntp(void* ptr, uint prop);
fixed_t         P_GetFixedp(void* ptr, uint prop);
angle_t         P_GetAnglep(void* ptr, uint prop);
float           P_GetFloatp(void* ptr, uint prop);
void*           P_GetPtrp(void* ptr, uint prop);

void            P_GetBoolpv(void* ptr, uint prop, boolean* params);
void            P_GetBytepv(void* ptr, uint prop, byte* params);
void            P_GetIntpv(void* ptr, uint prop, int* params);
void            P_GetFixedpv(void* ptr, uint prop, fixed_t* params);
void            P_GetAnglepv(void* ptr, uint prop, angle_t* params);
void            P_GetFloatpv(void* ptr, uint prop, float* params);
void            P_GetPtrpv(void* ptr, uint prop, void* params);

uint            P_CountGameMapObjs(int identifier);
byte            P_GetGMOByte(int identifier, uint elmIdx, int propIdentifier);
short           P_GetGMOShort(int identifier, uint elmIdx, int propIdentifier);
int             P_GetGMOInt(int identifier, uint elmIdx, int propIdentifier);
fixed_t         P_GetGMOFixed(int identifier, uint elmIdx, int propIdentifier);
angle_t         P_GetGMOAngle(int identifier, uint elmIdx, int propIdentifier);
float           P_GetGMOFloat(int identifier, uint elmIdx, int propIdentifier);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __DOOMSDAY_WORLD_H__
