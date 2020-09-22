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

#include "MockWaveModel.h"

#include <cmath>

using namespace std;

MockWaveModel::MockWaveModel(vector<Sort> sorts, int length, int pad)
{
    for (auto sort: sorts) {
        m_data.push_back(generate(sort, length, pad));
    }
}

floatvec_t
MockWaveModel::getData(int channel, sv_frame_t start, sv_frame_t count) const
{
    sv_frame_t i = 0;

//    cerr << "MockWaveModel::getData(" << channel << "," << start << "," << count << "): ";

    floatvec_t data;
    
    while (i < count) {
        sv_frame_t idx = start + i;
        if (!in_range_for(m_data[channel], idx)) break;
        data.push_back(m_data[channel][idx]);
//        cerr << data[i] << " ";
        ++i;
    }

//    cerr << endl;
    
    return data;
}

vector<floatvec_t>
MockWaveModel::getMultiChannelData(int fromchannel, int tochannel,
                                   sv_frame_t start, sv_frame_t count) const
{
    vector<floatvec_t> data(tochannel - fromchannel + 1);
    
    for (int c = fromchannel; c <= tochannel; ++c) {
        data[c] = getData(c, start, count);
    }

    return data;
}

vector<float>
MockWaveModel::generate(Sort sort, int length, int pad) const
{
    vector<float> data;

    for (int i = 0; i < pad; ++i) {
        data.push_back(0.f);
    }
    
    for (int i = 0; i < length; ++i) {

        double v = 0.0;
        
        switch (sort) {
        case DC: v = 1.0; break;
        case Sine: v = sin((2.0 * M_PI / 8.0) * i); break;
        case Cosine: v = cos((2.0 * M_PI / 8.0) * i); break;
        case Nyquist: v = (i % 2) * 2 - 1; break;
        case Dirac: v = (i == 0) ? 1.0 : 0.0; break;
        }

        data.push_back(float(v));
    }

    for (int i = 0; i < pad; ++i) {
        data.push_back(0.f);
    }

    return data;
}

