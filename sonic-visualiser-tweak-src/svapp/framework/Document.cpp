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

#include "Document.h"

#include "Align.h"

#include "data/model/WaveFileModel.h"
#include "data/model/WritableWaveFileModel.h"
#include "data/model/DenseThreeDimensionalModel.h"
#include "data/model/DenseTimeValueModel.h"
#include "data/model/AggregateWaveModel.h"

#include "layer/Layer.h"
#include "widgets/CommandHistory.h"
#include "base/Command.h"
#include "view/View.h"
#include "base/PlayParameterRepository.h"
#include "base/PlayParameters.h"
#include "transform/TransformFactory.h"
#include "transform/ModelTransformerFactory.h"
#include "transform/FeatureExtractionModelTransformer.h"
#include <QApplication>
#include <QTextStream>
#include <QSettings>
#include <iostream>
#include <typeinfo>

#include "data/model/AlignmentModel.h"
#include "Align.h"

using std::vector;

#define DEBUG_DOCUMENT 1

//!!! still need to handle command history, documentRestored/documentModified

Document::Document() :
    m_autoAlignment(false),
    m_align(new Align()),
    m_isIncomplete(false)
{
    connect(ModelTransformerFactory::getInstance(),
            SIGNAL(transformFailed(QString, QString)),
            this,
            SIGNAL(modelGenerationFailed(QString, QString)));

    connect(m_align, SIGNAL(alignmentComplete(ModelId)),
            this, SIGNAL(alignmentComplete(ModelId)));
}

Document::~Document()
{
    //!!! Document should really own the command history.  atm we
    //still refer to it in various places that don't have access to
    //the document, be nice to fix that

#ifdef DEBUG_DOCUMENT
    SVDEBUG << "\n\nDocument::~Document: about to clear command history" << endl;
#endif
    CommandHistory::getInstance()->clear();
    
#ifdef DEBUG_DOCUMENT
    SVCERR << "Document::~Document: about to delete layers" << endl;
#endif
    while (!m_layers.empty()) {
        deleteLayer(*m_layers.begin(), true);
    }

#ifdef DEBUG_DOCUMENT
    SVCERR << "Document::~Document: about to release normal models" << endl;
#endif
    for (auto mr: m_models) {
        ModelById::release(mr.first);
    }

#ifdef DEBUG_DOCUMENT
    SVCERR << "Document::~Document: about to release aggregate models" << endl;
#endif
    for (auto m: m_aggregateModels) {
        ModelById::release(m);
    }

#ifdef DEBUG_DOCUMENT
    SVCERR << "Document::~Document: about to release alignment models" << endl;
#endif
    for (auto m: m_alignmentModels) {
        ModelById::release(m);
    }

#ifdef DEBUG_DOCUMENT
    SVCERR << "Document::~Document: about to release main model" << endl;
#endif
    if (!m_mainModel.isNone()) {
        ModelById::release(m_mainModel);
    }
    
    m_mainModel = {};
    emit mainModelChanged({});
}

Layer *
Document::createLayer(LayerFactory::LayerType type)
{
    Layer *newLayer = LayerFactory::getInstance()->createLayer(type);
    if (!newLayer) return nullptr;

    newLayer->setObjectName(getUniqueLayerName(newLayer->objectName()));

    m_layers.push_back(newLayer);

#ifdef DEBUG_DOCUMENT
    SVDEBUG << "Document::createLayer: Added layer of type " << type
              << ", now have " << m_layers.size() << " layers" << endl;
#endif

    emit layerAdded(newLayer);

    return newLayer;
}

Layer *
Document::createMainModelLayer(LayerFactory::LayerType type)
{
    Layer *newLayer = createLayer(type);
    if (!newLayer) return nullptr;
    setModel(newLayer, m_mainModel);
    return newLayer;
}

Layer *
Document::createImportedLayer(ModelId modelId)
{
    LayerFactory::LayerTypeSet types =
        LayerFactory::getInstance()->getValidLayerTypes(modelId);


    if (types.empty()) {
        SVCERR << "WARNING: Document::importLayer: no valid display layer for model" << endl;
        return nullptr;
    }

    //!!! for now, just use the first suitable layer type
    LayerFactory::LayerType type = *types.begin();
    qDebug() << "JPMAUS Creating layer of type: " << type;

    Layer *newLayer = LayerFactory::getInstance()->createLayer(type);
    if (!newLayer) return nullptr;

    newLayer->setObjectName(getUniqueLayerName(newLayer->objectName()));

    addNonDerivedModel(modelId);
    setModel(newLayer, modelId);

    //!!! and all channels
    setChannel(newLayer, -1);

    m_layers.push_back(newLayer);

#ifdef DEBUG_DOCUMENT
    SVDEBUG << "Document::createImportedLayer: Added layer of type " << type
              << ", now have " << m_layers.size() << " layers" << endl;
#endif

    emit layerAdded(newLayer);
    return newLayer;
}

Layer *
Document::createEmptyLayer(LayerFactory::LayerType type)
{
    if (m_mainModel.isNone()) return nullptr;

    auto newModel =
        LayerFactory::getInstance()->createEmptyModel(type, m_mainModel);
    if (!newModel) return nullptr;
    
    Layer *newLayer = createLayer(type);
    if (!newLayer) {
        return nullptr;
    }

    auto newModelId = ModelById::add(newModel);
    addNonDerivedModel(newModelId);
    setModel(newLayer, newModelId);

    return newLayer;
}

Layer *
Document::createDerivedLayer(LayerFactory::LayerType type,
                             TransformId transform)
{
    Layer *newLayer = createLayer(type);
    if (!newLayer) return nullptr;

    newLayer->setObjectName(getUniqueLayerName
                            (TransformFactory::getInstance()->
                             getTransformFriendlyName(transform)));

    return newLayer;
}

Layer *
Document::createDerivedLayer(const Transform &transform,
                             const ModelTransformer::Input &input)
{
    Transforms transforms;
    transforms.push_back(transform);
    vector<Layer *> layers = createDerivedLayers(transforms, input);
    if (layers.empty()) return nullptr;
    else return layers[0];
}

