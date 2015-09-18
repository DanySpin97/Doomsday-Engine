/** @file plugindriver.cpp  Plugin based audio driver.
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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

#include "audio/drivers/plugindriver.h"

#include "api_audiod.h"
#include "audio/sound.h"
#include <de/Library>
#include <de/Log>
#include <de/NativeFile>

using namespace de;

namespace audio {

PluginDriver::CdPlayer::CdPlayer(PluginDriver &driver) : ICdPlayer(driver)
{
    de::zap(_imp);
}

String PluginDriver::CdPlayer::name() const
{
    char buf[256];  /// @todo This could easily overflow...
    DENG2_ASSERT(_imp.gen.Get);
    if(_imp.gen.Get(MUSIP_ID, buf)) return buf;

    DENG2_ASSERT(!"[MUSIP_ID not defined]");
    return "unnamed_music";
}

dint PluginDriver::CdPlayer::init()
{
    DENG2_ASSERT(_imp.gen.Init);
    return _imp.gen.Init();
}

void PluginDriver::CdPlayer::shutdown()
{
    DENG2_ASSERT(_imp.gen.Shutdown);
    _imp.gen.Shutdown();
}

void PluginDriver::CdPlayer::update()
{
    DENG2_ASSERT(_imp.gen.Update);
    _imp.gen.Update();
}

void PluginDriver::CdPlayer::setVolume(dfloat newVolume)
{
    DENG2_ASSERT(_imp.gen.Set);
    _imp.gen.Set(MUSIP_VOLUME, newVolume);
}

bool PluginDriver::CdPlayer::isPlaying() const
{
    DENG2_ASSERT(_imp.gen.Get);
    return _imp.gen.Get(MUSIP_PLAYING, nullptr);
}

void PluginDriver::CdPlayer::pause(dint pause)
{
    DENG2_ASSERT(_imp.gen.Pause);
    _imp.gen.Pause(pause);
}

void PluginDriver::CdPlayer::stop()
{
    DENG2_ASSERT(_imp.gen.Stop);
    _imp.gen.Stop();
}

dint PluginDriver::CdPlayer::play(dint track, dint looped)
{
    DENG2_ASSERT(_imp.Play);
    return _imp.Play(track, looped);
}

// ----------------------------------------------------------------------------------

PluginDriver::MusicPlayer::MusicPlayer(PluginDriver &driver) : IMusicPlayer(driver)
{
    de::zap(_imp);
}

String PluginDriver::MusicPlayer::name() const
{
    char buf[256];  /// @todo This could easily overflow...
    DENG2_ASSERT(_imp.gen.Get);
    if(_imp.gen.Get(MUSIP_ID, buf)) return buf;

    DENG2_ASSERT(!"[MUSIP_ID not defined]");
    return "unnamed_music";
}

dint PluginDriver::MusicPlayer::init()
{
    DENG2_ASSERT(_imp.gen.Init);
    return _imp.gen.Init();
}

void PluginDriver::MusicPlayer::shutdown()
{
    DENG2_ASSERT(_imp.gen.Shutdown);
    _imp.gen.Shutdown();
}

void PluginDriver::MusicPlayer::update()
{
    DENG2_ASSERT(_imp.gen.Update);
    _imp.gen.Update();
}

void PluginDriver::MusicPlayer::setVolume(dfloat newVolume)
{
    if(!_initialized) return;
    DENG2_ASSERT(_imp.gen.Set);
    _imp.gen.Set(MUSIP_VOLUME, newVolume);
}

bool PluginDriver::MusicPlayer::isPlaying() const
{
    if(!_initialized) return false;
    DENG2_ASSERT(_imp.gen.Get);
    return _imp.gen.Get(MUSIP_PLAYING, nullptr);
}

void PluginDriver::MusicPlayer::pause(dint pause)
{
    DENG2_ASSERT(_imp.gen.Pause);
    _imp.gen.Pause(pause);
}

void PluginDriver::MusicPlayer::stop()
{
    DENG2_ASSERT(_imp.gen.Stop);
    _imp.gen.Stop();
}

bool PluginDriver::MusicPlayer::canPlayBuffer() const
{
    return _imp.Play != nullptr && _imp.SongBuffer != nullptr;
}

void *PluginDriver::MusicPlayer::songBuffer(duint length)
{
    if(!_imp.SongBuffer) return nullptr;
    return _imp.SongBuffer(length);
}

dint PluginDriver::MusicPlayer::play(dint looped)
{
    if(!_imp.Play) return false;
    return _imp.Play(looped);
}

bool PluginDriver::MusicPlayer::canPlayFile() const
{
    return _imp.PlayFile != nullptr;
}

dint PluginDriver::MusicPlayer::playFile(char const *filename, dint looped)
{
    if(!_imp.PlayFile) return false;
    return _imp.PlayFile(filename, looped);
}

// ----------------------------------------------------------------------------------

PluginDriver::SoundPlayer::SoundPlayer(PluginDriver &driver) : ISoundPlayer(driver)
{
    de::zap(_imp);
}

String PluginDriver::SoundPlayer::name() const
{
    /// @todo SFX interfaces aren't named yet.
    return "unnamed_sfx";
}

dint PluginDriver::SoundPlayer::init()
{
    DENG2_ASSERT(_imp.gen.Init);
    return _imp.gen.Init();
}

bool PluginDriver::SoundPlayer::anyRateAccepted() const
{
    dint anyRateAccepted = 0;
    if(_imp.gen.Getv) _imp.gen.Getv(SFXIP_ANY_SAMPLE_RATE_ACCEPTED, &anyRateAccepted);
    return CPP_BOOL( anyRateAccepted );
}

sfxbuffer_t *PluginDriver::SoundPlayer::create(dint flags, dint bits, dint rate)
{
    DENG2_ASSERT(_imp.gen.Create);
    return _imp.gen.Create(flags, bits, rate);
}

Sound *PluginDriver::SoundPlayer::makeSound(bool stereoPositioning, dint bitsPer, dint rate)
{
    std::unique_ptr<Sound> sound(new Sound(*this));
    sound->setBuffer(create(stereoPositioning ? 0 : SFXBF_3D, bitsPer, rate));
    return sound.release();
}

void PluginDriver::SoundPlayer::destroy(sfxbuffer_t &buf)
{
    DENG2_ASSERT(_imp.gen.Destroy);
    _imp.gen.Destroy(&buf);
}

void PluginDriver::SoundPlayer::load(sfxbuffer_t &buf, sfxsample_t &sample)
{
    DENG2_ASSERT(_imp.gen.Load);
    _imp.gen.Load(&buf, &sample);
}

void PluginDriver::SoundPlayer::stop(sfxbuffer_t &buf)
{
    DENG2_ASSERT(_imp.gen.Stop);
    _imp.gen.Stop(&buf);
}

void PluginDriver::SoundPlayer::reset(sfxbuffer_t &buf)
{
    DENG2_ASSERT(_imp.gen.Reset);
    _imp.gen.Reset(&buf);
}

void PluginDriver::SoundPlayer::play(sfxbuffer_t &buf)
{
    DENG2_ASSERT(_imp.gen.Play);
    _imp.gen.Play(&buf);
}

bool PluginDriver::SoundPlayer::isPlaying(sfxbuffer_t &buf) const
{
    return (buf.flags & SFXBF_PLAYING) != 0;
}

bool PluginDriver::SoundPlayer::needsRefresh() const
{
    dint disableRefresh = false;
    if(_imp.gen.Getv) _imp.gen.Getv(SFXIP_DISABLE_CHANNEL_REFRESH, &disableRefresh);
    return !disableRefresh;
}

void PluginDriver::SoundPlayer::refresh(sfxbuffer_t &buf)
{
    DENG2_ASSERT(_imp.gen.Refresh);
    _imp.gen.Refresh(&buf);
}

void PluginDriver::SoundPlayer::setFrequency(sfxbuffer_t &buf, dfloat newFrequency)
{
    DENG2_ASSERT(_imp.gen.Set);
    _imp.gen.Set(&buf, SFXBP_FREQUENCY, newFrequency);
}

void PluginDriver::SoundPlayer::setOrigin(sfxbuffer_t &buf, Vector3d const &newOrigin)
{
    DENG2_ASSERT(_imp.gen.Setv);
    dfloat vec[3]; newOrigin.toVector3f().decompose(vec);
    _imp.gen.Setv(&buf, SFXBP_POSITION, vec);
}

void PluginDriver::SoundPlayer::setPan(sfxbuffer_t &buf, dfloat newPan)
{
    DENG2_ASSERT(_imp.gen.Set);
    _imp.gen.Set(&buf, SFXBP_PAN, newPan);
}

void PluginDriver::SoundPlayer::setPositioning(sfxbuffer_t &buf, bool headRelative)
{
    DENG2_ASSERT(_imp.gen.Set);
    _imp.gen.Set(&buf, SFXBP_RELATIVE_MODE, dfloat( headRelative ));
}

void PluginDriver::SoundPlayer::setVelocity(sfxbuffer_t &buf, Vector3d const &newVelocity)
{
    DENG2_ASSERT(_imp.gen.Setv);
    dfloat vec[3]; newVelocity.toVector3f().decompose(vec);
    _imp.gen.Setv(&buf, SFXBP_VELOCITY, vec);
}

void PluginDriver::SoundPlayer::setVolume(sfxbuffer_t &buf, dfloat newVolume)
{
    DENG2_ASSERT(_imp.gen.Set);
    _imp.gen.Set(&buf, SFXBP_VOLUME, newVolume);
}

void PluginDriver::SoundPlayer::setVolumeAttenuationRange(sfxbuffer_t &buf, Ranged const &newRange)
{
    DENG2_ASSERT(_imp.gen.Set);
    _imp.gen.Set(&buf, SFXBP_MIN_DISTANCE, dfloat( newRange.start ));
    _imp.gen.Set(&buf, SFXBP_MAX_DISTANCE, dfloat( newRange.end ));
}

void PluginDriver::SoundPlayer::listener(dint prop, dfloat value)
{
    DENG2_ASSERT(_imp.gen.Listener);
    _imp.gen.Listener(prop, value);
}

void PluginDriver::SoundPlayer::listenerv(dint prop, dfloat *values)
{
    DENG2_ASSERT(_imp.gen.Listenerv);
    _imp.gen.Listenerv(prop, values);
}

// ----------------------------------------------------------------------------------

DENG2_PIMPL(PluginDriver)
, DENG2_OBSERVES(audio::System, FrameBegins)
, DENG2_OBSERVES(audio::System, FrameEnds)
, DENG2_OBSERVES(audio::System, MidiFontChange)
{
    bool initialized   = false;
    ::Library *library = nullptr;  ///< Library instance (owned).

    /// @todo Extract this into a (base) Plugin class. -ds
    struct IPlugin
    {
        int (*Init) (void);
        void (*Shutdown) (void);
        void (*Event) (int type);
        int (*Get) (int prop, void *ptr);
        int (*Set) (int prop, void const *ptr);

        IPlugin() { de::zapPtr(this); }
    } iBase;

    CdPlayer iCd;
    MusicPlayer iMusic;
    SoundPlayer iSfx;

    Instance(Public *i)
        : Base(i)
        , iCd   (self)
        , iMusic(self)
        , iSfx  (self)
    {}

    ~Instance()
    {
        // Should have been deinitialized by now.
        DENG2_ASSERT(!initialized);

        // Unload the library.
        Library_Delete(library);
    }

    /**
     * Lookup the value of a named @em string property from the driver.
     */
    String getPropertyAsString(dint prop)
    {
        DENG2_ASSERT(iBase.Get);
        ddstring_t str; Str_InitStd(&str);
        if(iBase.Get(prop, &str))
        {
            auto string = String(Str_Text(&str));
            Str_Free(&str);
            return string;
        }
        /// @throw ReadPropertyError  Driver returned not successful.
        throw ReadPropertyError("audio::PluginDriver::Instance::getPropertyAsString", "Error reading property:" + String::number(prop));
    }

    void systemFrameBegins(audio::System &)
    {
        DENG2_ASSERT(initialized);
        if(iSfx._imp.gen.Init)   iBase.Event(SFXEV_BEGIN);
        if(iCd._imp.gen.Init)    iCd.update();
        if(iMusic._imp.gen.Init) iMusic.update();
    }

    void systemFrameEnds(audio::System &)
    {
        DENG2_ASSERT(initialized);
        iBase.Event(SFXEV_END);
    }

    void systemMidiFontChanged(String const &newMidiFontPath)
    {
        DENG2_ASSERT(initialized);
        DENG2_ASSERT(iBase.Set);
        iBase.Set(AUDIOP_SOUNDFONT_FILENAME, newMidiFontPath.toLatin1().constData());
    }
};

