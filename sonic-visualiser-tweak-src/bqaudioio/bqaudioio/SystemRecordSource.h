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

#ifndef BQAUDIOIO_SYSTEM_RECORD_SOURCE_H
#define BQAUDIOIO_SYSTEM_RECORD_SOURCE_H

#include "Suspendable.h"

namespace breakfastquay {

class ApplicationRecordTarget;

/**
 * Source of audio samples for recording, encapsulating the system
 * audio input. Created by AudioFactory. The caller supplies an
 * ApplicationRecordTarget implementation which accepts the samples
 * when called.
 *
 * The source will be continually providing samples for as long as it
 * is not suspended (see Suspendable interface). A newly-created
 * source is not suspended.
 *
 * The supplied ApplicationRecordTarget must outlive the source
 * object. That is, the application should delete the source before it
 * deletes the associated target.
 */ 
class SystemRecordSource : virtual public Suspendable
{
public:
    virtual ~SystemRecordSource();

    /**
     * Return true if the source has been constructed correctly and is
     * in a working state.
     */
    virtual bool isSourceOK() const = 0;

    /**
     * Return true if the source has been constructed correctly, is in
     * a working state, and is receiving samples (so for example any
     * callback it receives from the audio driver to report that the
     * stream is open has been received).
     */
    virtual bool isSourceReady() const { return isSourceOK(); }

protected:
    SystemRecordSource(ApplicationRecordTarget *target);

    ApplicationRecordTarget *m_target;

    SystemRecordSource(const SystemRecordSource &)=delete;
    SystemRecordSource &operator=(const SystemRecordSource &)=delete;
};

}
#endif

