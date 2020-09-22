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

#include "ModelTransformerFactory.h"

#include "FeatureExtractionModelTransformer.h"
#include "RealTimeEffectModelTransformer.h"

#include "TransformFactory.h"

#include "base/AudioPlaySource.h"

#include "plugin/FeatureExtractionPluginFactory.h"
#include "plugin/RealTimePluginFactory.h"
#include "plugin/PluginXml.h"

#include "data/model/DenseTimeValueModel.h"

#include <vamp-hostsdk/PluginHostAdapter.h>

#include <iostream>
#include <set>
#include <map>

#include <QRegExp>
#include <QMutexLocker>

using std::vector;
using std::set;
using std::map;

ModelTransformerFactory *
ModelTransformerFactory::m_instance = new ModelTransformerFactory;

ModelTransformerFactory *
ModelTransformerFactory::getInstance()
{
    return m_instance;
}

ModelTransformerFactory::~ModelTransformerFactory()
{
}

ModelTransformer::Input
ModelTransformerFactory::getConfigurationForTransform(Transform &transform,
                                                      vector<ModelId> candidateInputModels,
                                                      ModelId defaultInputModel,
                                                      AudioPlaySource *source,
                                                      sv_frame_t startFrame,
                                                      sv_frame_t duration,
                                                      UserConfigurator *configurator)
{
    QMutexLocker locker(&m_mutex);
    
    ModelTransformer::Input input({});

    if (candidateInputModels.empty()) return input;

    //!!! This will need revision -- we'll have to have a callback
    //from the dialog for when the candidate input model is changed,
    //as we'll need to reinitialise the channel settings in the dialog
    ModelId inputModel = candidateInputModels[0];
    QStringList candidateModelNames;
    QString defaultModelName;
    QMap<QString, ModelId> modelMap;

    sv_samplerate_t defaultSampleRate;
    {   auto im = ModelById::get(inputModel);
        if (!im) return input;
        defaultSampleRate = im->getSampleRate();
    }
    
    for (int i = 0; in_range_for(candidateInputModels, i); ++i) {

        auto model = ModelById::get(candidateInputModels[i]);
        if (!model) return input;
        
        QString modelName = model->objectName();
        QString origModelName = modelName;
        int dupcount = 1;
        while (modelMap.contains(modelName)) {
            modelName = tr("%1 <%2>").arg(origModelName).arg(++dupcount);
        }
        
        modelMap[modelName] = candidateInputModels[i];
        candidateModelNames.push_back(modelName);
        
        if (candidateInputModels[i] == defaultInputModel) {
            defaultModelName = modelName;
        }
    }

    QString id = transform.getPluginIdentifier();
    
    bool ok = true;
    QString configurationXml = m_lastConfigurations[transform.getIdentifier()];

    SVDEBUG << "ModelTransformer: last configuration for identifier " << transform.getIdentifier() << ": " << configurationXml << endl;

    Vamp::PluginBase *plugin = nullptr;

    if (RealTimePluginFactory::instanceFor(id)) {

        SVDEBUG << "ModelTransformerFactory::getConfigurationForTransform: instantiating real-time plugin" << endl;
        
        RealTimePluginFactory *factory = RealTimePluginFactory::instanceFor(id);

        sv_samplerate_t sampleRate = defaultSampleRate;
        int blockSize = 1024;
        int channels = 1;
        if (source) {
            sampleRate = source->getSourceSampleRate();
            blockSize = source->getTargetBlockSize();
            channels = source->getTargetChannelCount();
        }

        RealTimePluginInstance *rtp = factory->instantiatePlugin
            (id, 0, 0, sampleRate, blockSize, channels);

        plugin = rtp;

    } else {

        SVDEBUG << "ModelTransformerFactory::getConfigurationForTransform: instantiating Vamp plugin" << endl;

        Vamp::Plugin *vp =
            FeatureExtractionPluginFactory::instance()->instantiatePlugin
            (id, float(defaultSampleRate));

        plugin = vp;
    }

    if (plugin) {

        // Ensure block size etc are valid
        TransformFactory::getInstance()->
            makeContextConsistentWithPlugin(transform, plugin);

        // Prepare the plugin with any existing parameters already
        // found in the transform
        TransformFactory::getInstance()->
            setPluginParameters(transform, plugin);
        
        // For this interactive usage, we want to override those with
        // whatever the user chose last time around
        PluginXml(plugin).setParametersFromXml(configurationXml);

        if (configurator) {
            ok = configurator->configure(input, transform, plugin,
                                         inputModel, source,
                                         startFrame, duration,
                                         modelMap,
                                         candidateModelNames,
                                         defaultModelName);
        }
        

        TransformFactory::getInstance()->
            makeContextConsistentWithPlugin(transform, plugin);

        configurationXml = PluginXml(plugin).toXmlString();

        SVDEBUG << "ModelTransformerFactory::getConfigurationForTransform: got configuration, deleting plugin" << endl;
        
        delete plugin;
    }

    if (ok) {
        m_lastConfigurations[transform.getIdentifier()] = configurationXml;
        input.setModel(inputModel);
    }

    return input;
}

