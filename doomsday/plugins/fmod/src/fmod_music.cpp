/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "driver_fmod.h"
#include <string.h>

struct SongBuffer
{
    int size;
    char* data;

    SongBuffer(int newSize) : size(newSize) {
        data = new char[newSize];
    }

    ~SongBuffer() {
        delete [] data;
    }
};

static FMOD::Sound* song;
static FMOD::Channel* music;
static float musicVolume;
static SongBuffer* songBuffer;
static const char* soundFontFileName;

static FMOD_RESULT F_CALLBACK musicCallback(FMOD_CHANNEL* chanPtr,
                                            FMOD_CHANNEL_CALLBACKTYPE type,
                                            void* /*commanddata1*/,
                                            void* /*commanddata2*/)
{
    assert(reinterpret_cast<FMOD::Channel*>(chanPtr) == music);

    switch(type)
    {
    case FMOD_CHANNEL_CALLBACKTYPE_END:
        // The music has stopped.
        music = 0;
        break;

    default:
        break;
    }
    return FMOD_OK;
}

static void releaseSong()
{
    if(song)
    {
        song->release();
        song = 0;
    }
    music = 0;
}

static void releaseSongBuffer()
{
    if(songBuffer)
    {
        delete songBuffer;
        songBuffer = 0;
    }
}

void setDefaultStreamBufferSize()
{
    if(!fmodSystem) return;

    FMOD_RESULT result;
    result = fmodSystem->setStreamBufferSize(16*1024, FMOD_TIMEUNIT_RAWBYTES);
    DSFMOD_ERRCHECK(result);
}

int DM_Music_Init(void)
{
    music = 0;
    song = 0;
    musicVolume = 1.f;
    songBuffer = 0;
    soundFontFileName = 0; // empty for the default
    return fmodSystem != 0;
}

void DM_Music_Shutdown(void)
{
    if(!fmodSystem) return;

    releaseSongBuffer();
    releaseSong();

    soundFontFileName = 0;

    // Will be shut down with the rest of FMOD.
    DSFMOD_TRACE("Music_Shutdown.");
}

void DM_Music_SetSoundFont(const char* fileName)
{
    soundFontFileName = fileName;
}

void DM_Music_Set(int prop, float value)
{
    if(!fmodSystem)
        return;

    switch(prop)
    {
    case MUSIP_VOLUME:
        musicVolume = value;
        if(music) music->setVolume(musicVolume);
        DSFMOD_TRACE("Music_Set: MUSIP_VOLUME = " << musicVolume);
        break;

    default:
        break;
    }
}

int DM_Music_Get(int prop, void* ptr)
{
    switch(prop)
    {
    case MUSIP_ID:
        if(ptr)
        {
            strcpy((char*) ptr, "FMOD/Ext");
            return true;
        }
        break;

    case MUSIP_PLAYING:
        if(!fmodSystem) return false;
        return music != 0; // NULL when not playing.

    default:
        break;
    }

    return false;
}

void DM_Music_Update(void)
{
    // No need to do anything. The callback handles restarting.
}

void DM_Music_Stop(void)
{
    if(!fmodSystem || !music) return;

    DSFMOD_TRACE("Music_Stop.");

    music->stop();
}

static bool startSong()
{
    if(!fmodSystem || !song) return false;

    if(music) music->stop();

    // Start playing the song.
    FMOD_RESULT result;
    result = fmodSystem->playSound(FMOD_CHANNEL_FREE, song, true, &music);
    DSFMOD_ERRCHECK(result);

    // Properties.
    music->setVolume(musicVolume);
    music->setCallback(musicCallback);

    // Start playing.
    music->setPaused(false);
    return true;
}

int DM_Music_Play(int looped)
{
    if(!fmodSystem) return false;

    if(songBuffer)
    {
        // Get rid of the old song.
        releaseSong();

        setDefaultStreamBufferSize();

        FMOD_CREATESOUNDEXINFO extra;
        zeroStruct(extra);
        extra.length = songBuffer->size;
        extra.dlsname = soundFontFileName;

        // Load a new song.
        FMOD_RESULT result;
        result = fmodSystem->createSound(songBuffer->data,
                                         FMOD_CREATESTREAM | FMOD_OPENMEMORY |
                                         (looped? FMOD_LOOP_NORMAL : 0),
                                         &extra, &song);
        DSFMOD_TRACE("Music_Play: songBuffer has " << songBuffer->size << " bytes, created Sound " << song);
        DSFMOD_ERRCHECK(result);

        // The song buffer remains in memory, in case FMOD needs to stream from it.
    }
    return startSong();
}

void DM_Music_Pause(int setPause)
{
    if(!fmodSystem || !music) return;

    music->setPaused(setPause != 0);
}

void* DM_Music_SongBuffer(unsigned int length)
{
    if(!fmodSystem) return NULL;

    releaseSongBuffer();

    DSFMOD_TRACE("Music_SongBuffer: Allocating a song buffer for " << length << " bytes.");

    // The caller will put data in this buffer. Before playing, we will create the
    // FMOD sound based on the data in the song buffer.
    songBuffer = new SongBuffer(length);
    return songBuffer->data;
}

int DM_Music_PlayFile(const char *filename, int looped)
{
    if(!fmodSystem) return false;

    // Get rid of the current song.
    releaseSong();
    releaseSongBuffer();

    setDefaultStreamBufferSize();

    FMOD_CREATESOUNDEXINFO extra;
    zeroStruct(extra);
    extra.dlsname = soundFontFileName;

    FMOD_RESULT result;
    result = fmodSystem->createSound(filename, FMOD_CREATESTREAM | (looped? FMOD_LOOP_NORMAL : 0),
                                     &extra, &song);
    DSFMOD_TRACE("Music_Play: loaded '" << filename << "' => Sound " << song);
    DSFMOD_ERRCHECK(result);

    return startSong();
}
