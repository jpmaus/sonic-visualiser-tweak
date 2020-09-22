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

#ifdef HAVE_PIPER

#include "PiperVampPluginFactory.h"
#include "PluginIdentifier.h"

#include "system/System.h"

#include "PluginScan.h"

#ifdef _WIN32
#undef VOID
#undef ERROR
#define CAPNP_LITE 1
#endif

#include "vamp-client/qt/PiperAutoPlugin.h"
#include "vamp-client/qt/ProcessQtTransport.h"
#include "vamp-client/CapnpRRClient.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QCoreApplication>

#include <iostream>

#include "base/Profiler.h"
#include "base/HelperExecPath.h"

using namespace std;

//#define DEBUG_PLUGIN_SCAN_AND_INSTANTIATE 1

class PiperVampPluginFactory::Logger : public piper_vamp::client::LogCallback {
protected:
    void log(std::string message) const override {
        SVDEBUG << "PiperVampPluginFactory: " << message << endl;
    }
};

PiperVampPluginFactory::PiperVampPluginFactory() :
    m_logger(new Logger)
{
    QString serverName = "piper-vamp-simple-server";
    float minimumVersion = 2.0;

    HelperExecPath hep(HelperExecPath::AllInstalled);

    auto servers = hep.getHelperExecutables(serverName);

    for (auto n: servers) {
        SVDEBUG << "NOTE: PiperVampPluginFactory: Found server: "
                << n.executable << endl;
        if (serverMeetsMinimumVersion(n, minimumVersion)) {
            m_servers.push_back(n);
        } else {
            SVCERR << "WARNING: PiperVampPluginFactory: Server at "
                   << n.executable
                   << " does not meet minimum version requirement (version >= "
                   << minimumVersion << ")" << endl;
        }
    }
    
    if (m_servers.empty()) {
        SVDEBUG << "NOTE: No Piper Vamp servers found in installation;"
                << " the following paths are either absent or fail "
                << "minimum-version check:" << endl;
        for (auto d: hep.getHelperCandidatePaths(serverName)) {
            SVDEBUG << "NOTE: " << d << endl;
        }
    }
}

PiperVampPluginFactory::~PiperVampPluginFactory()
{
    delete m_logger;
}

bool
PiperVampPluginFactory::serverMeetsMinimumVersion(const HelperExecPath::HelperExec &server,
                                                  float minimumVersion)
{
    QProcess process;
    QString executable = server.executable;
    process.setReadChannel(QProcess::StandardOutput);
    process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    process.start(executable, { "--version" });

    if (!process.waitForStarted()) {
        QProcess::ProcessError err = process.error();
        if (err == QProcess::FailedToStart) {
            SVCERR << "WARNING: Unable to start server " << executable
                   << " for version check" << endl;
        } else if (err == QProcess::Crashed) {
            SVCERR << "WARNING: Server " << executable
                   << " crashed on version check" << endl;
        } else {
            SVCERR << "WARNING: Server " << executable
                   << " failed on version check with error code "
                   << err << endl;
        }
        return false;
    }
    process.waitForFinished();

    QByteArray output = process.readAllStandardOutput();
    while (output.endsWith('\n') || output.endsWith('\r')) {
        output.chop(1);
    }

    QString outputString(output);
    bool ok = false;
    float version = outputString.toFloat(&ok);
    if (!ok) {
        SVCERR << "WARNING: Failed to convert server version response \""
               << outputString << "\" into one- or two-part version number"
               << endl;
    }

    SVDEBUG << "Server " << executable << " reports version number "
            << version << endl;

    float eps = 1e-6f;
    return (version >= minimumVersion ||
            fabsf(version - minimumVersion) < eps); // arf
}

vector<QString>
PiperVampPluginFactory::getPluginIdentifiers(QString &errorMessage)
{
    Profiler profiler("PiperVampPluginFactory::getPluginIdentifiers");

    QMutexLocker locker(&m_mutex);

    if (m_servers.empty()) {
        errorMessage = QObject::tr("External plugin host executable does not appear to be installed");
        return {};
    }
    
    if (m_pluginData.empty()) {
        populate(errorMessage);
    }

    vector<QString> rv;

    for (const auto &d: m_pluginData) {
        rv.push_back(QString("vamp:") + QString::fromStdString(d.second.pluginKey));
    }

    return rv;
}

Vamp::Plugin *
PiperVampPluginFactory::instantiatePlugin(QString identifier,
                                          sv_samplerate_t inputSampleRate)
{
    Profiler profiler("PiperVampPluginFactory::instantiatePlugin");

    if (m_origins.find(identifier) == m_origins.end()) {
        SVCERR << "ERROR: No known server for identifier " << identifier << endl;
        return nullptr;
    }
    
    auto psd = getPluginStaticData(identifier);
    if (psd.pluginKey == "") {
        return nullptr;
    }

    SVDEBUG << "PiperVampPluginFactory: Creating PiperAutoPlugin for server "
        << m_origins[identifier] << ", identifier " << identifier << endl;
    
    auto ap = new piper_vamp::client::PiperAutoPlugin
        (m_origins[identifier].toStdString(),
         psd.pluginKey,
         float(inputSampleRate),
         0,
         m_logger);
    
    if (!ap->isOK()) {
        delete ap;
        return nullptr;
    }

    return ap;
}

