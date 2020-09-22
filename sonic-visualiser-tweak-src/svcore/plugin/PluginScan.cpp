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

#include "PluginScan.h"

#include "base/Debug.h"
#include "base/Preferences.h"
#include "base/HelperExecPath.h"

#include <sstream>

#include <QMutex>
#include <QCoreApplication>

using std::string;

class PluginScan::Logger
#ifdef HAVE_PLUGIN_CHECKER_HELPER
    : public PluginCandidates::LogCallback
#endif
{
protected:
    void log(std::string message) 
#ifdef HAVE_PLUGIN_CHECKER_HELPER
        override
#endif
    {
        SVDEBUG << "PluginScan: " << message << endl;
    }
};

PluginScan *PluginScan::getInstance()
{
    static QMutex mutex;
    static PluginScan *m_instance = nullptr;
    mutex.lock();
    if (!m_instance) m_instance = new PluginScan();
    mutex.unlock();
    return m_instance;
}

PluginScan::PluginScan() : m_succeeded(false), m_logger(new Logger) {
}

PluginScan::~PluginScan() {
    QMutexLocker locker(&m_mutex);
    clear();
    delete m_logger;
    SVDEBUG << "PluginScan::~PluginScan completed" << endl;
}

void
PluginScan::scan()
{
#ifdef HAVE_PLUGIN_CHECKER_HELPER

    QMutexLocker locker(&m_mutex);

    bool inProcess = Preferences::getInstance()->getRunPluginsInProcess();

    HelperExecPath hep(inProcess ?
                       HelperExecPath::NativeArchitectureOnly :
                       HelperExecPath::AllInstalled);

    QString helperName("vamp-plugin-load-checker");
    auto helpers = hep.getHelperExecutables(helperName);

    clear();

    for (auto p: helpers) {
        SVDEBUG << "NOTE: PluginScan: Found helper: " << p.executable << endl;
    }
    
    if (helpers.empty()) {
        SVDEBUG << "NOTE: No plugin checker helpers found in installation;"
             << " found none of the following:" << endl;
        for (auto d: hep.getHelperCandidatePaths(helperName)) {
            SVDEBUG << "NOTE: " << d << endl;
        }
    }

    for (auto p: helpers) {
        try {
            KnownPluginCandidates *kp = new KnownPluginCandidates
                (p.executable.toStdString(), m_logger);
            if (m_kp.find(p.tag) != m_kp.end()) {
                SVDEBUG << "WARNING: PluginScan::scan: Duplicate tag " << p.tag
                     << " for helpers" << endl;
                continue;
            }
            m_kp[p.tag] = kp;
            m_succeeded = true;
        } catch (const std::exception &e) {
            SVDEBUG << "ERROR: PluginScan::scan: " << e.what()
                 << " (with helper path = " << p.executable << ")" << endl;
        }
    }

    SVDEBUG << "PluginScan::scan complete" << endl;
#endif
}

bool
PluginScan::scanSucceeded() const
{
    QMutexLocker locker(&m_mutex);
    return m_succeeded;
}

void
PluginScan::clear()
{
    for (auto &p: m_kp) {
        delete p.second;
    }
    m_kp.clear();
    m_succeeded = false;
}

QList<PluginScan::Candidate>
PluginScan::getCandidateLibrariesFor(PluginType
#ifdef HAVE_PLUGIN_CHECKER_HELPER
                                     type
#endif
    ) const
{
#ifdef HAVE_PLUGIN_CHECKER_HELPER
    
    QMutexLocker locker(&m_mutex);

    KnownPlugins::PluginType kpt;
    switch (type) {
    case VampPlugin: kpt = KnownPlugins::VampPlugin; break;
    case LADSPAPlugin: kpt = KnownPlugins::LADSPAPlugin; break;
    case DSSIPlugin: kpt = KnownPlugins::DSSIPlugin; break;
    default: throw std::logic_error("Inconsistency in plugin type enums");
    }
    
    QList<Candidate> candidates;

    for (auto rec: m_kp) {

        KnownPluginCandidates *kp = rec.second;
        
        auto c = kp->getCandidateLibrariesFor(kpt);

        SVDEBUG << "PluginScan: helper \"" << kp->getHelperExecutableName()
                << "\" likes " << c.size() << " libraries of type "
                << kp->getTagFor(kpt) << endl;

        for (auto s: c) {
            candidates.push_back({ s.c_str(), rec.first });
        }

        if (type != VampPlugin) {
            // We are only interested in querying multiple helpers
            // when dealing with Vamp plugins, for which we can use
            // external servers and so in some cases can support
            // additional architectures. Other plugin formats are
            // loaded directly and so must match the host, which is
            // what the first helper is supposed to handle -- so
            // break after the first one if not querying Vamp
            break;
        }
    }

    return candidates;
    
#else
    return {};
#endif
}

