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

#ifndef SV_PLUGIN_PATH_SETTER_H
#define SV_PLUGIN_PATH_SETTER_H

#include <QString>
#include <QStringList>
#include <QMutex>

#include <map>

#include "checker/knownplugins.h"

class PluginPathSetter
{
public:
    typedef std::pair<KnownPlugins::PluginType,
                      KnownPlugins::BinaryFormat> TypeKey;

    typedef std::vector<TypeKey> TypeKeys;

    struct PathConfig {
        QStringList directories; // Actual list of directories arising
                                 // from user settings, environment
                                 // variables, and defaults as
                                 // appropriate
        
        QString envVariable; // Name of env var, e.g. LADSPA_PATH
        
        bool useEnvVariable; // True if env variable should override
                             // any user settings for this
    };

    typedef std::map<TypeKey, PathConfig> Paths;

    /// Update *_PATH environment variables from the settings, on
    /// application startup. Must be called exactly once, before any
    /// of the other functions in this class has been called
    static void initialiseEnvironmentVariables();

    /// Return default values of paths only, without any environment
    /// variables or user-defined preferences
    static Paths getDefaultPaths();

    /// Return paths arising from environment variables only, falling
    /// back to the defaults, without any user-defined preferences
    static Paths getEnvironmentPaths();

    /// Return paths arising from user settings + environment
    /// variables + defaults as appropriate
    static Paths getPaths();

    /// Save the given paths to the settings
    static void savePathSettings(Paths paths);

    /// Return the original value observed on startup for the given
    /// environment variable, if it is one of the variables used by a
    /// known path config.
    static QString getOriginalEnvironmentValue(QString envVariable);
    
private:
    static Paths m_defaultPaths;
    static Paths m_environmentPaths;
    static std::map<QString, QString> m_originalEnvValues;
    static TypeKeys m_supportedKeys;
    static QMutex m_mutex;

    static std::vector<TypeKey> getSupportedKeys();
    static Paths getEnvironmentPathsUncached(const TypeKeys &keys);
    static QString getSettingTagFor(TypeKey);
};

#endif