vector<Layer *>
Document::createDerivedLayers(const Transforms &transforms,
                              const ModelTransformer::Input &input)
{
    QString message;
    vector<ModelId> newModels =
        addDerivedModels(transforms, input, message, nullptr);

    if (newModels.empty()) {
        //!!! This identifier may be wrong!
        emit modelGenerationFailed(transforms[0].getIdentifier(), message);
        return vector<Layer *>();
    } else if (message != "") {
        //!!! This identifier may be wrong!
        emit modelGenerationWarning(transforms[0].getIdentifier(), message);
    }

    QStringList names;
    for (int i = 0; in_range_for(newModels, i); ++i) {
        names.push_back(getUniqueLayerName
                        (TransformFactory::getInstance()->
                         getTransformFriendlyName
                         (transforms[i].getIdentifier())));
    }

    vector<Layer *> layers = createLayersForDerivedModels(newModels, names);
    return layers;
}

class AdditionalModelConverter : 
    public ModelTransformerFactory::AdditionalModelHandler
{
public:
    AdditionalModelConverter(Document *doc, 
                             Document::LayerCreationHandler *handler) :
        m_doc(doc),
        m_handler(handler) {
    }

    ~AdditionalModelConverter() override { }

    void
    setPrimaryLayers(vector<Layer *> layers) {
        m_primary = layers;
    }

    void
    moreModelsAvailable(vector<ModelId> models) override {
        SVDEBUG << "AdditionalModelConverter::moreModelsAvailable: " << models.size() << " model(s)" << endl;
        // We can't automatically regenerate the additional models on
        // reload - so they go in m_additionalModels instead of m_models
        QStringList names;
        foreach (ModelId modelId, models) {
            m_doc->addAdditionalModel(modelId);
            names.push_back(QString());
        }
        vector<Layer *> layers = m_doc->createLayersForDerivedModels
            (models, names);
        m_handler->layersCreated(this, m_primary, layers);
        delete this;
    }

    void
    noMoreModelsAvailable() override {
        SVDEBUG << "AdditionalModelConverter::noMoreModelsAvailable" << endl;
        m_handler->layersCreated(this, m_primary, vector<Layer *>());
        delete this;
    }

    void cancel() {
        foreach (Layer *layer, m_primary) {
            m_doc->setModel(layer, {});
        }
    }

private:
    Document *m_doc;
    vector<Layer *> m_primary;
    Document::LayerCreationHandler *m_handler; //!!! how to handle destruction of this?
};

Document::LayerCreationAsyncHandle
Document::createDerivedLayersAsync(const Transforms &transforms,
                                   const ModelTransformer::Input &input,
                                   LayerCreationHandler *handler)
{
    QString message;

    AdditionalModelConverter *amc = new AdditionalModelConverter(this, handler);
    
    vector<ModelId> newModels = addDerivedModels
        (transforms, input, message, amc);

    QStringList names;
    for (int i = 0; in_range_for(newModels, i); ++i) {
        names.push_back(getUniqueLayerName
                        (TransformFactory::getInstance()->
                         getTransformFriendlyName
                         (transforms[i].getIdentifier())));
    }

    vector<Layer *> layers = createLayersForDerivedModels(newModels, names);
    amc->setPrimaryLayers(layers);

    if (newModels.empty()) {
        //!!! This identifier may be wrong!
        emit modelGenerationFailed(transforms[0].getIdentifier(), message);
        //!!! what to do with amc?
    } else if (message != "") {
        //!!! This identifier may be wrong!
        emit modelGenerationWarning(transforms[0].getIdentifier(), message);
        //!!! what to do with amc?
    }

    return amc;
}

void
Document::cancelAsyncLayerCreation(Document::LayerCreationAsyncHandle h)
{
    AdditionalModelConverter *conv = static_cast<AdditionalModelConverter *>(h);
    conv->cancel();
}

vector<Layer *>
Document::createLayersForDerivedModels(vector<ModelId> newModels, 
                                       QStringList names)
{
    vector<Layer *> layers;
    
    for (int i = 0; in_range_for(newModels, i); ++i) {

        ModelId newModelId = newModels[i];

        LayerFactory::LayerTypeSet types =
            LayerFactory::getInstance()->getValidLayerTypes(newModelId);

        if (types.empty()) {
            SVCERR << "WARNING: Document::createLayerForTransformer: no valid display layer for output of transform " << names[i] << endl;
            releaseModel(newModelId);
            return vector<Layer *>();
        }

        //!!! for now, just use the first suitable layer type

        Layer *newLayer = createLayer(*types.begin());
        setModel(newLayer, newModelId);

        //!!! We need to clone the model when adding the layer, so that it
        //can be edited without affecting other layers that are based on
        //the same model.  Unfortunately we can't just clone it now,
        //because it probably hasn't been completed yet -- the transform
        //runs in the background.  Maybe the transform has to handle
        //cloning and cacheing models itself.
        //
        // Once we do clone models here, of course, we'll have to avoid
        // leaking them too.
        //
        // We want the user to be able to add a model to a second layer
        // _while it's still being calculated in the first_ and have it
        // work quickly.  That means we need to put the same physical
        // model pointer in both layers, so they can't actually be cloned.
    
        if (newLayer) {
            newLayer->setObjectName(names[i]);
        }

        emit layerAdded(newLayer);
        layers.push_back(newLayer);
    }

    return layers;
}

