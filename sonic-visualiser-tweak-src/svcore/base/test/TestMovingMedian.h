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

#ifndef TEST_MOVING_MEDIAN_H
#define TEST_MOVING_MEDIAN_H

#include "../MovingMedian.h"

#include <QObject>
#include <QtTest>
#include <QDir>

#include <iostream>

using namespace std;

class TestMovingMedian : public QObject
{
    Q_OBJECT

    template <typename T>
    void checkExpected(const vector<T> &output,
                       const vector<T> &expected) {
        if (output.size() != expected.size()) {
            std::cerr << "ERROR: output array size " << output.size()
                      << " differs from expected size " << expected.size()
                      << std::endl;
        }
        for (int i = 0; i < int(output.size()); ++i) {
            if (output[i] != expected[i]) {
                std::cerr << "ERROR: Value at index " << i
                          << " in output array differs from expected"
                          << std::endl;
                std::cerr << "Output:   ";
                for (auto v: output) std::cerr << v << " ";
                std::cerr << "\nExpected: ";
                for (auto v: expected) std::cerr << v << " ";
                std::cerr << std::endl;
                break;
            }
        }
        QCOMPARE(output, expected);
    }

    template <typename T>
    void testFixed(int n,
                   const vector<T> &input,
                   const vector<T> &expected,
                   double percentile = 50.0) {
        vector<T> output;
        MovingMedian<T> mm(n, percentile);
        for (auto v: input) {
            mm.push(v);
            mm.checkIntegrity();
            output.push_back(mm.get());
        }
        mm.checkIntegrity();
        checkExpected<T>(output, expected);
    }

private slots:

    void empty() {
        MovingMedian<double> mm(3);
        QCOMPARE(mm.get(), 0.0);
    }
    
    void zeros() {
        vector<double> input { 0.0, 0.0, 0.0, 0.0, 0.0 };
        vector<double> expected { 0.0, 0.0, 0.0, 0.0, 0.0 };
        testFixed<double>(3, input, expected);
    }
    
    void ascending() {
        vector<double> input { 1.0, 2.0, 3.0, 4.0, 5.0 };
        vector<double> expected { 0.0, 1.0, 2.0, 3.0, 4.0 };
        testFixed<double>(3, input, expected);
    }

    void ascendingInt() {
        vector<int> input { 1, 2, 3, 4, 5 };
        vector<int> expected { 0, 1, 2, 3, 4 };
        testFixed<int>(3, input, expected);
    }

    void descending() {
        vector<double> input { 5.0, 4.0, 3.0, 2.0, 1.0 };
        vector<double> expected { 0.0, 4.0, 4.0, 3.0, 2.0 };
        testFixed<double>(3, input, expected);
    }

    void descendingInt() {
        vector<int> input { 5, 4, 3, 2, 1 };
        vector<int> expected { 0, 4, 4, 3, 2 };
        testFixed<int>(3, input, expected);
    }

    void duplicates() {
        vector<double> input { 2.0, 2.0, 3.0, 4.0, 3.0 };
        vector<double> expected { 0.0, 2.0, 2.0, 3.0, 3.0 };
        testFixed<double>(3, input, expected);
    }
    
    void percentile10() {
        vector<double> input { 1.0, 2.0, 3.0, 4.0, 5.0 };
        vector<double> expected { 0.0, 0.0, 1.0, 2.0, 3.0 };
        testFixed<double>(3, input, expected, 10);
    }
    
    void percentile90() {
        vector<double> input { 1.0, 2.0, 3.0, 4.0, 5.0 };
        vector<double> expected { 1.0, 2.0, 3.0, 4.0, 5.0 };
        testFixed<double>(3, input, expected, 90);
    }

    void even() {
        vector<double> input { 5.0, 4.0, 3.0, 2.0, 1.0 };
        vector<double> expected { 0.0, 4.0, 4.0, 4.0, 3.0 };
        testFixed<double>(4, input, expected);
    }

    void growing() {
        vector<double> input { 2.0, 4.0, 3.0, 2.5, 2.5, 3.0, 1.0, 2.0, 1.0, 0.0 };
        vector<double> expected { 2.0, 4.0, 4.0, 3.0, 2.5, 2.5, 2.5, 2.5, 2.0, 1.0 };
        vector<double> output;
        MovingMedian<double> mm(1);
        for (int i = 0; i < int(input.size()); ++i) {
            // sizes 1, 1, 2, 2, 3, 3, 4, 4, 5, 5
            int sz = i/2 + 1;
            mm.resize(sz);
            QCOMPARE(mm.size(), sz);
            mm.push(input[i]);
            mm.checkIntegrity();
            output.push_back(mm.get());
        }
        mm.checkIntegrity();
        checkExpected<double>(output, expected);
    }
        
    void shrinking() {
        vector<double> input { 2.0, 4.0, 3.0, 2.5, 2.5, 3.0, 1.0, 2.0, 1.0, 0.0 };
        vector<double> expected { 0.0, 0.0, 3.0, 3.0, 2.5, 2.5, 3.0, 2.0, 1.0, 0.0 };
        vector<double> output;
        MovingMedian<double> mm(99);
        for (int i = 0; i < int(input.size()); ++i) {
            // sizes 5, 5, 4, 4, 3, 3, 2, 2, 1, 1
            int sz = 5 - i/2;
            mm.resize(sz);
            QCOMPARE(mm.size(), sz);
            mm.push(input[i]);
            mm.checkIntegrity();
            output.push_back(mm.get());
        }
        mm.checkIntegrity();
        checkExpected<double>(output, expected);
    }
};

#endif
