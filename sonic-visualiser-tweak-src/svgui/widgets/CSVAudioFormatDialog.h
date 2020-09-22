/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2018 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_CSV_AUDIO_FORMAT_DIALOG_H
#define SV_CSV_AUDIO_FORMAT_DIALOG_H

#include "data/fileio/CSVFormat.h"

class QTableWidget;
class QComboBox;
class QLabel;
    
#include <QDialog>

class CSVAudioFormatDialog : public QDialog
{
    Q_OBJECT
    
public:
    CSVAudioFormatDialog(QWidget *parent,
                         CSVFormat initialFormat,
                         int maxDisplayCols = 5);
    ~CSVAudioFormatDialog();
    
    CSVFormat getFormat() const;
    
protected slots:
    void sampleRateChanged(QString);
    void sampleRangeChanged(int);
    void columnPurposeChanged(int purpose);

    void updateFormatFromDialog();

protected:
    CSVFormat m_format;
    int m_maxDisplayCols;
    
    QComboBox *m_sampleRateCombo;
    QComboBox *m_sampleRangeCombo;

    QList<QComboBox *> m_columnPurposeCombos;
    int m_fuzzyColumn;
};

#endif