void
Document::setMainModel(ModelId modelId)
{
    ModelId oldMainModel = m_mainModel;
    m_mainModel = modelId;
    
    emit modelAdded(m_mainModel);
    
    if (auto model = ModelById::get(modelId)) {
        emit activity(tr("Set main model to %1").arg(model->objectName()));
    } else {
        emit activity(tr("Clear main model"));
    }

    std::vector<Layer *> obsoleteLayers;
    std::set<QString> failedTransformers;

    // We need to ensure that no layer is left using oldMainModel or
    // any of the old derived models as its model.  Either replace the
    // model, or delete the layer for each layer that is currently
    // using one of these.  Carry out this replacement before we
    // delete any of the models.

#ifdef DEBUG_DOCUMENT
    SVDEBUG << "Document::setMainModel: Have "
              << m_layers.size() << " layers" << endl;
    SVDEBUG << "Models now: ";
    for (const auto &r: m_models) {
        SVDEBUG << r.first << " ";
    }
    SVDEBUG << endl;
    SVDEBUG << "Old main model: " << oldMainModel << endl;
#endif

    for (Layer *layer: m_layers) {

        ModelId modelId = layer->getModel();

#ifdef DEBUG_DOCUMENT
        SVDEBUG << "Document::setMainModel: inspecting model "
                << modelId << " in layer " << layer->objectName() << endl;
#endif

        if (modelId == oldMainModel) {
#ifdef DEBUG_DOCUMENT
            SVDEBUG << "... it uses the old main model, replacing" << endl;
#endif
            LayerFactory::getInstance()->setModel(layer, m_mainModel);
            continue;
        }

        if (modelId.isNone()) {
            SVCERR << "WARNING: Document::setMainModel: Null model in layer "
                   << layer << endl;
            // get rid of this hideous degenerate
            obsoleteLayers.push_back(layer);
            continue;
        }

        if (m_models.find(modelId) == m_models.end()) {
            SVCERR << "WARNING: Document::setMainModel: Unknown model "
                   << modelId << " in layer " << layer << endl;
            // and this one
            obsoleteLayers.push_back(layer);
            continue;
        }

        ModelRecord record = m_models[modelId];
        
        if (!record.source.isNone() && (record.source == oldMainModel)) {

#ifdef DEBUG_DOCUMENT
            SVDEBUG << "... it uses a model derived from the old main model, regenerating" << endl;
#endif

            // This model was derived from the previous main
            // model: regenerate it.
            
            const Transform &transform = record.transform;
            QString transformId = transform.getIdentifier();
            
            //!!! We have a problem here if the number of channels in
            //the main model has changed.

            QString message;
            ModelId replacementModel =
                addDerivedModel(transform,
                                ModelTransformer::Input
                                (m_mainModel, record.channel),
                                message);
            
            if (replacementModel.isNone()) {
                SVCERR << "WARNING: Document::setMainModel: Failed to regenerate model for transform \""
                       << transformId << "\"" << " in layer " << layer << endl;
                if (failedTransformers.find(transformId)
                    == failedTransformers.end()) {
                    emit modelRegenerationFailed(layer->objectName(),
                                                 transformId,
                                                 message);
                    failedTransformers.insert(transformId);
                }
                obsoleteLayers.push_back(layer);
            } else {
                if (message != "") {
                    emit modelRegenerationWarning(layer->objectName(),
                                                  transformId,
                                                  message);
                }
#ifdef DEBUG_DOCUMENT
                SVDEBUG << "Replacing model " << modelId << ") with model "
                        << replacementModel << ") in layer "
                        << layer << " (name " << layer->objectName() << ")"
                        << endl;

                auto rm = ModelById::getAs<RangeSummarisableTimeValueModel>(replacementModel);
                if (rm) {
                    SVDEBUG << "new model has " << rm->getChannelCount() << " channels " << endl;
                } else {
                    SVDEBUG << "new model " << replacementModel << " is not a RangeSummarisableTimeValueModel!" << endl;
                }
#endif
                setModel(layer, replacementModel);
            }
        }            
    }

    for (size_t k = 0; k < obsoleteLayers.size(); ++k) {
        deleteLayer(obsoleteLayers[k], true);
    }

    std::set<ModelId> additionalModels;
    for (const auto &rec : m_models) {
        if (rec.second.additional) {
            additionalModels.insert(rec.first);
        }
    }
    for (ModelId a: additionalModels) {
        m_models.erase(a);
    }

    for (const auto &rec : m_models) {

        auto m = ModelById::get(rec.first);
        if (!m) continue;

#ifdef DEBUG_DOCUMENT
        SVDEBUG << "considering alignment for model " << rec.first << endl;
#endif

        if (m_autoAlignment) {

            alignModel(rec.first);

        } else if (!oldMainModel.isNone() && 
                   (m->getAlignmentReference() == oldMainModel)) {

            alignModel(rec.first);
        }
    }

    if (m_autoAlignment) {
        SVDEBUG << "Document::setMainModel: auto-alignment is on, aligning model if possible" << endl;
        alignModel(m_mainModel);
    } else {
        SVDEBUG << "Document::setMainModel: auto-alignment is off" << endl;
    }

    emit mainModelChanged(m_mainModel);

    if (!oldMainModel.isNone()) {

        // Remove the playable explicitly - the main model's dtor will
        // do this, but just in case something is still hanging onto a
        // shared_ptr to the old main model so it doesn't get deleted
        PlayParameterRepository::getInstance()->removePlayable
            (oldMainModel.untyped);

        ModelById::release(oldMainModel);
    }
}

void
Document::addAlreadyDerivedModel(const Transform &transform,
                                 const ModelTransformer::Input &input,
                                 ModelId outputModelToAdd)
{
    if (m_models.find(outputModelToAdd) != m_models.end()) {
        SVCERR << "WARNING: Document::addAlreadyDerivedModel: Model already added"
               << endl;
        return;
    }
    
#ifdef DEBUG_DOCUMENT
    SVDEBUG << "Document::addAlreadyDerivedModel: source is " << input.getModel() << endl;
#endif

    ModelRecord rec;
    rec.source = input.getModel();
    rec.channel = input.getChannel();
    rec.transform = transform;
    rec.additional = false;

    if (auto m = ModelById::get(outputModelToAdd)) {
        m->setSourceModel(input.getModel());
    }

    m_models[outputModelToAdd] = rec;

#ifdef DEBUG_DOCUMENT
    SVDEBUG << "Document::addAlreadyDerivedModel: Added model " << outputModelToAdd << endl;
    SVDEBUG << "Models now: ";
    for (const auto &rec : m_models) {
        SVDEBUG << rec.first << " ";
    } 
    SVDEBUG << endl;
#endif

    emit modelAdded(outputModelToAdd);
}

