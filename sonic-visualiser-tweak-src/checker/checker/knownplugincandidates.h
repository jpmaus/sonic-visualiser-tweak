/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/*
    Copyright (c) 2016-2018 Queen Mary, University of London

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of the Centre for
    Digital Music and Queen Mary, University of London shall not be
    used in advertising or otherwise to promote the sale, use or other
    dealings in this Software without prior written authorization.
*/

#ifndef KNOWN_PLUGIN_CANDIDATES_H
#define KNOWN_PLUGIN_CANDIDATES_H

#include "plugincandidates.h"
#include "knownplugins.h"

#include <string>
#include <map>
#include <vector>

/**
 * Class to identify and list candidate shared-library files possibly
 * containing plugins in a hardcoded set of known formats. Uses a
 * separate process (the "helper", whose executable name must be
 * provided at construction) to test-load each library in order to
 * winnow out any that fail to load or crash on load.
 *
 * In v1 of the checker library this was the role of the KnownPlugins
 * class, but that has been changed so that it only provides static
 * known information about plugin formats and paths, and this class
 * has been introduced instead (tying that static information together
 * with a PluginCandidates object that handles the run-time query).
 *
 * Requires C++11 and the Qt5 QtCore library.
 */
class KnownPluginCandidates
{
    typedef std::vector<std::string> stringlist;
    
public:
    KnownPluginCandidates(std::string helperExecutableName,
                          PluginCandidates::LogCallback *cb = 0);
    
    std::vector<KnownPlugins::PluginType> getKnownPluginTypes() const {
        return m_known.getKnownPluginTypes();
    }

    std::string getTagFor(KnownPlugins::PluginType type) const {
        return m_known.getTagFor(type);
    }
    
    stringlist getCandidateLibrariesFor(KnownPlugins::PluginType type) const {
        return m_candidates.getCandidateLibrariesFor(m_known.getTagFor(type));
    }

    std::string getHelperExecutableName() const {
        return m_helperExecutableName;
    }

    std::vector<PluginCandidates::FailureRec> getFailures() const;

    /** Return a non-localised HTML failure report */
    std::string getFailureReport() const;
    
private:
    KnownPlugins m_known;
    PluginCandidates m_candidates;
    std::string m_helperExecutableName;
};

#endif