PluginDriver::PluginDriver() : d(new Instance(this))
{}

PluginDriver::~PluginDriver()
{
    LOG_AS("~audio::PluginDriver");
    deinitialize();  // If necessary.
}

PluginDriver *PluginDriver::newFromLibrary(LibraryFile &libFile)  // static
{
    try
    {
        std::unique_ptr<PluginDriver> driver(new PluginDriver);
        PluginDriver &inst = *driver;

        inst.d->library = Library_New(libFile.path().toUtf8().constData());
        if(!inst.d->library)
        {
            /// @todo fixme: Should not be failing here! -ds
            return nullptr;
        }

        de::Library &lib = libFile.library();

        lib.setSymbolPtr( inst.d->iBase.Init,     "DS_Init");
        lib.setSymbolPtr( inst.d->iBase.Shutdown, "DS_Shutdown");
        lib.setSymbolPtr( inst.d->iBase.Event,    "DS_Event");
        lib.setSymbolPtr( inst.d->iBase.Get,      "DS_Get");
        lib.setSymbolPtr( inst.d->iBase.Set,      "DS_Set", de::Library::OptionalSymbol);

        if(lib.hasSymbol("DS_SFX_Init"))
        {
            lib.setSymbolPtr( inst.d->iSfx._imp.gen.Init,      "DS_SFX_Init");
            lib.setSymbolPtr( inst.d->iSfx._imp.gen.Create,    "DS_SFX_CreateBuffer");
            lib.setSymbolPtr( inst.d->iSfx._imp.gen.Destroy,   "DS_SFX_DestroyBuffer");
            lib.setSymbolPtr( inst.d->iSfx._imp.gen.Load,      "DS_SFX_Load");
            lib.setSymbolPtr( inst.d->iSfx._imp.gen.Reset,     "DS_SFX_Reset");
            lib.setSymbolPtr( inst.d->iSfx._imp.gen.Play,      "DS_SFX_Play");
            lib.setSymbolPtr( inst.d->iSfx._imp.gen.Stop,      "DS_SFX_Stop");
            lib.setSymbolPtr( inst.d->iSfx._imp.gen.Refresh,   "DS_SFX_Refresh");
            lib.setSymbolPtr( inst.d->iSfx._imp.gen.Set,       "DS_SFX_Set");
            lib.setSymbolPtr( inst.d->iSfx._imp.gen.Setv,      "DS_SFX_Setv");
            lib.setSymbolPtr( inst.d->iSfx._imp.gen.Listener,  "DS_SFX_Listener");
            lib.setSymbolPtr( inst.d->iSfx._imp.gen.Listenerv, "DS_SFX_Listenerv");
            lib.setSymbolPtr( inst.d->iSfx._imp.gen.Getv,      "DS_SFX_Getv", de::Library::OptionalSymbol);
        }

        if(lib.hasSymbol("DM_Music_Init"))
        {
            lib.setSymbolPtr( inst.d->iMusic._imp.gen.Init,    "DM_Music_Init");
            lib.setSymbolPtr( inst.d->iMusic._imp.gen.Update,  "DM_Music_Update");
            lib.setSymbolPtr( inst.d->iMusic._imp.gen.Get,     "DM_Music_Get");
            lib.setSymbolPtr( inst.d->iMusic._imp.gen.Set,     "DM_Music_Set");
            lib.setSymbolPtr( inst.d->iMusic._imp.gen.Pause,   "DM_Music_Pause");
            lib.setSymbolPtr( inst.d->iMusic._imp.gen.Stop,    "DM_Music_Stop");
            lib.setSymbolPtr( inst.d->iMusic._imp.SongBuffer,  "DM_Music_SongBuffer", de::Library::OptionalSymbol);
            lib.setSymbolPtr( inst.d->iMusic._imp.Play,        "DM_Music_Play",       de::Library::OptionalSymbol);
            lib.setSymbolPtr( inst.d->iMusic._imp.PlayFile,    "DM_Music_PlayFile",   de::Library::OptionalSymbol);
        }

        if(lib.hasSymbol("DM_CDAudio_Init"))
        {
            lib.setSymbolPtr( inst.d->iCd._imp.gen.Init,       "DM_CDAudio_Init");
            lib.setSymbolPtr( inst.d->iCd._imp.gen.Update,     "DM_CDAudio_Update");
            lib.setSymbolPtr( inst.d->iCd._imp.gen.Set,        "DM_CDAudio_Set");
            lib.setSymbolPtr( inst.d->iCd._imp.gen.Get,        "DM_CDAudio_Get");
            lib.setSymbolPtr( inst.d->iCd._imp.gen.Pause,      "DM_CDAudio_Pause");
            lib.setSymbolPtr( inst.d->iCd._imp.gen.Stop,       "DM_CDAudio_Stop");
            lib.setSymbolPtr( inst.d->iCd._imp.Play,           "DM_CDAudio_Play");
        }

        return driver.release();
    }
    catch(de::Library::SymbolMissingError const &er)
    {
        LOG_AS("audio::PluginDriver");
        LOG_AUDIO_ERROR("") << er.asText();
    }
    return nullptr;
}

