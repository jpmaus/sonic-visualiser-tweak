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

#include "LayerFactory.h"

#include "WaveformLayer.h"
#include "SpectrogramLayer.h"
#include "TimeRulerLayer.h"
#include "TimeInstantLayer.h"
#include "TimeValueLayer.h"
#include "NoteLayer.h"
#include "FlexiNoteLayer.h"
#include "RegionLayer.h"
#include "BoxLayer.h"
#include "TextLayer.h"
#include "ImageLayer.h"
#include "Colour3DPlotLayer.h"
#include "SpectrumLayer.h"
#include "SliceLayer.h"
#include "SliceableLayer.h"

#include "base/Clipboard.h"

#include "data/model/RangeSummarisableTimeValueModel.h"
#include "data/model/DenseTimeValueModel.h"
#include "data/model/SparseOneDimensionalModel.h"
#include "data/model/SparseTimeValueModel.h"
#include "data/model/NoteModel.h"
#include "data/model/RegionModel.h"
#include "data/model/BoxModel.h"
#include "data/model/TextModel.h"
#include "data/model/ImageModel.h"
#include "data/model/DenseThreeDimensionalModel.h"
#include "data/model/WaveFileModel.h"
#include "data/model/WritableWaveFileModel.h"

#include <QDomDocument>
#include <QDomElement>
#include <QDomNamedNodeMap>
#include <QDomAttr>

#include <QSettings>

LayerFactory *
LayerFactory::m_instance = new LayerFactory;

LayerFactory *
LayerFactory::getInstance()
{
    return m_instance;
}

LayerFactory::~LayerFactory()
{
}

QString
LayerFactory::getLayerPresentationName(LayerType type)
{
    switch (type) {
    case Waveform:     return Layer::tr("Waveform");
    case Spectrogram:  return Layer::tr("Spectrogram");
    case TimeRuler:    return Layer::tr("Ruler");
    case TimeInstants: return Layer::tr("Time Instants");
    case TimeValues:   return Layer::tr("Time Values");
    case Notes:        return Layer::tr("Notes");
    case FlexiNotes:   return Layer::tr("Flexible Notes");
    case Regions:      return Layer::tr("Regions");
    case Boxes:        return Layer::tr("Boxes");
    case Text:         return Layer::tr("Text");
    case Image:        return Layer::tr("Images");
    case Colour3DPlot: return Layer::tr("Colour 3D Plot");
    case Spectrum:     return Layer::tr("Spectrum");
    case Slice:        return Layer::tr("Time Slice");

    case MelodicRangeSpectrogram:
        // The user can change all the parameters of this after the
        // fact -- there's nothing permanently melodic-range about it
        // that should be encoded in its name
        return Layer::tr("Spectrogram");

    case PeakFrequencySpectrogram:
        // likewise
        return Layer::tr("Spectrogram");

    case UnknownLayer:
    default:
        cerr << "WARNING: LayerFactory::getLayerPresentationName passed unknown layer" << endl;
        return Layer::tr("Unknown Layer");
    }
}

bool
LayerFactory::isLayerSliceable(const Layer *layer)
{
    if (dynamic_cast<const SliceableLayer *>(layer)) {
        if (dynamic_cast<const SpectrogramLayer *>(layer)) {

            //!!! We can create slices of spectrograms, but there's a
            // problem managing the models.  The source model for the
            // slice layer has to be one of the spectrogram's FFT
            // models -- that's fine, except that we can't store &
            // recall the slice layer with a reference to that model
            // because the model is internal to the spectrogram layer
            // and the document has no record of it.  We would need
            // some other way of managing models that are used in this
            // way.  For the moment we just don't allow slices of
            // spectrograms -- and provide a spectrum layer for this
            // instead.
            //
            // This business needs a bit more thought -- either come
            // up with a sensible way to deal with that stuff, or
            // simplify the existing slice layer logic so that it
            // doesn't have to deal with models disappearing on it at
            // all (and use the normal Document setModel mechanism to
            // set its sliceable model instead of the fancy pants
            // nonsense it's doing at the moment).

            return false;
        }
        return true;
    }
    return false;
}

