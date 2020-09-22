/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_LAYER_FACTORY_H
#define SV_LAYER_FACTORY_H

#include <QString>
#include <set>

#include "data/model/Model.h"

class Layer;
class Clipboard;

class LayerFactory
{
public:
    enum LayerType {

        // Standard layers
        Waveform,
        Spectrogram,
        TimeRuler,
        TimeInstants,
        TimeValues,
        Notes,
        FlexiNotes,
        Regions,
        Boxes,
        Text,
        Image,
        Colour3DPlot,
        Spectrum,
        Slice,

        // Layers with different initial parameters
        MelodicRangeSpectrogram,
        PeakFrequencySpectrogram,

        // Not-a-layer-type
        UnknownLayer = 255
    };

    static LayerFactory *getInstance();
    
    virtual ~LayerFactory();

    typedef std::set<LayerType> LayerTypeSet;
    LayerTypeSet getValidLayerTypes(ModelId modelId);

    /**
     * Return the set of layer types that an end user should be
     * allowed to create, empty, for subsequent editing.
     */
    LayerTypeSet getValidEmptyLayerTypes();

    LayerType getLayerType(const Layer *);

    Layer *createLayer(LayerType type);

    /**
     * Set the default properties of a layer, from the XML string
     * contained in the LayerDefaults settings group for the given
     * layer type. Leave unchanged any properties not mentioned in the
     * settings.
     */
    void setLayerDefaultProperties(LayerType type, Layer *layer);

    /**
     * Set the properties of a layer, from the XML string
     * provided. Leave unchanged any properties not mentioned.
     */
    void setLayerProperties(Layer *layer, QString xmlString);

    QString getLayerPresentationName(LayerType type);

    bool isLayerSliceable(const Layer *);

    void setModel(Layer *layer, ModelId model);
    std::shared_ptr<Model> createEmptyModel(LayerType type, ModelId baseModel);

    int getChannel(Layer *layer);
    void setChannel(Layer *layer, int channel);

    QString getLayerIconName(LayerType);
    QString getLayerTypeName(LayerType);
    LayerType getLayerTypeForName(QString);

    LayerType getLayerTypeForClipboardContents(const Clipboard &);

protected:
    template <typename LayerClass, typename ModelClass>
    bool trySetModel(Layer *layerBase, ModelId modelId) {
        LayerClass *layer = dynamic_cast<LayerClass *>(layerBase);
        if (!layer) return false;
        if (!modelId.isNone()) {
            auto model = ModelById::getAs<ModelClass>(modelId);
            if (!model) return false;
        }
        layer->setModel(modelId);
        return true;
    }

    static LayerFactory *m_instance;
};

#endif

