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

#include <iostream>

using namespace std;

struct LogCallback : PluginCandidates::LogCallback {
    virtual void log(string message) {
        cerr << "checker: log: " << message << "\n";
    }
};

int main(int, char **)
{
    LogCallback cb;
    KnownPluginCandidates kp("./vamp-plugin-load-checker", &cb);
    
    for (auto t: kp.getKnownPluginTypes()) {
        cout << "successful libraries for plugin type \""
             << kp.getTagFor(t) << "\":" << endl;
        for (auto lib: kp.getCandidateLibrariesFor(t)) {
            cout << lib << endl;
        }
    }

    cout << "Failure message (if any):" << endl;
    cout << kp.getFailureReport() << endl;
}

