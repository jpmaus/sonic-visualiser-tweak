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

#ifndef TEST_FFT_MODEL_H
#define TEST_FFT_MODEL_H

#include "../FFTModel.h"

#include "MockWaveModel.h"

#include "Compares.h"

#include <QObject>
#include <QtTest>
#include <QDir>

#include <iostream>
#include <complex>

using namespace std;

class TestFFTModel : public QObject
{
    Q_OBJECT

private:
    void test(ModelId model, // a DenseTimeValueModel
              WindowType window, int windowSize, int windowIncrement, int fftSize,
              int columnNo, vector<vector<complex<float>>> expectedValues,
              int expectedWidth) {
        for (int ch = 0; in_range_for(expectedValues, ch); ++ch) {
            FFTModel fftm(model, ch, window, windowSize, windowIncrement, fftSize);
            QCOMPARE(fftm.getWidth(), expectedWidth);
            int hs1 = fftSize/2 + 1;
            QCOMPARE(fftm.getHeight(), hs1);
            vector<float> reals(hs1 + 1, 0.f);
            vector<float> imags(hs1 + 1, 0.f);
            reals[hs1] = 999.f; // overrun guards
            imags[hs1] = 999.f;
            for (int stepThrough = 0; stepThrough <= 1; ++stepThrough) {
                if (stepThrough) {
                    // Read through the columns in order instead of
                    // randomly accessing the one we want. This is to
                    // exercise the case where the FFT model saves
                    // part of each input frame and moves along by
                    // only the non-overlapping distance
                    for (int sc = 0; sc < columnNo; ++sc) {
                        fftm.getValuesAt(sc, &reals[0], &imags[0]);
                    }
                }
                fftm.getValuesAt(columnNo, &reals[0], &imags[0]);
                for (int i = 0; i < hs1; ++i) {
                    float eRe = expectedValues[ch][i].real();
                    float eIm = expectedValues[ch][i].imag();
                    float thresh = 1e-5f;
                    if (abs(reals[i] - eRe) > thresh ||
                        abs(imags[i] - eIm) > thresh) {
                        SVCERR << "ERROR: output is not as expected for column "
                             << i << " in channel " << ch << " (stepThrough = "
                             << stepThrough << ")" << endl;
                        SVCERR << "expected : ";
                        for (int j = 0; j < hs1; ++j) {
                            SVCERR << expectedValues[ch][j] << " ";
                        }
                        SVCERR << "\nactual   : ";
                        for (int j = 0; j < hs1; ++j) {
                            SVCERR << complex<float>(reals[j], imags[j]) << " ";
                        }
                        SVCERR << endl;
                    }
                    COMPARE_FUZZIER_F(reals[i], eRe);
                    COMPARE_FUZZIER_F(imags[i], eIm);
                }
                QCOMPARE(reals[hs1], 999.f);
                QCOMPARE(imags[hs1], 999.f);
            }
        }
    }

    ModelId makeMock(std::vector<Sort> sorts, int length, int pad) {
        auto mwm = std::make_shared<MockWaveModel>(sorts, length, pad);
        return ModelById::add(mwm);
    }

    void releaseMock(ModelId id) {
        ModelById::release(id);
    }

private slots:

    // NB. FFTModel columns are centred on the sample frame, and in
    // particular this means column 0 is centred at sample 0 (i.e. it
    // contains only half the window-size worth of real samples, the
    // others are 0-valued from before the origin).  Generally in
    // these tests we are padding our signal with half a window of
    // zeros, in order that the result for column 0 is all zeros
    // (rather than something with a step in it that is harder to
    // reason about the FFT of) and the results for subsequent columns
    // are those of our expected signal.
    
    void dc_simple_rect() {
        auto mwm = makeMock({ DC }, 16, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 0,
             { { {}, {}, {}, {}, {} } }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 1,
             { { { 4.f, 0.f }, {}, {}, {}, {} } }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 2,
             { { { 4.f, 0.f }, {}, {}, {}, {} } }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 3,
             { { {}, {}, {}, {}, {} } }, 4);
        releaseMock(mwm);
    }

    void dc_simple_hann() {
        // The Hann window function is a simple sinusoid with period
        // equal to twice the window size, and it halves the DC energy
        auto mwm = makeMock({ DC }, 16, 4);
        test(mwm, HanningWindow, 8, 8, 8, 0,
             { { {}, {}, {}, {}, {} } }, 4);
        test(mwm, HanningWindow, 8, 8, 8, 1,
             { { { 4.f, 0.f }, { 2.f, 0.f }, {}, {}, {} } }, 4);
        test(mwm, HanningWindow, 8, 8, 8, 2,
             { { { 4.f, 0.f }, { 2.f, 0.f }, {}, {}, {} } }, 4);
        test(mwm, HanningWindow, 8, 8, 8, 3,
             { { {}, {}, {}, {}, {} } }, 4);
        releaseMock(mwm);
    }
    
    void dc_simple_hann_halfoverlap() {
        auto mwm = makeMock({ DC }, 16, 4);
        test(mwm, HanningWindow, 8, 4, 8, 0,
             { { {}, {}, {}, {}, {} } }, 7);
        test(mwm, HanningWindow, 8, 4, 8, 2,
             { { { 4.f, 0.f }, { 2.f, 0.f }, {}, {}, {} } }, 7);
        test(mwm, HanningWindow, 8, 4, 8, 3,
             { { { 4.f, 0.f }, { 2.f, 0.f }, {}, {}, {} } }, 7);
        test(mwm, HanningWindow, 8, 4, 8, 6,
             { { {}, {}, {}, {}, {} } }, 7);
        releaseMock(mwm);
    }
    
