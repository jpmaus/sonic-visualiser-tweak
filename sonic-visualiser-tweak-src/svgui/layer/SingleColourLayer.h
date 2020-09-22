/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_SINGLE_COLOUR_LAYER_H
#define SV_SINGLE_COLOUR_LAYER_H

#include "Layer.h"
#include <QColor>
#include <vector>
#include <map>

class SingleColourLayer : public Layer
{
    Q_OBJECT
    
public:
    /**
     * Set the colour used to draw primary items in the layer. The
     * colour value is a colour database index as returned by
     * ColourDatabase::getColourIndex().
     */
    virtual void setBaseColour(int);

    /**
     * Retrieve the current primary drawing colour, as a
     * ColourDatabase index value.
     */
    virtual int getBaseColour() const;

    /**
     * Return true if the layer currently has a dark colour on a light
     * background, false if it has a light colour on a dark
     * background.
     */
    bool hasLightBackground() const override;

    /**
     * Implements Layer::getLayerColourSignificance()
     */
    ColourSignificance getLayerColourSignificance() const override {
        return ColourDistinguishes;
    }

    QPixmap getLayerPresentationPixmap(QSize size) const override;

    PropertyList getProperties() const override;
    QString getPropertyLabel(const PropertyName &) const override;
    PropertyType getPropertyType(const PropertyName &) const override;
    QString getPropertyGroupName(const PropertyName &) const override;
    int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const override;
    QString getPropertyValueLabel(const PropertyName &,
                                          int value) const override;
    RangeMapper *getNewPropertyRangeMapper(const PropertyName &) const override;
    void setProperty(const PropertyName &, int value) override;

    void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const override;

    void setProperties(const QXmlAttributes &attributes) override;

    virtual void setDefaultColourFor(LayerGeometryProvider *v);

protected:
    SingleColourLayer();
    virtual ~SingleColourLayer();

    virtual QColor getBaseQColor() const;
    virtual QColor getBackgroundQColor(LayerGeometryProvider *v) const;
    virtual QColor getForegroundQColor(LayerGeometryProvider *v) const;
    std::vector<QColor> getPartialShades(LayerGeometryProvider *v) const;

    virtual void flagBaseColourChanged() { }
    virtual int getDefaultColourHint(bool /* darkBackground */,
                                     bool & /* impose */) { return -1; }

    typedef std::map<int, int> ColourRefCount;
    static ColourRefCount m_colourRefCount;

    int m_colour;
    bool m_colourExplicitlySet;
    bool m_defaultColourSet;

private:
    void refColor();
    void unrefColor();
};

#endif
