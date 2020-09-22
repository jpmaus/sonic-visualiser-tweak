/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_CSV_FORMAT_DIALOG_H
#define SV_CSV_FORMAT_DIALOG_H

#include "data/fileio/CSVFormat.h"

class QTableWidget;
class QComboBox;
class QLabel;
class QFrame;
    
#include <QDialog>

class CSVFormatDialog : public QDialog
{
    Q_OBJECT
    
public:
    CSVFormatDialog(QWidget *parent,
                    CSVFormat initialFormat,
                    int maxDisplayCols);

    CSVFormatDialog(QWidget *parent,
                    QString csvFilePath, // to guess format of
                    sv_samplerate_t referenceSampleRate,
                    int maxDisplayCols);

    ~CSVFormatDialog();

    CSVFormat getFormat() const;
    
protected slots:
    void separatorChanged(QString);
    void timingTypeChanged(int type);
    void sampleRateChanged(QString);
    void windowSizeChanged(QString);
    void columnPurposeChanged(int purpose);

    void updateFormatFromDialog();
    void updateModelLabel();

protected:
    QString m_csvFilePath;
    sv_samplerate_t m_referenceSampleRate;
    CSVFormat m_format;
    int m_maxDisplayCols;
    
    enum TimingOption {
        TimingExplicitSeconds = 0,
        TimingExplicitMsec,
        TimingExplicitSamples,
        TimingImplicit
    };
    std::map<TimingOption, QString> m_timingLabels;
    TimingOption m_initialTimingOption;

    void init();
    void repopulate();
    void columnPurposeChangedForAnnotationType(QComboBox *, int purpose);
    void updateComboVisibility();
    void applyStartTimePurpose();
    void removeStartTimePurpose();

    QFrame *m_exampleFrame;
    int m_exampleFrameRow;
    
    QComboBox *m_separatorCombo;
    QComboBox *m_timingTypeCombo;
    QLabel *m_sampleRateLabel;
    QComboBox *m_sampleRateCombo;
    QLabel *m_windowSizeLabel;
    QComboBox *m_windowSizeCombo;
    QLabel *m_modelLabel;

    QList<QComboBox *> m_columnPurposeCombos;
    int m_fuzzyColumn;
};

#endif
