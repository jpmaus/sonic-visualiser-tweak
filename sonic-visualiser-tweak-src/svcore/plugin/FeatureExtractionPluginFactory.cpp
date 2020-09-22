/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2016 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "PiperVampPluginFactory.h"
#include "NativeVampPluginFactory.h"

#include <QMutex>
#include <QMutexLocker>

#include "base/Preferences.h"
#include "base/Debug.h"

FeatureExtractionPluginFactory *
FeatureExtractionPluginFactory::instance()
{
    static QMutex mutex;
    static FeatureExtractionPluginFactory *instance = nullptr;

    QMutexLocker locker(&mutex);
    
    if (!instance) {

#ifdef HAVE_PIPER
        if (Preferences::getInstance()->getRunPluginsInProcess()) {
            SVDEBUG << "FeatureExtractionPluginFactory: in-process preference set, using native factory" << endl;
            instance = new NativeVampPluginFactory();
        } else {
            SVDEBUG << "FeatureExtractionPluginFactory: in-process preference not set, using Piper factory" << endl;
            instance = new PiperVampPluginFactory();
        }
#else
        SVDEBUG << "FeatureExtractionPluginFactory: no Piper support compiled in, using native factory" << endl;
        instance = new NativeVampPluginFactory();
#endif
    }

    return instance;
}
