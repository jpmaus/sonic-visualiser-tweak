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

#include "WindowShapePreview.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QString>

#include <bqfft/FFT.h>

#include <vector>
#include <complex>
#include <iostream>

using namespace std;


WindowShapePreview::WindowShapePreview(QWidget *parent) :
    QFrame(parent),
    m_windowType(HanningWindow)
{
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setMargin(0);
    setLayout(layout);
    m_windowTimeExampleLabel = new QLabel;
    m_windowFreqExampleLabel = new QLabel;
    layout->addWidget(m_windowTimeExampleLabel);
    layout->addWidget(m_windowFreqExampleLabel);
}

WindowShapePreview::~WindowShapePreview()
{
}

void
WindowShapePreview::updateLabels()
{
    float scaleRatio = float(QFontMetrics(font()).height()) / 14.f;
    if (scaleRatio < 1.f) scaleRatio = 1.f;

    int step = int(24 * scaleRatio);
    float peak = float(48 * scaleRatio);

    int w = step * 4, h = int((peak * 4) / 3);

    WindowType type = m_windowType;
    Window<float> windower = Window<float>(type, step * 2);
    
    QPixmap timeLabel(w, h + 1);
    timeLabel.fill(Qt::white);
    QPainter timePainter(&timeLabel);

    QPainterPath path;

    path.moveTo(0, float(h) - peak + 1);
    path.lineTo(w, float(h) - peak + 1);

    timePainter.setPen(Qt::gray);
    timePainter.setRenderHint(QPainter::Antialiasing, true);
    timePainter.drawPath(path);
    
    path = QPainterPath();

    float *acc = new float[w];
    for (int i = 0; i < w; ++i) acc[i] = 0.f;
    for (int j = 0; j < 3; ++j) {
        for (int i = 0; i < step * 2; ++i) {
            acc[j * step + i] += windower.getValue(i);
        }
    }
    for (int i = 0; i < w; ++i) {
        int y = h - int(peak * acc[i] + 0.001f) + 1;
        if (i == 0) path.moveTo(i, y);
        else path.lineTo(i, y);
    }
    delete[] acc;

    timePainter.drawPath(path);
    timePainter.setRenderHint(QPainter::Antialiasing, false);

    path = QPainterPath();

    timePainter.setPen(Qt::black);
    
    for (int i = 0; i < step * 2; ++i) {
        int y = h - int(peak * windower.getValue(i) + 0.001) + 1;
        if (i == 0) path.moveTo(i + step, float(y));
        else path.lineTo(i + step, float(y));
    }

    if (type == RectangularWindow) {
        timePainter.drawPath(path);
        path = QPainterPath();
    }

    timePainter.setRenderHint(QPainter::Antialiasing, true);
    path.addRect(0, 0, w, h + 1);
    timePainter.drawPath(path);

    // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
    // replacement (horizontalAdvance) was only added in Qt 5.11
    // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    QFont font;
    font.setPixelSize(int(10 * scaleRatio));
    font.setItalic(true);
    timePainter.setFont(font);
    QString label = tr("V / time");
    timePainter.drawText(w - timePainter.fontMetrics().width(label) - 4,
                         timePainter.fontMetrics().ascent() + 1, label);

    m_windowTimeExampleLabel->setPixmap(timeLabel);
    
    QPixmap freqLabel(w, h + 1);
    freqLabel.fill(Qt::white);
    QPainter freqPainter(&freqLabel);
    path = QPainterPath();

    int fftsize = 512;

    breakfastquay::FFT fft(fftsize);

    vector<float> input(fftsize);
    vector<complex<float>> output(fftsize/2 + 1);
    
    for (int i = 0; i < fftsize; ++i) input[i] = 0.f;
    for (int i = 0; i < step * 2; ++i) {
        input[fftsize/2 - step + i] = windower.getValue(i);
    }

    fft.forwardInterleaved(input.data(), reinterpret_cast<float *>(output.data()));

    float maxdb = 0.f;
    float mindb = 0.f;
    bool first = true;
    for (int i = 0; i < fftsize/2; ++i) {
        float power =
            output[i].real() * output[i].real() +
            output[i].imag() * output[i].imag();
        float db = mindb;
        if (power > 0) {
            db = 20.f * log10f(power);
            if (first || db > maxdb) maxdb = db;
            if (first || db < mindb) mindb = db;
            first = false;
        }
    }

    if (mindb > -80.f) mindb = -80.f;

    // -- no, don't use the actual mindb -- it's easier to compare
    // plots with a fixed min value
    mindb = -170.f;

    float maxval = maxdb + -mindb;

//    float ly = h - ((-80.f + -mindb) / maxval) * peak + 1;

    path.moveTo(0, float(h) - peak + 1);
    path.lineTo(w, float(h) - peak + 1);

    freqPainter.setPen(Qt::gray);
    freqPainter.setRenderHint(QPainter::Antialiasing, true);
    freqPainter.drawPath(path);
    
    path = QPainterPath();
    freqPainter.setPen(Qt::black);

//    cerr << "maxdb = " << maxdb << ", mindb = " << mindb << ", maxval = " <<maxval << endl;

    for (int i = 0; i < fftsize/2; ++i) {
        float power =
            output[i].real() * output[i].real() +
            output[i].imag() * output[i].imag();
        float db = 20.f * log10f(power);
        float val = db + -mindb;
        if (val < 0) val = 0;
        float norm = val / maxval;
        float x = (float(w) / float(fftsize/2)) * float(i);
        float y = float(h) - norm * peak + 1;
        if (i == 0) path.moveTo(x, y);
        else path.lineTo(x, y);
    }

    freqPainter.setRenderHint(QPainter::Antialiasing, true);
    path.addRect(0, 0, w, h + 1);
    freqPainter.drawPath(path);

    freqPainter.setFont(font);
    label = tr("dB / freq");
    freqPainter.drawText(w - freqPainter.fontMetrics().width(label) - 4,
                         freqPainter.fontMetrics().ascent() + 1, label);

    m_windowFreqExampleLabel->setPixmap(freqLabel);
}

void
WindowShapePreview::setWindowType(WindowType type)
{
    m_windowType = type;
    updateLabels();
}