LayerFactory::LayerTypeSet
LayerFactory::getValidLayerTypes(ModelId modelId)
{
    LayerTypeSet types;

    if (ModelById::getAs<DenseThreeDimensionalModel>(modelId)) {
        types.insert(Colour3DPlot);
        types.insert(Slice);
    }

    if (ModelById::getAs<RangeSummarisableTimeValueModel>(modelId)) {
        types.insert(Waveform);
    }

    if (ModelById::getAs<DenseTimeValueModel>(modelId)) {
        types.insert(Spectrogram);
        types.insert(MelodicRangeSpectrogram);
        types.insert(PeakFrequencySpectrogram);
    }

    if (ModelById::getAs<SparseOneDimensionalModel>(modelId)) {
        types.insert(TimeInstants);
    }

    if (ModelById::getAs<SparseTimeValueModel>(modelId)) {
        types.insert(TimeValues);
    }

    if (ModelById::getAs<NoteModel>(modelId)) {
        auto nm = ModelById::getAs<NoteModel>(modelId);
        if (nm && nm->getSubtype() == NoteModel::FLEXI_NOTE) {
            types.insert(FlexiNotes);
        } else {
            types.insert(Notes);
        }
    }

    if (ModelById::getAs<RegionModel>(modelId)) {
        types.insert(Regions);
    }

    if (ModelById::getAs<BoxModel>(modelId)) {
        types.insert(Boxes);
    }

    if (ModelById::getAs<TextModel>(modelId)) {
        types.insert(Text);
    }

    if (ModelById::getAs<ImageModel>(modelId)) {
        types.insert(Image);
    }

    if (ModelById::getAs<DenseTimeValueModel>(modelId)) {
        types.insert(Spectrum);
    }

    // We don't count TimeRuler here as it doesn't actually display
    // the data, although it can be backed by any model

    return types;
}

LayerFactory::LayerTypeSet
LayerFactory::getValidEmptyLayerTypes()
{
    LayerTypeSet types;
    types.insert(TimeInstants);
    types.insert(TimeValues);
    // Because this is strictly a UI function -- list the layer types
    // to show in a menu -- it should not contain FlexiNotes; the
    // layer isn't meaningfully editable in SV
//    types.insert(FlexiNotes);
    types.insert(Notes);
    types.insert(Regions);
    types.insert(Boxes);
    types.insert(Text);
    types.insert(Image);
    //!!! and in principle Colour3DPlot -- now that's a challenge
    return types;
}

LayerFactory::LayerType
LayerFactory::getLayerType(const Layer *layer)
{
    if (dynamic_cast<const WaveformLayer *>(layer)) return Waveform;
    if (dynamic_cast<const SpectrogramLayer *>(layer)) return Spectrogram;
    if (dynamic_cast<const TimeRulerLayer *>(layer)) return TimeRuler;
    if (dynamic_cast<const TimeInstantLayer *>(layer)) return TimeInstants;
    if (dynamic_cast<const TimeValueLayer *>(layer)) return TimeValues;
    if (dynamic_cast<const FlexiNoteLayer *>(layer)) return FlexiNotes;
    if (dynamic_cast<const NoteLayer *>(layer)) return Notes;
    if (dynamic_cast<const RegionLayer *>(layer)) return Regions;
    if (dynamic_cast<const BoxLayer *>(layer)) return Boxes;
    if (dynamic_cast<const TextLayer *>(layer)) return Text;
    if (dynamic_cast<const ImageLayer *>(layer)) return Image;
    if (dynamic_cast<const Colour3DPlotLayer *>(layer)) return Colour3DPlot;
    if (dynamic_cast<const SpectrumLayer *>(layer)) return Spectrum;
    if (dynamic_cast<const SliceLayer *>(layer)) return Slice;
    return UnknownLayer;
}

QString
LayerFactory::getLayerIconName(LayerType type)
{
    switch (type) {
    case Waveform: return "waveform";
    case Spectrogram: return "spectrogram";
    case TimeRuler: return "timeruler";
    case TimeInstants: return "instants";
    case TimeValues: return "values";
    case Notes: return "notes";
    case FlexiNotes: return "flexinotes";
    case Regions: return "regions";
    case Boxes: return "boxes";
    case Text: return "text";
    case Image: return "image";
    case Colour3DPlot: return "colour3d";
    case Spectrum: return "spectrum";
    case Slice: return "spectrum";
    case MelodicRangeSpectrogram: return "spectrogram";
    case PeakFrequencySpectrogram: return "spectrogram";
    case UnknownLayer:
    default:
        cerr << "WARNING: LayerFactory::getLayerIconName passed unknown layer" << endl;
        return "unknown";
    }
}

QString
LayerFactory::getLayerTypeName(LayerType type)
{
    switch (type) {
    case Waveform: return "waveform";
    case Spectrogram: return "spectrogram";
    case TimeRuler: return "timeruler";
    case TimeInstants: return "timeinstants";
    case TimeValues: return "timevalues";
    case Notes: return "notes";
    case FlexiNotes: return "flexinotes";
    case Regions: return "regions";
    case Boxes: return "boxes";
    case Text: return "text";
    case Image: return "image";
    case Colour3DPlot: return "colour3dplot";
    case Spectrum: return "spectrum";
    case Slice: return "slice";
    case MelodicRangeSpectrogram: return "melodicrange";
    case PeakFrequencySpectrogram: return "peakfrequency";
    case UnknownLayer:
    default:
        cerr << "WARNING: LayerFactory::getLayerTypeName passed unknown layer" << endl;
        return "unknown";
    }
}