#ifdef HAVE_PLUGIN_CHECKER_HELPER
QString
PluginScan::formatFailureReport(QString tag,
                                std::vector<PluginCandidates::FailureRec> failures) const
{
    int n = int(failures.size());
    int i = 0;

    std::ostringstream os;
    
    os << "<ul>";
    for (auto f: failures) {
        os << "<li><code>" + f.library + "</code>";

        SVDEBUG << "PluginScan::formatFailureReport: tag is \"" << tag
                << "\", failure code is " << int(f.code) << ", message is \""
                << f.message << "\"" << endl;
        
        QString userMessage = QString::fromStdString(f.message);

        switch (f.code) {

        case PluginCheckCode::FAIL_LIBRARY_NOT_FOUND:
            userMessage = QObject::tr("Library file could not be opened");
            break;

        case PluginCheckCode::FAIL_WRONG_ARCHITECTURE:
            if (tag == "64" || (sizeof(void *) == 8 && tag == "")) {
                userMessage = QObject::tr
                    ("Library has wrong architecture - possibly a 32-bit plugin installed in a 64-bit plugin folder");
            } else if (tag == "32" || (sizeof(void *) == 4 && tag == "")) {
                userMessage = QObject::tr
                    ("Library has wrong architecture - possibly a 64-bit plugin installed in a 32-bit plugin folder");
            }
            break;

        case PluginCheckCode::FAIL_DEPENDENCY_MISSING:
            userMessage = QObject::tr
                ("Library depends on another library that cannot be found: %1")
                .arg(userMessage);
            break;

        case PluginCheckCode::FAIL_NOT_LOADABLE:
            userMessage = QObject::tr
                ("Library cannot be loaded: %1").arg(userMessage);
            break;

        case PluginCheckCode::FAIL_FORBIDDEN:
            userMessage = QObject::tr
                ("Permission to load library was refused");
            break;

        case PluginCheckCode::FAIL_DESCRIPTOR_MISSING:
            userMessage = QObject::tr
                ("Not a valid plugin library (no descriptor found)");
            break;

        case PluginCheckCode::FAIL_NO_PLUGINS:
            userMessage = QObject::tr
                ("Library contains no plugins");
            break;

        case PluginCheckCode::FAIL_OTHER:
            if (userMessage == "") {
                userMessage = QObject::tr
                    ("Unknown error");
            }
            break;

        case PluginCheckCode::SUCCESS:
            // success shouldn't happen here!
            break;
        }
        
        os << "<br><i>" + userMessage.toStdString() + "</i>";
        os << "</li>";

        if (n > 10) {
            if (++i == 5) {
                os << "<li>";
                os << QObject::tr("... and %n further failure(s)",
                                  "", n - i)
                    .toStdString();
                os << "</li>";
                break;
            }
        }
    }
    os << "</ul>";

    return QString::fromStdString(os.str());
}
#endif

QString
PluginScan::getStartupFailureReport() const
{
#ifdef HAVE_PLUGIN_CHECKER_HELPER
    
    QMutexLocker locker(&m_mutex);

    if (!m_succeeded) {
        return QObject::tr("<b>Failed to scan for plugins</b>"
                           "<p>Failed to scan for plugins at startup. Possibly "
                           "the plugin checker program was not correctly "
                           "installed alongside %1?</p>")
            .arg(QCoreApplication::applicationName());
    }
    if (m_kp.empty()) {
        return QObject::tr("<b>Did not scan for plugins</b>"
                           "<p>Apparently no scan for plugins was attempted "
                           "(internal error?)</p>");
    }

    QString report;
    for (auto kp: m_kp) {
        auto failures = kp.second->getFailures();
        if (!failures.empty()) {
            report += formatFailureReport(kp.first, failures);
        }
    }
    if (report == "") {
        return report;
    }

    return QObject::tr("<p>Failed to load one or more plugin libraries:</p>")
        + report
        + QObject::tr("<p>These plugins may be incompatible with the system, "
                      "and will be ignored during this run of %1.</p>")
        .arg(QCoreApplication::applicationName());

#else
    return "";
#endif
}

