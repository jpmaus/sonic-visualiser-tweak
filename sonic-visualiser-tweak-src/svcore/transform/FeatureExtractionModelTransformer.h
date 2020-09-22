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

#ifndef SV_FEATURE_EXTRACTION_MODEL_TRANSFORMER_H
#define SV_FEATURE_EXTRACTION_MODEL_TRANSFORMER_H

#include "ModelTransformer.h"

#include <QString>
#include <QMutex>
#include <QWaitCondition>

#include <vamp-hostsdk/Plugin.h>

#include <iostream>
#include <map>

class DenseTimeValueModel;
class SparseTimeValueModel;

class FeatureExtractionModelTransformer : public ModelTransformer // + is a Thread
{
    Q_OBJECT

public:
    FeatureExtractionModelTransformer(Input input,
                                      const Transform &transform);

    /**
     * Obtain outputs for a set of transforms that all use the same
     * plugin and input (but with different outputs). i.e. run the
     * plugin once only and collect more than one output from it.
     */
    FeatureExtractionModelTransformer(Input input,
                                      const Transforms &relatedTransforms);

    virtual ~FeatureExtractionModelTransformer();

    // ModelTransformer method, retrieve the additional models
    Models getAdditionalOutputModels() override;
    bool willHaveAdditionalOutputModels() override;

protected:
    bool initialise();
    void deinitialise();

    void run() override;

    Vamp::Plugin *m_plugin;

    // descriptors per transform
    std::vector<Vamp::Plugin::OutputDescriptor> m_descriptors;

    // to assign times to FixedSampleRate features
    std::vector<int> m_fixedRateFeatureNos;

    // list of plugin output indexes required for this group of transforms
    std::vector<int> m_outputNos;

    void createOutputModels(int n);

    // map from transformNo -> necessity
    std::map<int, bool> m_needAdditionalModels;

    // map from transformNo -> binNo -> SparseTimeValueModel id
    typedef std::map<int, std::map<int, ModelId> > AdditionalModelMap;
    
    AdditionalModelMap m_additionalModels;
    
    ModelId getAdditionalModel(int transformNo, int binNo);

    void addFeature(int n,
                    sv_frame_t blockFrame,
                    const Vamp::Plugin::Feature &feature);

    void setCompletion(int, int);

    void getFrames(int channelCount, sv_frame_t startFrame, sv_frame_t size,
                   float **buffer);

    bool m_haveOutputs;
    QMutex m_outputMutex;
    QWaitCondition m_outputsCondition;
    void awaitOutputModels() override;
    
    template <typename T> bool isOutputType(int n) {
        if (!ModelById::getAs<T>(m_outputs[n])) {
            return false;
        } else {
            return true;
        }
    }

    template <typename T> bool setOutputCompletion(int n, int completion) {
        auto model = ModelById::getAs<T>(m_outputs[n]);
        if (!model) {
            return false;
        } else {
            model->setCompletion(completion, true);
            return true;
        }
    }
};

#endif

