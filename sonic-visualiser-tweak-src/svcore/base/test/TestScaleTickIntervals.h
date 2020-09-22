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

#ifndef TEST_SCALE_TICK_INTERVALS_H
#define TEST_SCALE_TICK_INTERVALS_H

#include "../ScaleTickIntervals.h"

#include <QObject>
#include <QtTest>
#include <QDir>

#include <iostream>

using namespace std;

class TestScaleTickIntervals : public QObject
{
    Q_OBJECT

    void printDiff(vector<ScaleTickIntervals::Tick> ticks,
                   vector<ScaleTickIntervals::Tick> expected) {

        SVCERR << "Have " << ticks.size() << " ticks, expected "
               << expected.size() << endl;
        for (int i = 0; i < int(ticks.size()); ++i) {
            SVCERR << i << ": have " << ticks[i].value << " \""
                   << ticks[i].label << "\", expected ";
            if (i < int(expected.size())) {
                SVCERR << expected[i].value << " \"" << expected[i].label
                       << "\"" << endl;
            } else {
                SVCERR << "(n/a)" << endl;
            }
        }
    }
    
    void compareTicks(ScaleTickIntervals::Ticks ticks,
                      ScaleTickIntervals::Ticks expected,
                      bool fuzzier = false)
    {
        for (int i = 0; i < int(expected.size()); ++i) {
            if (i < int(ticks.size())) {
                bool pass = true;
                if (ticks[i].label != expected[i].label) {
                    pass = false;
                } else {
                    double eps = fuzzier ? 1e-5 : 1e-10;
                    double diff = fabs(ticks[i].value - expected[i].value);
                    double limit = max(eps, fabs(ticks[i].value) * eps);
                    if (diff > limit) {
                        pass = false;
                    }
                }
                if (!pass) {
                    printDiff(ticks, expected);
                }
                QCOMPARE(ticks[i].label, expected[i].label);
                QCOMPARE(ticks[i].value, expected[i].value);
            }
        }
        if (ticks.size() != expected.size()) {
            printDiff(ticks, expected);
        }
        QCOMPARE(ticks.size(), expected.size());
    }
    
private slots:
    void linear_0_1_10()
    {
        auto ticks = ScaleTickIntervals::linear({ 0, 1, 10 });
        ScaleTickIntervals::Ticks expected {
            { 0.0, "0.0" },
            { 0.1, "0.1" },
            { 0.2, "0.2" },
            { 0.3, "0.3" },
            { 0.4, "0.4" },
            { 0.5, "0.5" },
            { 0.6, "0.6" },
            { 0.7, "0.7" },
            { 0.8, "0.8" },
            { 0.9, "0.9" },
            { 1.0, "1.0" }
        };
        compareTicks(ticks, expected);
    }

    void linear_0_5_5()
    {
        auto ticks = ScaleTickIntervals::linear({ 0, 5, 5 });
        ScaleTickIntervals::Ticks expected {
            { 0, "0" },
            { 1, "1" },
            { 2, "2" },
            { 3, "3" },
            { 4, "4" },
            { 5, "5" },
        };
        compareTicks(ticks, expected);
    }

    void linear_0_10_5()
    {
        auto ticks = ScaleTickIntervals::linear({ 0, 10, 5 });
        ScaleTickIntervals::Ticks expected {
            { 0, "0" },
            { 2, "2" },
            { 4, "4" },
            { 6, "6" },
            { 8, "8" },
            { 10, "10" }
        };
        compareTicks(ticks, expected);
    }

    void linear_10_0_5()
    {
        auto ticks = ScaleTickIntervals::linear({ 10, 0, 5 });
        ScaleTickIntervals::Ticks expected {
            { 0, "0" },
            { 2, "2" },
            { 4, "4" },
            { 6, "6" },
            { 8, "8" },
            { 10, "10" }
        };
        compareTicks(ticks, expected);
    }

    void linear_m10_0_5()
    {
        auto ticks = ScaleTickIntervals::linear({ -10, 0, 5 });
        ScaleTickIntervals::Ticks expected {
            { -10, "-10" },
            { -8, "-8" },
            { -6, "-6" },
            { -4, "-4" },
            { -2, "-2" },
            { 0, "0" }
        };
        compareTicks(ticks, expected);
    }

    void linear_0_m10_5()
    {
        auto ticks = ScaleTickIntervals::linear({ 0, -10, 5 });
        ScaleTickIntervals::Ticks expected {
            { -10, "-10" },
            { -8, "-8" },
            { -6, "-6" },
            { -4, "-4" },
            { -2, "-2" },
            { 0, "0" }
        };
        compareTicks(ticks, expected);
    }

