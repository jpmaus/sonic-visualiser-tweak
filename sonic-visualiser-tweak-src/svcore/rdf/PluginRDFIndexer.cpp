/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2008-2012 QMUL.
   
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "PluginRDFIndexer.h"

#include "data/fileio/CachedFile.h"
#include "data/fileio/FileSource.h"
#include "data/fileio/PlaylistFileReader.h"
#include "plugin/PluginIdentifier.h"

#include "base/Profiler.h"
#include "base/Debug.h"

#include <vamp-hostsdk/PluginHostAdapter.h>

#include <dataquay/BasicStore.h>
#include <dataquay/RDFException.h>

#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QDateTime>
#include <QSettings>
#include <QFile>

#include <iostream>

using std::vector;
using std::string;
using Vamp::PluginHostAdapter;

using Dataquay::Uri;
using Dataquay::Node;
using Dataquay::Nodes;
using Dataquay::Triple;
using Dataquay::Triples;
using Dataquay::BasicStore;
using Dataquay::RDFException;
using Dataquay::RDFDuplicateImportException;

PluginRDFIndexer *
PluginRDFIndexer::m_instance = nullptr;

PluginRDFIndexer *
PluginRDFIndexer::getInstance() 
{
    if (!m_instance) m_instance = new PluginRDFIndexer();
    return m_instance;
}

PluginRDFIndexer::PluginRDFIndexer() :
    m_index(new Dataquay::BasicStore)
{
    m_index->addPrefix("vamp", Uri("http://purl.org/ontology/vamp/"));
    m_index->addPrefix("foaf", Uri("http://xmlns.com/foaf/0.1/"));
    m_index->addPrefix("dc", Uri("http://purl.org/dc/elements/1.1/"));
    indexInstalledURLs();
}

const BasicStore *
PluginRDFIndexer::getIndex()
{
    return m_index;
}

PluginRDFIndexer::~PluginRDFIndexer()
{
    QMutexLocker locker(&m_mutex);
}

void
PluginRDFIndexer::indexInstalledURLs()
{
    vector<string> paths = PluginHostAdapter::getPluginPath();

//    SVDEBUG << "\nPluginRDFIndexer::indexInstalledURLs: pid is " << getpid() << endl;

    QStringList filters;
    filters << "*.ttl";
    filters << "*.TTL";
    filters << "*.n3";
    filters << "*.N3";
    filters << "*.rdf";
    filters << "*.RDF";

    // Search each Vamp plugin path for an RDF file that either has
    // name "soname", "soname:label" or "soname/label" plus RDF
    // extension.  Use that order of preference, and prefer ttl over
    // n3 over rdf extension.

    for (vector<string>::const_iterator i = paths.begin(); i != paths.end(); ++i) {

        QDir dir(i->c_str());
        if (!dir.exists()) continue;

        QStringList entries = dir.entryList
            (filters, QDir::Files | QDir::Readable);

        for (QStringList::const_iterator j = entries.begin();
             j != entries.end(); ++j) {

            QFileInfo fi(dir.filePath(*j));
            pullFile(fi.absoluteFilePath());
        }

        QStringList subdirs = dir.entryList
            (QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Readable);

        for (QStringList::const_iterator j = subdirs.begin();
             j != subdirs.end(); ++j) {

            QDir subdir(dir.filePath(*j));
            if (subdir.exists()) {
                entries = subdir.entryList
                    (filters, QDir::Files | QDir::Readable);
                for (QStringList::const_iterator k = entries.begin();
                     k != entries.end(); ++k) {
                    QFileInfo fi(subdir.filePath(*k));
                    pullFile(fi.absoluteFilePath());
                }
            }
        }
    }

    reindex();
}

bool
PluginRDFIndexer::indexConfiguredURLs()
{
    SVDEBUG << "PluginRDFIndexer::indexConfiguredURLs" << endl;

    QSettings settings;
    settings.beginGroup("RDF");
    
    QString indexKey("rdf-indices");
    QStringList indices = settings.value(indexKey).toStringList();
    
    for (int i = 0; i < indices.size(); ++i) {

        QString index = indices[i];

        SVDEBUG << "PluginRDFIndexer::indexConfiguredURLs: index url is "
                  << index << endl;

        CachedFile cf(index);
        if (!cf.isOK()) continue;

        FileSource indexSource(cf.getLocalFilename());

        PlaylistFileReader reader(indexSource);
        if (!reader.isOK()) continue;

        PlaylistFileReader::Playlist list = reader.load();
        for (PlaylistFileReader::Playlist::const_iterator j = list.begin();
             j != list.end(); ++j) {
            SVDEBUG << "PluginRDFIndexer::indexConfiguredURLs: url is "
                  << *j << endl;
            pullURL(*j);
        }
    }

    QString urlListKey("rdf-urls");
    QStringList urls = settings.value(urlListKey).toStringList();

    for (int i = 0; i < urls.size(); ++i) {
        pullURL(urls[i]);
    }
    
    settings.endGroup();
    reindex();
    return true;
}

QString
PluginRDFIndexer::getURIForPluginId(QString pluginId)
{
    QMutexLocker locker(&m_mutex);

    if (m_idToUriMap.find(pluginId) == m_idToUriMap.end()) return "";
    return m_idToUriMap[pluginId];
}

