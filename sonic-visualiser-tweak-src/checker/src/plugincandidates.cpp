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

#include "plugincandidates.h"

#include "../version.h"

#include <set>
#include <stdexcept>
#include <iostream>

#include <QProcess>
#include <QDir>
#include <QTime>

#if defined(_WIN32)
#define PLUGIN_GLOB "*.dll"
#elif defined(__APPLE__)
#define PLUGIN_GLOB "*.dylib *.so"
#else
#define PLUGIN_GLOB "*.so"
#endif

using namespace std;

PluginCandidates::PluginCandidates(string helperExecutableName) :
    m_helper(helperExecutableName),
    m_logCallback(nullptr)
{
}

void
PluginCandidates::setLogCallback(LogCallback *cb)
{
    m_logCallback = cb;
}

vector<string>
PluginCandidates::getCandidateLibrariesFor(string tag) const
{
    if (m_candidates.find(tag) == m_candidates.end()) return {};
    else return m_candidates.at(tag);
}

vector<PluginCandidates::FailureRec>
PluginCandidates::getFailedLibrariesFor(string tag) const
{
    if (m_failures.find(tag) == m_failures.end()) return {};
    else return m_failures.at(tag);
}

void
PluginCandidates::log(string message)
{
    if (m_logCallback) {
        m_logCallback->log("PluginCandidates: " + message);
    } else {
        cerr << "PluginCandidates: " << message << endl;
    }
}

vector<string>
PluginCandidates::getLibrariesInPath(vector<string> path)
{
    vector<string> candidates;

    for (string dirname: path) {

        log("Scanning directory " + dirname);

        QDir dir(dirname.c_str(), PLUGIN_GLOB,
                 QDir::Name | QDir::IgnoreCase,
                 QDir::Files | QDir::Readable);

        for (unsigned int i = 0; i < dir.count(); ++i) {
            QString soname = dir.filePath(dir[i]);
            // NB this means the library names passed to the helper
            // are UTF-8 encoded
            candidates.push_back(soname.toStdString());
        }
    }

    return candidates;
}

void
PluginCandidates::scan(string tag,
                       vector<string> pluginPath,
                       string descriptorSymbolName)
{
    string helperVersion = getHelperCompatibilityVersion();
    if (helperVersion != CHECKER_COMPATIBILITY_VERSION) {
        log("Wrong plugin checker helper version found: expected v" +
            string(CHECKER_COMPATIBILITY_VERSION) + ", found v" +
            helperVersion);
        throw runtime_error("wrong version of plugin load helper found");
    }
    
    vector<string> libraries = getLibrariesInPath(pluginPath);
    vector<string> remaining = libraries;

    int runlimit = 20;
    int runcount = 0;
    
    vector<string> result;
    
    while (result.size() < libraries.size() && runcount < runlimit) {
        vector<string> output = runHelper(remaining, descriptorSymbolName);
        result.insert(result.end(), output.begin(), output.end());
        int shortfall = int(remaining.size()) - int(output.size());
        if (shortfall > 0) {
            // Helper bailed out for some reason presumably associated
            // with the plugin following the last one it reported
            // on. Add a failure entry for that one and continue with
            // the following ones.
            string failed = *(remaining.rbegin() + shortfall - 1);
            log("Helper output ended before result for plugin " + failed);
            result.push_back("FAILURE|" + failed + "|Plugin load check failed or timed out");
            remaining = vector<string>
                (remaining.rbegin(), remaining.rbegin() + shortfall - 1);
        }
        ++runcount;
    }

    recordResult(tag, result);
}

string
PluginCandidates::getHelperCompatibilityVersion()
{
    QProcess process;
    process.setReadChannel(QProcess::StandardOutput);
    process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    process.start(m_helper.c_str(), { "--version" });

    if (!process.waitForStarted()) {
        QProcess::ProcessError err = process.error();
        if (err == QProcess::FailedToStart) {
            std::cerr << "Unable to start helper process " << m_helper
                      << std::endl;
        } else if (err == QProcess::Crashed) {
            std::cerr << "Helper process " << m_helper
                      << " crashed on startup" << std::endl;
        } else {
            std::cerr << "Helper process " << m_helper
                      << " failed on startup with error code "
                      << err << std::endl;
        }
        throw runtime_error("plugin load helper failed to start");
    }
    process.waitForFinished();

    QByteArray output = process.readAllStandardOutput();
    while (output.endsWith('\n') || output.endsWith('\r')) {
        output.chop(1);
    }

    string versionString = QString(output).toStdString();
    log("Read version string from helper: " + versionString);
    return versionString;
}

