/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2016 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "ColourMapper.h"

#include <iostream>

#include <cmath>

#include "base/Debug.h"

#include <vector>

#include <QPainter>

using namespace std;

static vector<QColor> convertStrings(const vector<QString> &strs,
                                     bool reversed)
{
    vector<QColor> converted;
    for (const auto &s: strs) converted.push_back(QColor(s));
    if (reversed) {
        reverse(converted.begin(), converted.end());
    }
    return converted;
}

static vector<QColor> ice = convertStrings({
        // Based on ColorBrewer ylGnBu
        "#ffffff", "#ffff00", "#f7fcf0", "#e0f3db", "#ccebc5", "#a8ddb5",
        "#7bccc4", "#4eb3d3", "#2b8cbe", "#0868ac", "#084081", "#042040"
    },
    true);

static vector<QColor> cherry = convertStrings({
        "#f7f7f7", "#fddbc7", "#f4a582", "#d6604d", "#b2182b", "#dd3497",
        "#ae017e", "#7a0177", "#49006a"
    },
    true);

static vector<QColor> magma = convertStrings({
        "#FCFFB2", "#FCDF96", "#FBC17D", "#FBA368", "#FA8657", "#F66B4D",
        "#ED504A", "#E03B50", "#C92D59", "#B02363", "#981D69", "#81176D",
        "#6B116F", "#57096E", "#43006A", "#300060", "#1E0848", "#110B2D",
        "#080616", "#000005"
    },
    true);

static vector<QColor> cividis = convertStrings({
        "#00204c", "#00204e", "#002150", "#002251", "#002353", "#002355",
        "#002456", "#002558", "#00265a", "#00265b", "#00275d", "#00285f",
        "#002861", "#002963", "#002a64", "#002a66", "#002b68", "#002c6a",
        "#002d6c", "#002d6d", "#002e6e", "#002e6f", "#002f6f", "#002f6f",
        "#00306f", "#00316f", "#00316f", "#00326e", "#00336e", "#00346e",
        "#00346e", "#01356e", "#06366e", "#0a376d", "#0e376d", "#12386d",
        "#15396d", "#17396d", "#1a3a6c", "#1c3b6c", "#1e3c6c", "#203c6c",
        "#223d6c", "#243e6c", "#263e6c", "#273f6c", "#29406b", "#2b416b",
        "#2c416b", "#2e426b", "#2f436b", "#31446b", "#32446b", "#33456b",
        "#35466b", "#36466b", "#37476b", "#38486b", "#3a496b", "#3b496b",
        "#3c4a6b", "#3d4b6b", "#3e4b6b", "#404c6b", "#414d6b", "#424e6b",
        "#434e6b", "#444f6b", "#45506b", "#46506b", "#47516b", "#48526b",
        "#49536b", "#4a536b", "#4b546b", "#4c556b", "#4d556b", "#4e566b",
        "#4f576c", "#50586c", "#51586c", "#52596c", "#535a6c", "#545a6c",
        "#555b6c", "#565c6c", "#575d6d", "#585d6d", "#595e6d", "#5a5f6d",
        "#5b5f6d", "#5c606d", "#5d616e", "#5e626e", "#5f626e", "#5f636e",
        "#60646e", "#61656f", "#62656f", "#63666f", "#64676f", "#65676f",
        "#666870", "#676970", "#686a70", "#686a70", "#696b71", "#6a6c71",
        "#6b6d71", "#6c6d72", "#6d6e72", "#6e6f72", "#6f6f72", "#6f7073",
        "#707173", "#717273", "#727274", "#737374", "#747475", "#757575",
        "#757575", "#767676", "#777776", "#787876", "#797877", "#7a7977",
        "#7b7a77", "#7b7b78", "#7c7b78", "#7d7c78", "#7e7d78", "#7f7e78",
        "#807e78", "#817f78", "#828078", "#838178", "#848178", "#858278",
        "#868378", "#878478", "#888578", "#898578", "#8a8678", "#8b8778",
        "#8c8878", "#8d8878", "#8e8978", "#8f8a78", "#908b78", "#918c78",
        "#928c78", "#938d78", "#948e78", "#958f78", "#968f77", "#979077",
        "#989177", "#999277", "#9a9377", "#9b9377", "#9c9477", "#9d9577",
        "#9e9676", "#9f9776", "#a09876", "#a19876", "#a29976", "#a39a75",
        "#a49b75", "#a59c75", "#a69c75", "#a79d75", "#a89e74", "#a99f74",
        "#aaa074", "#aba174", "#aca173", "#ada273", "#aea373", "#afa473",
        "#b0a572", "#b1a672", "#b2a672", "#b4a771", "#b5a871", "#b6a971",
        "#b7aa70", "#b8ab70", "#b9ab70", "#baac6f", "#bbad6f", "#bcae6e",
        "#bdaf6e", "#beb06e", "#bfb16d", "#c0b16d", "#c1b26c", "#c2b36c",
        "#c3b46c", "#c5b56b", "#c6b66b", "#c7b76a", "#c8b86a", "#c9b869",
        "#cab969", "#cbba68", "#ccbb68", "#cdbc67", "#cebd67", "#d0be66",
        "#d1bf66", "#d2c065", "#d3c065", "#d4c164", "#d5c263", "#d6c363",
        "#d7c462", "#d8c561", "#d9c661", "#dbc760", "#dcc860", "#ddc95f",
        "#deca5e", "#dfcb5d", "#e0cb5d", "#e1cc5c", "#e3cd5b", "#e4ce5b",
        "#e5cf5a", "#e6d059", "#e7d158", "#e8d257", "#e9d356", "#ebd456",
        "#ecd555", "#edd654", "#eed753", "#efd852", "#f0d951", "#f1da50",
        "#f3db4f", "#f4dc4e", "#f5dd4d", "#f6de4c", "#f7df4b", "#f9e049",
        "#fae048", "#fbe147", "#fce246", "#fde345", "#ffe443", "#ffe542",
        "#ffe642", "#ffe743", "#ffe844", "#ffe945"
    },
    false);

