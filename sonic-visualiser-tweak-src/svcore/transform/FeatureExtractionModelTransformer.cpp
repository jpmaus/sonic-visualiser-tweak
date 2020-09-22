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

#include "FeatureExtractionModelTransformer.h"

#include "plugin/FeatureExtractionPluginFactory.h"

#include "plugin/PluginXml.h"
#include <vamp-hostsdk/Plugin.h>

#include "data/model/Model.h"
#include "base/Window.h"
#include "base/Exceptions.h"
#include "data/model/SparseOneDimensionalModel.h"
#include "data/model/SparseTimeValueModel.h"
#include "data/model/BasicCompressedDenseThreeDimensionalModel.h"
#include "data/model/DenseTimeValueModel.h"
#include "data/model/NoteModel.h"
#include "data/model/RegionModel.h"
#include "data/model/FFTModel.h"
#include "data/model/WaveFileModel.h"
#include "rdf/PluginRDFDescription.h"

#include "TransformFactory.h"

#include <iostream>

#include <QSettings>

//#define DEBUG_FEATURE_EXTRACTION_TRANSFORMER_RUN 1

FeatureExtractionModelTransformer::FeatureExtractionModelTransformer(Input in,
                                                                     const Transform &transform) :
    ModelTransformer(in, transform),
    m_plugin(nullptr),
    m_haveOutputs(false)
{
    SVDEBUG << "FeatureExtractionModelTransformer::FeatureExtractionModelTransformer: plugin " << m_transforms.begin()->getPluginIdentifier() << ", outputName " << m_transforms.begin()->getOutput() << endl;
}

FeatureExtractionModelTransformer::FeatureExtractionModelTransformer(Input in,
                                                                     const Transforms &transforms) :
    ModelTransformer(in, transforms),
    m_plugin(nullptr),
    m_haveOutputs(false)
{
    if (m_transforms.empty()) {
        SVDEBUG << "FeatureExtractionModelTransformer::FeatureExtractionModelTransformer: " << transforms.size() << " transform(s)" << endl;
    } else {
        SVDEBUG << "FeatureExtractionModelTransformer::FeatureExtractionModelTransformer: " << transforms.size() << " transform(s), first has plugin " << m_transforms.begin()->getPluginIdentifier() << ", outputName " << m_transforms.begin()->getOutput() << endl;
    }
}

static bool
areTransformsSimilar(const Transform &t1, const Transform &t2)
{
    Transform t2o(t2);
    t2o.setOutput(t1.getOutput());
    return t1 == t2o;
}

