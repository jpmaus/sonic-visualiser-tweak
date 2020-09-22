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

#ifndef SV_FEATURE_EXTRACTION_PLUGIN_FACTORY_H
#define SV_FEATURE_EXTRACTION_PLUGIN_FACTORY_H

#include <vamp-hostsdk/Plugin.h>

#include "vamp-support/PluginStaticData.h"

#include "base/BaseTypes.h"

#include <QString>

class FeatureExtractionPluginFactory
{
public:
    static FeatureExtractionPluginFactory *instance();
    
    virtual ~FeatureExtractionPluginFactory() { }

    /**
     * Return all installed plugin identifiers.
     */
    virtual std::vector<QString> getPluginIdentifiers(QString &errorMsg) = 0;
    
    /**
     * Return static data for the given plugin.
     */
    virtual piper_vamp::PluginStaticData getPluginStaticData(QString ident) = 0;
    
    /**
     * Instantiate (load) and return pointer to the plugin with the
     * given identifier, at the given sample rate. We don't set
     * blockSize or channels on this -- they're negotiated and handled
     * via initialize() on the plugin itself after loading.
     */
    virtual Vamp::Plugin *instantiatePlugin(QString identifier,
                                            sv_samplerate_t inputSampleRate) = 0;
    
    /**
     * Get category metadata about a plugin (without instantiating it).
     */
    virtual QString getPluginCategory(QString identifier) = 0;

    /**
     * Get the full file path (including both directory and filename)
     * of the library file that provides a given plugin
     * identifier. Note getPluginIdentifiers() must have been called
     * before this has access to the necessary information.
     */
    virtual QString getPluginLibraryPath(QString identifier) = 0;
};

#endif
