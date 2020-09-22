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

#ifndef SV_COLOUR_DATABASE_H
#define SV_COLOUR_DATABASE_H

#include <QObject>
#include <QString>
#include <QColor>
#include <QSize>
#include <QPixmap>
#include <vector>

class ColourDatabase : public QObject
{
    Q_OBJECT

public:
    static ColourDatabase *getInstance();

    /**
     * Return the number of colours in the database.
     */
    int getColourCount() const;

    /**
     * Return the name of the colour at index c.
     */
    QString getColourName(int c) const;

    /**
     * Return the colour at index c.
     */
    QColor getColour(int c) const;

    /**
     * Return the colour with the given name, if found in the
     * database. If not found, return Qt::black.
     */
    QColor getColour(QString name) const;

    /**
     * Return the index of the colour with the given name, if found in
     * the database. If not found, return -1.
     */
    int getColourIndex(QString name) const;

    /**
     * Return the index of the given colour, if found in the
     * database. If not found, return -1. Note that it is possible for
     * a colour to appear more than once in the database: names have
     * to be unique in the database, but colours don't. This always
     * returns the first match.
     */
    int getColourIndex(QColor c) const;

    /**
     * Return true if the given colour exists in the database.
     */
    bool haveColour(QColor c) const;

    /**
     * Return the index of the colour in the database that is closest
     * to the given one, by some simplistic measure (Manhattan
     * distance in RGB space). This always returns some valid index,
     * unless the database is empty, in which case it returns -1.
     */
    int getNearbyColourIndex(QColor c) const;

    /**
     * Add a colour to the database, with the associated name. Return
     * the index of the colour in the database. Names are unique
     * within the database: if another colour exists already with the
     * given name, its colour value is replaced with the given
     * one. Colours may appear more than once under different names.
     */
    int addColour(QColor c, QString name);

    /** 
     * Remove the colour with the given name from the database.
     */
    void removeColour(QString);

    /**
     * Return true if the colour at index c is marked as using a dark
     * background. Such colours are presumably "bright" ones, but all
     * this reports is whether the colour has been marked with
     * setUseDarkBackground, not any intrinsic property of the colour.
     */
    bool useDarkBackground(int c) const;
    
    /**
     * Mark the colour at index c as using a dark
     * background. Generally this should be called for "bright"
     * colours.
     */
    void setUseDarkBackground(int c, bool dark);

    /**
     * Return a colour that contrasts with the one at index c,
     * according to some simplistic algorithm. The returned colour is
     * not necessarily in the database; pass it to
     * getNearbyColourIndex if you need one that is.
     */
    QColor getContrastingColour(int c) const;

    // for use in XML export
    void getStringValues(int index,
                         QString &colourName,
                         QString &colourSpec,
                         QString &darkbg) const;

    // for use in XML import
    int putStringValues(QString colourName,
                        QString colourSpec,
                        QString darkbg);

    // for use by PropertyContainer getPropertyRangeAndValue methods
    void getColourPropertyRange(int *min, int *max) const;

    /**
     * Generate a swatch pixmap illustrating the colour at index c.
     */
    QPixmap getExamplePixmap(int c, QSize size) const;
    
signals:
    void colourDatabaseChanged();

protected:
    ColourDatabase();

    struct ColourRec {
        QColor colour;
        QString name;
        bool darkbg;
    };
    
    typedef std::vector<ColourRec> ColourList;
    ColourList m_colours;

    static ColourDatabase m_instance;
};

#endif
