/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2007 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_COLOUR_MAPPER_H
#define SV_COLOUR_MAPPER_H

#include <QObject>
#include <QColor>
#include <QString>
#include <QPixmap>

/**
 * A class for mapping intensity values onto various colour maps.
 */
class ColourMapper
{
public:
    ColourMapper(int map,
                 bool inverted,
                 double minValue,
                 double maxValue);
    ~ColourMapper();

    ColourMapper(const ColourMapper &) =default;
    ColourMapper &operator=(const ColourMapper &) =default;

    enum ColourMap {
        Green,
        Sunset,
        WhiteOnBlack,
        BlackOnWhite,
        Cherry,
        Wasp,
        Ice,
        FruitSalad,
        Banded,
        Highlight,
        Printer,
        HighGain,
        BlueOnBlack,
        Cividis,
        Magma
    };

    int getMap() const { return m_map; }
    bool isInverted() const { return m_inverted; }
    double getMinValue() const { return m_min; }
    double getMaxValue() const { return m_max; }

    /**
     * Return the number of known colour maps.
     */
    static int getColourMapCount();

    /**
     * Return a human-readable label for the colour map with the given
     * index. This may have been subject to translation.
     */
    static QString getColourMapLabel(int n);

    /**
     * Return a machine-readable id string for the colour map with the
     * given index. This is not translated and is intended for use in
     * file I/O.
     */
    static QString getColourMapId(int n);

    /**
     * Return the index for the colour map with the given
     * machine-readable id string, or -1 if the id is not recognised.
     */
    static int getColourMapById(QString id);

    /**
     * Older versions of colour-handling code save and reload colour
     * maps by numerical index and can't properly handle situations in
     * which the index order changes between releases, or new indices
     * are added. So when we save a colour map by id, we should also
     * save a compatibility value that can be re-read by such
     * code. This value is an index into the series of colours used by
     * pre-3.2 SV code, namely (Default/Green, Sunset, WhiteOnBlack,
     * BlackOnWhite, RedOnBlue, YellowOnBlack, BlueOnBlack,
     * FruitSalad, Banded, Highlight, Printer, HighGain). It should
     * represent the closest equivalent to the current colour scheme
     * available in that set. This function returns that index.
     */    
    static int getBackwardCompatibilityColourMap(int n);
    
    /**
     * Map the given value to a colour. The value will be clamped to
     * the range minValue to maxValue (where both are drawn from the
     * constructor arguments).
     */
    QColor map(double value) const;

    /**
     * Return a colour that contrasts somewhat with the colours in the
     * map, so as to be used for cursors etc.
     */
    QColor getContrastingColour() const;

    /**
     * Return true if the colour map is intended to be placed over a
     * light background, false otherwise. This is typically true if
     * the colours corresponding to higher values are darker than
     * those corresponding to lower values.
     */
    bool hasLightBackground() const;

    /**
     * Return a pixmap of the given size containing a preview swatch
     * for the colour map.
     */
    QPixmap getExamplePixmap(QSize size) const;
    
protected:
    int m_map;
    bool m_inverted;
    double m_min;
    double m_max;
};

#endif