bool
FeatureExtractionModelTransformer::initialise()
{
    // This is (now) called from the run thread. The plugin is
    // constructed, initialised, used, and destroyed all from a single
    // thread.
    
    // All transforms must use the same plugin, parameters, and
    // inputs: they can differ only in choice of plugin output. So we
    // initialise based purely on the first transform in the list (but
    // first check that they are actually similar as promised)

    for (int j = 1; in_range_for(m_transforms, j); ++j) {
        if (!areTransformsSimilar(m_transforms[0], m_transforms[j])) {
            m_message = tr("Transforms supplied to a single FeatureExtractionModelTransformer instance must be similar in every respect except plugin output");
            SVCERR << m_message << endl;
            return false;
        }
    }

    Transform primaryTransform = m_transforms[0];

    QString pluginId = primaryTransform.getPluginIdentifier();

    FeatureExtractionPluginFactory *factory =
        FeatureExtractionPluginFactory::instance();

    if (!factory) {
        m_message = tr("No factory available for feature extraction plugin id \"%1\" (unknown plugin type, or internal error?)").arg(pluginId);
        SVCERR << m_message << endl;
        return false;
    }

    auto input = ModelById::getAs<DenseTimeValueModel>(getInputModel());
    if (!input) {
        m_message = tr("Input model for feature extraction plugin \"%1\" is of wrong type (internal error?)").arg(pluginId);
        SVCERR << m_message << endl;
        return false;
    }

    SVDEBUG << "FeatureExtractionModelTransformer: Instantiating plugin for transform in thread "
            << QThread::currentThreadId() << endl;
    
    m_plugin = factory->instantiatePlugin(pluginId, input->getSampleRate());
    if (!m_plugin) {
        m_message = tr("Failed to instantiate plugin \"%1\"").arg(pluginId);
        SVCERR << m_message << endl;
        return false;
    }

    TransformFactory::getInstance()->makeContextConsistentWithPlugin
        (primaryTransform, m_plugin);
    
    TransformFactory::getInstance()->setPluginParameters
        (primaryTransform, m_plugin);
    
    int channelCount = input->getChannelCount();
    if ((int)m_plugin->getMaxChannelCount() < channelCount) {
        channelCount = 1;
    }
    if ((int)m_plugin->getMinChannelCount() > channelCount) {
        m_message = tr("Cannot provide enough channels to feature extraction plugin \"%1\" (plugin min is %2, max %3; input model has %4)")
            .arg(pluginId)
            .arg(m_plugin->getMinChannelCount())
            .arg(m_plugin->getMaxChannelCount())
            .arg(input->getChannelCount());
        SVCERR << m_message << endl;
        return false;
    }

    int step = primaryTransform.getStepSize();
    int block = primaryTransform.getBlockSize();
    
    SVDEBUG << "Initialising feature extraction plugin with channels = "
            << channelCount << ", step = " << step
            << ", block = " << block << endl;

    if (!m_plugin->initialise(channelCount, step, block)) {

        int preferredStep = int(m_plugin->getPreferredStepSize());
        int preferredBlock = int(m_plugin->getPreferredBlockSize());
        
        if (step != preferredStep || block != preferredBlock) {

            SVDEBUG << "Initialisation failed, trying again with preferred step = "
                    << preferredStep << ", block = " << preferredBlock << endl;
            
            if (!m_plugin->initialise(channelCount,
                                      preferredStep,
                                      preferredBlock)) {

                SVDEBUG << "Initialisation failed again" << endl;
                
                m_message = tr("Failed to initialise feature extraction plugin \"%1\"").arg(pluginId);
                SVCERR << m_message << endl;
                return false;

            } else {
                
                SVDEBUG << "Initialisation succeeded this time" << endl;

                // Set these values into the primary transform in the list
                m_transforms[0].setStepSize(preferredStep);
                m_transforms[0].setBlockSize(preferredBlock);
                
                m_message = tr("Feature extraction plugin \"%1\" rejected the given step and block sizes (%2 and %3); using plugin defaults (%4 and %5) instead")
                    .arg(pluginId)
                    .arg(step)
                    .arg(block)
                    .arg(preferredStep)
                    .arg(preferredBlock);
                SVCERR << m_message << endl;
            }

        } else {

            SVDEBUG << "Initialisation failed (with step = " << step
                    << " and block = " << block
                    << ", both matching the plugin's preference)" << endl;
                
            m_message = tr("Failed to initialise feature extraction plugin \"%1\"").arg(pluginId);
            SVCERR << m_message << endl;
            return false;
        }
    } else {
        SVDEBUG << "Initialisation succeeded" << endl;
    }

    if (primaryTransform.getPluginVersion() != "") {
        QString pv = QString("%1").arg(m_plugin->getPluginVersion());
        if (pv != primaryTransform.getPluginVersion()) {
            QString vm = tr("Transform was configured for version %1 of plugin \"%2\", but the plugin being used is version %3")
                .arg(primaryTransform.getPluginVersion())
                .arg(pluginId)
                .arg(pv);
            if (m_message != "") {
                m_message = QString("%1; %2").arg(vm).arg(m_message);
            } else {
                m_message = vm;
            }
            SVCERR << m_message << endl;
        }
    }

    Vamp::Plugin::OutputList outputs = m_plugin->getOutputDescriptors();

    if (outputs.empty()) {
        m_message = tr("Plugin \"%1\" has no outputs").arg(pluginId);
        SVCERR << m_message << endl;
        return false;
    }

    for (int j = 0; in_range_for(m_transforms, j); ++j) {

        for (int i = 0; in_range_for(outputs, i); ++i) {

            if (m_transforms[j].getOutput() == "" ||
                outputs[i].identifier ==
                m_transforms[j].getOutput().toStdString()) {
                
                m_outputNos.push_back(i);
                m_descriptors.push_back(outputs[i]);
                m_fixedRateFeatureNos.push_back(-1); // we increment before use
                break;
            }
        }

        if (!in_range_for(m_descriptors, j)) {
            m_message = tr("Plugin \"%1\" has no output named \"%2\"")
                .arg(pluginId)
                .arg(m_transforms[j].getOutput());
            SVCERR << m_message << endl;
            return false;
        }
    }

    for (int j = 0; in_range_for(m_transforms, j); ++j) {
        createOutputModels(j);
    }

    m_outputMutex.lock();
    m_haveOutputs = true;
    m_outputsCondition.wakeAll();
    m_outputMutex.unlock();

    return true;
}

void
FeatureExtractionModelTransformer::deinitialise()
{
    SVDEBUG << "FeatureExtractionModelTransformer: deleting plugin for transform in thread "
            << QThread::currentThreadId() << endl;

    try {
        delete m_plugin;
    } catch (const std::exception &e) {
        // A destructor shouldn't throw an exception. But at one point
        // (now fixed) our plugin stub destructor could have
        // accidentally done so, so just in case:
        SVCERR << "FeatureExtractionModelTransformer: caught exception while deleting plugin: " << e.what() << endl;
        m_message = e.what();
    }
    m_plugin = nullptr;

    m_descriptors.clear();
}