static void
mapDiscrete(double norm, vector<QColor> &colours, double &r, double &g, double &b)
{
    int n = int(colours.size());
    double m = norm * (n-1);
    if (m >= n-1) { colours[n-1].getRgbF(&r, &g, &b, nullptr); return; }
    if (m <= 0) { colours[0].getRgbF(&r, &g, &b, nullptr); return; }
    int base(int(floor(m)));
    double prop0 = (base + 1.0) - m, prop1 = m - base;
    QColor c0(colours[base]), c1(colours[base+1]);
    r = c0.redF() * prop0 + c1.redF() * prop1;
    g = c0.greenF() * prop0 + c1.greenF() * prop1;
    b = c0.blueF() * prop0 + c1.blueF() * prop1;
}

ColourMapper::ColourMapper(int map, bool inverted, double min, double max) :
    m_map(map),
    m_inverted(inverted),
    m_min(min),
    m_max(max)
{
    if (m_min == m_max) {
        SVCERR << "WARNING: ColourMapper: min == max (== " << m_min
                  << "), adjusting" << endl;
        m_max = m_min + 1;
    }
}

ColourMapper::~ColourMapper()
{
}

int
ColourMapper::getColourMapCount()
{
    return 15;
}

QString
ColourMapper::getColourMapLabel(int n)
{
    // When adding a map, be sure to also update getColourMapCount()
    
    if (n >= getColourMapCount()) return QObject::tr("<unknown>");
    ColourMap map = (ColourMap)n;

    switch (map) {
    case Green:            return QObject::tr("Green");
    case WhiteOnBlack:     return QObject::tr("White on Black");
    case BlackOnWhite:     return QObject::tr("Black on White");
    case Cherry:           return QObject::tr("Cherry");
    case Wasp:             return QObject::tr("Wasp");
    case Ice:              return QObject::tr("Ice");
    case Sunset:           return QObject::tr("Sunset");
    case FruitSalad:       return QObject::tr("Fruit Salad");
    case Banded:           return QObject::tr("Banded");
    case Highlight:        return QObject::tr("Highlight");
    case Printer:          return QObject::tr("Printer");
    case HighGain:         return QObject::tr("High Gain");
    case BlueOnBlack:      return QObject::tr("Blue on Black");
    case Cividis:          return QObject::tr("Cividis");
    case Magma:            return QObject::tr("Magma");
    }

    return QObject::tr("<unknown>");
}

QString
ColourMapper::getColourMapId(int n)
{
    if (n >= getColourMapCount()) return "<unknown>";
    ColourMap map = (ColourMap)n;

    switch (map) {
    case Green:            return "Green";
    case WhiteOnBlack:     return "White on Black";
    case BlackOnWhite:     return "Black on White";
    case Cherry:           return "Cherry";
    case Wasp:             return "Wasp";
    case Ice:              return "Ice";
    case Sunset:           return "Sunset";
    case FruitSalad:       return "Fruit Salad";
    case Banded:           return "Banded";
    case Highlight:        return "Highlight";
    case Printer:          return "Printer";
    case HighGain:         return "High Gain";
    case BlueOnBlack:      return "Blue on Black";
    case Cividis:          return "Cividis";
    case Magma:            return "Magma";
    }

    return "<unknown>";
}

