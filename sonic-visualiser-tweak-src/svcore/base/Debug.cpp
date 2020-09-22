/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2010-2011 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "Debug.h"
#include "ResourceFinder.h"

#include <QMutex>
#include <QDir>
#include <QUrl>
#include <QCoreApplication>
#include <QDateTime>

#include <stdexcept>

static SVDebug *svdebug = nullptr;
static SVCerr *svcerr = nullptr;
static QMutex mutex;

SVDebug &getSVDebug() {
    mutex.lock();
    if (!svdebug) {
        svdebug = new SVDebug();
    }
    mutex.unlock();
    return *svdebug;
}

SVCerr &getSVCerr() {
    mutex.lock();
    if (!svcerr) {
        if (!svdebug) {
            svdebug = new SVDebug();
        }
        svcerr = new SVCerr(*svdebug);
    }
    mutex.unlock();
    return *svcerr;
}

bool SVDebug::m_silenced = false;
bool SVCerr::m_silenced = false;

SVDebug::SVDebug() :
    m_prefix(nullptr),
    m_ok(false),
    m_eol(true)
{
    if (m_silenced) return;
    
    if (qApp->applicationName() == "") {
        cerr << "ERROR: Can't use SVDEBUG before setting application name" << endl;
        throw std::logic_error("Can't use SVDEBUG before setting application name");
    }
    
    QString pfx = ResourceFinder().getUserResourcePrefix();
    QDir logdir(QString("%1/%2").arg(pfx).arg("log"));

    m_prefix = strdup(QString("[%1]")
                      .arg(QCoreApplication::applicationPid())
                      .toLatin1().data());

    //!!! what to do if mkpath fails?
    if (!logdir.exists()) logdir.mkpath(logdir.path());

    QString fileName = logdir.path() + "/sv-debug.log";

    m_stream.open(fileName.toLocal8Bit().data(), std::ios_base::out);

    if (!m_stream) {
        QDebug(QtWarningMsg) << (const char *)m_prefix
                             << "Failed to open debug log file "
                             << fileName << " for writing";
    } else {
        m_ok = true;
        cerr << "Log file is " << fileName << endl;
        (*this) << "Debug log started at "
                << QDateTime::currentDateTime().toString() << endl;
    }
}

SVDebug::~SVDebug()
{
    if (m_stream) m_stream.close();
}

QDebug &
operator<<(QDebug &dbg, const std::string &s)
{
    dbg << QString::fromUtf8(s.c_str());
    return dbg;
}

std::ostream &
operator<<(std::ostream &target, const QString &str)
{
    return target << str.toStdString();
}

std::ostream &
operator<<(std::ostream &target, const QUrl &u)
{
    return target << "<" << u.toString().toStdString() << ">";
}

