/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * gl_png.c: PNG Images
 *
 * Portable Network Graphics high-level handling.
 * You'll need libpng and zlib.
 */

// HEADER FILES ------------------------------------------------------------

#include <png.h>
#include <setjmp.h>

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void PNGAPI user_error_fn(png_structp png_ptr, png_const_charp error_msg)
{
    Con_Message("PNG-Error: %s\n", error_msg);
}

void PNGAPI user_warning_fn(png_structp png_ptr, png_const_charp warning_msg)
{
    VERBOSE(Con_Message("PNG-Warning: %s\n", warning_msg));
}

/**
 * libpng calls this to read from files.
 */
void PNGAPI my_read_data(png_structp read_ptr, png_bytep data,
                         png_size_t length)
{
    F_Read(data, length, png_get_io_ptr(read_ptr));
}

/**
 * Reads the given PNG image and returns a pointer to a planar RGB or
 * RGBA buffer. Width and height are set, and pixelSize is either 3 (RGB)
 * or 4 (RGBA). The caller must free the allocated buffer with Z_Free.
 * width, height and pixelSize can't be NULL. Handles 1-4 channels.
 */
unsigned char *PNG_Load(const char *fileName, int *width, int *height,
                        int *pixelSize)
{
    DFILE      *file;
    png_structp png_ptr = 0;
    png_infop   png_info = 0, end_info = 0;
    png_bytep  *rows, pixel;
    unsigned char *retbuf = 0;  // The return buffer.
    int         i, k, off;

    if((file = F_Open(fileName, "rb")) == NULL)
        return NULL;

    // Init libpng.
    png_ptr =
        png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, user_error_fn,
                               user_warning_fn);
    if(png_ptr)
    {
        png_info = png_create_info_struct(png_ptr);
        if(png_info)
        {
            end_info = png_create_info_struct(png_ptr);
            if(end_info)
            {
                if(!setjmp(png_jmpbuf(png_ptr)))
                {
                    boolean     canLoad;

                    //png_init_io(png_ptr, file);
                    png_set_read_fn(png_ptr, file, my_read_data);

                    png_read_png(png_ptr, png_info,
                                 PNG_TRANSFORM_IDENTITY, NULL);

                    // Check if it can be used.
                    canLoad = true;
                    if(png_get_bit_depth(png_ptr, png_info) != 8)
                    {
                        Con_Message("PNG_Load: \"%s\": Bit depth must be 8.\n", fileName);
                        canLoad = false;
                    }
                    else if(!png_get_image_width(png_ptr, png_info) || !png_get_image_height(png_ptr, png_info))
                    {
                        Con_Message("PNG_Load: \"%s\": Bad file? Size is zero.\n", fileName);
                        canLoad = false;
                    }
                    else if(png_get_channels(png_ptr, png_info) <= 2 &&
                            png_get_color_type(png_ptr, png_info) == PNG_COLOR_TYPE_PALETTE &&
                            !png_get_valid(png_ptr, png_info, PNG_INFO_PLTE))
                    {
                        Con_Message("PNG_Load: \"%s\": Palette is invalid.\n", fileName);
                        canLoad = false;
                    }

                    if(canLoad)
                    {
                        // Information about the image.
                        *width = png_get_image_width(png_ptr, png_info);
                        *height = png_get_image_height(png_ptr, png_info);
                        *pixelSize = png_get_channels(png_ptr, png_info);

                        // Paletted images have three color components
                        // per pixel.
                        if(*pixelSize == 1)
                            *pixelSize = 3;
                        if(*pixelSize == 2)
                            *pixelSize = 4;       // With alpha channel.

                        // OK, let's copy it into Doomsday's buffer.
                        // \fixme Why not load directly into it?
                        retbuf = M_Malloc(4 * png_get_image_width(png_ptr, png_info) *
                                          png_get_image_height(png_ptr, png_info));
                        rows = png_get_rows(png_ptr, png_info);
                        for(i = 0; i < *height; ++i)
                        {
                            if(png_get_channels(png_ptr, png_info) >= 3)
                            {
                                memcpy(retbuf + i * (*pixelSize) * (*width),
                                       rows[i], (*pixelSize) * (*width));
                            }
                            else // Paletted image.
                            {
                                // Get the palette.
                                png_colorp palette = 0;
                                int numPalette = 0;
                                png_get_PLTE(png_ptr, png_info, &palette, &numPalette);

                                for(k = 0; k < *width; ++k)
                                {
                                    pixel = retbuf + ((*pixelSize) * (i * (*width) + k));
                                    off = k * png_get_channels(png_ptr, png_info);
                                    if(png_get_color_type(png_ptr, png_info) == PNG_COLOR_TYPE_PALETTE)
                                    {
#ifdef RANGECHECK
                                        assert(rows[i][off] < numPalette);
#endif
                                        pixel[0] = palette[rows[i][off]].red;
                                        pixel[1] = palette[rows[i][off]].green;
                                        pixel[2] = palette[rows[i][off]].blue;
                                    }
                                    else
                                    {
                                        // Grayscale.
                                        pixel[0] = pixel[1] = pixel[2] = rows[i][off];
                                    }
                                    if(png_info->channels == 2) // Alpha data.
                                    {
                                        pixel[3] = rows[i][off + 1];
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Shutdown.
    png_destroy_read_struct(&png_ptr, &png_info, &end_info);
    F_Close(file);
    return retbuf;
}