int
ColourMapper::getColourMapById(QString id)
{
    ColourMap map = (ColourMap)getColourMapCount();

    if      (id == "Green")            { map = Green; }
    else if (id == "White on Black")   { map = WhiteOnBlack; }
    else if (id == "Black on White")   { map = BlackOnWhite; }
    else if (id == "Cherry")           { map = Cherry; }
    else if (id == "Wasp")             { map = Wasp; }
    else if (id == "Ice")              { map = Ice; }
    else if (id == "Sunset")           { map = Sunset; }
    else if (id == "Fruit Salad")      { map = FruitSalad; }
    else if (id == "Banded")           { map = Banded; }
    else if (id == "Highlight")        { map = Highlight; }
    else if (id == "Printer")          { map = Printer; }
    else if (id == "High Gain")        { map = HighGain; }
    else if (id == "Blue on Black")    { map = BlueOnBlack; }
    else if (id == "Cividis")          { map = Cividis; }
    else if (id == "Magma")            { map = Magma; }

    if (map == (ColourMap)getColourMapCount()) {
        return -1;
    } else {
        return int(map);
    }
}

int
ColourMapper::getBackwardCompatibilityColourMap(int n)
{
    /* Returned value should be an index into the series
     * (Default/Green, Sunset, WhiteOnBlack, BlackOnWhite, RedOnBlue,
     * YellowOnBlack, BlueOnBlack, FruitSalad, Banded, Highlight,
     * Printer, HighGain). Minimum 0, maximum 11.
     */
        
    if (n >= getColourMapCount()) return 0;
    ColourMap map = (ColourMap)n;

    switch (map) {
    case Green:            return 0;
    case WhiteOnBlack:     return 2;
    case BlackOnWhite:     return 3;
    case Cherry:           return 4;
    case Wasp:             return 5;
    case Ice:              return 6;
    case Sunset:           return 1;
    case FruitSalad:       return 7;
    case Banded:           return 8;
    case Highlight:        return 9;
    case Printer:          return 10;
    case HighGain:         return 11;
    case BlueOnBlack:      return 6;
    case Cividis:          return 6;
    case Magma:            return 1;
    }

    return 0;
}

