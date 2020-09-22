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

#include "CSVFormatDialog.h"

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

CSVFormatDialog::CSVFormatDialog(QWidget *parent,
                                 CSVFormat format,
                                 int maxDisplayCols) :
    QDialog(parent),
    m_csvFilePath(""),
    m_referenceSampleRate(0),
    m_format(format),
    m_maxDisplayCols(maxDisplayCols),
    m_fuzzyColumn(-1)
{
    init();
}

CSVFormatDialog::CSVFormatDialog(QWidget *parent,
                                 QString csvFilePath,
                                 sv_samplerate_t referenceSampleRate,
                                 int maxDisplayCols) :
    QDialog(parent),
    m_csvFilePath(csvFilePath),
    m_referenceSampleRate(referenceSampleRate),
    m_maxDisplayCols(maxDisplayCols),
    m_fuzzyColumn(-1)
{
    m_format = CSVFormat(csvFilePath);
    m_format.setSampleRate(referenceSampleRate);
    init();
}

CSVFormatDialog::~CSVFormatDialog()
{
}
    
static int sampleRates[] = {
    8000, 11025, 12000, 22050, 24000, 32000,
    44100, 48000, 88200, 96000, 176400, 192000
};

void
CSVFormatDialog::init()
{
    setModal(true);
    setWindowTitle(tr("Select Data Format"));
    
    QGridLayout *layout = new QGridLayout;

    int row = 0;

    layout->addWidget
        (new QLabel(tr("Please select the correct data format for this file.")),
         row++, 0, 1, 4);

    m_exampleFrame = nullptr;
    m_exampleFrameRow = row++;

    std::set<QChar> plausible = m_format.getPlausibleSeparators();
    SVDEBUG << "Have " << plausible.size() << " plausible separator(s)" << endl;

    if (m_csvFilePath != "" && plausible.size() > 1) {
        // can only update when separator changed if we still have a
        // file to refer to
        layout->addWidget(new QLabel(tr("Column separator:")), row, 0);
        m_separatorCombo = new QComboBox;
        for (QChar c: plausible) {
            m_separatorCombo->addItem(QString(c));
            if (c == m_format.getSeparator()) {
                m_separatorCombo->setCurrentIndex(m_separatorCombo->count()-1);
            }
        }
        m_separatorCombo->setEditable(false);

        layout->addWidget(m_separatorCombo, row++, 1);
        connect(m_separatorCombo, SIGNAL(activated(QString)),
                this, SLOT(separatorChanged(QString)));
    }

    layout->addWidget(new QLabel(tr("Timing is specified:")), row, 0);
    
    m_timingTypeCombo = new QComboBox;

    m_timingLabels = {
        { TimingExplicitSeconds, tr("Explicitly, in seconds") },
        { TimingExplicitMsec, tr("Explicitly, in milliseconds") },
        { TimingExplicitSamples, tr("Explicitly, in audio sample frames") },
        { TimingImplicit, tr("Implicitly: rows are equally spaced in time") }
    };

    for (auto &l: m_timingLabels) {
        m_timingTypeCombo->addItem(l.second);
    }
    
    layout->addWidget(m_timingTypeCombo, row++, 1, 1, 2);
    
    connect(m_timingTypeCombo, SIGNAL(activated(int)),
            this, SLOT(timingTypeChanged(int)));
    
    m_sampleRateLabel = new QLabel(tr("Audio sample rate (Hz):"));
    layout->addWidget(m_sampleRateLabel, row, 0);

    m_sampleRateCombo = new QComboBox;
    for (int i = 0; i < int(sizeof(sampleRates) / sizeof(sampleRates[0])); ++i) {
        m_sampleRateCombo->addItem(QString("%1").arg(sampleRates[i]));
    }
    m_sampleRateCombo->setEditable(true);

    layout->addWidget(m_sampleRateCombo, row++, 1);
    connect(m_sampleRateCombo, SIGNAL(activated(QString)),
            this, SLOT(sampleRateChanged(QString)));
    connect(m_sampleRateCombo, SIGNAL(editTextChanged(QString)),
            this, SLOT(sampleRateChanged(QString)));

    m_windowSizeLabel = new QLabel(tr("Frame increment between rows:"));
    layout->addWidget(m_windowSizeLabel, row, 0);
    
    m_windowSizeCombo = new QComboBox;
    for (int i = 0; i <= 16; ++i) {
        int value = 1 << i;
        m_windowSizeCombo->addItem(QString("%1").arg(value));
    }
    m_windowSizeCombo->setEditable(true);
    
    layout->addWidget(m_windowSizeCombo, row++, 1);
    connect(m_windowSizeCombo, SIGNAL(activated(QString)),
            this, SLOT(windowSizeChanged(QString)));
    connect(m_windowSizeCombo, SIGNAL(editTextChanged(QString)),
            this, SLOT(windowSizeChanged(QString)));

    m_modelLabel = new QLabel;
    QFont f(m_modelLabel->font());
    f.setItalic(true);
    m_modelLabel->setFont(f);
    layout->addWidget(m_modelLabel, row++, 0, 1, 4);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok |
                                                QDialogButtonBox::Cancel);
    layout->addWidget(bb, row++, 0, 1, 4);
    connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));

    setLayout(layout);

    repopulate();
}