void
Document::addNonDerivedModel(ModelId modelId)
{
    if (ModelById::isa<AggregateWaveModel>(modelId)) {
#ifdef DEBUG_DOCUMENT
        SVCERR << "Document::addNonDerivedModel: Model " << modelId << " is an aggregate model, adding it to aggregates" << endl;
#endif
        m_aggregateModels.insert(modelId);
        return;
    }
    if (ModelById::isa<AlignmentModel>(modelId)) {
#ifdef DEBUG_DOCUMENT
        SVCERR << "Document::addNonDerivedModel: Model " << modelId << " is an alignment model, adding it to alignments" << endl;
#endif
        m_alignmentModels.insert(modelId);
        return;
    }
    
    if (m_models.find(modelId) != m_models.end()) {
        SVCERR << "WARNING: Document::addNonDerivedModel: Model already added"
               << endl;
        return;
    }

    ModelRecord rec;
    rec.source = {};
    rec.channel = 0;
    rec.additional = false;

    m_models[modelId] = rec;

#ifdef DEBUG_DOCUMENT
    SVCERR << "Document::addNonDerivedModel: Added model " << modelId << endl;
    SVCERR << "Models now: ";
    for (const auto &rec : m_models) {
        SVCERR << rec.first << " ";
    } 
    SVCERR << endl;
#endif

    if (m_autoAlignment) {
        SVDEBUG << "Document::addNonDerivedModel: auto-alignment is on, aligning model if possible" << endl;
        alignModel(modelId);
    } else {
        SVDEBUG << "Document(" << this << "): addNonDerivedModel: auto-alignment is off" << endl;
    }

    emit modelAdded(modelId);
}

void
Document::addAdditionalModel(ModelId modelId)
{
    if (m_models.find(modelId) != m_models.end()) {
        SVCERR << "WARNING: Document::addAdditionalModel: Model already added"
               << endl;
        return;
    }

    ModelRecord rec;
    rec.source = {};
    rec.channel = 0;
    rec.additional = true;

    m_models[modelId] = rec;

#ifdef DEBUG_DOCUMENT
    SVDEBUG << "Document::addAdditionalModel: Added model " << modelId << endl;
    SVDEBUG << "Models now: ";
    for (const auto &rec : m_models) {
        SVDEBUG << rec.first << " ";
    } 
    SVDEBUG << endl;
#endif

    if (m_autoAlignment &&
        ModelById::isa<RangeSummarisableTimeValueModel>(modelId)) {
        SVDEBUG << "Document::addAdditionalModel: auto-alignment is on and model is an alignable type, aligning it if possible" << endl;
        alignModel(modelId);
    }

    emit modelAdded(modelId);
}

ModelId
Document::addDerivedModel(const Transform &transform,
                          const ModelTransformer::Input &input,
                          QString &message)
{
    for (auto &rec : m_models) {
        if (rec.second.transform == transform &&
            rec.second.source == input.getModel() && 
            rec.second.channel == input.getChannel()) {
            SVDEBUG << "derived model taken from map " << endl;
            return rec.first;
        }
    }

    Transforms tt;
    tt.push_back(transform);
    vector<ModelId> mm = addDerivedModels(tt, input, message, nullptr);
    if (mm.empty()) return {};
    else return mm[0];
}

vector<ModelId>
Document::addDerivedModels(const Transforms &transforms,
                           const ModelTransformer::Input &input,
                           QString &message,
                           AdditionalModelConverter *amc)
{
    vector<ModelId> mm = 
        ModelTransformerFactory::getInstance()->transformMultiple
        (transforms, input, message, amc);

    for (int j = 0; in_range_for(mm, j); ++j) {

        ModelId modelId = mm[j];
        Transform applied = transforms[j];

        if (modelId.isNone()) {
            SVCERR << "WARNING: Document::addDerivedModel: no output model for transform " << applied.getIdentifier() << endl;
            continue;
        }

        // The transform we actually used was presumably identical to
        // the one asked for, except that the version of the plugin
        // may differ.  It's possible that the returned message
        // contains a warning about this; that doesn't concern us
        // here, but we do need to ensure that the transform we
        // remember is correct for what was actually applied, with the
        // current plugin version.

        //!!! would be nice to short-circuit this -- the version is
        //!!! static data, shouldn't have to construct a plugin for it
        //!!! (which may be expensive in Piper-world)
        applied.setPluginVersion
            (TransformFactory::getInstance()->
             getDefaultTransformFor(applied.getIdentifier(),
                                    applied.getSampleRate())
             .getPluginVersion());

        addAlreadyDerivedModel(applied, input, modelId);
    }
        
    return mm;
}

void
Document::releaseModel(ModelId modelId)
{
    // This is called when a layer has been deleted or has replaced
    // its model, in order to reclaim storage for the old model. It
    // could be a no-op without making any functional difference, as
    // all the models stored in the ById pool are released when the
    // document is deleted. But models can sometimes be large, so if
    // we know no other layer is using one, we should release it. If
    // we happen to release one that is being used, the ModelById
    // borrowed-pointer mechanism will at least prevent memory errors,
    // although the other code will have to stop whatever it's doing.

// "warning: expression with side effects will be evaluated despite
// being used as an operand to 'typeid'"
#ifdef __clang__
#pragma clang diagnostic ignored "-Wpotentially-evaluated-expression"
#endif

    if (auto model = ModelById::get(modelId)) {
        SVCERR << "Document::releaseModel(" << modelId << "), name "
               << model->objectName() << ", type "
               << typeid(*model.get()).name() << endl;
    } else {
        SVCERR << "Document::releaseModel(" << modelId << ")" << endl;
    }
    
    if (modelId.isNone()) {
        return;
    }
    
#ifdef DEBUG_DOCUMENT
    SVCERR << "Document::releaseModel(" << modelId << ")" << endl;
#endif

    if (modelId == m_mainModel) {
#ifdef DEBUG_DOCUMENT
        SVCERR << "Document::releaseModel: It's the main model, ignoring"
               << endl;
#endif
        return;
    }

    if (m_models.find(modelId) == m_models.end()) {
        // No point in releasing aggregate and alignment models,
        // they're not large
#ifdef DEBUG_DOCUMENT
        SVCERR << "Document::releaseModel: It's not a regular layer model, ignoring" << endl;
#endif
        return;
    }

    for (auto layer: m_layers) {
        if (layer->getModel() == modelId) {
#ifdef DEBUG_DOCUMENT
            SVCERR << "Document::releaseModel: It's still in use in at least one layer (e.g. " << layer << ", \"" << layer->getLayerPresentationName() << "\"), ignoring" << endl;
#endif
            return;
        }
    }

#ifdef DEBUG_DOCUMENT
    SVCERR << "Document::releaseModel: Seems to be OK to release this one"
           << endl;
#endif

    int sourceCount = 0;

    for (auto &m: m_models) {
        if (m.second.source == modelId) {
            ++sourceCount;
            m.second.source = {};
        }
    }

    if (sourceCount > 0) {
        SVCERR << "Document::releaseModel: Request to release model "
               << modelId << " even though it was source for "
               << sourceCount << " other derived model(s) -- have cleared "
               << "their source fields" << endl;
    }

    m_models.erase(modelId);
    ModelById::release(modelId);
}

