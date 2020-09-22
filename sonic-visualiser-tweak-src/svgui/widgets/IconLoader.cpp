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

#include "IconLoader.h"

#include <QPixmap>
#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QFile>
#include <QSvgRenderer>
#include <QSettings>

#include <vector>
#include <set>

#include "base/Debug.h"

using namespace std;

static set<QString> autoInvertExceptions {
    // These are the icons that look OK in their default colours, even
    // in a colour scheme with a black background.  (They may also be
    // icons that would look worse if we tried to auto-invert them.)
    // If we have icons that look bad when auto-inverted but that are
    // not suitable for use without being inverted, we'll need to
    // supply inverted versions -- the loader will load xx_inverse.png
    // in preference to xx.png if a dark background is found.)
    "fileclose",
    "filenew",
    "fileopen",
    "fileopenaudio",
    "fileopensession",
    "filesave",
    "filesaveas",
    "filesaveas-sv",
    "help",
    "editcut",
    "editcopy",
    "editpaste",
    "editdelete",
    "exit",
    "zoom-fit",
    "zoom-in",
    "zoom-out",
    "zoom"
};

static vector<int> sizes { 0, 16, 22, 24, 32, 48, 64, 128 };

QIcon
IconLoader::load(QString name)
{
    QIcon icon;
    for (int sz: sizes) {
        QPixmap pmap(loadPixmap(name, sz));
        if (!pmap.isNull()) icon.addPixmap(pmap);
    }
    return icon;
}

bool
IconLoader::shouldInvert() const
{
    QSettings settings;
    settings.beginGroup("IconLoader");
    if (!settings.value("invert-icons-on-dark-background", true).toBool()) {
        return false;
    }
    QColor bg = QApplication::palette().window().color();
    bool darkBackground = (bg.red() + bg.green() + bg.blue() <= 384);
    return darkBackground;
}

bool
IconLoader::shouldAutoInvert(QString name) const
{
    if (shouldInvert()) {
        return (autoInvertExceptions.find(name) == autoInvertExceptions.end());
    } else {
        return false;
    }
}

QPixmap
IconLoader::loadPixmap(QString name, int size)
{
    bool invert = shouldInvert();

    QString scalableName, nonScalableName;
    QPixmap pmap;

    // attempt to load a pixmap with the right size and inversion
    nonScalableName = makeNonScalableFilename(name, size, invert);
    pmap = QPixmap(nonScalableName);

    if (pmap.isNull() && size > 0) {
        // if that failed, load a scalable vector with the right
        // inversion and scale it
        scalableName = makeScalableFilename(name, invert);
        pmap = loadScalable(scalableName, size);
    }

    if (pmap.isNull() && invert) {
        // if that failed, and we were asking for an inverted pixmap,
        // that may mean we don't have an inverted version of it. We
        // could either auto-invert or use the uninverted version
        nonScalableName = makeNonScalableFilename(name, size, false);
        pmap = QPixmap(nonScalableName);

        if (pmap.isNull() && size > 0) {
            scalableName = makeScalableFilename(name, false);
            pmap = loadScalable(scalableName, size);
        }
        
        if (!pmap.isNull() && shouldAutoInvert(name)) {
            pmap = invertPixmap(pmap);
        }
    }

    return pmap;
}

QPixmap
IconLoader::loadScalable(QString name, int size)
{
    if (!QFile(name).exists()) {
//        cerr << "loadScalable: no such file as: \"" << name << "\"" << endl;
        return QPixmap();
    }
    QPixmap pmap(size, size);
    pmap.fill(Qt::transparent);
    QSvgRenderer renderer(name);
    QPainter painter;
    painter.begin(&pmap);
//    cerr << "calling renderer for " << name << " at size " << size << "..." << endl;
    renderer.render(&painter);
//    cerr << "renderer completed" << endl;
    painter.end();
    return pmap;
}

QString
IconLoader::makeNonScalableFilename(QString name, int size, bool invert)
{
    if (invert) {
        if (size == 0) {
            return QString(":icons/%1_inverse.png").arg(name);
        } else {
            return QString(":icons/%1-%2_inverse.png").arg(name).arg(size);
        }
    } else {
        if (size == 0) {
            return QString(":icons/%1.png").arg(name);
        } else {
            return QString(":icons/%1-%2.png").arg(name).arg(size);
        }
    }
}

QString
IconLoader::makeScalableFilename(QString name, bool invert)
{
    if (invert) {
        return QString(":icons/scalable/%1_inverse.svg").arg(name);
    } else {
        return QString(":icons/scalable/%1.svg").arg(name);
    }
}

QPixmap
IconLoader::invertPixmap(QPixmap pmap)
{
    // No suitable inverted icon found for black background; try to
    // auto-invert the default one

    QImage img = pmap.toImage().convertToFormat(QImage::Format_ARGB32);

    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {

            QRgb rgba = img.pixel(x, y);
            QColor colour = QColor
                (qRed(rgba), qGreen(rgba), qBlue(rgba), qAlpha(rgba));

            int alpha = colour.alpha();
            if (colour.saturation() < 5 && colour.alpha() > 10) {
                colour.setHsv(colour.hue(),
                              colour.saturation(),
                              255 - colour.value());
                colour.setAlpha(alpha);
                img.setPixel(x, y, colour.rgba());
            }
        }
    }

    pmap = QPixmap::fromImage(img);
    return pmap;
}