void
FeatureExtractionModelTransformer::createOutputModels(int n)
{
    auto input = ModelById::getAs<DenseTimeValueModel>(getInputModel());
    if (!input) return;
    
    PluginRDFDescription description(m_transforms[n].getPluginIdentifier());
    QString outputId = m_transforms[n].getOutput();

    int binCount = 1;
    float minValue = 0.0, maxValue = 0.0;
    bool haveExtents = false;
    bool haveBinCount = m_descriptors[n].hasFixedBinCount;

    if (haveBinCount) {
        binCount = (int)m_descriptors[n].binCount;
    }

    m_needAdditionalModels[n] = false;

    if (binCount > 0 && m_descriptors[n].hasKnownExtents) {
        minValue = m_descriptors[n].minValue;
        maxValue = m_descriptors[n].maxValue;
        haveExtents = true;
    }

    sv_samplerate_t modelRate = input->getSampleRate();
    sv_samplerate_t outputRate = modelRate;
    int modelResolution = 1;

    if (m_descriptors[n].sampleType != 
        Vamp::Plugin::OutputDescriptor::OneSamplePerStep) {

        outputRate = m_descriptors[n].sampleRate;

        //!!! SV doesn't actually support display of models that have
        //!!! different underlying rates together -- so we always set
        //!!! the model rate to be the input model's rate, and adjust
        //!!! the resolution appropriately.  We can't properly display
        //!!! data with a higher resolution than the base model at all
        if (outputRate > input->getSampleRate()) {
            SVDEBUG << "WARNING: plugin reports output sample rate as "
                    << outputRate
                    << " (can't display features with finer resolution than the input rate of "
                    << modelRate << ")" << endl;
            outputRate = modelRate;
        }
    }

    switch (m_descriptors[n].sampleType) {

    case Vamp::Plugin::OutputDescriptor::VariableSampleRate:
        if (outputRate != 0.0) {
            modelResolution = int(round(modelRate / outputRate));
        }
        break;

    case Vamp::Plugin::OutputDescriptor::OneSamplePerStep:
        modelResolution = m_transforms[n].getStepSize();
        break;

    case Vamp::Plugin::OutputDescriptor::FixedSampleRate:
        if (outputRate <= 0.0) {
            SVDEBUG << "WARNING: Fixed sample-rate plugin reports invalid sample rate " << m_descriptors[n].sampleRate << "; defaulting to input rate of " << input->getSampleRate() << endl;
            modelResolution = 1;
        } else {
            modelResolution = int(round(modelRate / outputRate));
//            cerr << "modelRate = " << modelRate << ", descriptor rate = " << outputRate << ", modelResolution = " << modelResolution << endl;
        }
        break;
    }

    bool preDurationPlugin = (m_plugin->getVampApiVersion() < 2);

    std::shared_ptr<Model> out;

    if (binCount == 0 &&
        (preDurationPlugin || !m_descriptors[n].hasDuration)) {

        // Anything with no value and no duration is an instant

        out = std::make_shared<SparseOneDimensionalModel>
            (modelRate, modelResolution, false);

        QString outputEventTypeURI = description.getOutputEventTypeURI(outputId);
        out->setRDFTypeURI(outputEventTypeURI);

    } else if ((preDurationPlugin && binCount > 1 &&
                (m_descriptors[n].sampleType ==
                 Vamp::Plugin::OutputDescriptor::VariableSampleRate)) ||
               (!preDurationPlugin && m_descriptors[n].hasDuration)) {

        // For plugins using the old v1 API without explicit duration,
        // we treat anything that has multiple bins (i.e. that has the
        // potential to have value and duration) and a variable sample
        // rate as a note model, taking its values as pitch, duration
        // and velocity (if present) respectively.  This is the same
        // behaviour as always applied by SV to these plugins in the
        // past.

        // For plugins with the newer API, we treat anything with
        // duration as either a note model with pitch and velocity, or
        // a region model.

        // How do we know whether it's an interval or note model?
        // What's the essential difference?  Is a note model any
        // interval model using a Hz or "MIDI pitch" scale?  There
        // isn't really a reliable test for "MIDI pitch"...  Does a
        // note model always have velocity?  This is a good question
        // to be addressed by accompanying RDF, but for the moment we
        // will do the following...

        bool isNoteModel = false;
        
        // Regions have only value (and duration -- we can't extract a
        // region model from an old-style plugin that doesn't support
        // duration)
        if (binCount > 1) isNoteModel = true;

        // Regions do not have units of Hz or MIDI things (a sweeping
        // assumption!)
        if (m_descriptors[n].unit == "Hz" ||
            m_descriptors[n].unit.find("MIDI") != std::string::npos ||
            m_descriptors[n].unit.find("midi") != std::string::npos) {
            isNoteModel = true;
        }

        // If we had a "sparse 3D model", we would have the additional
        // problem of determining whether to use that here (if bin
        // count > 1).  But we don't.

        QSettings settings;

        if (isNoteModel) {

            QSettings settings;
            settings.beginGroup("Transformer");
            bool flexi = settings.value("use-flexi-note-model", false).toBool();
            settings.endGroup();

            SVCERR << "flexi = " << flexi << endl;
            
            NoteModel *model;
            if (haveExtents) {
                model = new NoteModel
                    (modelRate, modelResolution, minValue, maxValue, false,
                     flexi ? NoteModel::FLEXI_NOTE : NoteModel::NORMAL_NOTE);
            } else {
                model = new NoteModel
                    (modelRate, modelResolution, false,
                     flexi ? NoteModel::FLEXI_NOTE : NoteModel::NORMAL_NOTE);
            }
            model->setScaleUnits(m_descriptors[n].unit.c_str());
            out.reset(model);

        } else {

            RegionModel *model;
            if (haveExtents) {
                model = new RegionModel
                    (modelRate, modelResolution, minValue, maxValue, false);
            } else {
                model = new RegionModel
                    (modelRate, modelResolution, false);
            }
            model->setScaleUnits(m_descriptors[n].unit.c_str());
            out.reset(model);
        }

        QString outputEventTypeURI = description.getOutputEventTypeURI(outputId);
        out->setRDFTypeURI(outputEventTypeURI);

    } else if (binCount == 1 ||
               (m_descriptors[n].sampleType == 
                Vamp::Plugin::OutputDescriptor::VariableSampleRate)) {

        // Anything that is not a 1D, note, or interval model and that
        // has only one value per result must be a sparse time value
        // model.

        // Anything that is not a 1D, note, or interval model and that
        // has a variable sample rate is treated as a set of sparse
        // time value models, one per output bin, because we lack a
        // sparse 3D model.

        // Anything that is not a 1D, note, or interval model and that
        // has a fixed sample rate but an unknown number of values per
        // result is also treated as a set of sparse time value models.

        // For sets of sparse time value models, we create a single
        // model first as the "standard" output and then create models
        // for bins 1+ in the additional model map (mapping the output
        // descriptor to a list of models indexed by bin-1). But we
        // don't create the additional models yet, as this case has to
        // work even if the number of bins is unknown at this point --
        // we create an additional model (copying its parameters from
        // the default one) each time a new bin is encountered.

        if (!haveBinCount || binCount > 1) {
            m_needAdditionalModels[n] = true;
        }

        SparseTimeValueModel *model;
        if (haveExtents) {
            model = new SparseTimeValueModel
                (modelRate, modelResolution, minValue, maxValue, false);
        } else {
            model = new SparseTimeValueModel
                (modelRate, modelResolution, false);
        }

        Vamp::Plugin::OutputList outputs = m_plugin->getOutputDescriptors();
        model->setScaleUnits(outputs[m_outputNos[n]].unit.c_str());

        out.reset(model);

        QString outputEventTypeURI = description.getOutputEventTypeURI(outputId);
        out->setRDFTypeURI(outputEventTypeURI);

    } else {

        // Anything that is not a 1D, note, or interval model and that
        // has a fixed sample rate and more than one value per result
        // must be a dense 3D model.

        auto model =
            new BasicCompressedDenseThreeDimensionalModel
            (modelRate, modelResolution, binCount, false);

        if (!m_descriptors[n].binNames.empty()) {
            std::vector<QString> names;
            for (int i = 0; i < (int)m_descriptors[n].binNames.size(); ++i) {
                names.push_back(m_descriptors[n].binNames[i].c_str());
            }
            model->setBinNames(names);
        }
        
        out.reset(model);

        QString outputSignalTypeURI = description.getOutputSignalTypeURI(outputId);
        out->setRDFTypeURI(outputSignalTypeURI);
    }

    if (out) {
        out->setSourceModel(getInputModel());
        m_outputs.push_back(ModelById::add(out));
    }
}

