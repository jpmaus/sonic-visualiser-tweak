/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef MOCK_WAVE_MODEL_H
#define MOCK_WAVE_MODEL_H

#include "../DenseTimeValueModel.h"

#include <vector>

enum Sort {
    DC,
    Sine,
    Cosine,
    Nyquist,
    Dirac
};

class MockWaveModel : public DenseTimeValueModel
{
    Q_OBJECT

public:
    /** One Sort per channel! Length is in samples, and is in addition
     * to "pad" number of zero samples at the start and end */
    MockWaveModel(std::vector<Sort> sorts, int length, int pad);

    float getValueMinimum() const override { return -1.f; }
    float getValueMaximum() const override { return  1.f; }
    int getChannelCount() const override { return int(m_data.size()); }
    
    floatvec_t getData(int channel, sv_frame_t start, sv_frame_t count) const override;
    std::vector<floatvec_t> getMultiChannelData(int fromchannel, int tochannel, sv_frame_t start, sv_frame_t count) const override;

    bool canPlay() const override { return true; }
    QString getDefaultPlayClipId() const override { return ""; }

    sv_frame_t getStartFrame() const override { return 0; }
    sv_frame_t getTrueEndFrame() const override { return m_data[0].size(); }
    sv_samplerate_t getSampleRate() const override { return 44100; }
    bool isOK() const override { return true; }
    int getCompletion() const override { return 100; }
    
    QString getTypeName() const override { return tr("Mock Wave"); }

private:
    std::vector<std::vector<float> > m_data;
    std::vector<float> generate(Sort sort, int length, int pad) const;
};

#endif
