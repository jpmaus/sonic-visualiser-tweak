/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/*
    bqaudioio

    Copyright 2007-2016 Particular Programs Ltd.

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of Chris Cannam and
    Particular Programs Ltd shall not be used in advertising or
    otherwise to promote the sale, use or other dealings in this
    Software without prior written authorization.
*/

#ifndef BQAUDIOIO_SYSTEM_PLAYBACK_TARGET_H
#define BQAUDIOIO_SYSTEM_PLAYBACK_TARGET_H

#include "Suspendable.h"

namespace breakfastquay {

class ApplicationPlaybackSource;

/**
 * Target for audio samples for playback, encapsulating the system
 * audio output. Created by AudioFactory. The caller supplies an
 * ApplicationPlaybackSource implementation which provides the samples
 * on request.
 *
 * The target will be continually requesting and playing samples for
 * as long as it is not suspended (see Suspendable interface). A
 * newly-created target is not suspended.
 *
 * The supplied ApplicationPlaybackSource must outlive the target
 * object. That is, the application should delete the target before it
 * deletes the associated source.
 */ 
class SystemPlaybackTarget : virtual public Suspendable
{
public:
    virtual ~SystemPlaybackTarget();

    /**
     * Return true if the target has been constructed correctly and is
     * in a working state.
     */
    virtual bool isTargetOK() const = 0;

    /**
     * Return true if the target has been constructed correctly, is in
     * a working state, and is ready to play (so for example any
     * callback it receives from the audio driver to report that the
     * stream is open has been received).
     */
    virtual bool isTargetReady() const {
        return isTargetOK();
    }

    /**
     * Get the current stream time in seconds. This is continually
     * incrementing for as long as the target exists (possibly pausing
     * when suspended, though that is implementation-dependent).
     */
    virtual double getCurrentTime() const = 0;

    /**
     * Set the playback gain (0.0 = silence, 1.0 = levels unmodified
     * from the data provided by the source). The default is 1.0.
     */
    virtual void setOutputGain(float gain);

    /**
     * Retrieve the playback gain.
     */
    virtual float getOutputGain() const;

    /**
     * Set the playback balance for stereo output (-1.0 = hard left,
     * 1.0 = hard right, 0.0 = middle). The default is 0.0.
     */
    virtual void setOutputBalance(float balance);

    /**
     * Retrieve the playback balance.
     */
    virtual float getOutputBalance() const;

protected:
    SystemPlaybackTarget(ApplicationPlaybackSource *source);

    ApplicationPlaybackSource *m_source;
    float m_outputGain;
    float m_outputBalance;

    SystemPlaybackTarget(const SystemPlaybackTarget &)=delete;
    SystemPlaybackTarget &operator=(const SystemPlaybackTarget &)=delete;
};

}

#endif

