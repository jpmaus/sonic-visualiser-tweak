/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "SVFileReader.h"

#include "layer/Layer.h"
#include "view/View.h"
#include "base/PlayParameters.h"
#include "base/PlayParameterRepository.h"
#include "base/Preferences.h"

#include "data/fileio/AudioFileReaderFactory.h"
#include "data/fileio/FileSource.h"

#include "data/fileio/FileFinder.h"

#include "data/model/ReadOnlyWaveFileModel.h"
#include "data/model/EditableDenseThreeDimensionalModel.h"
#include "data/model/SparseOneDimensionalModel.h"
#include "data/model/SparseTimeValueModel.h"
#include "data/model/NoteModel.h"
#include "data/model/RegionModel.h"
#include "data/model/TextModel.h"
#include "data/model/ImageModel.h"
#include "data/model/BoxModel.h"
#include "data/model/AlignmentModel.h"
#include "data/model/AggregateWaveModel.h"

#include "transform/TransformFactory.h"

#include "view/Pane.h"

#include "widgets/ProgressDialog.h"

#include "Document.h"

#include <QString>
#include <QMessageBox>
#include <QFileDialog>

#include <iostream>

SVFileReader::SVFileReader(Document *document,
                           SVFileReaderPaneCallback &callback,
                           QString location) :
    m_document(document),
    m_paneCallback(callback),
    m_location(location),
    m_currentPane(nullptr),
    m_currentDataset(XmlExportable::NO_ID),
    m_currentLayer(nullptr),
    m_pendingDerivedModel(XmlExportable::NO_ID),
    m_currentTransformChannel(0),
    m_currentTransformIsNewStyle(true),
    m_datasetSeparator(" "),
    m_inRow(false),
    m_inLayer(false),
    m_inView(false),
    m_inData(false),
    m_inSelections(false),
    m_rowNumber(0),
    m_ok(false)
{
}

void
SVFileReader::parse(const QString &xmlData)
{
    QXmlInputSource inputSource;
    inputSource.setData(xmlData);
    parse(inputSource);
}

void
SVFileReader::parse(QXmlInputSource &inputSource)
{
    QXmlSimpleReader reader;
    reader.setContentHandler(this);
    reader.setErrorHandler(this);
    m_ok = reader.parse(inputSource);
}    

bool
SVFileReader::isOK()
{
    return m_ok;
}
        
SVFileReader::~SVFileReader()
{
    if (!m_awaitingDatasets.empty()) {
        SVCERR << "WARNING: SV-XML: File ended with "
                  << m_awaitingDatasets.size() << " unfilled model dataset(s)"
                  << endl;
    }

    std::set<ModelId> unaddedModels;

    for (auto i: m_models) {
        if (m_addedModels.find(i.second) == m_addedModels.end()) {
            unaddedModels.insert(i.second);
        }
    }

    if (!unaddedModels.empty()) {
        SVCERR << "WARNING: SV-XML: File contained "
               << unaddedModels.size() << " unused models"
               << endl;
        for (auto m: unaddedModels) {
            ModelById::release(m);
        }
    }

    if (!m_paths.empty()) {
        SVCERR << "WARNING: SV-XML: File contained "
               << m_paths.size() << " unused paths"
               << endl;
        for (auto p: m_paths) {
            delete p.second;
        }
    }
}

bool
SVFileReader::startElement(const QString &, const QString &,
                           const QString &qName,
                           const QXmlAttributes &attributes)
{
    QString name = qName.toLower();

    bool ok = false;

    // Valid element names:
    //
    // sv
    // data
    // dataset
    // display
    // derivation
    // playparameters
    // layer
    // model
    // point
    // row
    // view
    // window
    // plugin
    // transform
    // selections
    // selection
    // measurement

    if (name == "sv") {

        // nothing needed
        ok = true;

    } else if (name == "data") {

        // nothing needed
        m_inData = true;
        ok = true;

    } else if (name == "display") {

        // nothing needed
        ok = true;

    } else if (name == "window") {

        ok = readWindow(attributes);

    } else if (name == "model") {

        ok = readModel(attributes);
    
    } else if (name == "dataset") {
        
        ok = readDatasetStart(attributes);

    } else if (name == "bin") {
        
        ok = addBinToDataset(attributes);
    
    } else if (name == "point") {
        
        ok = addPointToDataset(attributes);

    } else if (name == "row") {

        ok = addRowToDataset(attributes);

    } else if (name == "layer") {

        addUnaddedModels(); // all models must be specified before first layer
        ok = readLayer(attributes);

    } else if (name == "view") {

        m_inView = true;
        ok = readView(attributes);

    } else if (name == "derivation") {

        makeAggregateModels(); // must be done before derivations that use them
        ok = readDerivation(attributes);

    } else if (name == "playparameters") {
        
        ok = readPlayParameters(attributes);

    } else if (name == "plugin") {

        ok = readPlugin(attributes);

    } else if (name == "selections") {

        m_inSelections = true;
        ok = true;

    } else if (name == "selection") {

        ok = readSelection(attributes);

    } else if (name == "measurement") {

        ok = readMeasurement(attributes);

    } else if (name == "transform") {
        
        ok = readTransform(attributes);

    } else if (name == "parameter") {

        ok = readParameter(attributes);

    } else {
        SVCERR << "WARNING: SV-XML: Unexpected element \""
                  << name << "\"" << endl;
    }

    if (!ok) {
        SVCERR << "WARNING: SV-XML: Failed to completely process element \""
                  << name << "\"" << endl;
    }

    return true;
}

bool
SVFileReader::characters(const QString &text)
{
    bool ok = false;

    if (m_inRow) {
        ok = readRowData(text);
        if (!ok) {
            SVCERR << "WARNING: SV-XML: Failed to read row data content for row " << m_rowNumber << endl;
        }
    }

    return true;
}