    void linear_0_0p1_5()
    {
        auto ticks = ScaleTickIntervals::linear({ 0, 0.1, 5 });
        ScaleTickIntervals::Ticks expected {
            { 0.00, "0.00" },
            { 0.02, "0.02" },
            { 0.04, "0.04" },
            { 0.06, "0.06" },
            { 0.08, "0.08" },
            { 0.10, "0.10" }
        };
        compareTicks(ticks, expected);
    }

    void linear_0_0p01_5()
    {
        auto ticks = ScaleTickIntervals::linear({ 0, 0.01, 5 });
        ScaleTickIntervals::Ticks expected {
            { 0.000, "0.000" },
            { 0.002, "0.002" },
            { 0.004, "0.004" },
            { 0.006, "0.006" },
            { 0.008, "0.008" },
            { 0.010, "0.010" }
        };
        compareTicks(ticks, expected);
    }

    void linear_0_0p005_5()
    {
        auto ticks = ScaleTickIntervals::linear({ 0, 0.005, 5 });
        ScaleTickIntervals::Ticks expected {
            { 0.000, "0.000" },
            { 0.001, "0.001" },
            { 0.002, "0.002" },
            { 0.003, "0.003" },
            { 0.004, "0.004" },
            { 0.005, "0.005" }
        };
        compareTicks(ticks, expected);
    }

    void linear_0_0p001_5()
    {
        auto ticks = ScaleTickIntervals::linear({ 0, 0.001, 5 });
        ScaleTickIntervals::Ticks expected {
            { 0.0000, "0.0e+00" },
            { 0.0002, "2.0e-04" },
            { 0.0004, "4.0e-04" },
            { 0.0006, "6.0e-04" },
            { 0.0008, "8.0e-04" },
            { 0.0010, "1.0e-03" }
        };
        compareTicks(ticks, expected);
    }
    
    void linear_1_1p001_5()
    {
        auto ticks = ScaleTickIntervals::linear({ 1, 1.001, 5 });
        ScaleTickIntervals::Ticks expected {
            { 1.0000, "1.0000" },
            { 1.0002, "1.0002" },
            { 1.0004, "1.0004" },
            { 1.0006, "1.0006" },
            { 1.0008, "1.0008" },
            { 1.0010, "1.0010" }
        };
        compareTicks(ticks, expected);
    }
    
    void linear_0p001_1_5()
    {
        auto ticks = ScaleTickIntervals::linear({ 0.001, 1, 5 });
        ScaleTickIntervals::Ticks expected {
            { 0.1, "0.1" },
            { 0.3, "0.3" },
            { 0.5, "0.5" },
            { 0.7, "0.7" },
            { 0.9, "0.9" },
        };
        compareTicks(ticks, expected);
    }
        
    void linear_10000_10010_5()
    {
        auto ticks = ScaleTickIntervals::linear({ 10000, 10010, 5 });
        ScaleTickIntervals::Ticks expected {
            { 10000, "10000" },
            { 10002, "10002" },
            { 10004, "10004" },
            { 10006, "10006" },
            { 10008, "10008" },
            { 10010, "10010" },
        };
        compareTicks(ticks, expected);
    }
    
    void linear_10000_20000_5()
    {
        auto ticks = ScaleTickIntervals::linear({ 10000, 20000, 5 });
        ScaleTickIntervals::Ticks expected {
            { 10000, "10000" },
            { 12000, "12000" },
            { 14000, "14000" },
            { 16000, "16000" },
            { 18000, "18000" },
            { 20000, "20000" },
        };
        compareTicks(ticks, expected);
    }
    
    void linear_m1_1_10()
    {
        auto ticks = ScaleTickIntervals::linear({ -1, 1, 10 });
        ScaleTickIntervals::Ticks expected {
            { -1.0, "-1.0" },
            { -0.8, "-0.8" },
            { -0.6, "-0.6" },
            { -0.4, "-0.4" },
            { -0.2, "-0.2" },
            { 0.0, "0.0" },
            { 0.2, "0.2" },
            { 0.4, "0.4" },
            { 0.6, "0.6" },
            { 0.8, "0.8" },
            { 1.0, "1.0" }
        };
        compareTicks(ticks, expected);
    }

    void linear_221p23_623p7_57p4()
    {
        auto ticks = ScaleTickIntervals::linear({ 221.23, 623.7, 4 });
        // only 4 ticks, not 5, because none of the rounded tick
        // values lies on an end value
        ScaleTickIntervals::Ticks expected {
            { 300, "300" },
            { 400, "400" },
            { 500, "500" },
            { 600, "600" },
        };
        compareTicks(ticks, expected);
    }

