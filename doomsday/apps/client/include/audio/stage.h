/** @file stage.h  Logically independent audio context (a.k.a., "soundstage").
 * @ingroup audio
 *
 * @authors Copyright � 2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef CLIENT_AUDIO_STAGE_H
#define CLIENT_AUDIO_STAGE_H

#ifdef __SERVER__
#  error "audio" is not available in a SERVER build
#endif

#include "audio/sound.h"
#include <de/Observers>
#include <functional>

namespace audio {

class Listener;

/**
 * Context for logically independent concurrent audio playback (a.k.a., soundstage).
 *
 * Marshalls Sound playback within the context on a purely logical level and enforces
 * specific play/stop/etc.. behaviors. Provides indexing and tracking facilities for
 * monitoring currently playing sounds.
 */
class Stage
{
public:
    /**
     * Concurrent Sound exclusion policy:
     */
    enum Exclusion
    {
        DontExclude,   ///< All are welcome.
        OnePerEmitter  ///< Only one per SoundEmitter (others will be removed).
    };

    /// Audience that is notified when a new sound is added/started.
    DENG2_DEFINE_AUDIENCE2(Addition, void stageSoundAdded(Stage &, Sound &))

public:
    Stage(Exclusion exclusion = DontExclude);
    virtual ~Stage();

    DENG2_AS_IS_METHODS()

    /**
     * Returns the current Sound exclusion policy.
     *
     * @see setExclusion()
     */
    Exclusion exclusion() const;

    /**
     * Change the Sound exclusion policy to @a newBehavior. Note that calling this will
     * @em not affect any currently playing sounds, which persist until they are purged
     * (because they've stopped) or manually removed.
     *
     * @see exclusion()
     */
    void setExclusion(Exclusion newBehavior);

    /**
     * Provides access to the sound stage Listener.
     */
    Listener       &listener();
    Listener const &listener() const;

    /**
     * Returns true if the referenced sound-effect is currently playing somewhere in the
     * soundstage (irrespective of whether it is actually audible, or not).
     *
     * @param effectId  @c 0= true if Sounds are playing using the specified @a emitter.
     * @param emitter   Emitter (originator) of the Sound(s) must match this.
     *
     * @see playSound()
     */
    bool soundIsPlaying(de::dint effectId, SoundEmitter *emitter = nullptr) const;

    /**
     * Start playing a Sound in the soundstage. The Addition audience is notified whenever
     * a Sound is added to the stage.
     *
     * @param sound    Parameters of the sound-effect to play.
     * @param emitter  Emitter to attribute the sound to, if any.
     *
     * @see soundIsPlaying()
     */
    void playSound(SoundParams sound, SoundEmitter *emitter = nullptr);

    /**
     * Stop playing Sound(s) in the soundstage.
     *
     * @param effectId  Unique identifier of the sound-effect(s) to stop.
     * @param emitter   Emitter (originator) of the Sound(s) must match this.
     *
     * @see stopAllSounds()
     */
    void stopSound(de::dint effectId, SoundEmitter *emitter = nullptr);

    /**
     * Stop @em all playing Sound(s) in the soundstage.
     *
     * @see stopSound()
     */
    void stopAllSounds();

    /**
     * Iterate through the Sounds, executing @a callback for each.
     */
    de::LoopResult forAllSounds(std::function<de::LoopResult (Sound &)> callback) const;

    /**
     * Remove stopped (logical) Sounds @em if a purge is due.
     * @todo make private. -ds
     */
    void maybeRunSoundPurge();

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_STAGE_H
