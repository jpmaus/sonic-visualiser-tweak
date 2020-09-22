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

#ifndef HIT_COUNT_H
#define HIT_COUNT_H

#include <string>
#include <iostream>

/**
 * Profile class for counting cache hits and the like.
 */
#ifndef NO_HIT_COUNTS

class HitCount
{
public:
    HitCount(std::string name) :
        m_name(name),
        m_hit(0),
        m_partial(0),
        m_miss(0)
    { }
    
    ~HitCount() {
        using namespace std;
        int total = m_hit + m_partial + m_miss;
        cerr << "Hit count: " << m_name << ": ";
        if (m_partial > 0) {
            cerr << m_hit << " hits, " << m_partial << " partial, "
                 << m_miss << " misses";
        } else {
            cerr << m_hit << " hits, " << m_miss << " misses";
        }
        if (total > 0) {
            if (m_partial > 0) {
                cerr << " (" << ((m_hit * 100.0) / total) << "%, "
                     << ((m_partial * 100.0) / total) << "%, "
                     << ((m_miss * 100.0) / total) << "%)";
            } else {
                cerr << " (" << ((m_hit * 100.0) / total) << "%, "
                     << ((m_miss * 100.0) / total) << "%)";
            }
        }
        cerr << endl;
    }

    void hit() { ++m_hit; }
    void partial() { ++m_partial; }
    void miss() { ++m_miss; }

private:
    std::string m_name;
    int m_hit;
    int m_partial;
    int m_miss;
};

#else // NO_HIT_COUNTS

class HitCount
{
public:
    HitCount(std::string) {}

    void hit() {}
    void partial() {}
    void miss() {}
};

#endif

#endif