    void linear_sqrt2_pi_7()
    {
        auto ticks = ScaleTickIntervals::linear({ sqrt(2.0), M_PI, 7 });
        // This would be better in steps of 0.25, but we only round to
        // integral powers of ten
        ScaleTickIntervals::Ticks expected {
            { 1.5, "1.5" },
            { 1.7, "1.7" },
            { 1.9, "1.9" },
            { 2.1, "2.1" },
            { 2.3, "2.3" },
            { 2.5, "2.5" },
            { 2.7, "2.7" },
            { 2.9, "2.9" },
            { 3.1, "3.1" },
        };
        compareTicks(ticks, expected);
    }

    void linear_pi_avogadro_7()
    {
        auto ticks = ScaleTickIntervals::linear({ M_PI, 6.022140857e23, 7 });
        ScaleTickIntervals::Ticks expected {
            // not perfect, but ok-ish
            { 0, "0.0e+00" },
            { 9e+22, "9.0e+22" },
            { 1.8e+23, "1.8e+23" },
            { 2.7e+23, "2.7e+23" },
            { 3.6e+23, "3.6e+23" },
            { 4.5e+23, "4.5e+23" },
            { 5.4e+23, "5.4e+23" },
        };
        compareTicks(ticks, expected);
    }

    void linear_2_3_1()
    {
        auto ticks = ScaleTickIntervals::linear({ 2, 3, 1 });
        ScaleTickIntervals::Ticks expected {
            { 2.0, "2" },
            { 3.0, "3" }
        };
        compareTicks(ticks, expected);
    }

    void linear_2_3_2()
    {
        auto ticks = ScaleTickIntervals::linear({ 2, 3, 2 });
        ScaleTickIntervals::Ticks expected {
            { 2.0, "2.0" },
            { 2.5, "2.5" },
            { 3.0, "3.0" }
        };
        compareTicks(ticks, expected);
    }

    void linear_2_3_3()
    {
        auto ticks = ScaleTickIntervals::linear({ 2, 3, 3 });
        ScaleTickIntervals::Ticks expected {
            { 2.0, "2.0" },
            { 2.3, "2.3" },
            { 2.6, "2.6" },
            { 2.9, "2.9" }
        };
        compareTicks(ticks, expected);
    }

    void linear_2_3_4()
    {
        auto ticks = ScaleTickIntervals::linear({ 2, 3, 4 });
        // This would be better in steps of 0.25, but we only round to
        // integral powers of ten
        ScaleTickIntervals::Ticks expected {
            { 2.0, "2.0" },
            { 2.3, "2.3" },
            { 2.6, "2.6" },
            { 2.9, "2.9" }
        };
        compareTicks(ticks, expected);
    }

    void linear_2_3_5()
    {
        auto ticks = ScaleTickIntervals::linear({ 2, 3, 5 });
        ScaleTickIntervals::Ticks expected {
            { 2.0, "2.0" },
            { 2.2, "2.2" },
            { 2.4, "2.4" },
            { 2.6, "2.6" },
            { 2.8, "2.8" },
            { 3.0, "3.0" }
        };
        compareTicks(ticks, expected);
    }

    void linear_2_3_6()
    {
        auto ticks = ScaleTickIntervals::linear({ 2, 3, 6 });
        ScaleTickIntervals::Ticks expected {
            { 2.0, "2.0" },
            { 2.2, "2.2" },
            { 2.4, "2.4" },
            { 2.6, "2.6" },
            { 2.8, "2.8" },
            { 3.0, "3.0" }
        };
        compareTicks(ticks, expected);
    }

    void linear_1_1_10()
    {
        // pathological range
        auto ticks = ScaleTickIntervals::linear({ 1, 1, 10 });
        ScaleTickIntervals::Ticks expected {
            { 1.0, "1" }
        };
        compareTicks(ticks, expected);
    }
    
    void linear_0_0_10()
    {
        // pathological range
        auto ticks = ScaleTickIntervals::linear({ 0, 0, 10 });
        ScaleTickIntervals::Ticks expected {
            { 0.0, "0.0" }
        };
        compareTicks(ticks, expected);
    }
    
    void linear_0_1_1()
    {
        auto ticks = ScaleTickIntervals::linear({ 0, 1, 1 });
        ScaleTickIntervals::Ticks expected {
            { 0.0, "0" },
            { 1.0, "1" }
        };
        compareTicks(ticks, expected);
    }
    
    void linear_0_1_0()
    {
        // senseless input
        auto ticks = ScaleTickIntervals::linear({ 0, 1, 0 });
        ScaleTickIntervals::Ticks expected {
            { 0.0, "0.0" },
        };
        compareTicks(ticks, expected);
    }
    