bool PluginDriver::recognize(LibraryFile &library)  // static
{
    // By convention, driver plugin names use a standard prefix.
    if(!library.name().beginsWith("audio_")) return false;

    // Driver plugins are native files.
    if(!library.source()->is<NativeFile>()) return false;

    // This appears to be usuable with PluginDriver.
    /// @todo Open the library and ensure it's type matches.
    return true;
}

String PluginDriver::identityKey() const
{
    return d->getPropertyAsString(AUDIOP_IDENTITYKEY).toLower();
}

String PluginDriver::title() const
{
    return d->getPropertyAsString(AUDIOP_TITLE);
}

audio::System::IDriver::Status PluginDriver::status() const
{
    if(d->initialized) return Initialized;
    DENG2_ASSERT(d->iBase.Init != nullptr);
    return Loaded;
}

void PluginDriver::initialize()
{
    LOG_AS("audio::PluginDriver");

    // Already been here?
    if(d->initialized) return;

    DENG2_ASSERT(d->iBase.Init != nullptr);
    d->initialized = d->iBase.Init();

    // We want notification at various times:
    audioSystem().audienceForFrameBegins() += d;
    audioSystem().audienceForFrameEnds()   += d;
    if(d->iBase.Set)
    {
        audioSystem().audienceForMidiFontChange() += d;
    }
}