bool
SVFileReader::endElement(const QString &, const QString &,
                         const QString &qName)
{
    QString name = qName.toLower();

    if (name == "dataset") {

        if (m_currentDataset != XmlExportable::NO_ID) {
            
            bool foundInAwaiting = false;

            for (auto i: m_awaitingDatasets) {
                if (i.second == m_currentDataset) {
                    m_awaitingDatasets.erase(i.first);
                    foundInAwaiting = true;
                    break;
                }
            }

            if (!foundInAwaiting) {
                SVCERR << "WARNING: SV-XML: Dataset precedes model, or no model uses dataset" << endl;
            }
        }

        m_currentDataset = XmlExportable::NO_ID;

    } else if (name == "data") {

        addUnaddedModels();
        m_inData = false;

    } else if (name == "derivation") {

        if (m_currentDerivedModel.isNone()) {
            if (m_pendingDerivedModel == XmlExportable::NO_ID) {
                SVCERR << "WARNING: SV-XML: No valid output model id "
                       << "for derivation" << endl;
            } else if (haveModel(m_pendingDerivedModel)) {
                SVCERR << "WARNING: SV-XML: Derivation has existing model "
                       << m_pendingDerivedModel
                       << " as target, not regenerating" << endl;
            } else {
                QString message;
                m_currentDerivedModel = m_models[m_pendingDerivedModel] =
                    m_document->addDerivedModel
                    (m_currentTransform,
                     ModelTransformer::Input(m_currentTransformSource,
                                             m_currentTransformChannel),
                     message);
                if (m_currentDerivedModel.isNone()) {
                    emit modelRegenerationFailed(tr("(derived model in SV-XML)"),
                                                 m_currentTransform.getIdentifier(),
                                                 message);
                } else if (message != "") {
                    emit modelRegenerationWarning(tr("(derived model in SV-XML)"),
                                                  m_currentTransform.getIdentifier(),
                                                  message);
                }                    
            }
        } else {
            m_document->addAlreadyDerivedModel
                (m_currentTransform,
                 ModelTransformer::Input(m_currentTransformSource,
                                         m_currentTransformChannel),
                 m_currentDerivedModel);
        }

        m_addedModels.insert(m_currentDerivedModel);
        m_currentDerivedModel = {};
        m_pendingDerivedModel = XmlExportable::NO_ID;
        m_currentTransformSource = {};
        m_currentTransform = Transform();
        m_currentTransformChannel = -1;

    } else if (name == "row") {
        m_inRow = false;
    } else if (name == "layer") {
        m_inLayer = false;
    } else if (name == "view") {
        m_inView = false;
    } else if (name == "selections") {
        m_inSelections = false;
    } else if (name == "playparameters") {
        m_currentPlayParameters = {};
    }

    return true;
}

bool
SVFileReader::error(const QXmlParseException &exception)
{
    m_errorString =
        QString("ERROR: SV-XML: %1 at line %2, column %3")
        .arg(exception.message())
        .arg(exception.lineNumber())
        .arg(exception.columnNumber());
    SVCERR << m_errorString << endl;
    return QXmlDefaultHandler::error(exception);
}

bool
SVFileReader::fatalError(const QXmlParseException &exception)
{
    m_errorString =
        QString("FATAL ERROR: SV-XML: %1 at line %2, column %3")
        .arg(exception.message())
        .arg(exception.lineNumber())
        .arg(exception.columnNumber());
    SVCERR << m_errorString << endl;
    return QXmlDefaultHandler::fatalError(exception);
}