void
FeatureExtractionModelTransformer::awaitOutputModels()
{
    m_outputMutex.lock();
    while (!m_haveOutputs && !m_abandoned) {
        m_outputsCondition.wait(&m_outputMutex, 500);
    }
    m_outputMutex.unlock();
}

FeatureExtractionModelTransformer::~FeatureExtractionModelTransformer()
{
    // Parent class dtor set the abandoned flag and waited for the run
    // thread to exit; the run thread owns the plugin, and should have
    // destroyed it before exiting (via a call to deinitialise)
}

FeatureExtractionModelTransformer::Models
FeatureExtractionModelTransformer::getAdditionalOutputModels()
{
    Models mm;
    for (auto mp : m_additionalModels) {
        for (auto m: mp.second) {
            mm.push_back(m.second);
        }
    }
    return mm;
}

bool
FeatureExtractionModelTransformer::willHaveAdditionalOutputModels()
{
    for (auto p : m_needAdditionalModels) {
        if (p.second) return true;
    }
    return false;
}

ModelId
FeatureExtractionModelTransformer::getAdditionalModel(int n, int binNo)
{
    if (binNo == 0) {
        SVCERR << "Internal error: binNo == 0 in getAdditionalModel (should be using primary model, not calling getAdditionalModel)" << endl;
        return {};
    }

    if (!in_range_for(m_outputs, n)) {
        SVCERR << "getAdditionalModel: Output " << n << " out of range" << endl;
        return {};
    }

    if (!in_range_for(m_needAdditionalModels, n) ||
        !m_needAdditionalModels[n]) {
        return {};
    }
    
    if (!m_additionalModels[n][binNo].isNone()) {
        return m_additionalModels[n][binNo];
    }

    SVDEBUG << "getAdditionalModel(" << n << ", " << binNo
            << "): creating" << endl;

    auto baseModel = ModelById::getAs<SparseTimeValueModel>(m_outputs[n]);
    if (!baseModel) {
        SVCERR << "getAdditionalModel: Output model not conformable, or has vanished" << endl;
        return {};
    }
    
    SVDEBUG << "getAdditionalModel(" << n << ", " << binNo
            << "): (from " << baseModel << ")" << endl;

    SparseTimeValueModel *additional =
        new SparseTimeValueModel(baseModel->getSampleRate(),
                                 baseModel->getResolution(),
                                 baseModel->getValueMinimum(),
                                 baseModel->getValueMaximum(),
                                 false);

    additional->setScaleUnits(baseModel->getScaleUnits());
    additional->setRDFTypeURI(baseModel->getRDFTypeURI());

    ModelId additionalId = ModelById::add
        (std::shared_ptr<SparseTimeValueModel>(additional));
    m_additionalModels[n][binNo] = additionalId;
    return additionalId;
}