void
Document::deleteLayer(Layer *layer, bool force)
{
    if (m_layerViewMap.find(layer) != m_layerViewMap.end() &&
        m_layerViewMap[layer].size() > 0) {

        if (force) {

            SVDEBUG << "NOTE: Document::deleteLayer: Layer "
                    << layer << " [" << layer->objectName() << "]"
                    << " is still used in " << m_layerViewMap[layer].size()
                    << " views. Force flag set, so removing from them" << endl;
            
            for (std::set<View *>::iterator j = m_layerViewMap[layer].begin();
                 j != m_layerViewMap[layer].end(); ++j) {
                // don't use removeLayerFromView, as it issues a command
                layer->setLayerDormant(*j, true);
                (*j)->removeLayer(layer);
            }
            
            m_layerViewMap.erase(layer);

        } else {

            SVCERR << "WARNING: Document::deleteLayer: Layer "
                   << layer << " [" << layer->objectName() << "]"
                   << " is still used in " << m_layerViewMap[layer].size()
                   << " views! Force flag is not set, so not deleting" << endl;
            
            return;
        }
    }

    bool found = false;
    for (auto itr = m_layers.begin(); itr != m_layers.end(); ++itr) {
        if (*itr == layer) {
            found = true;
            m_layers.erase(itr);
            break;
        }
    }
    if (!found) {
        SVDEBUG << "Document::deleteLayer: Layer "
                  << layer << " (typeid " << typeid(layer).name() <<
                  ") does not exist, or has already been deleted "
                  << "(this may not be as serious as it sounds)" << endl;
        return;
    }

#ifdef DEBUG_DOCUMENT
    SVDEBUG << "Document::deleteLayer: Removing (and about to release model), now have "
              << m_layers.size() << " layers" << endl;
#endif

    releaseModel(layer->getModel());
    emit layerRemoved(layer);
    emit layerAboutToBeDeleted(layer);
    delete layer;
}

void
Document::setModel(Layer *layer, ModelId modelId)
{
    if (!modelId.isNone() && 
        modelId != m_mainModel &&
        m_models.find(modelId) == m_models.end()) {
        SVCERR << "ERROR: Document::setModel: Layer " << layer
               << " (\"" << layer->objectName()
               << "\") wants to use unregistered model " << modelId
               << ": register the layer's model before setting it!"
               << endl;
        return;
    }

    ModelId previousModel = layer->getModel();

    if (previousModel == modelId) {
        SVDEBUG << "NOTE: Document::setModel: Layer " << layer << " (\""
                << layer->objectName()
                << "\") is already set to model "
                << modelId << endl;
        return;
    }

    if (!modelId.isNone() && !previousModel.isNone()) {
        PlayParameterRepository::getInstance()->copyParameters
            (previousModel.untyped, modelId.untyped);
    }

    LayerFactory::getInstance()->setModel(layer, modelId);

    releaseModel(previousModel);
}

void
Document::setChannel(Layer *layer, int channel)
{
    LayerFactory::getInstance()->setChannel(layer, channel);
}

void
Document::addLayerToView(View *view, Layer *layer)
{
    ModelId modelId = layer->getModel();
    if (modelId.isNone()) {
#ifdef DEBUG_DOCUMENT
        SVDEBUG << "Document::addLayerToView: Layer (\""
                  << layer->objectName()
                << "\") with no model being added to view: "
                  << "normally you want to set the model first" << endl;
#endif
    } else {
        if (modelId != m_mainModel &&
            m_models.find(modelId) == m_models.end()) {
            SVCERR << "ERROR: Document::addLayerToView: Layer " << layer
                      << " has unregistered model " << modelId
                      << " -- register the layer's model before adding the layer!" << endl;
            return;
        }
    }

    CommandHistory::getInstance()->addCommand
        (new Document::AddLayerCommand(this, view, layer));
}

void
Document::removeLayerFromView(View *view, Layer *layer)
{
    CommandHistory::getInstance()->addCommand
        (new Document::RemoveLayerCommand(this, view, layer));
}

void
Document::addToLayerViewMap(Layer *layer, View *view)
{
    bool firstView = (m_layerViewMap.find(layer) == m_layerViewMap.end() ||
                      m_layerViewMap[layer].empty());

    if (m_layerViewMap[layer].find(view) !=
        m_layerViewMap[layer].end()) {
        SVCERR << "WARNING: Document::addToLayerViewMap:"
                  << " Layer " << layer << " -> view " << view << " already in"
                  << " layer view map -- internal inconsistency" << endl;
    }

    m_layerViewMap[layer].insert(view);

    if (firstView) emit layerInAView(layer, true);
}
    
