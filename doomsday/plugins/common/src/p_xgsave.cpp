/** @file p_xgsave.cpp  XG data/thinker (de)serialization.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__

#include "common.h"
#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_saveg.h"
#include "p_xg.h"

void SV_WriteXGLine(Line *li)
{
    xgline_t *xg     = P_ToXLine(li)->xg;
    linetype_t *info = &xg->info;

    // Version byte.
    SV_WriteByte(1);

    /*
     * Remember, savegames are applied on top of an initialized map. No strings are saved,
     * because they are all const strings defined either in the maps's DDXGDATA lump or a
     * DED file. During loading, XL_SetLineType is called with the id in the savegame.
     */

    SV_WriteLong(info->id);
    SV_WriteLong(info->actCount);

    SV_WriteByte(xg->active);
    SV_WriteByte(xg->disabled);
    SV_WriteLong(xg->timer);
    SV_WriteLong(xg->tickerTimer);
    SV_WriteShort(SV_ThingArchiveId((mobj_t *)xg->activator));
    SV_WriteLong(xg->idata);
    SV_WriteFloat(xg->fdata);
    SV_WriteLong(xg->chIdx);
    SV_WriteFloat(xg->chTimer);
}

void SV_ReadXGLine(Line *li)
{
    xline_t *xline = P_ToXLine(li);

    // Read version.
    SV_ReadByte();

    // This'll set all the correct string pointers and other data.
    XL_SetLineType(li, SV_ReadLong());

    DENG_ASSERT(xline != 0);
    DENG_ASSERT(xline->xg != 0);

    xgline_t *xg = xline->xg;

    xg->info.actCount = SV_ReadLong();

    xg->active      = SV_ReadByte();
    xg->disabled    = SV_ReadByte();
    xg->timer       = SV_ReadLong();
    xg->tickerTimer = SV_ReadLong();

    // Will be updated later.
    xg->activator   = INT2PTR(void, SV_ReadShort());

    xg->idata       = SV_ReadLong();
    xg->fdata       = SV_ReadFloat();
    xg->chIdx       = SV_ReadLong();
    xg->chTimer     = SV_ReadFloat();
}

/**
 * @param fn  This function must belong to XG sector @a xg.
 */
void SV_WriteXGFunction(xgsector_t *xg, function_t *fn)
{
    // Version byte.
    SV_WriteByte(1);

    SV_WriteLong(fn->flags);
    SV_WriteShort(fn->pos);
    SV_WriteShort(fn->repeat);
    SV_WriteShort(fn->timer);
    SV_WriteShort(fn->maxTimer);
    SV_WriteFloat(fn->value);
    SV_WriteFloat(fn->oldValue);
}

/**
 * @param fn  This function must belong to XG sector @a xg.
 */
void SV_ReadXGFunction(xgsector_t *xg, function_t *fn)
{
    // Version byte.
    SV_ReadByte();

    fn->flags    = SV_ReadLong();
    fn->pos      = SV_ReadShort();
    fn->repeat   = SV_ReadShort();
    fn->timer    = SV_ReadShort();
    fn->maxTimer = SV_ReadShort();
    fn->value    = SV_ReadFloat();
    fn->oldValue = SV_ReadFloat();
}

void SV_WriteXGSector(Sector *sec)
{
    xsector_t *xsec = P_ToXSector(sec);

    xgsector_t *xg     = xsec->xg;
    sectortype_t *info = &xg->info;

    // Version byte.
    SV_WriteByte(1);

    SV_WriteLong(info->id);
    SV_Write(info->count, sizeof(info->count));
    SV_Write(xg->chainTimer, sizeof(xg->chainTimer));
    SV_WriteLong(xg->timer);
    SV_WriteByte(xg->disabled);
    for(int i = 0; i < 3; ++i)
    {
        SV_WriteXGFunction(xg, &xg->rgb[i]);
    }
    for(int i = 0; i < 2; ++i)
    {
        SV_WriteXGFunction(xg, &xg->plane[i]);
    }
    SV_WriteXGFunction(xg, &xg->light);
}

