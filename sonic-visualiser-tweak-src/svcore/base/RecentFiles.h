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

#ifndef SV_RECENT_FILES_H
#define SV_RECENT_FILES_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <vector>
#include <deque>

/**
 * RecentFiles manages a list of recently-used identifier strings,
 * saving and restoring that list via QSettings.  The identifiers do
 * not actually have to refer to files.
 *
 * Each entry must have a non-empty identifier, which is typically a
 * filename, path, URI, or internal id, and may optionally also have a
 * label, which is typically a user-visible convenience.
 *
 * RecentFiles is thread-safe - all access is serialised.
 */
class RecentFiles : public QObject
{
    Q_OBJECT

public:
    /**
     * Construct a RecentFiles object that saves and restores in the
     * given QSettings group and truncates when the given count of
     * identifiers is reached.
     */
    RecentFiles(QString settingsGroup = "RecentFiles",
                int maxCount = 10);

    virtual ~RecentFiles();

    /**
     * Return the settingsGroup as passed to the constructor.
     */
    QString getSettingsGroup() const {
        return m_settingsGroup;
    }

    /**
     * Return the maxCount as passed to the constructor.
     */
    int getMaxCount() const {
        return m_maxCount;
    }

    /**
     * Return the list of recent identifiers, without labels.
     */
    std::vector<QString> getRecentIdentifiers() const;

    /**
     * Return the list of recent identifiers, without labels. This is
     * an alias for getRecentIdentifiers included for backward
     * compatibility.
     */
    std::vector<QString> getRecent() const {
        return getRecentIdentifiers();
    }

    /**
     * Return the list of recent identifiers, with labels. Each
     * returned entry is a pair of identifier and label in that order.
     */
    std::vector<std::pair<QString, QString>> getRecentEntries() const;
    
    /**
     * Add a literal identifier, optionally with a label.
     *
     * If the identifier already exists in the recent entries list, it
     * is moved to the front of the list and its label is replaced
     * with the given one.
     */
    void add(QString identifier, QString label = "");
    
    /**
     * Add a name that is known to be either a file path or a URL,
     * optionally with a label.  If it looks like a URL, add it
     * literally; otherwise treat it as a file path and canonicalise
     * it appropriately.  Also take into account the user preference
     * for whether to include temporary files in the recent files
     * menu: the file will not be added if the preference is set and
     * the file appears to be a temporary one.
     *
     * If the identifier derived from the file path already exists in
     * the recent entries list, it is moved to the front of the list
     * and its label is replaced with the given one.
     */
    void addFile(QString filepath, QString label = "");

signals:
    void recentChanged();

private:
    mutable QMutex m_mutex;

    const QString m_settingsGroup;
    const int m_maxCount;

    std::deque<std::pair<QString, QString>> m_entries; // identifier, label

    void read();
    void write();
    void truncateAndWrite();
};

#endif