void PluginDriver::deinitialize()
{
    LOG_AS("audio::PluginDriver");

    // Already been here?
    if(!d->initialized) return;

    if(d->iBase.Shutdown)
    {
        d->iBase.Shutdown();
    }

    // Stop receiving notifications:
    if(d->iBase.Set)
    {
        audioSystem().audienceForMidiFontChange() -= d;
    }
    audioSystem().audienceForFrameEnds()   -= d;
    audioSystem().audienceForFrameBegins() -= d;

    d->initialized = false;
}

::Library *PluginDriver::library() const
{
    return d->library;
}

bool PluginDriver::hasCd() const
{
    return iCd().as<CdPlayer>()._imp.gen.Init != nullptr;
}

bool PluginDriver::hasMusic() const
{
    return iMusic().as<MusicPlayer>()._imp.gen.Init != nullptr;
}

bool PluginDriver::hasSfx() const
{
    return iSfx().as<SoundPlayer>()._imp.gen.Init != nullptr;
}

ICdPlayer /*const*/ &PluginDriver::iCd() const
{
    return d->iCd;
}

IMusicPlayer /*const*/ &PluginDriver::iMusic() const
{
    return d->iMusic;
}

ISoundPlayer /*const*/ &PluginDriver::iSfx() const
{
    return d->iSfx;
}

}  // namespace audio