ModelTransformer *
ModelTransformerFactory::createTransformer(const Transforms &transforms,
                                           const ModelTransformer::Input &input)
{
    ModelTransformer *transformer = nullptr;

    QString id = transforms[0].getPluginIdentifier();

    if (RealTimePluginFactory::instanceFor(id)) {

        transformer =
            new RealTimeEffectModelTransformer(input, transforms[0]);

    } else {

        transformer =
            new FeatureExtractionModelTransformer(input, transforms);
    }

    if (transformer) transformer->setObjectName(transforms[0].getIdentifier());
    return transformer;
}

ModelId
ModelTransformerFactory::transform(const Transform &transform,
                                   const ModelTransformer::Input &input,
                                   QString &message,
                                   AdditionalModelHandler *handler) 
{
    SVDEBUG << "ModelTransformerFactory::transform: Constructing transformer with input model " << input.getModel() << endl;

    Transforms transforms;
    transforms.push_back(transform);
    vector<ModelId> mm = transformMultiple(transforms, input, message, handler);
    if (mm.empty()) return {};
    else return mm[0];
}

vector<ModelId>
ModelTransformerFactory::transformMultiple(const Transforms &transforms,
                                           const ModelTransformer::Input &input,
                                           QString &message,
                                           AdditionalModelHandler *handler) 
{
    SVDEBUG << "ModelTransformerFactory::transformMultiple: Constructing transformer with input model " << input.getModel() << endl;
    
    QMutexLocker locker(&m_mutex);

    auto inputModel = ModelById::get(input.getModel());
    if (!inputModel) return {};
    
    ModelTransformer *t = createTransformer(transforms, input);
    if (!t) return {};

    if (handler) {
        m_handlers[t] = handler;
    }

    m_runningTransformers.insert(t);

    connect(t, SIGNAL(finished()), this, SLOT(transformerFinished()));

    t->start();
    vector<ModelId> models = t->getOutputModels();
    
    if (!models.empty()) {
        QString imn = inputModel->objectName();
        QString trn =
            TransformFactory::getInstance()->getTransformFriendlyName
            (transforms[0].getIdentifier());
        for (int i = 0; in_range_for(models, i); ++i) {
            auto model = ModelById::get(models[i]);
            if (!model) continue;
            if (imn != "") {
                if (trn != "") {
                    model->setObjectName(tr("%1: %2").arg(imn).arg(trn));
                } else {
                    model->setObjectName(imn);
                }
            } else if (trn != "") {
                model->setObjectName(trn);
            }
        }
    } else {
        t->wait();
    }

    message = t->getMessage();

    return models;
}

void
ModelTransformerFactory::transformerFinished()
{
    QObject *s = sender();
    ModelTransformer *transformer = dynamic_cast<ModelTransformer *>(s);
    
//    SVDEBUG << "ModelTransformerFactory::transformerFinished(" << transformer << ")" << endl;

    if (!transformer) {
        cerr << "WARNING: ModelTransformerFactory::transformerFinished: sender is not a transformer" << endl;
        return;
    }

    m_mutex.lock();
    
    if (m_runningTransformers.find(transformer) == m_runningTransformers.end()) {
        cerr << "WARNING: ModelTransformerFactory::transformerFinished(" 
                  << transformer
                  << "): I have no record of this transformer running!"
                  << endl;
    }

    m_runningTransformers.erase(transformer);

    map<AdditionalModelHandler *, vector<ModelId>> toNotifyOfMore;
    vector<AdditionalModelHandler *> toNotifyOfNoMore;
    
    if (m_handlers.find(transformer) != m_handlers.end()) {
        if (transformer->willHaveAdditionalOutputModels()) {
            vector<ModelId> mm = transformer->getAdditionalOutputModels();
            toNotifyOfMore[m_handlers[transformer]] = mm;
        } else {
            toNotifyOfNoMore.push_back(m_handlers[transformer]);
        }
        m_handlers.erase(transformer);
    }

    m_mutex.unlock();

    // We make these calls without the mutex held, in case they
    // ultimately call back on us - not such a concern as in the old
    // model lifecycle but just in case
    
    for (const auto &i: toNotifyOfMore) {
        i.first->moreModelsAvailable(i.second);
    }
    for (AdditionalModelHandler *handler: toNotifyOfNoMore) {
        handler->noMoreModelsAvailable();
    }
    
    if (transformer->isAbandoned()) {
        if (transformer->getMessage() != "") {
            emit transformFailed("", transformer->getMessage());
        }
    }
    
    transformer->wait(); // unnecessary but reassuring
    delete transformer;
}

bool
ModelTransformerFactory::haveRunningTransformers() const
{
    QMutexLocker locker(&m_mutex);
    
    return (!m_runningTransformers.empty());
}
