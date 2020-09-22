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

#include "knownplugincandidates.h"

#include <sstream>

using namespace std;

/** This returns true if the helper has a name ending in "-32". By our
 *  convention, this means that it is a 32-bit helper found on a
 *  64-bit system, so (depending on the OS) we may need to look in
 *  32-bit-specific paths. Note that is32bit() is *not* usually true
 *  on 32-bit systems; it's used specifically to indicate a
 *  "non-native" 32-bit helper.
 */
static
bool
is32bit(string helperExecutableName)
{
    return helperExecutableName.find("-32") != string::npos;
}

KnownPluginCandidates::KnownPluginCandidates(string helperExecutableName,
                                             PluginCandidates::LogCallback *cb) :
    m_known(is32bit(helperExecutableName) ?
            KnownPlugins::FormatNonNative32Bit :
            KnownPlugins::FormatNative),
    m_candidates(helperExecutableName),
    m_helperExecutableName(helperExecutableName)
{
    m_candidates.setLogCallback(cb);

    auto knownTypes = m_known.getKnownPluginTypes();
        
    for (auto type: knownTypes) {
        m_candidates.scan(m_known.getTagFor(type),
                          m_known.getPathFor(type),
                          m_known.getDescriptorFor(type));
    }
}

vector<PluginCandidates::FailureRec>
KnownPluginCandidates::getFailures() const
{
    vector<PluginCandidates::FailureRec> failures;

    for (auto t: getKnownPluginTypes()) {
        auto ff = m_candidates.getFailedLibrariesFor(m_known.getTagFor(t));
        failures.insert(failures.end(), ff.begin(), ff.end());
    }

    return failures;
}

string
KnownPluginCandidates::getFailureReport() const
{
    auto failures = getFailures();
    if (failures.empty()) return "";

    int n = int(failures.size());
    int i = 0;

    ostringstream os;
    
    os << "<ul>";
    for (auto f: failures) {
        os << "<li>" + f.library;
        if (f.message != "") {
            os << "<br><i>" + f.message + "</i>";
        } else {
            os << "<br><i>unknown error</i>";
        }
        os << "</li>";

        if (n > 10) {
            if (++i == 5) {
                os << "<li>(... and " << (n - i) << " further failures)</li>";
                break;
            }
        }
    }
    os << "</ul>";

    return os.str();
}
