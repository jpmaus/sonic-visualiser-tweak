/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_AUDIO_RECORD_TARGET_H
#define SV_AUDIO_RECORD_TARGET_H

#include "BaseTypes.h"

/**
 * The record target API used by the view manager. See also AudioPlaySource.
 */
class AudioRecordTarget
{
public:
    virtual ~AudioRecordTarget() { }

    /**
     * Return whether recording is currently happening.
     */
    virtual bool isRecording() const = 0;

    /**
     * Return the approximate duration of the audio recording so far.
     */
    virtual sv_frame_t getRecordDuration() const = 0;

    /**
     * Return the current (or thereabouts) input levels in the range
     * 0.0 -> 1.0, for metering purposes. Only valid while recording.
     * The values returned are peak values since the last call to this
     * function was made (i.e. calling this function also resets them).
     */
    virtual bool getInputLevels(float &left, float &right) = 0;
};

#endif