vector<string>
PluginCandidates::runHelper(vector<string> libraries, string descriptor)
{
    vector<string> output;

    log("Running helper " + m_helper + " with following library list:");
    for (auto &lib: libraries) log(lib);

    QProcess process;
    process.setReadChannel(QProcess::StandardOutput);

    if (m_logCallback) {
        log("Log callback is set: using separate-channels mode to gather stderr");
        process.setProcessChannelMode(QProcess::SeparateChannels);
    } else {
        process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    }
    
    process.start(m_helper.c_str(), { descriptor.c_str() });
    
    if (!process.waitForStarted()) {
        QProcess::ProcessError err = process.error();
        if (err == QProcess::FailedToStart) {
            std::cerr << "Unable to start helper process " << m_helper
                      << std::endl;
        } else if (err == QProcess::Crashed) {
            std::cerr << "Helper process " << m_helper
                      << " crashed on startup" << std::endl;
        } else {
            std::cerr << "Helper process " << m_helper
                      << " failed on startup with error code "
                      << err << std::endl;
        }
        logErrors(&process);
        throw runtime_error("plugin load helper failed to start");
    }

    log("Helper " + m_helper + " started OK");
    logErrors(&process);
    
    for (auto &lib: libraries) {
        process.write(lib.c_str(), lib.size());
        process.write("\n", 1);
    }

    QTime t;
    t.start();
    int timeout = 15000; // ms

    const int buflen = 4096;
    bool done = false;
    
    while (!done) {
        char buf[buflen];
        qint64 linelen = process.readLine(buf, buflen);
        if (linelen > 0) {
            output.push_back(buf);
            done = (output.size() == libraries.size());
        } else if (linelen < 0) {
            // error case
            log("Received error code while reading from helper");
            done = true;
        } else {
            // no error, but no line read (could just be between
            // lines, or could be eof)
            done = (process.state() == QProcess::NotRunning);
            if (!done) {
                if (t.elapsed() > timeout) {
                    // this is purely an emergency measure
                    log("Timeout: helper took too long, killing it");
                    process.kill();
                    done = true;
                } else {
                    process.waitForReadyRead(200);
                }
            }
        }
        logErrors(&process);
    }

    if (process.state() != QProcess::NotRunning) {
        process.close();
        process.waitForFinished();
        logErrors(&process);
    }

    log("Helper completed");
    
    return output;
}

void
PluginCandidates::logErrors(QProcess *p)
{
    p->setReadChannel(QProcess::StandardError);

    qint64 byteCount = p->bytesAvailable();
    if (byteCount == 0) {
        p->setReadChannel(QProcess::StandardOutput);
        return;
    }

    QByteArray buffer = p->read(byteCount);
    while (buffer.endsWith('\n') || buffer.endsWith('\r')) {
        buffer.chop(1);
    }
    std::string str(buffer.constData(), buffer.size());
    log("Helper stderr output follows:\n" + str);
    log("Helper stderr output ends");
    
    p->setReadChannel(QProcess::StandardOutput);
}

void
PluginCandidates::recordResult(string tag, vector<string> result)
{
    for (auto &r: result) {

        QString s(r.c_str());
        QStringList bits = s.split("|");

        log(("Read output line from helper: " + s.trimmed()).toStdString());
        
        if (bits.size() < 2 || bits.size() > 3) {
            log("Invalid output line (wrong number of |-separated fields)");
            continue;
        }

        string status = bits[0].toStdString();
        
        string library = bits[1].toStdString();
        if (bits.size() == 2) {
            library = bits[1].trimmed().toStdString();
        }

        if (status == "SUCCESS") {
            m_candidates[tag].push_back(library);

        } else if (status == "FAILURE") {
        
            QString messageAndCode = "";
            if (bits.size() > 2) {
                messageAndCode = bits[2].trimmed();
            }

            PluginCheckCode code = PluginCheckCode::FAIL_OTHER;
            string message = "";

            QRegExp codeRE("^(.*) *\\[([0-9]+)\\]$");
            if (codeRE.exactMatch(messageAndCode)) {
                QStringList caps(codeRE.capturedTexts());
                if (caps.length() == 3) {
                    message = caps[1].toStdString();
                    code = PluginCheckCode(caps[2].toInt());
                    log("Split failure report into message and failure code "
                        + caps[2].toStdString());
                } else {
                    log("Unable to split out failure code from report");
                }
            } else {
                log("Failure message does not give a failure code");
            }

            if (message == "") {
                message = messageAndCode.toStdString();
            }

            m_failures[tag].push_back({ library, code, message });

        } else {
            log("Unexpected status \"" + status + "\" in output line");
        }
    }
}