void
FeatureExtractionModelTransformer::run()
{
    try {
        if (!initialise()) {
            abandon();
            return;
        }
    } catch (const std::exception &e) {
        abandon();
        m_message = e.what();
        return;
    }

    if (m_outputs.empty()) {
        abandon();
        return;
    }

    Transform primaryTransform = m_transforms[0];

    ModelId inputId = getInputModel();

    bool ready = false;
    while (!ready && !m_abandoned) {
        { // scope so as to release input shared_ptr before sleeping
            auto input = ModelById::getAs<DenseTimeValueModel>(inputId);
            if (!input || !input->isOK()) {
                abandon();
                return;
            }
            ready = input->isReady();
        }
        if (!ready) {
            SVDEBUG << "FeatureExtractionModelTransformer::run: Waiting for input model "
                    << inputId << " to be ready..." << endl;
            usleep(500000);
        }
    }
    if (m_abandoned) return;

#ifdef DEBUG_FEATURE_EXTRACTION_TRANSFORMER_RUN
    SVDEBUG << "FeatureExtractionModelTransformer::run: Input model "
            << inputId << " is ready, going ahead" << endl;
#endif

    sv_samplerate_t sampleRate;
    int channelCount;
    sv_frame_t startFrame;
    sv_frame_t endFrame;
    
    { // scope so as not to have this borrowed pointer retained around
      // the edges of the process loop
        auto input = ModelById::getAs<DenseTimeValueModel>(inputId);
        if (!input) {
            abandon();
            return;
        }

        sampleRate = input->getSampleRate();

        channelCount = input->getChannelCount();
        if ((int)m_plugin->getMaxChannelCount() < channelCount) {
            channelCount = 1;
        }

        startFrame = input->getStartFrame();
        endFrame = input->getEndFrame();
    }

    float **buffers = new float*[channelCount];
    for (int ch = 0; ch < channelCount; ++ch) {
        buffers[ch] = new float[primaryTransform.getBlockSize() + 2];
    }

    int stepSize = primaryTransform.getStepSize();
    int blockSize = primaryTransform.getBlockSize();

    bool frequencyDomain = (m_plugin->getInputDomain() ==
                            Vamp::Plugin::FrequencyDomain);

    std::vector<FFTModel *> fftModels;

    if (frequencyDomain) {
#ifdef DEBUG_FEATURE_EXTRACTION_TRANSFORMER_RUN
        SVDEBUG << "FeatureExtractionModelTransformer::run: Input is frequency-domain" << endl;
#endif
        for (int ch = 0; ch < channelCount; ++ch) {
            FFTModel *model = new FFTModel
                (inputId,
                 channelCount == 1 ? m_input.getChannel() : ch,
                 primaryTransform.getWindowType(),
                 blockSize,
                 stepSize,
                 blockSize);
            if (!model->isOK() || model->getError() != "") {
                QString err = model->getError();
                delete model;
                for (int j = 0; in_range_for(m_outputNos, j); ++j) {
                    setCompletion(j, 100);
                }
                SVDEBUG << "FeatureExtractionModelTransformer::run: Failed to create FFT model for input model " << inputId << ": " << err << endl;
                m_message = "Failed to create the FFT model for this feature extraction model transformer: error is: " + err;
                for (int cch = 0; cch < ch; ++cch) {
                    delete fftModels[cch];
                }
                abandon();
                return;
            }
            fftModels.push_back(model);
        }
#ifdef DEBUG_FEATURE_EXTRACTION_TRANSFORMER_RUN
        SVDEBUG << "FeatureExtractionModelTransformer::run: Created FFT model(s) for frequency-domain input" << endl;
#endif
    }

    RealTime contextStartRT = primaryTransform.getStartTime();
    RealTime contextDurationRT = primaryTransform.getDuration();

    sv_frame_t contextStart =
        RealTime::realTime2Frame(contextStartRT, sampleRate);

    sv_frame_t contextDuration =
        RealTime::realTime2Frame(contextDurationRT, sampleRate);

    if (contextStart == 0 || contextStart < startFrame) {
        contextStart = startFrame;
    }

    if (contextDuration == 0) {
        contextDuration = endFrame - contextStart;
    }
    if (contextStart + contextDuration > endFrame) {
        contextDuration = endFrame - contextStart;
    }

    sv_frame_t blockFrame = contextStart;

    long prevCompletion = 0;

    for (int j = 0; in_range_for(m_outputNos, j); ++j) {
        setCompletion(j, 0);
    }

    float *reals = nullptr;
    float *imaginaries = nullptr;
    if (frequencyDomain) {
        reals = new float[blockSize/2 + 1];
        imaginaries = new float[blockSize/2 + 1];
    }

    QString error = "";

    try {
        while (!m_abandoned) {

            if (frequencyDomain) {
                if (blockFrame - int(blockSize)/2 >
                    contextStart + contextDuration) {
                    break;
                }
            } else {
                if (blockFrame >= 
                    contextStart + contextDuration) {
                    break;
                }
            }

#ifdef DEBUG_FEATURE_EXTRACTION_TRANSFORMER_RUN
            SVDEBUG << "FeatureExtractionModelTransformer::run: blockFrame "
                    << blockFrame << ", endFrame " << endFrame << ", blockSize "
                    << blockSize << endl;
#endif
        
            int completion = int
                ((((blockFrame - contextStart) / stepSize) * 99) /
                 (contextDuration / stepSize + 1));

            bool haveAllModels = true;
            if (!ModelById::get(inputId)) {
#ifdef DEBUG_FEATURE_EXTRACTION_TRANSFORMER_RUN
                SVDEBUG << "FeatureExtractionModelTransformer::run: Input model " << inputId << " no longer exists" << endl;
#endif
                haveAllModels = false;
            } else {
#ifdef DEBUG_FEATURE_EXTRACTION_TRANSFORMER_RUN
                SVDEBUG << "Input model " << inputId << " still exists" << endl;
#endif
            }
            for (auto mid: m_outputs) {
                if (!ModelById::get(mid)) {
#ifdef DEBUG_FEATURE_EXTRACTION_TRANSFORMER_RUN
                    SVDEBUG << "FeatureExtractionModelTransformer::run: Output model " << mid << " no longer exists" << endl;
#endif
                    haveAllModels = false;
                } else {
#ifdef DEBUG_FEATURE_EXTRACTION_TRANSFORMER_RUN
                    SVDEBUG << "Output model " << mid << " still exists" << endl;
#endif
                }
            }
            if (!haveAllModels) {
                abandon();
                break;
            }
            
#ifdef DEBUG_FEATURE_EXTRACTION_TRANSFORMER_RUN
            SVDEBUG << "FeatureExtractionModelTransformer::run: All models still exist" << endl;
#endif

            // channelCount is either input->channelCount or 1

            if (frequencyDomain) {
                for (int ch = 0; ch < channelCount; ++ch) {
                    int column = int((blockFrame - startFrame) / stepSize);
                    if (fftModels[ch]->getValuesAt(column, reals, imaginaries)) {
                        for (int i = 0; i <= blockSize/2; ++i) {
                            buffers[ch][i*2] = reals[i];
                            buffers[ch][i*2+1] = imaginaries[i];
                        }
                    } else {
                        for (int i = 0; i <= blockSize/2; ++i) {
                            buffers[ch][i*2] = 0.f;
                            buffers[ch][i*2+1] = 0.f;
                        }
                    }
                        
                    error = fftModels[ch]->getError();
                    if (error != "") {
                        SVCERR << "FeatureExtractionModelTransformer::run: Abandoning, error is " << error << endl;
                        m_abandoned = true;
                        m_message = error;
                        break;
                    }
                }
            } else {
                getFrames(channelCount, blockFrame, blockSize, buffers);
            }

            if (m_abandoned) break;

            auto features = m_plugin->process
                (buffers,
                 RealTime::frame2RealTime(blockFrame, sampleRate)
                 .toVampRealTime());
            
            if (m_abandoned) break;

            for (int j = 0; in_range_for(m_outputNos, j); ++j) {
                for (int fi = 0; in_range_for(features[m_outputNos[j]], fi); ++fi) {
                    auto feature = features[m_outputNos[j]][fi];
                    addFeature(j, blockFrame, feature);
                }
            }

            if (blockFrame == contextStart || completion > prevCompletion) {
                for (int j = 0; in_range_for(m_outputNos, j); ++j) {
                    setCompletion(j, completion);
                }
                prevCompletion = completion;
            }

            blockFrame += stepSize;

        }

        if (!m_abandoned) {
            auto features = m_plugin->getRemainingFeatures();

            for (int j = 0; in_range_for(m_outputNos, j); ++j) {
                for (int fi = 0; in_range_for(features[m_outputNos[j]], fi); ++fi) {
                    auto feature = features[m_outputNos[j]][fi];
                    addFeature(j, blockFrame, feature);
                    if (m_abandoned) {
                        break;
                    }
                }
            }
        }
    } catch (const std::exception &e) {
        SVCERR << "FeatureExtractionModelTransformer::run: Exception caught: "
               << e.what() << endl;
        m_abandoned = true;
        m_message = e.what();
    }

    for (int j = 0; j < (int)m_outputNos.size(); ++j) {
        setCompletion(j, 100);
    }

    if (frequencyDomain) {
        for (int ch = 0; ch < channelCount; ++ch) {
            delete fftModels[ch];
        }
        delete[] reals;
        delete[] imaginaries;
    }

    for (int ch = 0; ch < channelCount; ++ch) {
        delete[] buffers[ch];
    }
    delete[] buffers;

    deinitialise();
}