void
Document::removeFromLayerViewMap(Layer *layer, View *view)
{
    if (m_layerViewMap[layer].find(view) ==
        m_layerViewMap[layer].end()) {
        SVCERR << "WARNING: Document::removeFromLayerViewMap:"
                  << " Layer " << layer << " -> view " << view << " not in"
                  << " layer view map -- internal inconsistency" << endl;
    }

    m_layerViewMap[layer].erase(view);

    if (m_layerViewMap[layer].empty()) {
        m_layerViewMap.erase(layer);
        emit layerInAView(layer, false);
    }
}

QString
Document::getUniqueLayerName(QString candidate)
{
    for (int count = 1; ; ++count) {

        QString adjusted =
            (count > 1 ? QString("%1 <%2>").arg(candidate).arg(count) :
             candidate);
        
        bool duplicate = false;

        for (auto i = m_layers.begin(); i != m_layers.end(); ++i) {
            if ((*i)->objectName() == adjusted) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate) return adjusted;
    }
}

std::vector<ModelId>
Document::getTransformInputModels()
{
    std::vector<ModelId> models;

    if (m_mainModel.isNone()) return models;

    models.push_back(m_mainModel);

    //!!! This will pick up all models, including those that aren't visible...

    for (auto rec: m_models) {

        ModelId modelId = rec.first;
        if (modelId == m_mainModel) continue;

        auto dtvm = ModelById::getAs<DenseTimeValueModel>(modelId);
        if (dtvm) {
            models.push_back(modelId);
        }
    }

    return models;
}

bool
Document::isKnownModel(const ModelId modelId) const
{
    if (modelId == m_mainModel) return true;
    for (auto rec: m_models) {
        if (rec.first == modelId) return true;
    }
    return false;
}

bool
Document::canAlign()
{
    return Align::canAlign();
}

void
Document::alignModel(ModelId modelId, bool forceRecalculate)
{
    SVDEBUG << "Document::alignModel(" << modelId << ", " << forceRecalculate
            << ") (main model is " << m_mainModel << ")" << endl;

    auto rm = ModelById::getAs<RangeSummarisableTimeValueModel>(modelId);
    if (!rm) {
        SVDEBUG << "(model " << modelId << " is not an alignable sort)" << endl;
        return;
    }

    if (m_mainModel.isNone()) {
        SVDEBUG << "(no main model to align to)" << endl;
        if (forceRecalculate && !rm->getAlignment().isNone()) {
            SVDEBUG << "(but model is aligned, and forceRecalculate is true, "
                    << "so resetting alignment to nil)" << endl;
            rm->setAlignment({});
        }
        return;
    }

    if (rm->getAlignmentReference() == m_mainModel) {
        SVDEBUG << "(model " << modelId << " is already aligned to main model "
                << m_mainModel << ")" << endl;
        if (!forceRecalculate) {
            return;
        } else {
            SVDEBUG << "(but forceRecalculate is true, so realigning anyway)"
                    << endl;
        }
    }
    
    if (modelId == m_mainModel) {
        // The reference has an empty alignment to itself.  This makes
        // it possible to distinguish between the reference and any
        // unaligned model just by looking at the model itself,
        // without also knowing what the main model is
        SVDEBUG << "Document::alignModel(" << modelId
                << "): is main model, setting alignment to itself" << endl;
        auto alignment = std::make_shared<AlignmentModel>(modelId, modelId,
                                                          ModelId());

        ModelId alignmentModelId = ModelById::add(alignment);
        rm->setAlignment(alignmentModelId);
        m_alignmentModels.insert(alignmentModelId);
        return;
    }

    auto w = ModelById::getAs<WritableWaveFileModel>(modelId);
    if (w && w->getWriteProportion() < 100) {
        SVDEBUG << "Document::alignModel(" << modelId
                << "): model write is not complete, deferring"
                << endl;
        connect(w.get(), SIGNAL(writeCompleted(ModelId)),
                this, SLOT(performDeferredAlignment(ModelId)));
        return;
    }

    SVDEBUG << "Document::alignModel: aligning..." << endl;
    if (!rm->getAlignmentReference().isNone()) {
        SVDEBUG << "(Note: model " << rm << " is currently aligned to model "
                << rm->getAlignmentReference() << "; this will replace that)"
                << endl;
    }

    QString err;
    if (!m_align->alignModel(this, m_mainModel, modelId, err)) {
        SVCERR << "Alignment failed: " << err << endl;
        emit alignmentFailed(err);
    }
}

void
Document::performDeferredAlignment(ModelId modelId)
{
    SVDEBUG << "Document::performDeferredAlignment: aligning..." << endl;
    alignModel(modelId);
}

void
Document::alignModels()
{
    for (auto rec: m_models) {
        alignModel(rec.first);
    }
    alignModel(m_mainModel);
}

void
Document::realignModels()
{
    for (auto rec: m_models) {
        alignModel(rec.first, true);
    }
    alignModel(m_mainModel);
}

Document::AddLayerCommand::AddLayerCommand(Document *d,
                                           View *view,
                                           Layer *layer) :
    m_d(d),
    m_view(view),
    m_layer(layer),
    m_name(qApp->translate("AddLayerCommand", "Add %1 Layer").arg(layer->objectName())),
    m_added(false)
{
}

Document::AddLayerCommand::~AddLayerCommand()
{
#ifdef DEBUG_DOCUMENT
    SVDEBUG << "Document::AddLayerCommand::~AddLayerCommand" << endl;
#endif
    if (!m_added) {
        m_d->deleteLayer(m_layer);
    }
}

QString
Document::AddLayerCommand::getName() const
{
#ifdef DEBUG_DOCUMENT
    SVDEBUG << "Document::AddLayerCommand::getName(): Name is "
              << m_name << endl;
#endif
    return m_name;
}

void
Document::AddLayerCommand::execute()
{
    for (int i = 0; i < m_view->getLayerCount(); ++i) {
        if (m_view->getLayer(i) == m_layer) {
            // already there
            m_layer->setLayerDormant(m_view, false);
            m_added = true;
            return;
        }
    }

    m_view->addLayer(m_layer);
    m_layer->setLayerDormant(m_view, false);

    m_d->addToLayerViewMap(m_layer, m_view);
    m_added = true;
}

