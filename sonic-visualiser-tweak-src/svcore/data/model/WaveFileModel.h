/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_WAVE_FILE_MODEL_H
#define SV_WAVE_FILE_MODEL_H

#include "RangeSummarisableTimeValueModel.h"

#include <stdlib.h>

class WaveFileModel : public RangeSummarisableTimeValueModel
{
    Q_OBJECT

public:
    virtual ~WaveFileModel();

    virtual sv_frame_t getFrameCount() const = 0;
    int getChannelCount() const override = 0;
    sv_samplerate_t getSampleRate() const override = 0;
    sv_samplerate_t getNativeRate() const override = 0;

    QString getTitle() const override = 0;
    QString getMaker() const override = 0;
    QString getLocation() const override = 0;

    sv_frame_t getStartFrame() const override = 0;
    sv_frame_t getTrueEndFrame() const override = 0;

    virtual void setStartFrame(sv_frame_t startFrame) = 0;

protected:
    WaveFileModel() { } // only accessible from subclasses
};    

#endif
