
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

#ifndef SV_SPECTRUM_LAYER_H
#define SV_SPECTRUM_LAYER_H

#include "SliceLayer.h"

#include "base/Window.h"

#include "data/model/DenseTimeValueModel.h"

#include "HorizontalScaleProvider.h"

#include <QColor>
#include <QMutex>

class SpectrumLayer : public SliceLayer,
                      public HorizontalScaleProvider
{
    Q_OBJECT

public:
    SpectrumLayer();
    ~SpectrumLayer();
    
    void setModel(ModelId model); // a DenseTimeValueModel
    virtual ModelId getModel() const override { return m_originModel; }

    virtual bool getCrosshairExtents(LayerGeometryProvider *, QPainter &, QPoint cursorPos,
                                     std::vector<QRect> &extents) const override;
    virtual void paintCrosshairs(LayerGeometryProvider *, QPainter &, QPoint) const override;

    virtual int getHorizontalScaleHeight(LayerGeometryProvider *, QPainter &) const override;
    virtual void paintHorizontalScale(LayerGeometryProvider *, QPainter &, int xorigin) const;
    
    virtual QString getFeatureDescription(LayerGeometryProvider *v, QPoint &) const override;

    virtual void paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const override;

    virtual VerticalPosition getPreferredFrameCountPosition() const override {
        return PositionTop;
    }

    virtual PropertyList getProperties() const override;
    virtual QString getPropertyLabel(const PropertyName &) const override;
    virtual QString getPropertyIconName(const PropertyName &) const override;
    virtual PropertyType getPropertyType(const PropertyName &) const override;
    virtual QString getPropertyGroupName(const PropertyName &) const override;
    virtual int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const override;
    virtual QString getPropertyValueLabel(const PropertyName &,
                                          int value) const override;
    virtual RangeMapper *getNewPropertyRangeMapper(const PropertyName &) const override;
    virtual void setProperty(const PropertyName &, int value) override;
    virtual void setProperties(const QXmlAttributes &) override;

    virtual bool setDisplayExtents(double min, double max) override;
    
    virtual bool getXScaleValue(const LayerGeometryProvider *v, int x,
                                double &value, QString &unit) const override;

    virtual bool getYScaleValue(const LayerGeometryProvider *, int y,
                                double &value, QString &unit) const override;

    virtual bool getYScaleDifference(const LayerGeometryProvider *, int y0, int y1,
                                     double &diff, QString &unit) const override;

    virtual bool isLayerScrollable(const LayerGeometryProvider *) const override { return false; }

    void setChannel(int);
    int getChannel() const { return m_channel; }

    void setWindowSize(int);
    int getWindowSize() const { return m_windowSize; }
    
    void setWindowHopLevel(int level);
    int getWindowHopLevel() const { return m_windowHopLevel; }

    void setOversampling(int oversampling);
    int getOversampling() const;

    int getFFTSize() const { return getWindowSize() * getOversampling(); }
    
    void setWindowType(WindowType type);
    WindowType getWindowType() const { return m_windowType; }
    
    void setShowPeaks(bool);
    bool getShowPeaks() const { return m_showPeaks; }

    bool needsTextLabelHeight() const override { return true; }

    virtual void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const override;

    virtual double getFrequencyForX(const LayerGeometryProvider *, double x)
        const override;
    virtual double getXForFrequency(const LayerGeometryProvider *, double freq)
        const override;

protected slots:
    void preferenceChanged(PropertyContainer::PropertyName name);

protected:
    ModelId                 m_originModel; // a DenseTimeValueModel
    int                     m_channel;
    bool                    m_channelSet;
    int                     m_windowSize;
    WindowType              m_windowType;
    int                     m_windowHopLevel;
    int                     m_oversampling;
    bool                    m_showPeaks;
    mutable bool            m_newFFTNeeded;

    double                  m_freqOfMinBin; // used to ensure accurate
                                            // alignment when changing
                                            // fft size

    mutable QMutex m_fftMutex;

    void setupFFT();

    virtual double getBinForFrequency(double freq) const;
    virtual double getFrequencyForBin(double bin) const;
    
    virtual double getXForBin(const LayerGeometryProvider *, double bin)
        const override;
    virtual double getBinForX(const LayerGeometryProvider *, double x)
        const override;

    virtual void getBiasCurve(BiasCurve &) const override;
    BiasCurve m_biasCurve;

    int getWindowIncrement() const {
        if (m_windowHopLevel == 0) return m_windowSize;
        else if (m_windowHopLevel == 1) return (m_windowSize * 3) / 4;
        else return m_windowSize / (1 << (m_windowHopLevel - 1));
    }
};

#endif
