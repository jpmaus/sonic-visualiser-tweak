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

#ifndef PLUGIN_SCAN_H
#define PLUGIN_SCAN_H

#include <QStringList>
#include <QMutex>
#include <vector>
#include <map>

#ifdef HAVE_PLUGIN_CHECKER_HELPER
#include "checker/knownplugincandidates.h"
#else
class KnownPluginCandidates {};
#endif

class PluginScan
{
public:
    static PluginScan *getInstance();

    /**
     * Carry out startup scan of available plugins. Do not call
     * getCandidateLibrariesFor() unless this has been called and
     * scanSucceeded() is returning true.
     */
    void scan();

    /**
     * Return true if scan() completed successfully. If the scan
     * failed, consider using the normal plugin path to load any
     * available plugins (as if they had all been found to be
     * loadable) rather than rejecting all of them -- i.e. consider
     * falling back on the behaviour of code from before the scan
     * logic was added.
     */
    bool scanSucceeded() const;
    
    enum PluginType {
        VampPlugin,
        LADSPAPlugin,
        DSSIPlugin
    };
    struct Candidate {
        QString libraryPath;    // full path, not just soname
        QString helperTag;      // identifies the helper that found it
                                // (see HelperExecPath) 
    };

    /**
     * Return the candidate plugin libraries of the given type that
     * were found by helpers during the startup scan.
     *
     * This could return an empty list for two reasons: the scan
     * succeeded but no libraries were found; or the scan failed. Call
     * scanSucceeded() to distinguish between them.
     */
    QList<Candidate> getCandidateLibrariesFor(PluginType) const;

    QString getStartupFailureReport() const;

private:
    PluginScan();
    ~PluginScan();

    void clear();

#ifdef HAVE_PLUGIN_CHECKER_HELPER
    QString formatFailureReport(QString helperTag,
                                std::vector<PluginCandidates::FailureRec>)
        const;
#endif

    mutable QMutex m_mutex; // while scanning; definitely can't multi-thread this
    
    std::map<QString, KnownPluginCandidates *> m_kp; // tag -> KnownPlugins client
    bool m_succeeded;

    class Logger;
    Logger *m_logger;
};

#endif
