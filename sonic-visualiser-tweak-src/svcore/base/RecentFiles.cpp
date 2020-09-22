/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "RecentFiles.h"

#include "Preferences.h"

#include <QFileInfo>
#include <QSettings>
#include <QRegExp>
#include <QMutexLocker>

RecentFiles::RecentFiles(QString settingsGroup, int maxCount) :
    m_settingsGroup(settingsGroup),
    m_maxCount(maxCount)
{
    read();
}

RecentFiles::~RecentFiles()
{
    // nothing
}

void
RecentFiles::read()
{
    // Private method - called only from constructor - no mutex lock required
    
    m_entries.clear();
    QSettings settings;
    settings.beginGroup(m_settingsGroup);

    for (int i = 0; i < 100; ++i) {

        QString idKey = QString("recent-%1").arg(i);
        QString identifier = settings.value(idKey, "").toString();
        if (identifier == "") break;

        QString labelKey = QString("recent-%1-label").arg(i);
        QString label = settings.value(labelKey, "").toString();
        
        if (i < m_maxCount) m_entries.push_back({ identifier, label });
        else {
            settings.setValue(idKey, "");
            settings.setValue(labelKey, "");
        }
    }

    settings.endGroup();
}

void
RecentFiles::write()
{
    // Private method - must be serialised at call site
    
    QSettings settings;
    settings.beginGroup(m_settingsGroup);

    for (int i = 0; i < m_maxCount; ++i) {
        QString idKey = QString("recent-%1").arg(i);
        QString labelKey = QString("recent-%1-label").arg(i);
        QString identifier;
        QString label;
        if (in_range_for(m_entries, i)) {
            identifier = m_entries[i].first;
            label = m_entries[i].second;
        }
        settings.setValue(idKey, identifier);
        settings.setValue(labelKey, label);
    }

    settings.endGroup();
}

void
RecentFiles::truncateAndWrite()
{
    // Private method - must be serialised at call site
    
    while (int(m_entries.size()) > m_maxCount) {
        m_entries.pop_back();
    }
    write();
}

std::vector<QString>
RecentFiles::getRecentIdentifiers() const
{
    QMutexLocker locker(&m_mutex);

    std::vector<QString> identifiers;
    for (int i = 0; i < m_maxCount; ++i) {
        if (i < (int)m_entries.size()) {
            identifiers.push_back(m_entries[i].first);
        }
    }
    
    return identifiers;
}

std::vector<std::pair<QString, QString>>
RecentFiles::getRecentEntries() const
{
    QMutexLocker locker(&m_mutex);

    std::vector<std::pair<QString, QString>> entries;
    for (int i = 0; i < m_maxCount; ++i) {
        if (i < (int)m_entries.size()) {
            entries.push_back(m_entries[i]);
        }
    }
    
    return entries;
}

void
RecentFiles::add(QString identifier, QString label)
{
    {
        QMutexLocker locker(&m_mutex);

        bool have = false;
        for (int i = 0; i < int(m_entries.size()); ++i) {
            if (m_entries[i].first == identifier) {
                have = true;
                break;
            }
        }
    
        if (!have) {
            m_entries.push_front({ identifier, label });
        } else {
            std::deque<std::pair<QString, QString>> newEntries;
            newEntries.push_back({ identifier, label });
            for (int i = 0; in_range_for(m_entries, i); ++i) {
                if (m_entries[i].first == identifier) continue;
                newEntries.push_back(m_entries[i]);
            }
            m_entries = newEntries;
        }

        truncateAndWrite();
    }
    
    emit recentChanged();
}

void
RecentFiles::addFile(QString filepath, QString label)
{
    static QRegExp schemeRE("^[a-zA-Z]{2,5}://");
    static QRegExp tempRE("[\\/][Tt]e?mp[\\/]");
    if (schemeRE.indexIn(filepath) == 0) {
        add(filepath, label);
    } else {
        QString absPath = QFileInfo(filepath).absoluteFilePath();
        if (tempRE.indexIn(absPath) != -1) {
            Preferences *prefs = Preferences::getInstance();
            if (prefs && !prefs->getOmitTempsFromRecentFiles()) {
                add(absPath, label);
            }
        } else {
            add(absPath, label);
        }
    }
}


