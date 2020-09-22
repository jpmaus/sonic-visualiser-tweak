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

#ifndef COLOUR_3D_PLOT_LAYER_H
#define COLOUR_3D_PLOT_LAYER_H

#include "SliceableLayer.h"
#include "VerticalBinLayer.h"

#include "ColourScale.h"
#include "Colour3DPlotRenderer.h"

#include "data/model/DenseThreeDimensionalModel.h"

class View;
class QPainter;
class QImage;

/**
 * This is a view that displays dense 3-D data (time, some sort of
 * binned y-axis range, value) as a colour plot with value mapped to
 * colour range.  Its source is a DenseThreeDimensionalModel.
 *
 * This was the original implementation for the spectrogram view, but
 * it was replaced for that purpose with a more efficient
 * implementation that derived the spectrogram itself from a
 * DenseTimeValueModel instead of using a three-dimensional model.
 */
class Colour3DPlotLayer : public VerticalBinLayer
{
    Q_OBJECT

public:
    Colour3DPlotLayer();
    ~Colour3DPlotLayer();

    ModelId getModel() const override { return m_model; }

    const ZoomConstraint *getZoomConstraint() const override;
    
    void paint(LayerGeometryProvider *v,
               QPainter &paint, QRect rect) const override;
    void setSynchronousPainting(bool synchronous) override;

    int getVerticalScaleWidth(LayerGeometryProvider *v,
                              bool, QPainter &) const override;
    void paintVerticalScale(LayerGeometryProvider *v,
                            bool, QPainter &paint, QRect rect) const override;

    QString getFeatureDescription(LayerGeometryProvider *v,
                                  QPoint &) const override;

    bool snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame, 
                            int &resolution,
                            SnapType snap, int ycoord) const override;

    void setLayerDormant(const LayerGeometryProvider *v,
                         bool dormant) override;

    bool isLayerScrollable(const LayerGeometryProvider *v) const override;

    ColourSignificance getLayerColourSignificance() const override {
        return ColourHasMeaningfulValue;
    }

    void setModel(ModelId model); // a DenseThreeDimensionalModel

    int getCompletion(LayerGeometryProvider *) const override;

    PropertyList getProperties() const override;
    PropertyType getPropertyType(const PropertyName &) const override;
    QString getPropertyLabel(const PropertyName &) const override;
    QString getPropertyIconName(const PropertyName &) const override;
    QString getPropertyGroupName(const PropertyName &) const override;
    int getPropertyRangeAndValue(const PropertyName &,
                                 int *min, int *max, int *deflt) const override;
    QString getPropertyValueLabel(const PropertyName &,
                                  int value) const override;
    QString getPropertyValueIconName(const PropertyName &,
                                     int value) const override;
    RangeMapper *getNewPropertyRangeMapper(const PropertyName &) const override;
    void setProperty(const PropertyName &, int value) override;
    void setProperties(const QXmlAttributes &) override;
    
    void setColourScale(ColourScaleType);
    ColourScaleType getColourScale() const { return m_colourScale; }

    void setColourMap(int map);
    int getColourMap() const;

    /**
     * Set the gain multiplier for sample values in this view.
     * The default is 1.0.
     */
    void setGain(float gain);
    float getGain() const;
    
    /**
     * Specify the scale for the y axis.
     */
    void setBinScale(BinScale);
    BinScale getBinScale() const;

    /**
     * Specify the normalization mode for individual columns.
     */
    void setNormalization(ColumnNormalization);
    ColumnNormalization getNormalization() const;

    /**
     * Specify whether to normalize the visible area.
     */
    void setNormalizeVisibleArea(bool);
    bool getNormalizeVisibleArea() const;

    void setInvertVertical(bool i);
    bool getInvertVertical() const;

    void setOpaque(bool i);
    bool getOpaque() const;

    void setSmooth(bool i);
    bool getSmooth() const;

    bool hasLightBackground() const override;

    bool getValueExtents(double &min, double &max,
                         bool &logarithmic, QString &unit) const override;

    bool getDisplayExtents(double &min, double &max) const override;
    bool setDisplayExtents(double min, double max) override;

    bool getYScaleValue(const LayerGeometryProvider *, int /* y */,
                        double &/* value */, QString &/* unit */) const override;

    int getVerticalZoomSteps(int &defaultStep) const override;
    int getCurrentVerticalZoomStep() const override;
    void setVerticalZoomStep(int) override;
    RangeMapper *getNewVerticalZoomRangeMapper() const override;

    ModelId getSliceableModel() const override { return m_model; }

    void toXml(QTextStream &stream, QString indent = "",
               QString extraAttributes = "") const override;

protected slots:
    void handleModelChanged(ModelId);
    void handleModelChangedWithin(ModelId, sv_frame_t, sv_frame_t);

protected:
    ModelId m_model; // A DenseThreeDimensionalModel
    
    ColourScaleType m_colourScale;
    bool m_colourScaleSet;
    int m_colourMap;
    bool m_colourInverted;
    float m_gain;
    BinScale m_binScale;
    ColumnNormalization m_normalization; // of individual columns
    bool m_normalizeVisibleArea;
    bool m_invertVertical;
    bool m_opaque;
    bool m_smooth;
    int m_peakResolution;

    // Minimum and maximum bin numbers visible within the view. We
    // always snap to whole bins at view edges.
    int m_miny;
    int m_maxy;

    bool m_synchronous;

    static ColourScaleType convertToColourScale(int value);
    static int convertFromColourScale(ColourScaleType);
    static std::pair<ColumnNormalization, bool> convertToColumnNorm(int value);
    static int convertFromColumnNorm(ColumnNormalization norm, bool visible);

    mutable ModelId m_peakCache;
    const int m_peakCacheDivisor;
    void invalidatePeakCache();
    ModelId getPeakCache() const;

    typedef std::map<int, MagnitudeRange> ViewMagMap; // key is view id
    mutable ViewMagMap m_viewMags;
    mutable ViewMagMap m_lastRenderedMags; // when in normalizeVisibleArea mode
    void invalidateMagnitudes();

    typedef std::map<int, Colour3DPlotRenderer *> ViewRendererMap; // key is view id
    mutable ViewRendererMap m_renderers;
    
    Colour3DPlotRenderer *getRenderer(const LayerGeometryProvider *) const;
    void invalidateRenderers();
        
    /**
     * Return the y coordinate at which the given bin "starts"
     * (i.e. at the bottom of the bin, if the given bin is an integer
     * and the vertical scale is the usual way up). Bin number may be
     * fractional, to obtain a position part-way through a bin.
     */
    double getYForBin(const LayerGeometryProvider *, double bin) const override;
    
    /**
     * Return the bin number, possibly fractional, at the given y
     * coordinate. Note that the whole numbers occur at the positions
     * at which the bins "start" (i.e. the bottom of the visible bin,
     * if the vertical scale is the usual way up).
     */
    double getBinForY(const LayerGeometryProvider *, double y) const override;

    int getColourScaleWidth(QPainter &) const;

    void paintWithRenderer(LayerGeometryProvider *v, QPainter &paint, QRect rect) const;
};

#endif