#define READ_MANDATORY(TYPE, NAME, CONVERSION)                      \
    TYPE NAME = attributes.value(#NAME).trimmed().CONVERSION(&ok); \
    if (!ok) { \
        SVCERR << "WARNING: SV-XML: Missing or invalid mandatory " #TYPE " attribute \"" #NAME "\"" << endl; \
        return false; \
    }

bool
SVFileReader::readWindow(const QXmlAttributes &)
{
    // The window element contains window dimensions, which we used to
    // read and size the window accordingly. This was a Bad Idea [tm]
    // and we now do nothing instead. See #1769 Loading window
    // dimensions from session file is a really bad idea
    return true;
}

void
SVFileReader::makeAggregateModels()
{
    std::map<ExportId, PendingAggregateRec> stillPending;
    
    for (auto p: m_pendingAggregates) {

        int id = p.first;
        const PendingAggregateRec &rec = p.second;
        bool skip = false;

        AggregateWaveModel::ChannelSpecList specs;
        for (ExportId componentId: rec.components) {
            bool found = false;
            if (m_models.find(componentId) != m_models.end()) {
                ModelId modelId = m_models[componentId];
                auto rs = ModelById::getAs<RangeSummarisableTimeValueModel>
                    (modelId);
                if (rs) {
                    specs.push_back(AggregateWaveModel::ModelChannelSpec
                                    (modelId, -1));
                    found = true;
                } else {
                    SVDEBUG << "SVFileReader::makeAggregateModels: "
                            << "Component model id " << componentId
                            << "in aggregate model id " << id
                            << "does not appear to be convertible to "
                            << "RangeSummarisableTimeValueModel"
                            << endl;
                }
            }
            if (!found) {
                SVDEBUG << "SVFileReader::makeAggregateModels: "
                        << "Unknown component model id "
                        << componentId << " in aggregate model id " << id
                        << ", hoping we won't be needing it just yet"
                        << endl;
                skip = true;
            }                
        }

        if (skip) {
            stillPending[id] = rec;
        } else {
            auto model = std::make_shared<AggregateWaveModel>(specs);
            model->setObjectName(rec.name);
            m_models[id] = ModelById::add(model);

            SVDEBUG << "SVFileReader::makeAggregateModels: created aggregate "
                    << "model id " << id << " with " << specs.size()
                    << " components" << endl;
        }
    }

    m_pendingAggregates = stillPending;
}

void
SVFileReader::addUnaddedModels()
{
    makeAggregateModels();

    for (auto i: m_models) {

        ModelId modelId = i.second;

        if (m_addedModels.find(modelId) != m_addedModels.end()) {
            // already added this one
            continue;
        }

        m_document->addNonDerivedModel(modelId);
        
        // make a note of all models that have been added to the
        // document, so they don't get released by our own destructor
        m_addedModels.insert(modelId);
    }
}

bool
SVFileReader::readModel(const QXmlAttributes &attributes)
{
    bool ok = false;

    READ_MANDATORY(int, id, toInt);

    if (haveModel(id)) {
        SVCERR << "WARNING: SV-XML: Ignoring duplicate model id " << id
                  << endl;
        return false;
    }

    QString name = attributes.value("name");

    SVDEBUG << "SVFileReader::readModel: model name \"" << name << "\"" << endl;

    READ_MANDATORY(double, sampleRate, toDouble);

    QString type = attributes.value("type").trimmed();
    bool isMainModel = (attributes.value("mainModel").trimmed() == "true");

    if (type == "wavefile") {
        
        WaveFileModel *model = nullptr;
        FileFinder *ff = FileFinder::getInstance();
        QString originalPath = attributes.value("file");
        QString path = ff->find(FileFinder::AudioFile,
                                originalPath, m_location);

        SVDEBUG << "Wave file originalPath = " << originalPath << ", path = "
                  << path << endl;

        ProgressDialog dialog(tr("Opening file or URL..."), true, 2000);
        FileSource file(path, &dialog);
        file.waitForStatus();

        if (!file.isOK()) {
            SVCERR << "SVFileReader::readModel: Failed to retrieve file \"" << path << "\" for wave file model: " << file.getErrorString() << endl;
        } else if (!file.isAvailable()) {
            SVCERR << "SVFileReader::readModel: Failed to retrieve file \"" << path << "\" for wave file model: Source unavailable" << endl;
        } else {

            file.waitForData();

            sv_samplerate_t rate = sampleRate;

            if (Preferences::getInstance()->getFixedSampleRate() != 0) {
                rate = Preferences::getInstance()->getFixedSampleRate();
            } else if (rate == 0 &&
                       !isMainModel &&
                       Preferences::getInstance()->getResampleOnLoad()) {
                auto mm = ModelById::getAs<WaveFileModel>
                    (m_document->getMainModel());
                if (mm) rate = mm->getSampleRate();
            }

            model = new ReadOnlyWaveFileModel(file, rate);
            if (!model->isOK()) {
                delete model;
                model = nullptr;
            }
        }

        if (!model) {
            m_document->setIncomplete(true);
            return false;
        }

        model->setObjectName(name);

        ModelId modelId = ModelById::add(std::shared_ptr<Model>(model));
        m_models[id] = modelId;
        
        if (isMainModel) {
            m_document->setMainModel(modelId);
            m_addedModels.insert(modelId);
        }
        // Derived models will be added when their derivation
        // is found.

        return true;

    } else if (type == "aggregatewave") {

        QString components = attributes.value("components");
        QStringList componentIdStrings = components.split(",");
        std::vector<int> componentIds;
        for (auto cidStr: componentIdStrings) {
            bool ok = false;
            int cid = cidStr.toInt(&ok);
            if (!ok) {
                SVCERR << "SVFileReader::readModel: Failed to convert component model id from part \"" << cidStr << "\" in \"" << components << "\"" << endl;
            } else {
                componentIds.push_back(cid);
            }
        }
        PendingAggregateRec rec { name, sampleRate, componentIds };
        m_pendingAggregates[id] = rec;

        // The aggregate model will be constructed from its pending
        // record in makeAggregateModels; it can't happen here because
        // the component models might not all have been observed yet
        // (an unfortunate accident of the way the file is written)

        return true;

    } else if (type == "dense") {
        
        READ_MANDATORY(int, dimensions, toInt);
                    
        // Currently the only dense model we support here is the dense
        // 3d model.  Dense time-value models are always file-backed
        // waveform data, at this point, and they come in as wavefile
        // models.
        
        if (dimensions == 3) {
            
            READ_MANDATORY(int, windowSize, toInt);
            READ_MANDATORY(int, yBinCount, toInt);
            
            auto model = std::make_shared<EditableDenseThreeDimensionalModel>
                (sampleRate, windowSize, yBinCount);

            model->setObjectName(name);
            m_models[id] = ModelById::add(model);
            
            float minimum = attributes.value("minimum").trimmed().toFloat(&ok);
            if (ok) model->setMinimumLevel(minimum);
            
            float maximum = attributes.value("maximum").trimmed().toFloat(&ok);
            if (ok) model->setMaximumLevel(maximum);

            int dataset = attributes.value("dataset").trimmed().toInt(&ok);
            if (ok) m_awaitingDatasets[dataset] = id;

            int startFrame = attributes.value("startFrame").trimmed().toInt(&ok);
            if (ok) model->setStartFrame(startFrame);

            return true;

        } else {

            SVCERR << "WARNING: SV-XML: Unexpected dense model dimension ("
                      << dimensions << ")" << endl;
        }
    } else if (type == "sparse") {

        READ_MANDATORY(int, dimensions, toInt);
                  
        if (dimensions == 1) {
            
            READ_MANDATORY(int, resolution, toInt);
            
            if (attributes.value("subtype") == "image") {

                bool notifyOnAdd = (attributes.value("notifyOnAdd") == "true");
                auto model = std::make_shared<ImageModel>
                    (sampleRate, resolution, notifyOnAdd);
                model->setObjectName(name);
                m_models[id] = ModelById::add(model);

            } else {

                auto model = std::make_shared<SparseOneDimensionalModel>
                    (sampleRate, resolution);
                model->setObjectName(name);
                m_models[id] = ModelById::add(model);
            }

            int dataset = attributes.value("dataset").trimmed().toInt(&ok);
            if (ok) m_awaitingDatasets[dataset] = id;

            return true;

        } else if (dimensions == 2 || dimensions == 3) {
            
            READ_MANDATORY(int, resolution, toInt);

            bool haveMinMax = true;
            float minimum = attributes.value("minimum").trimmed().toFloat(&ok);
            if (!ok) haveMinMax = false;
            float maximum = attributes.value("maximum").trimmed().toFloat(&ok);
            if (!ok) haveMinMax = false;

            float valueQuantization =
                attributes.value("valueQuantization").trimmed().toFloat(&ok);

            bool notifyOnAdd = (attributes.value("notifyOnAdd") == "true");

            QString units = attributes.value("units");

            if (dimensions == 2) {
                if (attributes.value("subtype") == "text") {
                    auto model = std::make_shared<TextModel>
                        (sampleRate, resolution, notifyOnAdd);
                    model->setObjectName(name);
                    m_models[id] = ModelById::add(model);
                } else if (attributes.value("subtype") == "path") {
                    // Paths are no longer actually models
                    Path *path = new Path(sampleRate, resolution);
                    m_paths[id] = path;
                } else if (attributes.value("subtype") == "box" ||
                           attributes.value("subtype") == "timefrequencybox") {
                    auto model = std::make_shared<BoxModel>
                        (sampleRate, resolution, notifyOnAdd);
                    model->setScaleUnits(units);
                    model->setObjectName(name);
                    m_models[id] = ModelById::add(model);
                } else {
                    std::shared_ptr<SparseTimeValueModel> model;
                    if (haveMinMax) {
                        model = std::make_shared<SparseTimeValueModel>
                            (sampleRate, resolution, minimum, maximum,
                             notifyOnAdd);
                    } else {
                        model = std::make_shared<SparseTimeValueModel>
                            (sampleRate, resolution, notifyOnAdd);
                    }
                    model->setScaleUnits(units);
                    model->setObjectName(name);
                    m_models[id] = ModelById::add(model);
                }
            } else {
                if (attributes.value("subtype") == "region") {
                    std::shared_ptr<RegionModel> model;
                    if (haveMinMax) {
                        model = std::make_shared<RegionModel>
                            (sampleRate, resolution, minimum, maximum,
                             notifyOnAdd);
                    } else {
                        model = std::make_shared<RegionModel>
                            (sampleRate, resolution, notifyOnAdd);
                    }
                    model->setValueQuantization(valueQuantization);
                    model->setScaleUnits(units);
                    model->setObjectName(name);
                    m_models[id] = ModelById::add(model);
                } else if (attributes.value("subtype") == "flexinote") {
                    std::shared_ptr<NoteModel> model;
                    if (haveMinMax) {
                        model = std::make_shared<NoteModel>
                            (sampleRate, resolution, minimum, maximum,
                             notifyOnAdd,
                             NoteModel::FLEXI_NOTE);
                    } else {
                        model = std::make_shared<NoteModel>
                            (sampleRate, resolution, notifyOnAdd,
                             NoteModel::FLEXI_NOTE);
                    }
                    model->setValueQuantization(valueQuantization);
                    model->setScaleUnits(units);
                    model->setObjectName(name);
                    m_models[id] = ModelById::add(model);
                } else {
                    // note models written out by SV 1.3 and earlier
                    // have no subtype, so we can't test that
                    std::shared_ptr<NoteModel> model;
                    if (haveMinMax) {
                        model = std::make_shared<NoteModel>
                            (sampleRate, resolution, minimum, maximum, notifyOnAdd);
                    } else {
                        model = std::make_shared<NoteModel>
                            (sampleRate, resolution, notifyOnAdd);
                    }
                    model->setValueQuantization(valueQuantization);
                    model->setScaleUnits(units);
                    model->setObjectName(name);
                    m_models[id] = ModelById::add(model);
                }
            }

            int dataset = attributes.value("dataset").trimmed().toInt(&ok);
            if (ok) m_awaitingDatasets[dataset] = id;

            return true;

        } else {

            SVCERR << "WARNING: SV-XML: Unexpected sparse model dimension ("
                      << dimensions << ")" << endl;
        }

    } else if (type == "alignment") {

        READ_MANDATORY(int, reference, toInt);
        READ_MANDATORY(int, aligned, toInt);
        READ_MANDATORY(int, path, toInt);

        ModelId refModel, alignedModel;
        Path *pathPtr = nullptr;

        if (m_models.find(reference) != m_models.end()) {
            refModel = m_models[reference];
        } else {
            SVCERR << "WARNING: SV-XML: Unknown reference model id "
                      << reference << " in alignment model id " << id
                      << endl;
        }

        if (m_models.find(aligned) != m_models.end()) {
            alignedModel = m_models[aligned];
        } else {
            SVCERR << "WARNING: SV-XML: Unknown aligned model id "
                      << aligned << " in alignment model id " << id
                      << endl;
        }

        if (m_paths.find(path) != m_paths.end()) {
            pathPtr = m_paths[path];
        } else {
            SVCERR << "WARNING: SV-XML: Unknown path id "
                      << path << " in alignment model id " << id
                      << endl;
        }

        if (!refModel.isNone() && !alignedModel.isNone() && pathPtr) {
            auto model = std::make_shared<AlignmentModel>
                (refModel, alignedModel, ModelId());
            model->setPath(*pathPtr);
            model->setObjectName(name);
            m_models[id] = ModelById::add(model);
            if (auto am = ModelById::get(alignedModel)) {
                am->setAlignment(m_models[id]);
            }
            return true;
        }

        if (pathPtr) {
            delete pathPtr;
            m_paths.erase(path);
        }
        
    } else {

        SVCERR << "WARNING: SV-XML: Unexpected model type \""
               << type << "\" for model id " << id << endl;
    }

    return false;
}

bool
SVFileReader::readView(const QXmlAttributes &attributes)
{
    QString type = attributes.value("type");
    m_currentPane = nullptr;
    
    if (type != "pane") {
        SVCERR << "WARNING: SV-XML: Unexpected view type \""
                  << type << "\"" << endl;
        return false;
    }

    m_currentPane = m_paneCallback.addPane();

    SVDEBUG << "SVFileReader::addPane: pane is " << m_currentPane << endl;

    if (!m_currentPane) {
        SVCERR << "WARNING: SV-XML: Internal error: Failed to add pane!"
                  << endl;
        return false;
    }

    bool ok = false;

    View *view = m_currentPane;

    // The view properties first

    READ_MANDATORY(int, centre, toInt);
    READ_MANDATORY(int, zoom, toInt);
    READ_MANDATORY(int, followPan, toInt);
    READ_MANDATORY(int, followZoom, toInt);
    QString tracking = attributes.value("tracking");

    ZoomLevel zoomLevel;
    int deepZoom = attributes.value("deepZoom").trimmed().toInt(&ok);
    if (ok && zoom == 1 && deepZoom > 1) {
        zoomLevel = { ZoomLevel::PixelsPerFrame, deepZoom };
    } else {
        zoomLevel = { ZoomLevel::FramesPerPixel, zoom };
    }

    // Specify the follow modes before we set the actual values
    view->setFollowGlobalPan(followPan);
    view->setFollowGlobalZoom(followZoom);
    view->setPlaybackFollow(tracking == "scroll" ? PlaybackScrollContinuous :
                            tracking == "page" ? PlaybackScrollPageWithCentre :
                            tracking == "daw" ? PlaybackScrollPage
                            : PlaybackIgnore);

    // Then set these values
    view->setCentreFrame(centre);
    view->setZoomLevel(zoomLevel);

    // And pane properties
    READ_MANDATORY(int, centreLineVisible, toInt);
    m_currentPane->setCentreLineVisible(centreLineVisible);

    int height = attributes.value("height").toInt(&ok);
    if (ok) {
        m_currentPane->resize(m_currentPane->width(), height);
    }

    return true;
}

bool
SVFileReader::readLayer(const QXmlAttributes &attributes)
{
    QString type = attributes.value("type");

    int id;
    bool ok = false;
    id = attributes.value("id").trimmed().toInt(&ok);

    if (!ok) {
        SVCERR << "WARNING: SV-XML: No layer id for layer of type \""
                  << type
                  << "\"" << endl;
        return false;
    }

    Layer *layer = nullptr;
    bool isNewLayer = false;

    // Layers are expected to be defined in layer elements in the data
    // section, and referred to in layer elements in the view
    // sections.  So if we're in the data section, we expect this
    // layer not to exist already; if we're in the view section, we
    // expect it to exist.

    if (m_inData) {

        if (m_layers.find(id) != m_layers.end()) {
            SVCERR << "WARNING: SV-XML: Ignoring duplicate layer id " << id
                      << " in data section" << endl;
            return false;
        }

        layer = m_layers[id] = m_document->createLayer
            (LayerFactory::getInstance()->getLayerTypeForName(type));

        if (layer) {
            m_layers[id] = layer;
            isNewLayer = true;
        }

    } else {

        if (!m_currentPane) {
            SVCERR << "WARNING: SV-XML: No current pane for layer " << id
                      << " in view section" << endl;
            return false;
        }

        if (m_layers.find(id) != m_layers.end()) {
            
            layer = m_layers[id];
        
        } else {
            SVCERR << "WARNING: SV-XML: Layer id " << id 
                      << " in view section has not been defined -- defining it here"
                      << endl;

            layer = m_document->createLayer
                (LayerFactory::getInstance()->getLayerTypeForName(type));

            if (layer) {
                m_layers[id] = layer;
                isNewLayer = true;
            }
        }
    }
            
    if (!layer) {
        SVCERR << "WARNING: SV-XML: Failed to add layer of type \""
                  << type
                  << "\"" << endl;
        return false;
    }

    if (isNewLayer) {

        QString name = attributes.value("name");
        layer->setObjectName(name);

        QString presentationName = attributes.value("presentationName");
        layer->setPresentationName(presentationName);

        int modelId;
        bool modelOk = false;
        modelId = attributes.value("model").trimmed().toInt(&modelOk);

        if (modelOk) {
            if (haveModel(modelId)) {
                m_document->setModel(layer, m_models[modelId]);
            } else {
                SVCERR << "WARNING: SV-XML: Unknown model id " << modelId
                       << " in layer definition" << endl;
                if (!layer->canExistWithoutModel()) {
                    // Don't add a layer with an unknown model id
                    // unless it explicitly supports this state
                    m_document->deleteLayer(layer);
                    m_layers[id] = layer = nullptr;
                    return false;
                }
            }
        }

        if (layer) layer->setProperties(attributes);
    }

    if (!m_inData && m_currentPane && layer) {

        QString visible = attributes.value("visible");
        bool dormant = (visible == "false");

        // We need to do this both before and after adding the layer
        // to the view -- we need it to be dormant if appropriate
        // before it's actually added to the view so that any property
        // box gets the right state when it's added, but the add layer
        // command sets dormant to false because it assumes it may be
        // restoring a previously dormant layer, so we need to set it
        // again afterwards too.  Hm
        layer->setLayerDormant(m_currentPane, dormant);

        m_document->addLayerToView(m_currentPane, layer);

        layer->setLayerDormant(m_currentPane, dormant);
    }

    m_currentLayer = layer;
    m_inLayer = (layer != nullptr);

    return true;
}

bool
SVFileReader::readDatasetStart(const QXmlAttributes &attributes)
{
    bool ok = false;

    READ_MANDATORY(int, id, toInt);
    READ_MANDATORY(int, dimensions, toInt);
    
    if (m_awaitingDatasets.find(id) == m_awaitingDatasets.end()) {
        SVCERR << "WARNING: SV-XML: Unwanted dataset " << id << endl;
        return false;
    }
    
    int awaitingId = m_awaitingDatasets[id];

    ModelId modelId;
    Path *path = nullptr;
    
    if (haveModel(awaitingId)) {
        modelId = m_models[awaitingId];
    } else if (m_paths.find(awaitingId) != m_paths.end()) {
        path = m_paths[awaitingId];
    } else {
        SVCERR << "WARNING: SV-XML: Internal error: Unknown model or path "
               << modelId << " awaiting dataset " << id << endl;
        return false;
    }

    bool good = false;

    switch (dimensions) {
    case 1:
        good =
            (ModelById::isa<SparseOneDimensionalModel>(modelId) ||
             ModelById::isa<ImageModel>(modelId));
        break;

    case 2:
        good =
            (ModelById::isa<SparseTimeValueModel>(modelId) ||
             ModelById::isa<TextModel>(modelId) ||
             ModelById::isa<BoxModel>(modelId) ||
             path);
        break;

    case 3:
        if (ModelById::isa<EditableDenseThreeDimensionalModel>(modelId)) {
            good = true;
            m_datasetSeparator = attributes.value("separator");
        } else {
            good =
                (ModelById::isa<NoteModel>(modelId) ||
                 ModelById::isa<RegionModel>(modelId));
        }
        break;
    }

    if (!good) {
        SVCERR << "WARNING: SV-XML: Model id " << modelId << " has wrong number of dimensions or inappropriate type for " << dimensions << "-D dataset " << id << endl;
        m_currentDataset = XmlExportable::NO_ID;
        return false;
    }

    m_currentDataset = awaitingId;
    return true;
}

bool
SVFileReader::addPointToDataset(const QXmlAttributes &attributes)
{
    bool ok = false;

    READ_MANDATORY(int, frame, toInt);

    if (m_paths.find(m_currentDataset) != m_paths.end()) {
        Path *path = m_paths[m_currentDataset];
        int mapframe = attributes.value("mapframe").trimmed().toInt(&ok);
        path->add(PathPoint(frame, mapframe));
        return ok;
    }

    if (!haveModel(m_currentDataset)) {
        SVCERR << "WARNING: SV-XML: Point element found in non-point dataset"
               << endl;
        return false;
    }
        
    ModelId modelId = m_models[m_currentDataset];        

    if (auto sodm = ModelById::getAs<SparseOneDimensionalModel>(modelId)) {
        QString label = attributes.value("label");
        sodm->add(Event(frame, label));
        return true;
    }

    if (auto stvm = ModelById::getAs<SparseTimeValueModel>(modelId)) {
        float value = attributes.value("value").trimmed().toFloat(&ok);
        QString label = attributes.value("label");
        stvm->add(Event(frame, value, label));
        return ok;
    }
        
    if (auto nm = ModelById::getAs<NoteModel>(modelId)) {
        float value = attributes.value("value").trimmed().toFloat(&ok);
        int duration = attributes.value("duration").trimmed().toInt(&ok);
        QString label = attributes.value("label");
        float level = attributes.value("level").trimmed().toFloat(&ok);
        if (!ok) { // level is optional
            level = 1.f;
            ok = true;
        }
        nm->add(Event(frame, value, duration, level, label));
        return ok;
    }

    if (auto rm = ModelById::getAs<RegionModel>(modelId)) {
        float value = attributes.value("value").trimmed().toFloat(&ok);
        int duration = attributes.value("duration").trimmed().toInt(&ok);
        QString label = attributes.value("label");
        rm->add(Event(frame, value, duration, label));
        return ok;
    }

    if (auto tm = ModelById::getAs<TextModel>(modelId)) {
        float height = attributes.value("height").trimmed().toFloat(&ok);
        QString label = attributes.value("label");
        tm->add(Event(frame, height, label));
        return ok;
    }

    if (auto bm = ModelById::getAs<BoxModel>(modelId)) {
        float value = attributes.value("value").trimmed().toFloat(&ok);
        if (!ok) {
            value = attributes.value("frequency").trimmed().toFloat(&ok);
            if (bm->getScaleUnits() == "") {
                bm->setScaleUnits("Hz");
            }
        }
        float extent = attributes.value("extent").trimmed().toFloat(&ok);
        int duration = attributes.value("duration").trimmed().toInt(&ok);
        QString label = attributes.value("label");
        bm->add(Event(frame, value, duration, extent, label));
        return ok;
    }

    if (auto im = ModelById::getAs<ImageModel>(modelId)) {
        QString image = attributes.value("image");
        QString label = attributes.value("label");
        im->add(Event(frame).withURI(image).withLabel(label));
        return ok;
    }

    SVCERR << "WARNING: SV-XML: Point element found in non-point dataset"
           << endl;

    return false;
}

bool
SVFileReader::addBinToDataset(const QXmlAttributes &attributes)
{
    if (!haveModel(m_currentDataset)) {
        SVCERR << "WARNING: SV-XML: Bin definition found in incompatible dataset"
               << endl;
        return false;
    }
        
    ModelId modelId = m_models[m_currentDataset];        

    if (auto dtdm = ModelById::getAs<EditableDenseThreeDimensionalModel>
        (modelId)) {

        bool ok = false;
        int n = attributes.value("number").trimmed().toInt(&ok);
        if (!ok) {
            SVCERR << "WARNING: SV-XML: Missing or invalid bin number"
                      << endl;
            return false;
        }

        QString name = attributes.value("name");
        dtdm->setBinName(n, name);
        return true;
    }

    SVCERR << "WARNING: SV-XML: Bin definition found in incompatible dataset"
           << endl;

    return false;
}


bool
SVFileReader::addRowToDataset(const QXmlAttributes &attributes)
{
    m_inRow = false;

    bool ok = false;
    m_rowNumber = attributes.value("n").trimmed().toInt(&ok);
    if (!ok) {
        SVCERR << "WARNING: SV-XML: Missing or invalid row number"
                  << endl;
        return false;
    }
    
    m_inRow = true;

//    SVCERR << "SV-XML: In row " << m_rowNumber << endl;
    
    return true;
}

bool
SVFileReader::readRowData(const QString &text)
{
    if (!haveModel(m_currentDataset)) {
        SVCERR << "WARNING: SV-XML: Row data found in non-row dataset" << endl;
        return false;
    }
        
    ModelId modelId = m_models[m_currentDataset];        
    bool warned = false;

    if (auto dtdm = ModelById::getAs<EditableDenseThreeDimensionalModel>
        (modelId)) {

        QStringList data = text.split(m_datasetSeparator);

        DenseThreeDimensionalModel::Column values;

        for (QStringList::iterator i = data.begin(); i != data.end(); ++i) {

            if (int(values.size()) == dtdm->getHeight()) {
                if (!warned) {
                    SVCERR << "WARNING: SV-XML: Too many y-bins in 3-D dataset row "
                              << m_rowNumber << endl;
                    warned = true;
                }
            }

            bool ok;
            float value = i->toFloat(&ok);
            if (!ok) {
                SVCERR << "WARNING: SV-XML: Bad floating-point value "
                          << i->toLocal8Bit().data()
                          << " in row data" << endl;
            } else {
                values.push_back(value);
            }
        }

        dtdm->setColumn(m_rowNumber, values);
        return true;
    }

    SVCERR << "WARNING: SV-XML: Row data found in non-row dataset" << endl;
    return false;
}

bool
SVFileReader::readDerivation(const QXmlAttributes &attributes)
{
    int modelExportId = 0;
    bool modelOk = false;
    modelExportId = attributes.value("model").trimmed().toInt(&modelOk);

    if (!modelOk) {
        SVCERR << "WARNING: SV-XML: No model id specified for derivation" << endl;
        return false;
    }

    if (haveModel(modelExportId)) {
        m_currentDerivedModel = m_models[modelExportId];
    } else {
        // we'll regenerate the model when the derivation element ends
        m_currentDerivedModel = {};
    }
    
    m_pendingDerivedModel = modelExportId;
    
    int sourceId = 0;
    bool sourceOk = false;
    sourceId = attributes.value("source").trimmed().toInt(&sourceOk);

    if (sourceOk && haveModel(sourceId)) {
        m_currentTransformSource = m_models[sourceId];
    } else {
        SVDEBUG << "NOTE: SV-XML: Can't find a model with id " << sourceId
                << " for derivation source, falling back to main model" << endl;
        m_currentTransformSource = m_document->getMainModel();
    }

    m_currentTransform = Transform();

    bool ok = false;
    int channel = attributes.value("channel").trimmed().toInt(&ok);
    if (ok) m_currentTransformChannel = channel;
    else m_currentTransformChannel = -1;

    QString type = attributes.value("type");

    if (type == "transform") {
        m_currentTransformIsNewStyle = true;
        return true;
    } else {
        m_currentTransformIsNewStyle = false;
        SVDEBUG << "NOTE: SV-XML: Reading old-style derivation element"
                  << endl;
    }

    QString transformId = attributes.value("transform");

    m_currentTransform.setIdentifier(transformId);

    int stepSize = attributes.value("stepSize").trimmed().toInt(&ok);
    if (ok) m_currentTransform.setStepSize(stepSize);

    int blockSize = attributes.value("blockSize").trimmed().toInt(&ok);
    if (ok) m_currentTransform.setBlockSize(blockSize);

    int windowType = attributes.value("windowType").trimmed().toInt(&ok);
    if (ok) m_currentTransform.setWindowType(WindowType(windowType));

    auto currentTransformSourceModel = ModelById::get(m_currentTransformSource);
    if (!currentTransformSourceModel) return true;

    sv_samplerate_t sampleRate = currentTransformSourceModel->getSampleRate();

    QString startFrameStr = attributes.value("startFrame");
    QString durationStr = attributes.value("duration");

    int startFrame = 0;
    int duration = 0;

    if (startFrameStr != "") {
        startFrame = startFrameStr.trimmed().toInt(&ok);
        if (!ok) startFrame = 0;
    }
    if (durationStr != "") {
        duration = durationStr.trimmed().toInt(&ok);
        if (!ok) duration = 0;
    }

    m_currentTransform.setStartTime
        (RealTime::frame2RealTime(startFrame, sampleRate));

    m_currentTransform.setDuration
        (RealTime::frame2RealTime(duration, sampleRate));

    return true;
}

bool
SVFileReader::readPlayParameters(const QXmlAttributes &attributes)
{
    m_currentPlayParameters = {};

    int modelExportId = 0;
    bool modelOk = false;
    modelExportId = attributes.value("model").trimmed().toInt(&modelOk);

    if (!modelOk) {
        SVCERR << "WARNING: SV-XML: No model id specified for play parameters" << endl;
        return false;
    }

    if (haveModel(modelExportId)) {

        bool ok = false;

        auto parameters = PlayParameterRepository::getInstance()->
            getPlayParameters(m_models[modelExportId].untyped);

        if (!parameters) {
            SVCERR << "WARNING: SV-XML: Play parameters for model "
                      << modelExportId
                      << " not found - has model been added to document?"
                      << endl;
            return false;
        }
        
        bool muted = (attributes.value("mute").trimmed() == "true");
        parameters->setPlayMuted(muted);
        
        float pan = attributes.value("pan").toFloat(&ok);
        if (ok) parameters->setPlayPan(pan);
        
        float gain = attributes.value("gain").toFloat(&ok);
        if (ok) parameters->setPlayGain(gain);
        
        QString clipId = attributes.value("clipId");
        if (clipId != "") parameters->setPlayClipId(clipId);
        
        m_currentPlayParameters = parameters;

//        SVCERR << "Current play parameters for model: " << m_models[modelExportId] << ": " << m_currentPlayParameters << endl;

    } else {

        SVCERR << "WARNING: SV-XML: Unknown model " << modelExportId
               << " for play parameters" << endl;
        return false;
    }

    return true;
}

bool
SVFileReader::readPlugin(const QXmlAttributes &attributes)
{
    if (m_pendingDerivedModel != XmlExportable::NO_ID) {
        return readPluginForTransform(attributes);
    } else if (m_currentPlayParameters) {
        return readPluginForPlayback(attributes);
    } else {
        SVCERR << "WARNING: SV-XML: Plugin found outside derivation or play parameters" << endl;
        return false;
    }
}

bool
SVFileReader::readPluginForTransform(const QXmlAttributes &attributes)
{
    if (m_currentTransformIsNewStyle) {
        // Not needed, we have the transform element instead
        return true;
    }

    QString configurationXml = "<plugin";

    for (int i = 0; i < attributes.length(); ++i) {
        configurationXml += QString(" %1=\"%2\"")
            .arg(attributes.qName(i))
            .arg(XmlExportable::encodeEntities(attributes.value(i)));
    }

    configurationXml += "/>";

    TransformFactory::getInstance()->
        setParametersFromPluginConfigurationXml(m_currentTransform,
                                                configurationXml);
    return true;
}

bool
SVFileReader::readPluginForPlayback(const QXmlAttributes &attributes)
{
    // Obsolete but supported for compatibility

    QString ident = attributes.value("identifier");
    if (ident == "sample_player") {
        QString clipId = attributes.value("program");
        if (clipId != "") m_currentPlayParameters->setPlayClipId(clipId);
    }

    return true;
}

bool
SVFileReader::readTransform(const QXmlAttributes &attributes)
{
    if (m_pendingDerivedModel == XmlExportable::NO_ID) {
        SVCERR << "WARNING: SV-XML: Transform found outside derivation" << endl;
        return false;
    }

    m_currentTransform = Transform();
    m_currentTransform.setFromXmlAttributes(attributes);
    return true;
}

bool
SVFileReader::readParameter(const QXmlAttributes &attributes)
{
    if (m_pendingDerivedModel == XmlExportable::NO_ID) {
        SVCERR << "WARNING: SV-XML: Parameter found outside derivation" << endl;
        return false;
    }

    QString name = attributes.value("name");
    if (name == "") {
        SVCERR << "WARNING: SV-XML: Ignoring nameless transform parameter"
                  << endl;
        return false;
    }

    float value = attributes.value("value").trimmed().toFloat();

    m_currentTransform.setParameter(name, value);
    return true;
}

bool
SVFileReader::readSelection(const QXmlAttributes &attributes)
{
    bool ok;

    READ_MANDATORY(int, start, toInt);
    READ_MANDATORY(int, end, toInt);

    m_paneCallback.addSelection(start, end);

    return true;
}

bool
SVFileReader::readMeasurement(const QXmlAttributes &attributes)
{
    SVDEBUG << "SVFileReader::readMeasurement: inLayer "
              << m_inLayer << ", layer " << m_currentLayer << endl;

    if (!m_inLayer) {
        SVCERR << "WARNING: SV-XML: Measurement found outside layer" << endl;
        return false;
    }

    m_currentLayer->addMeasurementRect(attributes);
    return true;
}

SVFileReaderPaneCallback::~SVFileReaderPaneCallback()
{
}


class SVFileIdentifier : public QXmlDefaultHandler
{
public:
    SVFileIdentifier() :
        m_inSv(false),
        m_inData(false),
        m_type(SVFileReader::UnknownFileType)
    { }
    ~SVFileIdentifier() override { }

    void parse(QXmlInputSource &source) {
        QXmlSimpleReader reader;
        reader.setContentHandler(this);
        reader.setErrorHandler(this);
        reader.parse(source);
    }

    SVFileReader::FileType getType() const { return m_type; }

    bool startElement(const QString &,
                              const QString &,
                              const QString &qName,
                              const QXmlAttributes& atts) override
    {
        QString name = qName.toLower();

        // SV session files have an sv element containing a data
        // element containing a model element with mainModel="true".

        // If the sv element is present but the rest does not satisfy,
        // then it's (probably) an SV layer file.

        // Otherwise, it's of unknown type.

        if (name == "sv") {
            m_inSv = true;
            if (m_type == SVFileReader::UnknownFileType) {
                m_type = SVFileReader::SVLayerFile;
            }
            return true;
        } else if (name == "data") {
            if (!m_inSv) return true;
            m_inData = true;
        } else if (name == "model") {
            if (!m_inData) return true;
            if (atts.value("mainModel").trimmed() == "true") {
                if (m_type == SVFileReader::SVLayerFile) {
                    m_type = SVFileReader::SVSessionFile;
                    return false; // done
                }
            }
        }
        return true;
    }

    bool endElement(const QString &,
                            const QString &,
                            const QString &qName) override
    {
        QString name = qName.toLower();

        if (name == "sv") {
            if (m_inSv) {
                m_inSv = false;
                return false; // done
            }
        } else if (name == "data") {
            if (m_inData) {
                m_inData = false;
                return false; // also done, nothing after the first
                              // data element is of use here
            }
        }
        return true;
    }

private:
    bool m_inSv;
    bool m_inData;
    SVFileReader::FileType m_type;
};


SVFileReader::FileType
SVFileReader::identifyXmlFile(QString path)
{
    QFile file(path);
    SVFileIdentifier identifier;
    QXmlInputSource source(&file);
    identifier.parse(source);
    return identifier.getType();
}

    
    
