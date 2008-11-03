/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_anim.c: Flat and Texture animations, parsed from ANIMDEFS.
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "p_mapsetup.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void parseAnimGroup(materialgroup_t group)
{
    boolean             ignore;
    boolean             done;
    int                 groupNumber = 0;
    materialnum_t       matNumBase = 0, lumpNumBase = 0;

    if(!(group == MG_FLATS || group == MG_TEXTURES))
        Con_Error("parseAnimGroup: Internal Error, invalid material group %i.",
                  (int) group);

    if(!SC_GetString()) // Name.
    {
        SC_ScriptError("Missing string.");
    }

    ignore = true;
    if(group == MG_TEXTURES)
    {
        if((matNumBase = R_MaterialCheckNumForName(sc_String,
                                                   MG_TEXTURES)) != 0)
            ignore = false;
    }
    else
    {
        if((lumpNumBase = W_CheckNumForName(sc_String)) != -1)
            ignore = false;
    }

    if(!ignore)
        groupNumber = R_CreateAnimGroup(AGF_SMOOTH | AGF_FIRST_ONLY);

    done = false;
    do
    {
        if(SC_GetString())
        {
            if(SC_Compare("pic"))
            {
                int                 picNum, min, max = 0;

                SC_MustGetNumber();
                picNum = sc_Number;

                SC_MustGetString();
                if(SC_Compare("tics"))
                {
                    SC_MustGetNumber();
                    min = sc_Number;
                }
                else if(SC_Compare("rand"))
                {
                    SC_MustGetNumber();
                    min = sc_Number;
                    SC_MustGetNumber();
                    max = sc_Number;
                }
                else
                {
                    SC_ScriptError(NULL);
                }

                if(!ignore)
                {
                    if(group == MG_TEXTURES)
                    {
                        /**
                         * \fixme Here an assumption is made that
                         * MG_TEXTURES type materials are registered in the
                         * same order as they are defined in the
                         * TEXTURE(1...) lump(s).
                         */
                        R_AddToAnimGroup(groupNumber, matNumBase + picNum - 1,
                                         min, (max > 0? max - min : 0));
                    }
                    else
                    {
                        materialnum_t       frame =
                            R_MaterialCheckNumForName(W_LumpName(lumpNumBase + picNum - 1),
                                                      MG_FLATS);

                        R_AddToAnimGroup(groupNumber, frame,
                                         min, (max > 0? max - min : 0));
                    }
                }
            }
            else
            {
                SC_UnGet();
                done = true;
            }
        }
        else
        {
            done = true;
        }
    } while(!done);
}

/**
 * Parse an ANIMDEFS definition for flat/texture animations.
 */
void P_InitPicAnims(void)
{
    lumpnum_t       lump = W_CheckNumForName("ANIMDEFS");

    if(lump != -1)
    {
        SC_OpenLump(lump);

        while(SC_GetString())
        {
            if(SC_Compare("flat"))
            {
                parseAnimGroup(MG_FLATS);
            }
            else if(SC_Compare("texture"))
            {
                parseAnimGroup(MG_TEXTURES);
            }
            else
            {
                SC_ScriptError(NULL);
            }
        }

        SC_Close();
    }
}
