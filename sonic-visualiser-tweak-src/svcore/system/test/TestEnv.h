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

#ifndef TEST_ENV_H
#define TEST_ENV_H

#include "../System.h"

#include <QObject>
#include <QtTest>
#include <QDir>

#include <iostream>

using namespace std;

const std::string utf8_name_sprkt = "\343\202\271\343\203\235\343\203\274\343\202\257\343\201\256\345\257\272\351\231\242";

class TestEnv : public QObject
{
    Q_OBJECT

private slots:
    void getAbsent()
    {
        string value = "blather";
        bool rv = getEnvUtf8("nonexistent_environment_variable_I_sincerely_hope_including_a_missspellling_just_to_be_sure",
                             value);
        QCOMPARE(rv, false);
        QCOMPARE(value, string());
    }

    void getExpected()
    {
        string value;
        bool rv = getEnvUtf8("PATH", value);
        QCOMPARE(rv, true);
        QVERIFY(value != "");
        QVERIFY(value.size() > 5); // Not quite but nearly certain,
                                   // and weeds out an unfortunate
                                   // case where we accidentally
                                   // returned the variable's name
                                   // instead of its value!
    }
    
    void roundTripAsciiAscii()
    {
        bool rv = false;
        rv = putEnvUtf8("SV_CORE_TEST_SYSTEM_RT_A_A", "EXPECTED_VALUE");
        QCOMPARE(rv, true);
        string value;
        rv = getEnvUtf8("SV_CORE_TEST_SYSTEM_RT_A_A", value);
        QCOMPARE(rv, true);
        QCOMPARE(value, string("EXPECTED_VALUE"));
    }
    
    void roundTripAsciiUtf8()
    {
        bool rv = false;
        rv = putEnvUtf8("SV_CORE_TEST_SYSTEM_RT_A_U", utf8_name_sprkt);
        QCOMPARE(rv, true);
        string value;
        rv = getEnvUtf8("SV_CORE_TEST_SYSTEM_RT_A_U", value);
        QCOMPARE(rv, true);
        QCOMPARE(value, utf8_name_sprkt);
    }
    
    void roundTripUtf8Ascii()
    {
        bool rv = false;
        rv = putEnvUtf8("SV_CORE_TEST_SYSTEM_RT_\351\207\215\345\272\206_A", "EXPECTED_VALUE");
        QCOMPARE(rv, true);
        string value;
        rv = getEnvUtf8("SV_CORE_TEST_SYSTEM_RT_\351\207\215\345\272\206_A", value);
        QCOMPARE(rv, true);
        QCOMPARE(value, string("EXPECTED_VALUE"));
    }

    void roundTripUtf8Utf8()
    {
        bool rv = false;
        rv = putEnvUtf8("SV_CORE_TEST_SYSTEM_RT_\351\207\215\345\272\206_A", utf8_name_sprkt);
        QCOMPARE(rv, true);
        string value;
        rv = getEnvUtf8("SV_CORE_TEST_SYSTEM_RT_\351\207\215\345\272\206_A", value);
        QCOMPARE(rv, true);
        QCOMPARE(value, utf8_name_sprkt);
    }
};

#endif

    

    
        
