/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_SERIALISER_H
#define SV_SERIALISER_H

#include <QString>
#include <QMutex>

#include <map>

class Serialiser
{
public:
    Serialiser(QString id);
    ~Serialiser();

    QString getId() const { return m_id; }

protected:
    QString m_id;
    static QMutex m_mapMutex;
    static std::map<QString, QMutex *> m_mutexMap;
};

#endif
