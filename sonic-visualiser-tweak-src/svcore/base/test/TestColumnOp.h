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

#ifndef TEST_COLUMN_OP_H
#define TEST_COLUMN_OP_H

#include "../ColumnOp.h"

#include <QObject>
#include <QtTest>
#include <QDir>

#include <iostream>

//#define REPORT 1

using namespace std;

class TestColumnOp : public QObject
{
    Q_OBJECT

    typedef ColumnOp C;
    typedef ColumnOp::Column Column;
    typedef vector<double> BinMapping;

#ifdef REPORT
    template <typename T>
    void report(vector<T> v) {
        cerr << "Vector is: [ ";
        for (int i = 0; i < int(v.size()); ++i) {
            if (i > 0) cerr << ", ";
            cerr << v[i];
        }
        cerr << " ]\n";
    }
#else
    template <typename T>
    void report(vector<T> ) { }
#endif
                                     
private slots:
    void applyGain() {
        QCOMPARE(C::applyGain({}, 1.0), Column());
        Column c { 1, 2, 3, -4, 5, 6 };
        Column actual(C::applyGain(c, 1.5));
        Column expected { 1.5f, 3, 4.5f, -6, 7.5f, 9 };
        QCOMPARE(actual, expected);
        actual = C::applyGain(c, 1.0);
        QCOMPARE(actual, c);
        actual = C::applyGain(c, 0.0);
        expected = { 0, 0, 0, 0, 0, 0 };
        QCOMPARE(actual, expected);
    }

    void fftScale() {
        QCOMPARE(C::fftScale({}, 2.0), Column());
        Column c { 1, 2, 3, -4, 5 };
        Column actual(C::fftScale(c, 8));
        Column expected { 0.25f, 0.5f, 0.75f, -1, 1.25f };
        QCOMPARE(actual, expected);
    }

    void isPeak_null() {
        QVERIFY(!C::isPeak({}, 0));
        QVERIFY(!C::isPeak({}, 1));
        QVERIFY(!C::isPeak({}, -1));
    }

    void isPeak_obvious() {
        Column c { 0.4f, 0.5f, 0.3f };
        QVERIFY(!C::isPeak(c, 0));
        QVERIFY(C::isPeak(c, 1));
        QVERIFY(!C::isPeak(c, 2));
    }

    void isPeak_edges() {
        Column c { 0.5f, 0.4f, 0.3f };
        QVERIFY(C::isPeak(c, 0));
        QVERIFY(!C::isPeak(c, 1));
        QVERIFY(!C::isPeak(c, 2));
        QVERIFY(!C::isPeak(c, 3));
        QVERIFY(!C::isPeak(c, -1));
        c = { 1.4f, 1.5f };
        QVERIFY(!C::isPeak(c, 0));
        QVERIFY(C::isPeak(c, 1));
    }

    void isPeak_flat() {
        Column c { 0.0f, 0.0f, 0.0f };
        QVERIFY(C::isPeak(c, 0));
        QVERIFY(!C::isPeak(c, 1));
        QVERIFY(!C::isPeak(c, 2));
    }

    void isPeak_mixedSign() {
        Column c { 0.4f, -0.5f, -0.3f, -0.6f, 0.1f, -0.3f };
        QVERIFY(C::isPeak(c, 0));
        QVERIFY(!C::isPeak(c, 1));
        QVERIFY(C::isPeak(c, 2));
        QVERIFY(!C::isPeak(c, 3));
        QVERIFY(C::isPeak(c, 4));
        QVERIFY(!C::isPeak(c, 5));
    }

    void isPeak_duplicate() {
        Column c({ 0.5f, 0.5f, 0.4f, 0.4f });
        QVERIFY(C::isPeak(c, 0));
        QVERIFY(!C::isPeak(c, 1));
        QVERIFY(!C::isPeak(c, 2));
        QVERIFY(!C::isPeak(c, 3));
        c = { 0.4f, 0.4f, 0.5f, 0.5f };
        QVERIFY(C::isPeak(c, 0)); // counterintuitive but necessary
        QVERIFY(!C::isPeak(c, 1));
        QVERIFY(C::isPeak(c, 2));
        QVERIFY(!C::isPeak(c, 3));
    }

    void peakPick() {
        QCOMPARE(C::peakPick({}), Column());
        Column c({ 0.5f, 0.5f, 0.4f, 0.4f });
        QCOMPARE(C::peakPick(c), Column({ 0.5f, 0.0f, 0.0f, 0.0f }));
        c = Column({ 0.4f, -0.5f, -0.3f, -0.6f, 0.1f, -0.3f });
        QCOMPARE(C::peakPick(c), Column({ 0.4f, 0.0f, -0.3f, 0.0f, 0.1f, 0.0f }));
    }

    void normalize_null() {
        QCOMPARE(C::normalize({}, ColumnNormalization::None), Column());
        QCOMPARE(C::normalize({}, ColumnNormalization::Sum1), Column());
        QCOMPARE(C::normalize({}, ColumnNormalization::Max1), Column());
        QCOMPARE(C::normalize({}, ColumnNormalization::Range01), Column());
        QCOMPARE(C::normalize({}, ColumnNormalization::Hybrid), Column());
    }

    void normalize_none() {
        Column c { 1, 2, 3, 4 };
        QCOMPARE(C::normalize(c, ColumnNormalization::None), c);
    }

    void normalize_none_mixedSign() {
        Column c { 1, 2, -3, -4 };
        QCOMPARE(C::normalize(c, ColumnNormalization::None), c);
    }