void
Document::AddLayerCommand::unexecute()
{
    m_view->removeLayer(m_layer);
    m_layer->setLayerDormant(m_view, true);

    m_d->removeFromLayerViewMap(m_layer, m_view);
    m_added = false;
}

Document::RemoveLayerCommand::RemoveLayerCommand(Document *d,
                                                 View *view,
                                                 Layer *layer) :
    m_d(d),
    m_view(view),
    m_layer(layer),
    m_wasDormant(layer->isLayerDormant(view)),
    m_name(qApp->translate("RemoveLayerCommand", "Delete %1 Layer").arg(layer->objectName())),
    m_added(true)
{
}

Document::RemoveLayerCommand::~RemoveLayerCommand()
{
#ifdef DEBUG_DOCUMENT
    SVDEBUG << "Document::RemoveLayerCommand::~RemoveLayerCommand" << endl;
#endif
    if (!m_added) {
        m_d->deleteLayer(m_layer);
    }
}

QString
Document::RemoveLayerCommand::getName() const
{
#ifdef DEBUG_DOCUMENT
    SVDEBUG << "Document::RemoveLayerCommand::getName(): Name is "
              << m_name << endl;
#endif
    return m_name;
}

void
Document::RemoveLayerCommand::execute()
{
    bool have = false;
    for (int i = 0; i < m_view->getLayerCount(); ++i) {
        if (m_view->getLayer(i) == m_layer) {
            have = true;
            break;
        }
    }

    if (!have) { // not there!
        m_layer->setLayerDormant(m_view, true);
        m_added = false;
        return;
    }

    m_view->removeLayer(m_layer);
    m_layer->setLayerDormant(m_view, true);

    m_d->removeFromLayerViewMap(m_layer, m_view);
    m_added = false;
}

void
Document::RemoveLayerCommand::unexecute()
{
    m_view->addLayer(m_layer);
    m_layer->setLayerDormant(m_view, m_wasDormant);

    m_d->addToLayerViewMap(m_layer, m_view);
    m_added = true;
}

void
Document::toXml(QTextStream &out, QString indent, QString extraAttributes) const
{
    toXml(out, indent, extraAttributes, false);
}

void
Document::toXmlAsTemplate(QTextStream &out, QString indent, QString extraAttributes) const
{
    toXml(out, indent, extraAttributes, true);
}

void
Document::toXml(QTextStream &out, QString indent, QString extraAttributes,
                bool asTemplate) const
{
    out << indent + QString("<data%1%2>\n")
        .arg(extraAttributes == "" ? "" : " ").arg(extraAttributes);

    auto mainModel = ModelById::getAs<WaveFileModel>(m_mainModel);
    if (mainModel) {

#ifdef DEBUG_DOCUMENT
        SVDEBUG << "Document::toXml: writing main model" << endl;
#endif
        
        if (asTemplate) {
            writePlaceholderMainModel(out, indent + "  ");
        } else {
            mainModel->toXml(out, indent + "  ", "mainModel=\"true\"");
        }

        auto playParameters =
            PlayParameterRepository::getInstance()->getPlayParameters
            (m_mainModel.untyped);
        if (playParameters) {
            playParameters->toXml
                (out, indent + "  ",
                 QString("model=\"%1\"")
                 .arg(mainModel->getExportId()));
        }
    } else {
#ifdef DEBUG_DOCUMENT
        SVDEBUG << "Document::toXml: have no main model to write" << endl;
#endif
    }

    // Models that are not used in a layer that is in a view should
    // not be written.  Get our list of required models first.

    std::set<ModelId> used;

    for (LayerViewMap::const_iterator i = m_layerViewMap.begin();
         i != m_layerViewMap.end(); ++i) {

        if (i->first && !i->second.empty()) { // Layer exists, is in views
            ModelId modelId = i->first->getModel();
            ModelId sourceId = i->first->getSourceModel();
            if (!modelId.isNone()) used.insert(modelId);
            if (!sourceId.isNone()) used.insert(sourceId);
        }
    }

    // Write aggregate models first, so that when re-reading
    // derivations we already know about their existence. But only
    // those that are actually used
    //
    // Later note: This turns out not to be a great idea - we can't
    // use an aggregate model to drive a derivation unless its
    // component models have all also already been loaded. So we
    // really should have written non-aggregate read-only
    // (i.e. non-derived) wave-type models first, then aggregate
    // models, then models that have derivations. But we didn't do
    // that, so existing sessions will always have the aggregate
    // models first and we might as well stick with that.

    for (auto modelId: m_aggregateModels) {

#ifdef DEBUG_DOCUMENT
        SVDEBUG << "Document::toXml: checking aggregate model "
                << modelId << endl;
#endif

        auto aggregate = ModelById::getAs<AggregateWaveModel>(modelId);
        if (!aggregate) continue; 
        if (used.find(modelId) == used.end()) {
#ifdef DEBUG_DOCUMENT
            SVDEBUG << "(unused, skipping)" << endl;
#endif
            continue;
        }

#ifdef DEBUG_DOCUMENT
        SVDEBUG << "(used, writing)" << endl;
#endif

        aggregate->toXml(out, indent + "  ");
    }

    std::set<ModelId> written;

    // Now write the other models in two passes: first the models that
    // aren't derived from anything (in case they are source
    // components for an aggregate model, in which case we need to
    // have seen them before we see any models derived from aggregates
    // that use them - see the lament above) and then the models that
    // have derivations.

    const int nonDerivedPass = 0, derivedPass = 1;
    for (int pass = nonDerivedPass; pass <= derivedPass; ++pass) {
    
        for (auto rec: m_models) {

            ModelId modelId = rec.first;

            if (used.find(modelId) == used.end()) continue;
        
            auto model = ModelById::get(modelId);
            if (!model) continue;
            
#ifdef DEBUG_DOCUMENT
            SVDEBUG << "Document::toXml: looking at model " << modelId
                    << " [pass = " << pass << "]" << endl;
#endif
            
            // We need an intelligent way to determine which models
            // need to be streamed (i.e. have been edited, or are
            // small) and which should not be (i.e. remain as
            // generated by a transform, and are large).
            //
            // At the moment we can get away with deciding not to
            // stream dense 3d models or writable wave file models,
            // provided they were generated from a transform, because
            // at the moment there is no way to edit those model types
            // so it should be safe to regenerate them.  That won't
            // always work in future though.  It would be particularly
            // nice to be able to ask the user, as well as making an
            // intelligent guess.

            bool writeModel = true;
            bool haveDerivation = false;
        
            if (!rec.second.source.isNone() &&
                rec.second.transform.getIdentifier() != "") {
                haveDerivation = true;
            }

            if (pass == nonDerivedPass) {
                if (haveDerivation) {
                    SVDEBUG << "skipping derived model " << model->objectName() << " during nonDerivedPass" << endl;
                    continue;
                }
            } else {
                if (!haveDerivation) {
                    SVDEBUG << "skipping non-derived model " << model->objectName() << " during derivedPass" << endl;
                    continue;
                }
            }            

            if (haveDerivation) {
                if (ModelById::isa<WritableWaveFileModel>(modelId) ||
                    ModelById::isa<DenseThreeDimensionalModel>(modelId)) {
                    writeModel = false;
                }
            }
            
            if (writeModel) {
                model->toXml(out, indent + "  ");
                written.insert(modelId);
            }
            
            if (haveDerivation) {
                writeBackwardCompatibleDerivation(out, indent + "  ",
                                                  modelId, rec.second);
            }

            auto playParameters =
                PlayParameterRepository::getInstance()->getPlayParameters
                (modelId.untyped);
            if (playParameters) {
                playParameters->toXml
                    (out, indent + "  ",
                     QString("model=\"%1\"")
                     .arg(model->getExportId()));
            }
        }
    }

    // We should write out the alignment models here.  AlignmentModel
    // needs a toXml that writes out the export IDs of its reference
    // and aligned models, and then streams its path model.  Note that
    // this will only work when the alignment is complete, so we
    // should probably wait for it if it isn't already by this point.

    for (auto modelId: written) {

        auto model = ModelById::get(modelId);
        if (!model) continue;

        auto alignment = ModelById::get(model->getAlignment());
        if (!alignment) continue;

        alignment->toXml(out, indent + "  ");
    }

    for (auto i = m_layers.begin(); i != m_layers.end(); ++i) {
        (*i)->toXml(out, indent + "  ");
    }

    out << indent + "</data>\n";
}