QColor
ColourMapper::map(double value) const
{
    double norm = (value - m_min) / (m_max - m_min);
    if (norm < 0.0) norm = 0.0;
    if (norm > 1.0) norm = 1.0;

    if (m_inverted) {
        norm = 1.0 - norm;
    }
    
    double h = 0.0, s = 0.0, v = 0.0, r = 0.0, g = 0.0, b = 0.0;
    bool hsv = true;

    double blue = 0.6666, pieslice = 0.3333;

    if (m_map >= getColourMapCount()) return Qt::black;
    ColourMap map = (ColourMap)m_map;

    switch (map) {

    case Green:
        h = blue - norm * 2.0 * pieslice;
        s = 0.5f + norm/2.0;
        v = norm;
        break;

    case WhiteOnBlack:
        r = g = b = norm;
        hsv = false;
        break;

    case BlackOnWhite:
        r = g = b = 1.0 - norm;
        hsv = false;
        break;

    case Cherry:
        hsv = false;
        mapDiscrete(norm, cherry, r, g, b);
        break;

    case Wasp:
        h = 0.15;
        s = 1.0;
        v = norm;
        break;
        
    case BlueOnBlack:
        h = blue;
        s = 1.0;
        v = norm * 2.0;
        if (v > 1.0) {
            v = 1.0;
            s = 1.0 - (sqrt(norm) - 0.707) * 3.413;
            if (s < 0.0) s = 0.0;
            if (s > 1.0) s = 1.0;
        }
        break;

    case Sunset:
        r = (norm - 0.24) * 2.38;
        if (r > 1.0) r = 1.0;
        if (r < 0.0) r = 0.0;
        g = (norm - 0.64) * 2.777;
        if (g > 1.0) g = 1.0;
        if (g < 0.0) g = 0.0;
        b = (3.6f * norm);
        if (norm > 0.277) b = 2.0 - b;
        if (b > 1.0) b = 1.0;
        if (b < 0.0) b = 0.0;
        hsv = false;
        break;

    case FruitSalad:
        h = blue + (pieslice/6.0) - norm;
        if (h < 0.0) h += 1.0;
        s = 1.0;
        v = 1.0;
        break;

    case Banded:
        if      (norm < 0.125) return Qt::darkGreen;
        else if (norm < 0.25)  return Qt::green;
        else if (norm < 0.375) return Qt::darkBlue;
        else if (norm < 0.5)   return Qt::blue;
        else if (norm < 0.625) return Qt::darkYellow;
        else if (norm < 0.75)  return Qt::yellow;
        else if (norm < 0.875) return Qt::darkRed;
        else                   return Qt::red;
        break;

    case Highlight:
        if (norm > 0.99) return Qt::white;
        else return Qt::darkBlue;

    case Printer:
        if (norm > 0.8) {
            r = 1.0;
        } else if (norm > 0.7) {
            r = 0.9;
        } else if (norm > 0.6) {
            r = 0.8;
        } else if (norm > 0.5) {
            r = 0.7;
        } else if (norm > 0.4) {
            r = 0.6;
        } else if (norm > 0.3) {
            r = 0.5;
        } else if (norm > 0.2) {
            r = 0.4;
        } else {
            r = 0.0;
        }
        r = g = b = 1.0 - r;
        hsv = false;
        break;

    case HighGain:
        if (norm <= 1.0 / 256.0) {
            norm = 0.0;
        } else {
            norm = 0.1f + (pow(((norm - 0.5) * 2.0), 3.0) + 1.0) / 2.081;
        }
        // now as for Sunset
        r = (norm - 0.24) * 2.38;
        if (r > 1.0) r = 1.0;
        if (r < 0.0) r = 0.0;
        g = (norm - 0.64) * 2.777;
        if (g > 1.0) g = 1.0;
        if (g < 0.0) g = 0.0;
        b = (3.6f * norm);
        if (norm > 0.277) b = 2.0 - b;
        if (b > 1.0) b = 1.0;
        if (b < 0.0) b = 0.0;
        hsv = false;
/*
        if (r > 1.0) r = 1.0;
        r = g = b = 1.0 - r;
        hsv = false;
*/
        break;

    case Ice:
        hsv = false;
        mapDiscrete(norm, ice, r, g, b);
        break;

    case Cividis:
        hsv = false;
        mapDiscrete(norm, cividis, r, g, b);
        break;

    case Magma:
        hsv = false;
        mapDiscrete(norm, magma, r, g, b);
        break;
    }

    if (hsv) {
        return QColor::fromHsvF(h, s, v);
    } else {
        return QColor::fromRgbF(r, g, b);
    }
}

QColor
ColourMapper::getContrastingColour() const
{
    if (m_map >= getColourMapCount()) return Qt::white;
    ColourMap map = (ColourMap)m_map;

    switch (map) {

    case Green:
        return QColor(255, 150, 50);

    case WhiteOnBlack:
        return Qt::red;

    case BlackOnWhite:
        return Qt::darkGreen;

    case Cherry:
        return Qt::green;

    case Wasp:
        return QColor::fromHsv(240, 255, 255);

    case Ice:
        return Qt::red;

    case Sunset:
        return Qt::white;

    case FruitSalad:
        return Qt::white;

    case Banded:
        return Qt::cyan;

    case Highlight:
        return Qt::red;

    case Printer:
        return Qt::red;

    case HighGain:
        return Qt::red;

    case BlueOnBlack:
        return Qt::red;

    case Cividis:
        return Qt::white;

    case Magma:
        return Qt::white;
    }

    return Qt::white;
}

bool
ColourMapper::hasLightBackground() const
{
    if (m_map >= getColourMapCount()) return false;
    ColourMap map = (ColourMap)m_map;

    switch (map) {

    case BlackOnWhite:
    case Printer:
    case HighGain:
        return true;

    case Green:
    case Sunset:
    case WhiteOnBlack:
    case Cherry:
    case Wasp:
    case Ice:
    case FruitSalad:
    case Banded:
    case Highlight:
    case BlueOnBlack:
    case Cividis:
    case Magma:
        
    default:
        return false;
    }
}

QPixmap
ColourMapper::getExamplePixmap(QSize size) const
{
    QPixmap pmap(size);
    pmap.fill(Qt::white);
    QPainter paint(&pmap);

    int w = size.width(), h = size.height();
    
    int margin = 2;
    if (w < 4 || h < 4) margin = 0;
    else if (w < 8 || h < 8) margin = 1;

    int n = w - margin*2;
    
    for (int x = 0; x < n; ++x) {
        double value = m_min + ((m_max - m_min) * x) / (n-1);
        QColor colour(map(value));
        paint.setPen(colour);
        paint.drawLine(x + margin, margin, x + margin, h - margin);
    }
    
    return pmap;
}


