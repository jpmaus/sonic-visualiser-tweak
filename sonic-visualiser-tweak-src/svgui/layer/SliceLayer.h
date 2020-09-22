/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_SLICE_LAYER_H
#define SV_SLICE_LAYER_H

#include "SingleColourLayer.h"

#include "base/Window.h"

#include "data/model/DenseThreeDimensionalModel.h"

#include <QColor>

class SliceLayer : public SingleColourLayer
{
    Q_OBJECT

public:
    SliceLayer();
    ~SliceLayer();
    
    ModelId getModel() const override { return {}; }

    void setSliceableModel(ModelId model);

    void paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const override;

    QString getFeatureDescription(LayerGeometryProvider *v, QPoint &) const override;

    int getVerticalScaleWidth(LayerGeometryProvider *v, bool, QPainter &) const override;
    void paintVerticalScale(LayerGeometryProvider *v, bool, QPainter &paint, QRect rect) const override;

    ColourSignificance getLayerColourSignificance() const override {
        return ColourAndBackgroundSignificant;
    }

    bool hasLightBackground() const override;

    PropertyList getProperties() const override;
    QString getPropertyLabel(const PropertyName &) const override;
    QString getPropertyIconName(const PropertyName &) const override;
    PropertyType getPropertyType(const PropertyName &) const override;
    QString getPropertyGroupName(const PropertyName &) const override;
    int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const override;
    QString getPropertyValueLabel(const PropertyName &,
                                          int value) const override;
    RangeMapper *getNewPropertyRangeMapper(const PropertyName &) const override;
    void setProperty(const PropertyName &, int value) override;
    void setProperties(const QXmlAttributes &) override;

    bool getValueExtents(double &min, double &max,
                                 bool &logarithmic, QString &unit) const override;

    bool getDisplayExtents(double &min, double &max) const override;
    bool setDisplayExtents(double min, double max) override;

    int getVerticalZoomSteps(int &defaultStep) const override;
    int getCurrentVerticalZoomStep() const override;
    void setVerticalZoomStep(int) override;
    RangeMapper *getNewVerticalZoomRangeMapper() const override;

    virtual bool hasTimeXAxis() const override { return false; }

    virtual void zoomToRegion(const LayerGeometryProvider *, QRect) override;

    bool isLayerScrollable(const LayerGeometryProvider *) const override { return false; }

    enum EnergyScale { LinearScale, MeterScale, dBScale, AbsoluteScale };

    enum SamplingMode { NearestSample, SampleMean, SamplePeak };

    enum PlotStyle { PlotLines, PlotSteps, PlotBlocks, PlotFilledBlocks };

    enum BinScale { LinearBins, LogBins, InvertedLogBins };

    bool usesSolidColour() const { return m_plotStyle == PlotFilledBlocks; }
    
    void setFillColourMap(int);
    int getFillColourMap() const { return m_colourMap; }

    void setEnergyScale(EnergyScale);
    EnergyScale getEnergyScale() const { return m_energyScale; }

    void setSamplingMode(SamplingMode);
    SamplingMode getSamplingMode() const { return m_samplingMode; }

    void setPlotStyle(PlotStyle style);
    PlotStyle getPlotStyle() const { return m_plotStyle; }

    void setBinScale(BinScale scale);
    BinScale getBinScale() const { return m_binScale; }

    void setThreshold(float);
    float getThreshold() const { return m_threshold; }

    void setGain(float gain);
    float getGain() const;

    void setNormalize(bool n);
    bool getNormalize() const;

    void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const override;

public slots:
    void sliceableModelReplaced(ModelId, ModelId);

protected:
    /// Convert a (possibly non-integral) bin into x-coord. May be overridden
    virtual double getXForBin(const LayerGeometryProvider *, double bin) const;
    
    /// Convert an x-coord into (possibly non-integral) bin. May be overridden
    virtual double getBinForX(const LayerGeometryProvider *, double x) const;

    /// Convert a point such as a bin number into x-coord, given max &
    /// min. For use by getXForBin etc
    double getXForScalePoint(const LayerGeometryProvider *,
                             double p, double pmin, double pmax) const;

    /// Convert an x-coord into a point such as a bin number, given
    /// max & min. For use by getBinForX etc
    double getScalePointForX(const LayerGeometryProvider *,
                             double x, double pmin, double pmax) const;

    virtual double getYForValue(const LayerGeometryProvider *v, double value, double &norm) const;
    virtual double getValueForY(const LayerGeometryProvider *v, double y) const;
    
    virtual QString getFeatureDescriptionAux(LayerGeometryProvider *v, QPoint &,
                                             bool includeBinDescription,
                                             int &minbin, int &maxbin,
                                             int &range) const;

    // This curve may, of course, be flat -- the spectrum uses it for
    // normalizing the fft results by the fft size (with 1/(fftsize/2)
    // in each bin).
    typedef std::vector<float> BiasCurve;
    virtual void getBiasCurve(BiasCurve &) const { return; }

    virtual float getThresholdDb() const;

    int getDefaultColourHint(bool dark, bool &impose) override;

    // Determine how the bins are lined up
    // horizontally. BinsCentredOnScalePoint means we operate like a
    // spectrum, where a bin maps to a specific frequency, and so the
    // bin should be visually centred on the scale point that
    // corresponds to that frequency. BinsSpanScalePoints means we
    // have numbered or labelled bins that are not mapped to a
    // continuous scale, like a typical chromagram output, and so bin
    // N spans from scale point N to N+1.  This is a fundamental
    // quality of the class or input data, not a user-configurable
    // property.
    //
    enum BinAlignment {
        BinsCentredOnScalePoints,
        BinsSpanScalePoints
    };

    ModelId                     m_sliceableModel; // a DenseThreeDimensionalModel
    BinAlignment                m_binAlignment;
    int                         m_colourMap;
    bool                        m_colourInverted;
    EnergyScale                 m_energyScale;
    SamplingMode                m_samplingMode;
    PlotStyle                   m_plotStyle;
    BinScale                    m_binScale;
    bool                        m_normalize;
    float                       m_threshold;
    float                       m_initialThreshold;
    float                       m_gain;
    int                         m_minbin;
    int                         m_maxbin;
    mutable std::vector<int>    m_scalePoints;
    mutable int                 m_scalePaintHeight;
    mutable std::map<int, int>  m_xorigins; // LayerGeometryProvider id -> x
    mutable std::map<int, int>  m_yorigins; // LayerGeometryProvider id -> y
    mutable std::map<int, int>  m_heights;  // LayerGeometryProvider id -> h
    mutable sv_frame_t          m_currentf0;
    mutable sv_frame_t          m_currentf1;
    mutable std::vector<float>  m_values;
};

#endif