    void sine_simple_rect() {
        auto mwm = makeMock({ Sine }, 16, 4);
        // Sine: output is purely imaginary. Note the sign is flipped
        // (normally the first half of the output would have negative
        // sign for a sine starting at 0) because the model does an
        // FFT shift to centre the phase
        test(mwm, RectangularWindow, 8, 8, 8, 0,
             { { {}, {}, {}, {}, {} } }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 1,
             { { {}, { 0.f, 2.f }, {}, {}, {} } }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 2,
             { { {}, { 0.f, 2.f }, {}, {}, {} } }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 3,
             { { {}, {}, {}, {}, {} } }, 4);
        releaseMock(mwm);
    }
    
    void cosine_simple_rect() {
        auto mwm = makeMock({ Cosine }, 16, 4);
        // Cosine: output is purely real. Note the sign is flipped
        // because the model does an FFT shift to centre the phase
        test(mwm, RectangularWindow, 8, 8, 8, 0,
             { { {}, {}, {}, {}, {} } }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 1,
             { { {}, { -2.f, 0.f }, {}, {}, {} } }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 2,
             { { {}, { -2.f, 0.f }, {}, {}, {} } }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 3,
             { { {}, {}, {}, {}, {} } }, 4);
        releaseMock(mwm);
    }
    
    void twochan_simple_rect() {
        auto mwm = makeMock({ Sine, Cosine }, 16, 4);
        // Test that the two channels are read and converted separately
        test(mwm, RectangularWindow, 8, 8, 8, 0,
             {
                 { {}, {}, {}, {}, {} },
                 { {}, {}, {}, {}, {} }
             }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 1,
             {
                 { {}, {  0.f, 2.f }, {}, {}, {} },
                 { {}, { -2.f, 0.f }, {}, {}, {} }
             }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 2,
             {
                 { {}, {  0.f, 2.f }, {}, {}, {} },
                 { {}, { -2.f, 0.f }, {}, {}, {} }
             }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 3,
             {
                 { {}, {}, {}, {}, {} },
                 { {}, {}, {}, {}, {} }
             }, 4);
        releaseMock(mwm);
    }
    
    void nyquist_simple_rect() {
        auto mwm = makeMock({ Nyquist }, 16, 4);
        // Again, the sign is flipped. This has the same amount of
        // energy as the DC example
        test(mwm, RectangularWindow, 8, 8, 8, 0,
             { { {}, {}, {}, {}, {} } }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 1,
             { { {}, {}, {}, {}, { -4.f, 0.f } } }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 2,
             { { {}, {}, {}, {}, { -4.f, 0.f } } }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 3,
             { { {}, {}, {}, {}, {} } }, 4);
        releaseMock(mwm);
    }
    
    void dirac_simple_rect() {
        auto mwm = makeMock({ Dirac }, 16, 4);
        // The window scales by 0.5 and some signs are flipped. Only
        // column 1 has any data (the single impulse).
        test(mwm, RectangularWindow, 8, 8, 8, 0,
             { { {}, {}, {}, {}, {} } }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 1,
             { { { 0.5f, 0.f }, { -0.5f, 0.f }, { 0.5f, 0.f }, { -0.5f, 0.f }, { 0.5f, 0.f } } }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 2,
             { { {}, {}, {}, {}, {} } }, 4);
        test(mwm, RectangularWindow, 8, 8, 8, 3,
             { { {}, {}, {}, {}, {} } }, 4);
        releaseMock(mwm);
    }
    
    void dirac_simple_rect_2() {
        auto mwm = makeMock({ Dirac }, 16, 8);
        // With 8 samples padding, the FFT shift places the first
        // Dirac impulse at the start of column 1, thus giving all
        // positive values
        test(mwm, RectangularWindow, 8, 8, 8, 0,
             { { {}, {}, {}, {}, {} } }, 5);
        test(mwm, RectangularWindow, 8, 8, 8, 1,
             { { { 0.5f, 0.f }, { 0.5f, 0.f }, { 0.5f, 0.f }, { 0.5f, 0.f }, { 0.5f, 0.f } } }, 5);
        test(mwm, RectangularWindow, 8, 8, 8, 2,
             { { {}, {}, {}, {}, {} } }, 5);
        test(mwm, RectangularWindow, 8, 8, 8, 3,
             { { {}, {}, {}, {}, {} } }, 5);
        test(mwm, RectangularWindow, 8, 8, 8, 4,
             { { {}, {}, {}, {}, {} } }, 5);
        releaseMock(mwm);
    }

    void dirac_simple_rect_halfoverlap() {
        auto mwm = makeMock({ Dirac }, 16, 4);
        test(mwm, RectangularWindow, 8, 4, 8, 0,
             { { {}, {}, {}, {}, {} } }, 7);
        test(mwm, RectangularWindow, 8, 4, 8, 1,
             { { { 0.5f, 0.f }, { 0.5f, 0.f }, { 0.5f, 0.f }, { 0.5f, 0.f }, { 0.5f, 0.f } } }, 7);
        test(mwm, RectangularWindow, 8, 4, 8, 2,
             { { { 0.5f, 0.f }, { -0.5f, 0.f }, { 0.5f, 0.f }, { -0.5f, 0.f }, { 0.5f, 0.f } } }, 7);
        test(mwm, RectangularWindow, 8, 4, 8, 3,
             { { {}, {}, {}, {}, {} } }, 7);
        releaseMock(mwm);
    }
    
};

#endif