void
Document::writePlaceholderMainModel(QTextStream &out, QString indent) const
{
    auto mainModel = ModelById::get(m_mainModel);
    if (!mainModel) return;
    out << indent;
    out << QString("<model id=\"%1\" name=\"placeholder\" sampleRate=\"%2\" type=\"wavefile\" file=\":samples/silent.wav\" mainModel=\"true\"/>\n")
        .arg(mainModel->getExportId())
        .arg(mainModel->getSampleRate());
}

void
Document::writeBackwardCompatibleDerivation(QTextStream &out, QString indent,
                                            ModelId targetModelId,
                                            const ModelRecord &rec) const
{
    // There is a lot of redundancy in the XML we output here, because
    // we want it to work with older SV session file reading code as
    // well.
    //
    // Formerly, a transform was described using a derivation element
    // which set out the source and target models, execution context
    // (step size, input channel etc) and transform id, containing a
    // plugin element which set out the transform parameters and so
    // on.  (The plugin element came from a "configurationXml" string
    // obtained from PluginXml.)
    // 
    // This has been replaced by a derivation element setting out the
    // source and target models and input channel, containing a
    // transform element which sets out everything in the Transform.
    //
    // In order to retain compatibility with older SV code, however,
    // we have to write out the same stuff into the derivation as
    // before, and manufacture an appropriate plugin element as well
    // as the transform element.  In order that newer code knows it's
    // dealing with a newer format, we will also write an attribute
    // 'type="transform"' in the derivation element.

    const Transform &transform = rec.transform;
    
    auto targetModel = ModelById::get(targetModelId);
    if (!targetModel) return;
    
    // Just for reference, this is what we would write if we didn't
    // have to be backward compatible:
    //
    //    out << indent
    //        << QString("<derivation type=\"transform\" source=\"%1\" "
    //                   "model=\"%2\" channel=\"%3\">\n")
    //        .arg(rec.source->getExportId())
    //        .arg(targetModel->getExportId())
    //        .arg(rec.channel);
    //
    //    transform.toXml(out, indent + "  ");
    //
    //    out << indent << "</derivation>\n";
    // 
    // Unfortunately, we can't just do that.  So we do this...

    QString extentsAttributes;
    if (transform.getStartTime() != RealTime::zeroTime ||
        transform.getDuration() != RealTime::zeroTime) {
        extentsAttributes = QString("startFrame=\"%1\" duration=\"%2\" ")
            .arg(RealTime::realTime2Frame(transform.getStartTime(),
                                          targetModel->getSampleRate()))
            .arg(RealTime::realTime2Frame(transform.getDuration(),
                                          targetModel->getSampleRate()));
    }
            
    out << indent;
    out << QString("<derivation type=\"transform\" source=\"%1\" "
                   "model=\"%2\" channel=\"%3\" domain=\"%4\" "
                   "stepSize=\"%5\" blockSize=\"%6\" %7windowType=\"%8\" "
                   "transform=\"%9\">\n")
        .arg(ModelById::getExportId(rec.source))
        .arg(targetModel->getExportId())
        .arg(rec.channel)
        .arg(TransformFactory::getInstance()->getTransformInputDomain
             (transform.getIdentifier()))
        .arg(transform.getStepSize())
        .arg(transform.getBlockSize())
        .arg(extentsAttributes)
        .arg(int(transform.getWindowType()))
        .arg(XmlExportable::encodeEntities(transform.getIdentifier()));

    transform.toXml(out, indent + "  ");
    
    out << indent << "  "
        << TransformFactory::getInstance()->getPluginConfigurationXml(transform);

    out << indent << "</derivation>\n";
}

