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

#ifndef SV_HELPER_EXEC_PATH_H
#define SV_HELPER_EXEC_PATH_H

#include <QStringList>

/**
 * Class to find helper executables that have been installed alongside
 * the application. There may be more than one executable available
 * with a given base name, because it's possible to have more than one
 * implementation of a given service. For example, a plugin helper or
 * scanner may exist in both 32-bit and 64-bit variants.
 *
 * This class encodes both the expected locations of helper
 * executables, and the expected priority between different
 * implementations (e.g. preferring the architecture that matches that
 * of the host).
 */
class HelperExecPath
{
public:
    enum SearchType {
        NativeArchitectureOnly,
        AllInstalled
    };
    
    HelperExecPath(SearchType type) : m_type(type) { }
    
    /**
     * Find a helper executable with the given base name in the bundle
     * directory or installation location, if one exists, and return
     * its full path. Equivalent to calling getHelperExecutables() and
     * taking the first result from the returned list (or "" if empty).
     */
    QString getHelperExecutable(QString basename);

    struct HelperExec {
        QString executable;
        QString tag;
    };
    
    /**
     * Find all helper executables with the given base name in the
     * bundle directory or installation location, and return their
     * full paths in order of priority. The "tag" string contains an
     * identifier for the location or architecture of the helper, for
     * example "32", "64", "js" etc. An empty tag signifies a default
     * helper that matches the application's architecture.
     */
    QList<HelperExec> getHelperExecutables(QString basename);

    /**
     * Return the list of directories searched for helper
     * executables.
     */
    QStringList getHelperDirPaths();
    
    /**
     * Return the list of executable paths examined in the search for
     * the helper executable with the given basename.
     */
    QStringList getHelperCandidatePaths(QString basename);

private:
    SearchType m_type;
    QList<HelperExec> search(QString, QStringList &);
    QStringList getTags();
};

#endif