piper_vamp::PluginStaticData
PiperVampPluginFactory::getPluginStaticData(QString identifier)
{
    if (m_pluginData.find(identifier) != m_pluginData.end()) {
        return m_pluginData[identifier];
    } else {
        return {};
    }
}

QString
PiperVampPluginFactory::getPluginCategory(QString identifier)
{
    if (m_taxonomy.find(identifier) != m_taxonomy.end()) {
        return m_taxonomy[identifier];
    } else {
        return {};
    }
}

QString
PiperVampPluginFactory::getPluginLibraryPath(QString identifier)
{
    // What we want to return here is the file path of the library in
    // which the plugin was actually found -- we want to be paranoid
    // about that and not just query
    // Vamp::HostExt::PluginLoader::getLibraryPathForPlugin to return
    // what the SDK thinks the likely location would be (in case our
    // search order turns out to have been different)

    QStringList bits = identifier.split(':');
    if (bits.size() > 1) {
        QString soname = bits[bits.size() - 2];
        auto i = m_libraries.find(soname);
        if (i != m_libraries.end()) {
            return i->second;
        }
    }
    return QString();
}

void
PiperVampPluginFactory::populate(QString &errorMessage)
{
    QString someError;

    for (auto s: m_servers) {

        populateFrom(s, someError);

        if (someError != "" && errorMessage == "") {
            errorMessage = someError;
        }
    }
}

void
PiperVampPluginFactory::populateFrom(const HelperExecPath::HelperExec &server,
                                     QString &errorMessage)
{
    QString tag = server.tag;
    string executable = server.executable.toStdString();

    PluginScan *scan = PluginScan::getInstance();
    auto candidateLibraries =
        scan->getCandidateLibrariesFor(PluginScan::VampPlugin);

    SVDEBUG << "PiperVampPluginFactory: Populating from " << executable << endl;
    SVDEBUG << "INFO: Have " << candidateLibraries.size()
            << " candidate Vamp plugin libraries from scanner" << endl;
        
    vector<string> from;
    for (const auto &c: candidateLibraries) {
        if (c.helperTag == tag) {
            string soname = QFileInfo(c.libraryPath).baseName().toStdString();
            SVDEBUG << "INFO: For tag \"" << tag << "\" giving library " << soname << endl;
            from.push_back(soname);
            QString qsoname = QString::fromStdString(soname);
            if (m_libraries.find(qsoname) == m_libraries.end()) {
                m_libraries[qsoname] = c.libraryPath;
            }
        }
    }

    if (from.empty()) {
        SVDEBUG << "PiperVampPluginFactory: No candidate libraries for tag \""
             << tag << "\"";
        if (scan->scanSucceeded()) {
            // we have to assume that they all failed to load (i.e. we
            // exclude them all) rather than sending an empty list
            // (which would mean no exclusions)
            SVDEBUG << ", skipping" << endl;
            return;
        } else {
            SVDEBUG << ", but it seems the scan failed, so bumbling on anyway" << endl;
        }
    }
    
    piper_vamp::client::ProcessQtTransport transport(executable, "capnp", m_logger);
    if (!transport.isOK()) {
        SVDEBUG << "PiperVampPluginFactory: Failed to start Piper process transport" << endl;
        errorMessage = QObject::tr("Could not start external plugin host");
        return;
    }

    piper_vamp::client::CapnpRRClient client(&transport, m_logger);

    piper_vamp::ListRequest req;
    req.from = from;
    
    piper_vamp::ListResponse resp;

    try {
        resp = client.list(req);
    } catch (const piper_vamp::client::ServerCrashed &) {
        SVDEBUG << "PiperVampPluginFactory: Piper server crashed" << endl;
        errorMessage = QObject::tr
            ("External plugin host exited unexpectedly while listing plugins");
        return;
    } catch (const std::exception &e) {
        SVDEBUG << "PiperVampPluginFactory: Exception caught: " << e.what() << endl;
        errorMessage = QObject::tr("External plugin host invocation failed: %1")
            .arg(e.what());
        return;
    }

    SVDEBUG << "PiperVampPluginFactory: server \"" << executable << "\" lists "
            << resp.available.size() << " plugin(s)" << endl;

    for (const auto &pd: resp.available) {
        
        QString identifier =
            QString("vamp:") + QString::fromStdString(pd.pluginKey);

        if (m_origins.find(identifier) != m_origins.end()) {
            // have it already, from a higher-priority server
            // (e.g. 64-bit instead of 32-bit)
            continue;
        }

        m_origins[identifier] = server.executable;
        
        m_pluginData[identifier] = pd;

        QStringList catlist;
        for (const auto &cs: pd.category) {
            catlist.push_back(QString::fromStdString(cs));
        }

        m_taxonomy[identifier] = catlist.join(" > ");
    }
}

#endif
