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

#ifndef SV_NATIVE_VAMP_PLUGIN_FACTORY_H
#define SV_NATIVE_VAMP_PLUGIN_FACTORY_H

#include "FeatureExtractionPluginFactory.h"

#include <vector>
#include <map>

#include "base/Debug.h"

#include <QMutex>

/**
 * FeatureExtractionPluginFactory type for Vamp plugins hosted
 * in-process.
 */
class NativeVampPluginFactory : public FeatureExtractionPluginFactory
{
public:
    virtual ~NativeVampPluginFactory() { }

    virtual std::vector<QString> getPluginIdentifiers(QString &errorMessage)
        override;

    virtual piper_vamp::PluginStaticData getPluginStaticData(QString identifier)
        override;

    virtual Vamp::Plugin *instantiatePlugin(QString identifier,
                                            sv_samplerate_t inputSampleRate)
        override;

    virtual QString getPluginCategory(QString identifier) override;

    virtual QString getPluginLibraryPath(QString identifier) override;

protected:
    QMutex m_mutex;
    std::vector<QString> m_pluginPath;
    std::vector<QString> m_identifiers;
    std::map<QString, QString> m_taxonomy; // identifier -> category string
    std::map<QString, piper_vamp::PluginStaticData> m_pluginData; // identifier -> data (created opportunistically)
    std::map<QString, QString> m_libraries; // identifier -> full file path

    friend class PluginDeletionNotifyAdapter;
    void pluginDeleted(Vamp::Plugin *);
    std::map<Vamp::Plugin *, void *> m_handleMap;

    QString findPluginFile(QString soname, QString inDir = "");
    std::vector<QString> getPluginPath();
    void generateTaxonomy();
};

#endif