    void linear_0_1_m1()
    {
        // senseless input
        auto ticks = ScaleTickIntervals::linear({ 0, 1, -1 });
        ScaleTickIntervals::Ticks expected {
            { 0.0, "0.0" },
        };
        compareTicks(ticks, expected);
    }

    void linear_0p465_778_10()
    {
        // a case that gave unsatisfactory results in real life
        // (initially it had the first tick at 1)
        auto ticks = ScaleTickIntervals::linear({ 0.465, 778.08, 10 });
        ScaleTickIntervals::Ticks expected {
            { 10, "10" },
            { 90, "90" },
            { 170, "170" },
            { 250, "250" },
            { 330, "330" },
            { 410, "410" },
            { 490, "490" },
            { 570, "570" },
            { 650, "650" },
            { 730, "730" },
        };
        compareTicks(ticks, expected);
    }
    
    void log_1_10_2()
    {
        auto ticks = ScaleTickIntervals::logarithmic({ 1, 10, 2 });
        ScaleTickIntervals::Ticks expected {
            { 1.0, "1.0" },
            { 3.2, "3.2" },
            { 10.0, "10" },
        };
        compareTicks(ticks, expected);
    }
    
    void log_0_10_2()
    {
        auto ticks = ScaleTickIntervals::logarithmic({ 0, 10, 2 });
        ScaleTickIntervals::Ticks expected {
            { 1e-6, "1e-06" },
            { 1, "1" },
        };
        compareTicks(ticks, expected);
    }

    void log_pi_avogadro_7()
    {
        auto ticks = ScaleTickIntervals::logarithmic({ M_PI, 6.022140857e23, 7 });
        ScaleTickIntervals::Ticks expected {
            { 1000, "1000" },
            { 1e+06, "1e+06" },
            { 1e+09, "1e+09" },
            { 1e+12, "1e+12" },
            { 1e+15, "1e+15" },
            { 1e+18, "1e+18" },
            { 1e+21, "1e+21" },
        };
        compareTicks(ticks, expected, true);
    }

    void log_0p465_778_10()
    {
        auto ticks = ScaleTickIntervals::logarithmic({ 0.465, 778.08, 10 });
        ScaleTickIntervals::Ticks expected {
            { 0.5, "0.50" },
            { 1, "1.0" },
            { 2, "2.0" },
            { 4, "4.0" },
            { 8, "8.0" },
            { 16, "16" },
            { 32, "32" },
            { 64, "64" },
            { 130, "130" },
            { 260, "260" },
            { 510, "510" },
        };
        compareTicks(ticks, expected);
    }
    
    void log_1_10k_10()
    {
        auto ticks = ScaleTickIntervals::logarithmic({ 1.0, 10000.0, 10 });
        ScaleTickIntervals::Ticks expected {
            { 1.0, "1.0" },
            { 2.5, "2.5" },
            { 6.3, "6.3" },
            { 16.0, "16" },
            { 40.0, "40" },
            { 100.0, "100" },
            { 250.0, "250" },
            { 630.0, "630" },
            { 1600.0, "1600" },
            { 4000.0, "4000" },
            { 10000.0, "1e+04" },
        };
        compareTicks(ticks, expected, true);
    }
    
    void log_80_10k_6()
    {
        auto ticks = ScaleTickIntervals::logarithmic({ 80.0, 10000.0, 6 });
        ScaleTickIntervals::Ticks expected {
            { 130, "130" },
            { 260, "260" },
            { 510, "510" },
            { 1000, "1000" },
            { 2000, "2000" },
            { 4100, "4100" },
            { 8200, "8200" }
        };
        compareTicks(ticks, expected, true);
    }
    
    void log_80_800k_10()
    {
        auto ticks = ScaleTickIntervals::logarithmic({ 80.0, 800000.0, 10 });
        ScaleTickIntervals::Ticks expected {
            { 100, "100" },
            { 250, "250" },
            { 630, "630" },
            { 1600, "1600" },
            { 4000, "4000" },
            { 10000, "1e+04" },
            { 25000, "2.5e+04" },
            { 63000, "6.3e+04" },
            { 160000, "1.6e+05" },
            { 400000, "4e+05" },
        };
        compareTicks(ticks, expected, true);
    }
    
    void log_0_1_0()
    {
        // senseless input
        auto ticks = ScaleTickIntervals::logarithmic({ 0, 1, 0 });
        ScaleTickIntervals::Ticks expected {
        };
        compareTicks(ticks, expected);
    }
    
    void log_0_1_m1()
    {
        // senseless input
        auto ticks = ScaleTickIntervals::logarithmic({ 0, 1, -1 });
        ScaleTickIntervals::Ticks expected {
        };
        compareTicks(ticks, expected);
    }

};

#endif