void SV_ReadXGSector(Sector *sec)
{
    xsector_t *xsec = P_ToXSector(sec);

    // Version byte.
    SV_ReadByte();

    // This'll init all the data.
    XS_SetSectorType(sec, SV_ReadLong());

    xgsector_t *xg = xsec->xg;
    SV_Read(xg->info.count, sizeof(xg->info.count));
    SV_Read(xg->chainTimer, sizeof(xg->chainTimer));
    xg->timer    = SV_ReadLong();
    xg->disabled = SV_ReadByte();
    for(int i = 0; i < 3; ++i)
    {
        SV_ReadXGFunction(xg, &xg->rgb[i]);
    }
    for(int i = 0; i < 2; ++i)
    {
        SV_ReadXGFunction(xg, &xg->plane[i]);
    }
    SV_ReadXGFunction(xg, &xg->light);
}

void xgplanemover_t::write(Writer *writer) const
{
    Writer_WriteByte(writer, 3); // Version.

    Writer_WriteInt32(writer, P_ToIndex(sector));
    Writer_WriteByte(writer, ceiling);
    Writer_WriteInt32(writer, flags);

    int i = P_ToIndex(origin);
    if(i >= 0 && i < numlines) // Is it a real line?
        i++;
    else // No.
        i = 0;

    Writer_WriteInt32(writer, i); // Zero means there is no origin.

    Writer_WriteInt32(writer, FLT2FIX(destination));
    Writer_WriteInt32(writer, FLT2FIX(speed));
    Writer_WriteInt32(writer, FLT2FIX(crushSpeed));
    Writer_WriteInt32(writer, MaterialArchive_FindUniqueSerialId(SV_MaterialArchive(), setMaterial));
    Writer_WriteInt32(writer, setSectorType);
    Writer_WriteInt32(writer, startSound);
    Writer_WriteInt32(writer, endSound);
    Writer_WriteInt32(writer, moveSound);
    Writer_WriteInt32(writer, minInterval);
    Writer_WriteInt32(writer, maxInterval);
    Writer_WriteInt32(writer, timer);
}

int xgplanemover_t::read(Reader *reader, int mapVersion)
{
    byte ver = Reader_ReadByte(reader); // Version.

    sector      = (Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader));
    ceiling     = Reader_ReadByte(reader);
    flags       = Reader_ReadInt32(reader);

    int lineIndex = SV_ReadLong();
    if(lineIndex > 0)
        origin  = (Line *)P_ToPtr(DMU_LINE, lineIndex - 1);

    destination = FIX2FLT(Reader_ReadInt32(reader));
    speed       = FIX2FLT(Reader_ReadInt32(reader));
    crushSpeed  = FIX2FLT(Reader_ReadInt32(reader));

    if(ver >= 3)
    {
        setMaterial = SV_GetArchiveMaterial(Reader_ReadInt32(reader), 0);
    }
    else
    {
        // Flat number is an absolute lump index.
        Uri *uri = Uri_NewWithPath2("Flats:", RC_NULL);
        ddstring_t name; Str_Init(&name);
        F_FileName(&name, Str_Text(W_LumpName(Reader_ReadInt32(reader))));
        Uri_SetPath(uri, Str_Text(&name));
        setMaterial = (Material *)P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
        Uri_Delete(uri);
        Str_Free(&name);
    }

    setSectorType = Reader_ReadInt32(reader);
    startSound    = Reader_ReadInt32(reader);
    endSound      = Reader_ReadInt32(reader);
    moveSound     = Reader_ReadInt32(reader);
    minInterval   = Reader_ReadInt32(reader);
    maxInterval   = Reader_ReadInt32(reader);
    timer         = Reader_ReadInt32(reader);

    thinker.function = (thinkfunc_t) XS_PlaneMover;

    return true; // Add this thinker.
}

#endif
