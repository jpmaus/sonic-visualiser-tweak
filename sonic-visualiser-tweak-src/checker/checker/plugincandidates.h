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

#ifndef PLUGIN_CANDIDATES_H
#define PLUGIN_CANDIDATES_H

#include <string>
#include <vector>
#include <map>

#include "checkcode.h"

class QProcess;

/**
 * Class to identify and list candidate shared-library files possibly
 * containing plugins. Uses a separate process (the "helper", whose
 * executable name must be provided at construction) to test-load each
 * library in order to winnow out any that fail to load or crash on
 * load.
 *
 * Requires C++11 and the Qt5 QtCore library.
 */
class PluginCandidates
{
    typedef std::vector<std::string> stringlist;

public:
    /** Construct a PluginCandidates scanner that uses the given
     *  executable as its load check helper.
     */
    PluginCandidates(std::string helperExecutableName);

    struct LogCallback {
        virtual ~LogCallback() { }

        /// implementation is responsible for adding \n and flushing output
        virtual void log(std::string) = 0;
    };
    
    /** Set a callback to be called for log output.
     */
    void setLogCallback(LogCallback *cb);

    /** Scan the libraries found in the given plugin path (i.e. list
     *  of plugin directories), checking that the given descriptor
     *  symbol can be looked up in each. Store the results
     *  internally, associated with the given (arbitrary) tag, for
     *  later querying using getCandidateLibrariesFor() and
     *  getFailedLibrariesFor().
     *
     *  Not thread-safe.
     */
    void scan(std::string tag,
              stringlist pluginPath,
              std::string descriptorSymbolName);

    /** Return list of plugin library paths that were checked
     *  successfully during the scan for the given tag.
     */
    stringlist getCandidateLibrariesFor(std::string tag) const;
    
    struct FailureRec {

        /// Path of failed library file
        std::string library;

        /// General class of failure
        PluginCheckCode code;

        /// Optional additional system-level message, already translated
        std::string message;
    };

    /** Return list of failure reports arising from the prior scan for
     *  the given tag. 
     */
    std::vector<FailureRec> getFailedLibrariesFor(std::string tag) const;

private:
    std::string m_helper;
    std::map<std::string, stringlist> m_candidates;
    std::map<std::string, std::vector<FailureRec> > m_failures;
    LogCallback *m_logCallback;

    stringlist getLibrariesInPath(stringlist path);
    std::string getHelperCompatibilityVersion();
    stringlist runHelper(stringlist libraries, std::string descriptor);
    void recordResult(std::string tag, stringlist results);
    void logErrors(QProcess *);
    void log(std::string);
};

#endif
