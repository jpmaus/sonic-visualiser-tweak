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

#include "PluginPathSetter.h"

#include <vamp-hostsdk/PluginHostAdapter.h>

#include "RealTimePluginFactory.h"
#include "LADSPAPluginFactory.h"
#include "DSSIPluginFactory.h"

#include <QSettings>
#include <QMutexLocker>

#include "system/System.h"
#include "base/Preferences.h"
#include "base/HelperExecPath.h"

QMutex
PluginPathSetter::m_mutex;

PluginPathSetter::Paths
PluginPathSetter::m_defaultPaths;

PluginPathSetter::Paths
PluginPathSetter::m_environmentPaths;

std::map<QString, QString>
PluginPathSetter::m_originalEnvValues;

PluginPathSetter::TypeKeys
PluginPathSetter::m_supportedKeys;

using namespace std;

PluginPathSetter::TypeKeys
PluginPathSetter::getSupportedKeys()
{
    QMutexLocker locker(&m_mutex);

    if (!m_supportedKeys.empty()) {
        return m_supportedKeys;
    }

    TypeKeys keys;
    keys.push_back({ KnownPlugins::VampPlugin, KnownPlugins::FormatNative });
    
    bool inProcess = Preferences::getInstance()->getRunPluginsInProcess();
    HelperExecPath hep(inProcess ?
                       HelperExecPath::NativeArchitectureOnly :
                       HelperExecPath::AllInstalled);
    auto execs = hep.getHelperExecutables("vamp-plugin-load-checker");
    if (execs.size() > 1) {
        keys.push_back({
                KnownPlugins::VampPlugin, KnownPlugins::FormatNonNative32Bit });
    }

    keys.push_back({ KnownPlugins::LADSPAPlugin, KnownPlugins::FormatNative });
    keys.push_back({ KnownPlugins::DSSIPlugin, KnownPlugins::FormatNative });

    m_supportedKeys = keys;
    return keys;
}

// call with mutex held please
PluginPathSetter::Paths
PluginPathSetter::getEnvironmentPathsUncached(const TypeKeys &keys)
{
    Paths paths;

    for (auto k: keys) {

        KnownPlugins kp(k.second);

        auto path = kp.getPathFor(k.first);
        QStringList qPath;
        for (auto s: path) {
            qPath.push_back(QString::fromStdString(s));
        }

        auto var = kp.getPathEnvironmentVariableFor(k.first);
        QString qVar = QString::fromStdString(var);
        
        paths[k] = { qPath, qVar, true };
    }

    return paths;
}

PluginPathSetter::Paths
PluginPathSetter::getDefaultPaths()
{
    TypeKeys keys = getSupportedKeys();
    
    QMutexLocker locker(&m_mutex);

    Paths paths;

    for (auto k: keys) {

        KnownPlugins kp(k.second);

        auto path = kp.getDefaultPathFor(k.first);
        QStringList qPath;
        for (auto s: path) {
            qPath.push_back(QString::fromStdString(s));
        }

        auto var = kp.getPathEnvironmentVariableFor(k.first);
        QString qVar = QString::fromStdString(var);
        
        paths[k] = { qPath, qVar, true };
    }

    return paths;
}

PluginPathSetter::Paths
PluginPathSetter::getEnvironmentPaths()
{
    TypeKeys keys = getSupportedKeys();
    
    QMutexLocker locker(&m_mutex);

    if (!m_environmentPaths.empty()) {
        return m_environmentPaths;
    }
        
    m_environmentPaths = getEnvironmentPathsUncached(keys);
    return m_environmentPaths;
}

QString
PluginPathSetter::getSettingTagFor(TypeKey tk)
{
    string tag = KnownPlugins(tk.second).getTagFor(tk.first);
    if (tk.second == KnownPlugins::FormatNonNative32Bit) {
        tag += "-32";
    }
    return QString::fromStdString(tag);
}

PluginPathSetter::Paths
PluginPathSetter::getPaths()
{
    Paths paths = getEnvironmentPaths();
       
    QSettings settings;
    settings.beginGroup("Plugins");

    for (auto p: paths) {

        TypeKey tk = p.first;

        QString settingTag = getSettingTagFor(tk);

        QStringList directories =
            settings.value(QString("directories-%1").arg(settingTag),
                           p.second.directories)
            .toStringList();
        QString envVariable =
            settings.value(QString("env-variable-%1").arg(settingTag),
                           p.second.envVariable)
            .toString();
        bool useEnvVariable =
            settings.value(QString("use-env-variable-%1").arg(settingTag),
                           p.second.useEnvVariable)
            .toBool();

        string envVarStr = envVariable.toStdString();
        string currentValue;
        (void)getEnvUtf8(envVarStr, currentValue);

        if (currentValue != "" && useEnvVariable) {
            directories = QString::fromStdString(currentValue).split(
#ifdef Q_OS_WIN
               ";"
#else
               ":"
#endif
                );
        }
        
        paths[tk] = { directories, envVariable, useEnvVariable };
    }

    settings.endGroup();

    return paths;
}

void
PluginPathSetter::savePathSettings(Paths paths)
{
    QSettings settings;
    settings.beginGroup("Plugins");

    for (auto p: paths) {
        QString settingTag = getSettingTagFor(p.first);
        settings.setValue(QString("directories-%1").arg(settingTag),
                          p.second.directories);
        settings.setValue(QString("env-variable-%1").arg(settingTag),
                          p.second.envVariable);
        settings.setValue(QString("use-env-variable-%1").arg(settingTag),
                          p.second.useEnvVariable);
    }

    settings.endGroup();
}

QString
PluginPathSetter::getOriginalEnvironmentValue(QString envVariable)
{
    if (m_originalEnvValues.find(envVariable) != m_originalEnvValues.end()) {
        return m_originalEnvValues.at(envVariable);
    } else {
        return QString();
    }
}

void
PluginPathSetter::initialiseEnvironmentVariables()
{
    // Set the relevant environment variables from user configuration,
    // so that later lookups through the standard APIs will follow the
    // same paths as we have in the user config

    // First ensure the default paths have been recorded for later, so
    // we don't erroneously re-read them from the environment
    // variables we've just set
    (void)getDefaultPaths();
    (void)getEnvironmentPaths();
    
    Paths paths = getPaths();

    for (auto p: paths) {
        QString envVariable = p.second.envVariable;
        string envVarStr = envVariable.toStdString();
        string currentValue;
        getEnvUtf8(envVarStr, currentValue);
        m_originalEnvValues[envVariable] = QString::fromStdString(currentValue);
        if (currentValue != "" && p.second.useEnvVariable) {
            // don't override
            SVDEBUG << "PluginPathSetter: for environment variable "
                    << envVariable << ", useEnvVariable setting is false; "
                    << "leaving current value alone: it is \""
                    << currentValue << "\"" << endl;
            continue;
        }
        QString separator =
#ifdef Q_OS_WIN
            ";"
#else
            ":"
#endif
            ;
        QString proposedValue = p.second.directories.join(separator);
        SVDEBUG << "PluginPathSetter: for environment variable "
                << envVariable << ", useEnvVariable setting is true or "
                << "variable is currently unset; "
                << "changing value from \"" << currentValue
                << "\" to setting preference of \"" << proposedValue
                << "\"" << endl;
        putEnvUtf8(envVarStr, proposedValue.toStdString());
    }
}

