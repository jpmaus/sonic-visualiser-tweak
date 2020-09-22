/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007-2016 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_COLOUR_COMBO_BOX_H
#define SV_COLOUR_COMBO_BOX_H

#include "NotifyingComboBox.h"

/**
 * Colour-picker combo box with swatches, optionally including "Add
 * New Colour..." entry to invoke a QColorDialog/ColourNameDialog
 */
class ColourComboBox : public NotifyingComboBox
{
    Q_OBJECT

public:
    ColourComboBox(bool withAddNewColourEntry, QWidget *parent = 0);

signals:
    void colourChanged(int colourIndex);

private slots:
    void rebuild();
    void comboActivated(int);
    
private:
    bool m_withAddNewColourEntry;
};

#endif