void
FeatureExtractionModelTransformer::getFrames(int channelCount,
                                             sv_frame_t startFrame,
                                             sv_frame_t size,
                                             float **buffers)
{
    sv_frame_t offset = 0;

    if (startFrame < 0) {
        for (int c = 0; c < channelCount; ++c) {
            for (sv_frame_t i = 0; i < size && startFrame + i < 0; ++i) {
                buffers[c][i] = 0.0f;
            }
        }
        offset = -startFrame;
        size -= offset;
        if (size <= 0) return;
        startFrame = 0;
    }

    auto input = ModelById::getAs<DenseTimeValueModel>(getInputModel());
    if (!input) {
        return;
    }
    
    sv_frame_t got = 0;

    if (channelCount == 1) {

        auto data = input->getData(m_input.getChannel(), startFrame, size);
        got = data.size();

        copy(data.begin(), data.end(), buffers[0] + offset);

        if (m_input.getChannel() == -1 && input->getChannelCount() > 1) {
            // use mean instead of sum, as plugin input
            float cc = float(input->getChannelCount());
            for (sv_frame_t i = 0; i < got; ++i) {
                buffers[0][i + offset] /= cc;
            }
        }

    } else {

        auto data = input->getMultiChannelData(0, channelCount-1, startFrame, size);
        if (!data.empty()) {
            got = data[0].size();
            for (int c = 0; in_range_for(data, c); ++c) {
                copy(data[c].begin(), data[c].end(), buffers[c] + offset);
            }
        }
    }

    while (got < size) {
        for (int c = 0; c < channelCount; ++c) {
            buffers[c][got + offset] = 0.0;
        }
        ++got;
    }
}