    void normalize_sum1() {
        Column c { 1, 2, 4, 3 };
        QCOMPARE(C::normalize(c, ColumnNormalization::Sum1),
                 Column({ 0.1f, 0.2f, 0.4f, 0.3f }));
    }

    void normalize_sum1_mixedSign() {
        Column c { 1, 2, -4, -3 };
        QCOMPARE(C::normalize(c, ColumnNormalization::Sum1),
                 Column({ 0.1f, 0.2f, -0.4f, -0.3f }));
    }

    void normalize_max1() {
        Column c { 4, 3, 2, 1 };
        QCOMPARE(C::normalize(c, ColumnNormalization::Max1),
                 Column({ 1.0f, 0.75f, 0.5f, 0.25f }));
    }

    void normalize_max1_mixedSign() {
        Column c { -4, -3, 2, 1 };
        QCOMPARE(C::normalize(c, ColumnNormalization::Max1),
                 Column({ -1.0f, -0.75f, 0.5f, 0.25f }));
    }

    void normalize_range01() {
        Column c { 4, 3, 2, 1 };
        QCOMPARE(C::normalize(c, ColumnNormalization::Range01),
                 Column({ 1.0f, 2.f/3.f, 1.f/3.f, 0.0f }));
    }

    void normalize_range01_mixedSign() {
        Column c { -2, -3, 2, 1 };
        QCOMPARE(C::normalize(c, ColumnNormalization::Range01),
                 Column({ 0.2f, 0.0f, 1.0f, 0.8f }));
    }

    void normalize_hybrid() {
        // with max == 99, log10(max+1) == 2 so scale factor will be 2/99
        Column c { 22, 44, 99, 66 };
        QCOMPARE(C::normalize(c, ColumnNormalization::Hybrid),
                 Column({ 44.0f/99.0f, 88.0f/99.0f, 2.0f, 132.0f/99.0f }));
    }

    void normalize_hybrid_mixedSign() {
        // with max == 99, log10(max+1) == 2 so scale factor will be 2/99
        Column c { 22, 44, -99, -66 };
        QCOMPARE(C::normalize(c, ColumnNormalization::Hybrid),
                 Column({ 44.0f/99.0f, 88.0f/99.0f, -2.0f, -132.0f/99.0f }));
    }
    
    void distribute_simple() {
        Column in { 1, 2, 3 };
        BinMapping binfory { 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f };
        Column expected { 1, 1, 2, 2, 3, 3 };
        Column actual(C::distribute(in, 6, binfory, 0, false));
        report(actual);
        QCOMPARE(actual, expected);
    }
    
    void distribute_simple_interpolated() {
        Column in { 1, 2, 3 };
        BinMapping binfory { 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f };
        // There is a 0.5-bin offset from the distribution you might
        // expect, because this corresponds visually to the way that
        // bin values are duplicated upwards in simple_distribution.
        // It means that switching between interpolated and
        // non-interpolated views retains the visual position of each
        // bin peak as somewhere in the middle of the scale area for
        // that bin.
        Column expected { 1, 1, 1.5f, 2, 2.5f, 3 };
        Column actual(C::distribute(in, 6, binfory, 0, true));
        report(actual);
        QCOMPARE(actual, expected);
    }
    
    void distribute_nonlinear() {
        Column in { 1, 2, 3 };
        BinMapping binfory { 0.0f, 0.2f, 0.5f, 1.0f, 2.0f, 2.5f };
        Column expected { 1, 1, 1, 2, 3, 3 };
        Column actual(C::distribute(in, 6, binfory, 0, false));
        report(actual);
        QCOMPARE(actual, expected);
    }
    
    void distribute_nonlinear_interpolated() {
        // See distribute_simple_interpolated
        Column in { 1, 2, 3 };
        BinMapping binfory { 0.0f, 0.2f, 0.5f, 1.0f, 2.0f, 2.5f };
        Column expected { 1, 1, 1, 1.5, 2.5, 3 };
        Column actual(C::distribute(in, 6, binfory, 0, true));
        report(actual);
        QCOMPARE(actual, expected);
    }
    
    void distribute_shrinking() {
        Column in { 4, 1, 2, 3, 5, 6 };
        BinMapping binfory { 0.0f, 2.0f, 4.0f };
        Column expected { 4, 3, 6 };
        Column actual(C::distribute(in, 3, binfory, 0, false));
        report(actual);
        QCOMPARE(actual, expected);
    }
    
    void distribute_shrinking_interpolated() {
        // should be same as distribute_shrinking, we don't
        // interpolate when resizing down
        Column in { 4, 1, 2, 3, 5, 6 };
        BinMapping binfory { 0.0f, 2.0f, 4.0f };
        Column expected { 4, 3, 6 };
        Column actual(C::distribute(in, 3, binfory, 0, true));
        report(actual);
        QCOMPARE(actual, expected);
    }
    
    void distribute_nonlinear_someshrinking_interpolated() {
        // But we *should* interpolate if the mapping involves
        // shrinking some bins but expanding others.  See
        // distribute_simple_interpolated for note on 0.5 offset
        Column in { 4, 1, 2, 3, 5, 6 };
        BinMapping binfory { 0.0f, 3.0f, 4.0f, 4.5f };
        Column expected { 4.0f, 2.5f, 4.0f, 5.0f };
        Column actual(C::distribute(in, 4, binfory, 0, true));
        report(actual);
        QCOMPARE(actual, expected);
        binfory = BinMapping { 0.5f, 1.0f, 2.0f, 5.0f };
        expected = { 4.0f, 2.5f, 1.5f, 5.5f };
        actual = (C::distribute(in, 4, binfory, 0, true));
        report(actual);
        QCOMPARE(actual, expected);
    }
};
    
#endif

