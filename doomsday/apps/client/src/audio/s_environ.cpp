/** @file s_environ.cpp  Environmental audio effects.
 *
 * Calculation of the aural properties of sectors.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
#include "audio/s_environ.h"

#include "Sector"

using namespace de;

static AudioEnvironment envInfo[1 + NUM_AUDIO_ENVIRONMENTS] = {
    { "",          0,       0,      0   },
    { "Metal",     255,     255,    25  },
    { "Rock",      200,     160,    100 },
    { "Wood",      80,      50,     200 },
    { "Cloth",     5,       5,      255 }
};

char const *S_AudioEnvironmentName(AudioEnvironmentId id)
{
    DENG2_ASSERT(id >= AE_NONE && id < NUM_AUDIO_ENVIRONMENTS);
    return ::envInfo[1 + dint( id )].name;
}

AudioEnvironment const &S_AudioEnvironment(AudioEnvironmentId id)
{
    DENG2_ASSERT(id >= AE_NONE && id < NUM_AUDIO_ENVIRONMENTS);
    return ::envInfo[1 + dint( id )];
}

AudioEnvironmentId S_AudioEnvironmentId(de::Uri const *uri)
{
    if(uri)
    {
        for(dint i = 0; i < DED_Definitions()->textureEnv.size(); ++i)
        {
            ded_tenviron_t const *env = &DED_Definitions()->textureEnv[i];
            for(dint k = 0; k < env->materials.size(); ++k)
            {
                de::Uri *ref = env->materials[k].uri;
                if(!ref || *ref != *uri) continue;

                // Is this a known environment?
                for(dint m = 0; m < NUM_AUDIO_ENVIRONMENTS; ++m)
                {
                    AudioEnvironment const &envInfo = S_AudioEnvironment(AudioEnvironmentId(m));
                    if(!qstricmp(env->id, envInfo.name))
                        return AudioEnvironmentId(m);
                }
                return AE_NONE;
            }
        }
    }
    return AE_NONE;
}