LayerFactory::LayerType
LayerFactory::getLayerTypeForName(QString name)
{
    if (name == "waveform") return Waveform;
    if (name == "spectrogram") return Spectrogram;
    if (name == "timeruler") return TimeRuler;
    if (name == "timeinstants") return TimeInstants;
    if (name == "timevalues") return TimeValues;
    if (name == "notes") return Notes;
    if (name == "flexinotes") return FlexiNotes;
    if (name == "regions") return Regions;
    if (name == "boxes" || name == "timefrequencybox") return Boxes;
    if (name == "text") return Text;
    if (name == "image") return Image;
    if (name == "colour3dplot") return Colour3DPlot;
    if (name == "spectrum") return Spectrum;
    if (name == "slice") return Slice;
    return UnknownLayer;
}

void
LayerFactory::setModel(Layer *layer, ModelId model)
{
    if (trySetModel<WaveformLayer, WaveFileModel>(layer, model))
        return;

    if (trySetModel<WaveformLayer, WritableWaveFileModel>(layer, model))
        return;

    if (trySetModel<SpectrogramLayer, DenseTimeValueModel>(layer, model))
        return;

    if (trySetModel<TimeRulerLayer, Model>(layer, model))
        return;

    if (trySetModel<TimeInstantLayer, SparseOneDimensionalModel>(layer, model))
        return;

    if (trySetModel<TimeValueLayer, SparseTimeValueModel>(layer, model))
        return;

    if (trySetModel<NoteLayer, NoteModel>(layer, model)) 
        return; 

    if (trySetModel<FlexiNoteLayer, NoteModel>(layer, model)) 
        return; 
        
    if (trySetModel<RegionLayer, RegionModel>(layer, model))
        return;

    if (trySetModel<BoxLayer, BoxModel>(layer, model))
        return;

    if (trySetModel<TextLayer, TextModel>(layer, model))
        return;

    if (trySetModel<ImageLayer, ImageModel>(layer, model))
        return;

    if (trySetModel<Colour3DPlotLayer, DenseThreeDimensionalModel>(layer, model))
        return;

    if (trySetModel<SpectrumLayer, DenseTimeValueModel>(layer, model)) 
        return;
}

std::shared_ptr<Model>
LayerFactory::createEmptyModel(LayerType layerType, ModelId baseModelId)
{
    auto baseModel = ModelById::get(baseModelId);
    if (!baseModel) return {};

    sv_samplerate_t rate = baseModel->getSampleRate();
    
    if (layerType == TimeInstants) {
        return std::make_shared<SparseOneDimensionalModel>(rate, 1);
    } else if (layerType == TimeValues) {
        return std::make_shared<SparseTimeValueModel>(rate, 1, true);
    } else if (layerType == FlexiNotes) {
        return std::make_shared<NoteModel>(rate, 1, true);
    } else if (layerType == Notes) {
        return std::make_shared<NoteModel>(rate, 1, true);
    } else if (layerType == Regions) {
        return std::make_shared<RegionModel>(rate, 1, true);
    } else if (layerType == Boxes) {
        return std::make_shared<BoxModel>(rate, 1, true);
    } else if (layerType == Text) {
        return std::make_shared<TextModel>(rate, 1, true);
    } else if (layerType == Image) {
        return std::make_shared<ImageModel>(rate, 1, true);
    } else {
        return {};
    }
}

int
LayerFactory::getChannel(Layer *layer)
{
    if (dynamic_cast<WaveformLayer *>(layer)) {
        return dynamic_cast<WaveformLayer *>(layer)->getChannel();
    } 
    if (dynamic_cast<SpectrogramLayer *>(layer)) {
        return dynamic_cast<SpectrogramLayer *>(layer)->getChannel();
    }
    return -1;
}

void
LayerFactory::setChannel(Layer *layer, int channel)
{
    if (dynamic_cast<WaveformLayer *>(layer)) {
        dynamic_cast<WaveformLayer *>(layer)->setChannel(channel);
        return;
    } 
    if (dynamic_cast<SpectrogramLayer *>(layer)) {
        dynamic_cast<SpectrogramLayer *>(layer)->setChannel(channel);
        return;
    }
    if (dynamic_cast<SpectrumLayer *>(layer)) {
        dynamic_cast<SpectrumLayer *>(layer)->setChannel(channel);
        return;
    }
}