void
FeatureExtractionModelTransformer::addFeature(int n,
                                              sv_frame_t blockFrame,
                                              const Vamp::Plugin::Feature &feature)
{
    auto input = ModelById::get(getInputModel());
    if (!input) return;

    sv_samplerate_t inputRate = input->getSampleRate();

//    cerr << "FeatureExtractionModelTransformer::addFeature: blockFrame = "
//              << blockFrame << ", hasTimestamp = " << feature.hasTimestamp
//              << ", timestamp = " << feature.timestamp << ", hasDuration = "
//              << feature.hasDuration << ", duration = " << feature.duration
//              << endl;

    sv_frame_t frame = blockFrame;

    if (m_descriptors[n].sampleType ==
        Vamp::Plugin::OutputDescriptor::VariableSampleRate) {

        if (!feature.hasTimestamp) {
            SVDEBUG
                << "WARNING: FeatureExtractionModelTransformer::addFeature: "
                << "Feature has variable sample rate but no timestamp!"
                << endl;
            return;
        } else {
            frame = RealTime::realTime2Frame(feature.timestamp, inputRate);
        }

//        cerr << "variable sample rate: timestamp = " << feature.timestamp
//             << " at input rate " << inputRate << " -> " << frame << endl;
        
    } else if (m_descriptors[n].sampleType ==
               Vamp::Plugin::OutputDescriptor::FixedSampleRate) {

        sv_samplerate_t rate = m_descriptors[n].sampleRate;
        if (rate <= 0.0) {
            rate = inputRate;
        }
        
        if (!feature.hasTimestamp) {
            ++m_fixedRateFeatureNos[n];
        } else {
            RealTime ts(feature.timestamp.sec, feature.timestamp.nsec);
            m_fixedRateFeatureNos[n] = (int)lrint(ts.toDouble() * rate);
        }

//        cerr << "m_fixedRateFeatureNo = " << m_fixedRateFeatureNos[n]
//             << ", m_descriptor->sampleRate = " << m_descriptors[n].sampleRate
//             << ", inputRate = " << inputRate
//             << " giving frame = ";
        frame = lrint((double(m_fixedRateFeatureNos[n]) / rate) * inputRate);
//        cerr << frame << endl;
    }

    if (frame < 0) {
        SVDEBUG
            << "WARNING: FeatureExtractionModelTransformer::addFeature: "
            << "Negative frame counts are not supported (frame = " << frame
            << " from timestamp " << feature.timestamp
            << "), dropping feature" 
            << endl;
        return;
    }

    // Rather than repeat the complicated tests from the constructor
    // to determine what sort of model we must be adding the features
    // to, we instead test what sort of model the constructor decided
    // to create.

    ModelId outputId = m_outputs[n];

    if (isOutputType<SparseOneDimensionalModel>(n)) {

        auto model = ModelById::getAs<SparseOneDimensionalModel>(outputId);
        if (!model) return;
        model->add(Event(frame, feature.label.c_str()));
        
    } else if (isOutputType<SparseTimeValueModel>(n)) {

        auto model = ModelById::getAs<SparseTimeValueModel>(outputId);
        if (!model) return;

        for (int i = 0; in_range_for(feature.values, i); ++i) {

            float value = feature.values[i];

            QString label = feature.label.c_str();
            if (feature.values.size() > 1) {
                label = QString("[%1] %2").arg(i+1).arg(label);
            }

            auto targetModel = model;

            if (m_needAdditionalModels[n] && i > 0) {
                targetModel = ModelById::getAs<SparseTimeValueModel>
                    (getAdditionalModel(n, i));
                if (!targetModel) targetModel = model;
            }

            targetModel->add(Event(frame, value, label));
        }

    } else if (isOutputType<NoteModel>(n) || isOutputType<RegionModel>(n)) {
    
        int index = 0;

        float value = 0.0;
        if ((int)feature.values.size() > index) {
            value = feature.values[index++];
        }

        sv_frame_t duration = 1;
        if (feature.hasDuration) {
            duration = RealTime::realTime2Frame(feature.duration, inputRate);
        } else {
            if (in_range_for(feature.values, index)) {
                duration = lrintf(feature.values[index++]);
            }
        }

        auto noteModel = ModelById::getAs<NoteModel>(outputId);
        if (noteModel) {

            float velocity = 100;
            if ((int)feature.values.size() > index) {
                velocity = feature.values[index++];
            }
            if (velocity < 0) velocity = 127;
            if (velocity > 127) velocity = 127;
            
            noteModel->add(Event(frame, value, // value is pitch
                                 duration,
                                 velocity / 127.f,
                                 feature.label.c_str()));
        }

        auto regionModel = ModelById::getAs<RegionModel>(outputId);
        if (regionModel) {
            
            if (feature.hasDuration && !feature.values.empty()) {
                
                for (int i = 0; in_range_for(feature.values, i); ++i) {
                    
                    float value = feature.values[i];
                    
                    QString label = feature.label.c_str();
                    if (feature.values.size() > 1) {
                        label = QString("[%1] %2").arg(i+1).arg(label);
                    }
                    
                    regionModel->add(Event(frame,
                                           value,
                                           duration,
                                           label));
                }
            } else {
                
                regionModel->add(Event(frame,
                                       value,
                                       duration,
                                       feature.label.c_str()));
            }
        }

    } else if (isOutputType<BasicCompressedDenseThreeDimensionalModel>(n)) {

        auto model = ModelById::getAs
            <BasicCompressedDenseThreeDimensionalModel>(outputId);
        if (!model) return;
        
        DenseThreeDimensionalModel::Column values = feature.values;
        
        if (!feature.hasTimestamp && m_fixedRateFeatureNos[n] >= 0) {
            model->setColumn(m_fixedRateFeatureNos[n], values);
        } else {
            model->setColumn(int(frame / model->getResolution()), values);
        }
    } else {
        
        SVDEBUG << "FeatureExtractionModelTransformer::addFeature: Unknown output model type - possibly a deleted model" << endl;
        abandon();
    }
}

void
FeatureExtractionModelTransformer::setCompletion(int n, int completion)
{
#ifdef DEBUG_FEATURE_EXTRACTION_TRANSFORMER_RUN
    SVDEBUG << "FeatureExtractionModelTransformer::setCompletion("
              << completion << ")" << endl;
#endif

    (void)
        (setOutputCompletion<SparseOneDimensionalModel>(n, completion) ||
         setOutputCompletion<SparseTimeValueModel>(n, completion) ||
         setOutputCompletion<NoteModel>(n, completion) ||
         setOutputCompletion<RegionModel>(n, completion) ||
         setOutputCompletion<BasicCompressedDenseThreeDimensionalModel>(n, completion));
}

