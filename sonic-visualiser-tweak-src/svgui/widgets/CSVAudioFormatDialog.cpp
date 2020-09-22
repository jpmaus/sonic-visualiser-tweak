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

#include "CSVAudioFormatDialog.h"

#include "layer/LayerFactory.h"

#include "TextAbbrev.h"

#include <QFrame>
#include <QGridLayout>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QComboBox>
#include <QLabel>
#include <QDialogButtonBox>

#include <iostream>
#include <cmath>

#include "base/Debug.h"

CSVAudioFormatDialog::CSVAudioFormatDialog(QWidget *parent, CSVFormat format,
                                           int maxDisplayCols) :
    QDialog(parent),
    m_format(format),
    m_maxDisplayCols(maxDisplayCols),
    m_fuzzyColumn(-1)
{
    setModal(true);
    setWindowTitle(tr("Select Audio Data Format"));

    QGridLayout *layout = new QGridLayout;

    int row = 0;

    layout->addWidget
        (new QLabel(tr("Please select the correct data format for this file.")),
         row++, 0, 1, 4);

    QFrame *exampleFrame = new QFrame;
    exampleFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    exampleFrame->setLineWidth(2);
    QGridLayout *exampleLayout = new QGridLayout;
    exampleLayout->setSpacing(4);
    exampleFrame->setLayout(exampleLayout);

    QPalette palette = exampleFrame->palette();
    palette.setColor(QPalette::Window, palette.color(QPalette::Base));
    exampleFrame->setPalette(palette);

    QFont fp;
    fp.setPointSize(int(floor(fp.pointSize() * 0.9)));
    
    int columns = format.getColumnCount();
    QList<QStringList> example = m_format.getExample();

    for (int i = 0; i < columns; ++i) {

        QComboBox *cpc = new QComboBox;
        m_columnPurposeCombos.push_back(cpc);
        exampleLayout->addWidget(cpc, 0, i);
        connect(cpc, SIGNAL(activated(int)), this, SLOT(columnPurposeChanged(int)));
        
        if (i == m_maxDisplayCols && columns > i + 2) {
            m_fuzzyColumn = i;

            cpc->addItem(tr("<ignore>"));
            cpc->addItem(tr("Audio channels"));
            cpc->setCurrentIndex
                (m_format.getColumnPurpose(i-1) == CSVFormat::ColumnValue ?
                 1 : 0);

            exampleLayout->addWidget
                (new QLabel(tr("(%1 more)").arg(columns - i)), 1, i);
            break;
        }

        cpc->addItem(tr("<ignore>"));
        cpc->addItem(tr("Audio channel"));
        cpc->setCurrentIndex
            (m_format.getColumnPurpose(i) == CSVFormat::ColumnValue ? 1 : 0);
        
        for (int j = 0; j < example.size() && j < 6; ++j) {
            if (i >= example[j].size()) {
                continue;
            }
            QLabel *label = new QLabel;
            label->setTextFormat(Qt::PlainText);
            QString text = TextAbbrev::abbreviate(example[j][i], 35);
            label->setText(text);
            label->setFont(fp);
            label->setPalette(palette);
            label->setIndent(8);
            exampleLayout->addWidget(label, j+1, i);
        }
    }

    layout->addWidget(exampleFrame, row, 0, 1, 4);
    layout->setColumnStretch(3, 10);
    layout->setRowStretch(row++, 10);

    layout->addWidget(new QLabel(tr("Audio sample rate (Hz):")), row, 0);
    
    int sampleRates[] = {
        8000, 11025, 12000, 22050, 24000, 32000,
        44100, 48000, 88200, 96000, 176400, 192000
    };

    m_sampleRateCombo = new QComboBox;
    for (int i = 0; i < int(sizeof(sampleRates) / sizeof(sampleRates[0])); ++i) {
        m_sampleRateCombo->addItem(QString("%1").arg(sampleRates[i]));
        if (sampleRates[i] == m_format.getSampleRate()) {
            m_sampleRateCombo->setCurrentIndex(i);
        }
    }
    m_sampleRateCombo->setEditable(true);

    layout->addWidget(m_sampleRateCombo, row++, 1);
    connect(m_sampleRateCombo, SIGNAL(activated(QString)),
            this, SLOT(sampleRateChanged(QString)));
    connect(m_sampleRateCombo, SIGNAL(editTextChanged(QString)),
            this, SLOT(sampleRateChanged(QString)));
    
    layout->addWidget(new QLabel(tr("Sample values are:")), row, 0);
    
    m_sampleRangeCombo = new QComboBox;
    // NB must be in the same order as the CSVFormat::AudioSampleRange enum
    m_sampleRangeCombo->addItem(tr("Floating-point in range -1 to 1"));
    m_sampleRangeCombo->addItem(tr("8-bit in range 0 to 255"));
    m_sampleRangeCombo->addItem(tr("16-bit in range -32768 to 32767"));
    m_sampleRangeCombo->addItem(tr("Unknown range: normalise on load"));
    m_sampleRangeCombo->setCurrentIndex(int(m_format.getAudioSampleRange()));

    layout->addWidget(m_sampleRangeCombo, row++, 1);
    connect(m_sampleRangeCombo, SIGNAL(activated(int)),
            this, SLOT(sampleRangeChanged(int)));
    
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok |
                                                QDialogButtonBox::Cancel);
    layout->addWidget(bb, row++, 0, 1, 4);
    connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));

    setLayout(layout);

    updateFormatFromDialog();
}

CSVAudioFormatDialog::~CSVAudioFormatDialog()
{
}

CSVFormat
CSVAudioFormatDialog::getFormat() const
{
    return m_format;
}

void
CSVAudioFormatDialog::sampleRateChanged(QString rateString)
{
    bool ok = false;
    int sampleRate = rateString.toInt(&ok);
    if (ok) m_format.setSampleRate(sampleRate);
}

void
CSVAudioFormatDialog::sampleRangeChanged(int range)
{
    m_format.setAudioSampleRange((CSVFormat::AudioSampleRange)range);
}

void
CSVAudioFormatDialog::columnPurposeChanged(int)
{
    updateFormatFromDialog();
}
    
void
CSVAudioFormatDialog::updateFormatFromDialog()
{
    m_format.setModelType(CSVFormat::WaveFileModel);
    m_format.setTimingType(CSVFormat::ImplicitTiming);
    m_format.setTimeUnits(CSVFormat::TimeAudioFrames);
    
    for (int i = 0; i < m_columnPurposeCombos.size(); ++i) {

        QComboBox *thisCombo = m_columnPurposeCombos[i];
        
        CSVFormat::ColumnPurpose purpose = (thisCombo->currentIndex() == 1 ?
                                            CSVFormat::ColumnValue :
                                            CSVFormat::ColumnUnknown);
        
        if (i == m_fuzzyColumn) {
            for (int j = i; j < m_format.getColumnCount(); ++j) {
                m_format.setColumnPurpose(j, purpose);
            }
        } else {
            m_format.setColumnPurpose(i, purpose);
        }
    }
}