void
CSVFormatDialog::repopulate()
{
    SVCERR << "CSVFormatDialog::repopulate()" << endl;
    
    QGridLayout *layout = qobject_cast<QGridLayout *>(this->layout());

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
    
    int columns = m_format.getColumnCount();
    QList<QStringList> example = m_format.getExample();

    m_columnPurposeCombos.clear();
    
    for (int i = 0; i < columns; ++i) {

        QComboBox *cpc = new QComboBox;
        m_columnPurposeCombos.push_back(cpc);
        exampleLayout->addWidget(cpc, 0, i);
        connect(cpc, SIGNAL(activated(int)), this, SLOT(columnPurposeChanged(int)));
        
        if (i == m_maxDisplayCols && columns > i + 2) {
            m_fuzzyColumn = i;

            cpc->addItem(tr("<ignore>"));
            cpc->addItem(tr("Values"));
            cpc->setCurrentIndex
                (m_format.getColumnPurpose(i-1) ==
                 CSVFormat::ColumnUnknown ? 0 : 1);

            exampleLayout->addWidget
                (new QLabel(tr("(%1 more)").arg(columns - i)), 1, i);
            break;
        }

        // NB must be in the same order as the CSVFormat::ColumnPurpose enum
        cpc->addItem(tr("<ignore>")); // ColumnUnknown
        cpc->addItem(tr("Time"));     // ColumnStartTime
        cpc->addItem(tr("End time")); // ColumnEndTime
        cpc->addItem(tr("Duration")); // ColumnDuration
        cpc->addItem(tr("Value"));    // ColumnValue
        cpc->addItem(tr("Pitch"));    // ColumnPitch
        cpc->addItem(tr("Label"));    // ColumnLabel
        cpc->setCurrentIndex(int(m_format.getColumnPurpose(i)));
        
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

    if (m_exampleFrame) {
        delete m_exampleFrame;
    }
    m_exampleFrame = exampleFrame;

    layout->addWidget(exampleFrame, m_exampleFrameRow, 0, 1, 4);
    layout->setColumnStretch(3, 10);
    layout->setRowStretch(m_exampleFrameRow, 10);
    
    m_initialTimingOption = TimingImplicit;
    if (m_format.getTimingType() == CSVFormat::ExplicitTiming) {
        switch (m_format.getTimeUnits()) {
        case CSVFormat::TimeSeconds:
            m_initialTimingOption = TimingExplicitSeconds; break;
        case CSVFormat::TimeMilliseconds:
            m_initialTimingOption = TimingExplicitMsec; break;
        case CSVFormat::TimeAudioFrames:
            m_initialTimingOption = TimingExplicitSamples; break;
        case CSVFormat::TimeWindows:
            m_initialTimingOption = TimingImplicit; break;
        }
    }
    m_timingTypeCombo->setCurrentIndex(int(m_initialTimingOption));
    
    for (int i = 0; i < int(sizeof(sampleRates) / sizeof(sampleRates[0])); ++i) {
        if (sampleRates[i] == m_format.getSampleRate()) {
            m_sampleRateCombo->setCurrentIndex(i);
        }
    }
    
    for (int i = 0; i <= 16; ++i) {
        int value = 1 << i;
        if (value == int(m_format.getWindowSize())) {
            m_windowSizeCombo->setCurrentIndex(i);
        }
    }

    timingTypeChanged(m_timingTypeCombo->currentIndex());
}

CSVFormat
CSVFormatDialog::getFormat() const
{
    return m_format;
}

void
CSVFormatDialog::updateModelLabel()
{
    if (!m_modelLabel) {
        return;
    }
    
    LayerFactory *f = LayerFactory::getInstance();

    QString s;
    switch (m_format.getModelType()) {
    case CSVFormat::OneDimensionalModel:
        s = f->getLayerPresentationName(LayerFactory::TimeInstants);
        break;
    case CSVFormat::TwoDimensionalModel:
        s = f->getLayerPresentationName(LayerFactory::TimeValues);
        break; 
    case CSVFormat::TwoDimensionalModelWithDuration:
        s = f->getLayerPresentationName(LayerFactory::Regions);
        break;
    case CSVFormat::TwoDimensionalModelWithDurationAndPitch:
        s = f->getLayerPresentationName(LayerFactory::Notes);
        break;
    case CSVFormat::TwoDimensionalModelWithDurationAndExtent:
        s = f->getLayerPresentationName(LayerFactory::Boxes);
        break;
    case CSVFormat::ThreeDimensionalModel:
        s = f->getLayerPresentationName(LayerFactory::Colour3DPlot);
        break;
    case CSVFormat::WaveFileModel:
        s = f->getLayerPresentationName(LayerFactory::Waveform);
        break;
    }   

    m_modelLabel->setText("\n" + tr("Data will be displayed in a %1 layer.")
                          .arg(s));
}

void
CSVFormatDialog::applyStartTimePurpose()
{
    // First check if we already have any. NB there may be fewer than
    // m_format.getColumnCount() elements in m_columnPurposeCombos
    // (because of the fuzzy column behaviour). Note also that the
    // fuzzy column (which is the one just showing how many more
    // columns there are) has a different combo with only two items
    // (ignore or Values)
    for (int i = 0; i < m_columnPurposeCombos.size(); ++i) {
        if (i == m_fuzzyColumn) continue;
        QComboBox *cb = m_columnPurposeCombos[i];
        if (cb->currentIndex() == int(CSVFormat::ColumnStartTime)) {
            return;
        }
    }
    // and if not, select one
    for (int i = 0; i < m_columnPurposeCombos.size(); ++i) {
        if (i == m_fuzzyColumn) continue;
        QComboBox *cb = m_columnPurposeCombos[i];
        if (cb->currentIndex() == int(CSVFormat::ColumnValue)) {
            cb->setCurrentIndex(int(CSVFormat::ColumnStartTime));
            return;
        }
    }
}

void
CSVFormatDialog::removeStartTimePurpose()
{
    // NB there may be fewer than m_format.getColumnCount() elements
    // in m_columnPurposeCombos (because of the fuzzy column
    // behaviour)
    for (int i = 0; i < m_columnPurposeCombos.size(); ++i) {
        if (i == m_fuzzyColumn) continue;
        QComboBox *cb = m_columnPurposeCombos[i];
        if (cb->currentIndex() == int(CSVFormat::ColumnStartTime)) {
            cb->setCurrentIndex(int(CSVFormat::ColumnValue));
        }
    }
}

void
CSVFormatDialog::updateComboVisibility()
{
    bool wantRate = (m_format.getTimingType() == CSVFormat::ImplicitTiming ||
                     m_format.getTimeUnits() == CSVFormat::TimeAudioFrames);
    bool wantWindow = (m_format.getTimingType() == CSVFormat::ImplicitTiming);
    
    m_sampleRateCombo->setEnabled(wantRate);
    m_sampleRateLabel->setEnabled(wantRate);

    m_windowSizeCombo->setEnabled(wantWindow);
    m_windowSizeLabel->setEnabled(wantWindow);
}

void
CSVFormatDialog::separatorChanged(QString sep)
{
    if (sep == "" || m_csvFilePath == "") {
        return;
    }

    m_format.setSeparator(sep[0]);
    m_format.guessFormatFor(m_csvFilePath);

    repopulate();
}

void
CSVFormatDialog::timingTypeChanged(int type)
{
    // Update any column purpose combos
    if (TimingOption(type) == TimingImplicit) {
        removeStartTimePurpose();
    } else {
        applyStartTimePurpose();
    }
    updateFormatFromDialog();
    updateComboVisibility();
}

void
CSVFormatDialog::sampleRateChanged(QString rateString)
{
    bool ok = false;
    int sampleRate = rateString.toInt(&ok);
    if (ok) m_format.setSampleRate(sampleRate);
}

void
CSVFormatDialog::windowSizeChanged(QString sizeString)
{
    bool ok = false;
    int size = sizeString.toInt(&ok);
    if (ok) m_format.setWindowSize(size);
}

void
CSVFormatDialog::columnPurposeChanged(int p)
{
    QObject *o = sender();
    QComboBox *cb = qobject_cast<QComboBox *>(o);
    if (!cb) return;

    // Ensure a consistent set of column purposes, in case of a
    // situation where some combinations are contradictory. Only
    // updates the UI, does not update the stored format record from
    // the UI - that's the job of updateFormatFromDialog
    
    CSVFormat::ColumnPurpose purpose = (CSVFormat::ColumnPurpose)p;

    bool haveStartTime = false; // so as to update timing type combo appropriately
    
    // Ensure the column purpose combos are consistent with one
    // another, without reference to m_format (which we'll update
    // separately)
    
    for (int i = 0; i < m_columnPurposeCombos.size(); ++i) {

        // The fuzzy column combo only has the entries <ignore> or
        // Values, so it can't affect the timing type and none of this
        // logic affects it
        if (i == m_fuzzyColumn) continue;

        QComboBox *thisCombo = m_columnPurposeCombos[i];
        
        CSVFormat::ColumnPurpose cp = (CSVFormat::ColumnPurpose)
            (thisCombo->currentIndex());
        bool thisChanged = (cb == thisCombo);
        
        if (!thisChanged) {

            // We can only have one ColumnStartTime column, and only
            // one of either ColumnDuration or ColumnEndTime

            if (purpose == CSVFormat::ColumnStartTime) {
                if (cp == purpose) {
                    cp = CSVFormat::ColumnValue;
                }
            } else if (purpose == CSVFormat::ColumnDuration ||
                       purpose == CSVFormat::ColumnEndTime) {
                if (cp == CSVFormat::ColumnDuration ||
                    cp == CSVFormat::ColumnEndTime) {
                    cp = CSVFormat::ColumnValue;
                }
            }

            // And we can only have one label
            if (purpose == CSVFormat::ColumnLabel) {
                if (cp == purpose) {
                    cp = CSVFormat::ColumnUnknown;
                }
            }

            if (cp == CSVFormat::ColumnStartTime) {
                haveStartTime = true;
            }
        
            thisCombo->setCurrentIndex(int(cp));

        } else {
            if (purpose == CSVFormat::ColumnStartTime) {
                haveStartTime = true;
            }
        }
    }

    if (!haveStartTime) {
        m_timingTypeCombo->setCurrentIndex(int(TimingImplicit));
    } else if (m_timingTypeCombo->currentIndex() == int(TimingImplicit)) {
        if (m_initialTimingOption == TimingImplicit) {
            m_timingTypeCombo->setCurrentIndex(TimingExplicitSeconds);
        } else {
            m_timingTypeCombo->setCurrentIndex(m_initialTimingOption);
        }
    }

    updateFormatFromDialog();
    updateComboVisibility();
}
    
void
CSVFormatDialog::updateFormatFromDialog()
{
    switch (TimingOption(m_timingTypeCombo->currentIndex())) {

    case TimingExplicitSeconds:
        m_format.setTimingType(CSVFormat::ExplicitTiming);
        m_format.setTimeUnits(CSVFormat::TimeSeconds);
        break;
        
    case TimingExplicitMsec:
        m_format.setTimingType(CSVFormat::ExplicitTiming);
        m_format.setTimeUnits(CSVFormat::TimeMilliseconds);
        break;
        
    case TimingExplicitSamples:
        m_format.setTimingType(CSVFormat::ExplicitTiming);
        m_format.setTimeUnits(CSVFormat::TimeAudioFrames);
        break;
        
    case TimingImplicit:
        m_format.setTimingType(CSVFormat::ImplicitTiming);
        m_format.setTimeUnits(CSVFormat::TimeWindows);
        break;
    }
    
    bool haveStartTime = false;
    bool haveDuration = false;
    bool havePitch = false;
    int valueCount = 0;

    for (int i = 0; i < m_columnPurposeCombos.size(); ++i) {

        QComboBox *thisCombo = m_columnPurposeCombos[i];
        
        CSVFormat::ColumnPurpose purpose =
            (CSVFormat::ColumnPurpose) (thisCombo->currentIndex());
        
        if (i == m_fuzzyColumn) {
            for (int j = i; j < m_format.getColumnCount(); ++j) {
                if (purpose == CSVFormat::ColumnUnknown) {
                    m_format.setColumnPurpose(j, CSVFormat::ColumnUnknown);
                } else { // Value
                    m_format.setColumnPurpose(j, CSVFormat::ColumnValue);
                    ++valueCount;
                }
            }
        } else {
        
            if (purpose == CSVFormat::ColumnStartTime) {
                haveStartTime = true;
            }
            if (purpose == CSVFormat::ColumnEndTime ||
                purpose == CSVFormat::ColumnDuration) {
                haveDuration = true;
            }
            if (purpose == CSVFormat::ColumnPitch) {
                havePitch = true;
            }
            if (purpose == CSVFormat::ColumnValue) {
                ++valueCount;
            }

            m_format.setColumnPurpose(i, purpose);
        }
    }

    if (haveStartTime && haveDuration) {
        if (havePitch) {
            m_format.setModelType(CSVFormat::TwoDimensionalModelWithDurationAndPitch);
        } else if (valueCount == 2) {
            m_format.setModelType(CSVFormat::TwoDimensionalModelWithDurationAndExtent);
        } else {
            m_format.setModelType(CSVFormat::TwoDimensionalModelWithDuration);
        }
    } else {
        if (valueCount > 1) {
            m_format.setModelType(CSVFormat::ThreeDimensionalModel);
        } else if (valueCount > 0) {
            m_format.setModelType(CSVFormat::TwoDimensionalModel);
        } else {
            m_format.setModelType(CSVFormat::OneDimensionalModel);
        }
    }

    updateModelLabel();
}