Layer *
LayerFactory::createLayer(LayerType type)
{
    Layer *layer = nullptr;

    switch (type) {

    case Waveform:
        layer = new WaveformLayer;
        break;

    case Spectrogram:
        layer = new SpectrogramLayer;
        break;

    case TimeRuler:
        layer = new TimeRulerLayer;
        break;

    case TimeInstants:
        layer = new TimeInstantLayer;
        break;

    case TimeValues:
        layer = new TimeValueLayer;
        break;

    case FlexiNotes:
        layer = new FlexiNoteLayer;
        break;

    case Notes:
        layer = new NoteLayer;
        break;

    case Regions:
        layer = new RegionLayer;
        break;

    case Boxes:
        layer = new BoxLayer;
        break;

    case Text:
        layer = new TextLayer;
        break;

    case Image:
        layer = new ImageLayer;
        break;

    case Colour3DPlot:
        layer = new Colour3DPlotLayer;
        break;

    case Spectrum:
        layer = new SpectrumLayer;
        break;

    case Slice:
        layer = new SliceLayer;
        break;

    case MelodicRangeSpectrogram: 
        layer = new SpectrogramLayer(SpectrogramLayer::MelodicRange);
        break;

    case PeakFrequencySpectrogram: 
        layer = new SpectrogramLayer(SpectrogramLayer::MelodicPeaks);
        break;

    case UnknownLayer:
    default:
        cerr << "WARNING: LayerFactory::createLayer passed unknown layer" << endl;
        break;
    }

    if (!layer) {
        cerr << "LayerFactory::createLayer: Unknown layer type " 
                  << type << endl;
    } else {
//        SVDEBUG << "LayerFactory::createLayer: Setting object name "
//                  << getLayerPresentationName(type) << " on " << layer << endl;
        layer->setObjectName(getLayerPresentationName(type));
        setLayerDefaultProperties(type, layer);
    }

    return layer;
}

void
LayerFactory::setLayerDefaultProperties(LayerType type, Layer *layer)
{
//    SVDEBUG << "LayerFactory::setLayerDefaultProperties: type " << type << " (name \"" << getLayerTypeName(type) << "\")" << endl;

    QSettings settings;
    settings.beginGroup("LayerDefaults");
    QString defaults = settings.value(getLayerTypeName(type), "").toString();
    if (defaults == "") return;
    setLayerProperties(layer, defaults);
    settings.endGroup();
}

void
LayerFactory::setLayerProperties(Layer *layer, QString newXml)
{
    QDomDocument docOld, docNew;
    QString oldXml = layer->toXmlString();

    if (!docOld.setContent(oldXml, false)) {
        SVCERR << "LayerFactory::setLayerProperties: Failed to parse XML for existing layer properties! XML string is: " << oldXml << endl;
        return;
    }

    if (!docNew.setContent(newXml, false)) {
        SVCERR << "LayerFactory::setLayerProperties: Failed to parse XML: " << newXml << endl;
        return;
    }
        
    QXmlAttributes attrs;
        
    QDomElement layerElt = docNew.firstChildElement("layer");
    QDomNamedNodeMap attrNodes = layerElt.attributes();
        
    for (int i = 0; i < attrNodes.length(); ++i) {
        QDomAttr attr = attrNodes.item(i).toAttr();
        if (attr.isNull()) continue;
//            cerr << "append \"" << attr.name()
//                      << "\" -> \"" << attr.value() << "\""
//                      << endl;
        attrs.append(attr.name(), "", "", attr.value());
    }
    
    layerElt = docOld.firstChildElement("layer");
    attrNodes = layerElt.attributes();
    for (int i = 0; i < attrNodes.length(); ++i) {
        QDomAttr attr = attrNodes.item(i).toAttr();
        if (attr.isNull()) continue;
        if (attrs.value(attr.name()) == "") {
//                cerr << "append \"" << attr.name()
//                          << "\" -> \"" << attr.value() << "\""
//                          << endl;
            attrs.append(attr.name(), "", "", attr.value());
        }
    }
    
    layer->setProperties(attrs);
}

LayerFactory::LayerType
LayerFactory::getLayerTypeForClipboardContents(const Clipboard &clip)
{
    const EventVector &contents = clip.getPoints();

    bool haveValue = false;
    bool haveDuration = false;
    bool haveLevel = false;

    for (EventVector::const_iterator i = contents.begin();
         i != contents.end(); ++i) {
        if (i->hasValue()) haveValue = true;
        if (i->hasDuration()) haveDuration = true;
        if (i->hasLevel()) haveLevel = true;
    }

    if (haveValue && haveDuration && haveLevel) return Notes;
    if (haveValue && haveDuration) return Regions;
    if (haveValue) return TimeValues;
    return TimeInstants;
}
    
