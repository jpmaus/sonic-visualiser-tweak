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

#ifndef BQAUDIOIO_SYSTEM_AUDIO_IO_H
#define BQAUDIOIO_SYSTEM_AUDIO_IO_H

#include "SystemRecordSource.h"
#include "SystemPlaybackTarget.h"

namespace breakfastquay {

/**
 * Interface that combines a SystemPlaybackTarget and a
 * SystemRecordSource, encapsulating the system audio input and output
 * for duplex audio. Created by AudioFactory. The caller supplies an
 * ApplicationPlaybackSource implementation which provides playback
 * samples on request, and an ApplicationRecordTarget which accepts
 * record samples when called.
 *
 * The target will be continually processing samples for as long as it
 * is not suspended (see Suspendable interface). A newly-created
 * target is not suspended.
 *
 * The supplied ApplicationPlaybackSource and ApplicationRecordTarget
 * must outlive the IO object. That is, the application should delete
 * the IO before it deletes the associated source and target.
 */ 
class SystemAudioIO : public SystemRecordSource,
                      public SystemPlaybackTarget
{
public:
    /**
     * Return true if the target has been constructed correctly and is
     * in a working state.
     */
    bool isOK() const { return isSourceOK() && isTargetOK(); }

    /**
     * Return true if the target has been constructed correctly, is in
     * a working state, and is ready to play (so for example any
     * callback it receives from the audio driver to report that the
     * stream is open has been received).
     */
    bool isReady() const { return isSourceReady() && isTargetReady(); }

protected:
    SystemAudioIO(ApplicationRecordTarget *target,
                  ApplicationPlaybackSource *source) :
        SystemRecordSource(target),
        SystemPlaybackTarget(source) { }

    SystemAudioIO(const SystemAudioIO &)=delete;
    SystemAudioIO &operator=(const SystemAudioIO &)=delete;
};

}

#endif