QString
PluginRDFIndexer::getIdForPluginURI(QString uri)
{
    m_mutex.lock();

    if (m_uriToIdMap.find(uri) == m_uriToIdMap.end()) {

        m_mutex.unlock();

        // Haven't found this uri referenced in any document on the
        // local filesystem; try resolving the pre-fragment part of
        // the uri as a document URL and reading that if possible.

        // Because we may want to refer to this document again, we
        // cache it locally if it turns out to exist.

        SVDEBUG << "PluginRDFIndexer::getIdForPluginURI: NOTE: Failed to find a local RDF document describing plugin <" << uri << ">: attempting to retrieve one remotely by guesswork" << endl;

        QString baseUrl = QUrl(uri).toString(QUrl::RemoveFragment);

        indexURL(baseUrl);

        m_mutex.lock();

        if (m_uriToIdMap.find(uri) == m_uriToIdMap.end()) {
            m_uriToIdMap[uri] = "";
        }
    }

    QString id = m_uriToIdMap[uri];
    m_mutex.unlock();
    return id;
}

QStringList
PluginRDFIndexer::getIndexedPluginIds() 
{
    QMutexLocker locker(&m_mutex);

    QStringList ids;
    for (StringMap::const_iterator i = m_idToUriMap.begin();
         i != m_idToUriMap.end(); ++i) {
        ids.push_back(i->first);
    }
    return ids;
}

bool
PluginRDFIndexer::pullFile(QString filepath)
{
    QUrl url = QUrl::fromLocalFile(filepath);
    QString urlString = url.toString();
    return pullURL(urlString);
}

bool
PluginRDFIndexer::indexURL(QString urlString)
{
    bool pulled = pullURL(urlString);
    if (!pulled) return false;
    reindex();
    return true;
}

bool
PluginRDFIndexer::pullURL(QString urlString)
{
    Profiler profiler("PluginRDFIndexer::indexURL");

//    SVDEBUG << "PluginRDFIndexer::indexURL(" << urlString << ")" << endl;

    QMutexLocker locker(&m_mutex);

    QUrl local = urlString;

    if (FileSource::isRemote(urlString) &&
        FileSource::canHandleScheme(urlString)) {

        CachedFile cf(urlString, nullptr, "application/rdf+xml");
        if (!cf.isOK()) {
            return false;
        }

        local = QUrl::fromLocalFile(cf.getLocalFilename());

    } else if (urlString.startsWith("file:")) {

        local = QUrl(urlString);

    } else {

        local = QUrl::fromLocalFile(urlString);
    }

    try {
        m_index->import(local, BasicStore::ImportFailOnDuplicates);
    } catch (RDFDuplicateImportException &e) {
        SVDEBUG << e.what() << endl;
        SVDEBUG << "PluginRDFIndexer::pullURL: Document at " << urlString
                 << " duplicates triples found in earlier loaded document -- skipping it" << endl;
        return false;
    } catch (RDFException &e) {
        SVDEBUG << e.what() << endl;
        SVDEBUG << "PluginRDFIndexer::pullURL: Failed to import document from "
                 << urlString << ": " << e.what() << endl;
        return false;
    }
    return true;
}

bool
PluginRDFIndexer::reindex()
{
    Triples tt = m_index->match
        (Triple(Node(), Uri("a"), m_index->expand("vamp:Plugin")));
    Nodes plugins = tt.subjects();

    bool foundSomething = false;
    bool addedSomething = false;

    foreach (Node plugin, plugins) {
        
        if (plugin.type != Node::URI) {
            SVDEBUG << "PluginRDFIndexer::reindex: Plugin has no URI: node is "
                 << plugin << endl;
            continue;
        }
        
        Node idn = m_index->complete
            (Triple(plugin, m_index->expand("vamp:identifier"), Node()));

        if (idn.type != Node::Literal) {
            SVDEBUG << "PluginRDFIndexer::reindex: Plugin " << plugin
                 << " lacks vamp:identifier literal" << endl;
            continue;
        }

        Node libn = m_index->complete
            (Triple(Node(), m_index->expand("vamp:available_plugin"), plugin));

        if (libn.type != Node::URI) {
            SVDEBUG << "PluginRDFIndexer::reindex: Plugin " << plugin 
                 << " is not vamp:available_plugin in any library" << endl;
            continue;
        }

        Node son = m_index->complete
            (Triple(libn, m_index->expand("vamp:identifier"), Node()));

        if (son.type != Node::Literal) {
            SVDEBUG << "PluginRDFIndexer::reindex: Library " << libn
                 << " lacks vamp:identifier for soname" << endl;
            continue;
        }

        QString pluginUri = plugin.value;
        QString identifier = idn.value;
        QString soname = son.value;

        QString pluginId = PluginIdentifier::createIdentifier
            ("vamp", soname, identifier);

        foundSomething = true;

        if (m_idToUriMap.find(pluginId) != m_idToUriMap.end()) {
            continue;
        }

        m_idToUriMap[pluginId] = pluginUri;

        addedSomething = true;

        if (pluginUri != "") {
            if (m_uriToIdMap.find(pluginUri) != m_uriToIdMap.end()) {
                SVDEBUG << "PluginRDFIndexer::reindex: WARNING: Found multiple plugins with the same URI:" << endl;
                SVDEBUG << "  1. Plugin id \"" << m_uriToIdMap[pluginUri] << "\"" << endl;
                SVDEBUG << "  2. Plugin id \"" << pluginId << "\"" << endl;
                SVDEBUG << "both claim URI <" << pluginUri << ">" << endl;
            } else {
                m_uriToIdMap[pluginUri] = pluginId;
            }
        }
    }

    if (!foundSomething) {
        SVDEBUG << "PluginRDFIndexer::reindex: NOTE: Plugins found, but none sufficiently described" << endl;
    }
    
    return addedSomething;
}
