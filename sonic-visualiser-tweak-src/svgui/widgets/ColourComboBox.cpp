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

#include "ColourComboBox.h"

#include "ColourNameDialog.h"

#include "layer/ColourDatabase.h"

#include "base/Debug.h"

#include <QFontMetrics>
#include <QColorDialog>

#include <iostream>

using namespace std;

ColourComboBox::ColourComboBox(bool withAddNewColourEntry, QWidget *parent) :
    NotifyingComboBox(parent),
    m_withAddNewColourEntry(withAddNewColourEntry)
{
    setEditable(false);
    rebuild();

    connect(this, SIGNAL(activated(int)), this, SLOT(comboActivated(int)));
    connect(ColourDatabase::getInstance(), SIGNAL(colourDatabaseChanged()),
            this, SLOT(rebuild()));

    if (count() < 20 && count() > maxVisibleItems()) {
        setMaxVisibleItems(count());
    }
}

void
ColourComboBox::comboActivated(int index)
{
    if (!m_withAddNewColourEntry ||
        index < int(ColourDatabase::getInstance()->getColourCount())) {
        emit colourChanged(index);
        return;
    }
    
    QColor newColour = QColorDialog::getColor();
    if (!newColour.isValid()) return;

    ColourNameDialog dialog(tr("Name New Colour"),
                            tr("Enter a name for the new colour:"),
                            newColour, newColour.name(), this);
    dialog.showDarkBackgroundCheckbox(tr("Prefer black background for this colour"));
    if (dialog.exec() == QDialog::Accepted) {
        //!!! command
        ColourDatabase *db = ColourDatabase::getInstance();
        int index = db->addColour(newColour, dialog.getColourName());
        db->setUseDarkBackground(index, dialog.isDarkBackgroundChecked());
        // addColour will have called back on rebuild(), and the new
        // colour will be at the index previously occupied by Add New
        // Colour, which is our current index
        emit colourChanged(currentIndex());
    }
}

void
ColourComboBox::rebuild()
{
    blockSignals(true);

    int ix = currentIndex();
    
    clear();

    int size = (QFontMetrics(QFont()).height() * 2) / 3;
    if (size < 12) size = 12;
    
    ColourDatabase *db = ColourDatabase::getInstance();
    for (int i = 0; i < db->getColourCount(); ++i) {
        QString name = db->getColourName(i);
        addItem(db->getExamplePixmap(i, QSize(size, size)), name);
    }

    if (m_withAddNewColourEntry) {
        addItem(tr("Add New Colour..."));
    }

    setCurrentIndex(ix);
    
    blockSignals(false);
}

