/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "PropertyBox.h"
#include "PluginParameterDialog.h"

#include "base/PropertyContainer.h"
#include "base/PlayParameters.h"
#include "base/PlayParameterRepository.h"
#include "layer/Layer.h"
#include "base/UnitDatabase.h"
#include "base/RangeMapper.h"

#include "AudioDial.h"
#include "LEDButton.h"
#include "IconLoader.h"
#include "LevelPanWidget.h"
#include "LevelPanToolButton.h"
#include "WidgetScale.h"

#include "NotifyingCheckBox.h"
#include "NotifyingComboBox.h"
#include "NotifyingPushButton.h"
#include "NotifyingToolButton.h"
#include "ColourComboBox.h"
#include "ColourMapComboBox.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QFrame>
#include <QApplication>
#include <QColorDialog>
#include <QInputDialog>
#include <QDir>

#include <cassert>
#include <iostream>
#include <cmath>

//#define DEBUG_PROPERTY_BOX 1

PropertyBox::PropertyBox(PropertyContainer *container) :
    m_container(container),
    m_showButton(nullptr),
    m_playButton(nullptr)
{
#ifdef DEBUG_PROPERTY_BOX
    SVDEBUG << "PropertyBox[" << this << "(\"" <<
        container->getPropertyContainerName() << "\" at " << container << ")]::PropertyBox" << endl;
#endif

    m_mainBox = new QVBoxLayout;
    setLayout(m_mainBox);

#ifdef Q_OS_MAC
    QMargins mm = m_mainBox->contentsMargins();
    QMargins mmhalf(mm.left()/2, mm.top()/3, mm.right()/2, mm.bottom()/3);
    m_mainBox->setContentsMargins(mmhalf);
#endif

//    m_nameWidget = new QLabel;
//    m_mainBox->addWidget(m_nameWidget);
//    m_nameWidget->setText(container->objectName());

    m_mainWidget = new QWidget;
    m_mainBox->addWidget(m_mainWidget);
    m_mainBox->insertStretch(2, 10);

    m_viewPlayFrame = nullptr;
    populateViewPlayFrame();

    m_layout = new QGridLayout;
    m_layout->setMargin(0);
    m_layout->setHorizontalSpacing(2);
    m_layout->setVerticalSpacing(1);
    m_mainWidget->setLayout(m_layout);

    PropertyContainer::PropertyList properties = m_container->getProperties();

    blockSignals(true);

    size_t i;

    for (i = 0; i < properties.size(); ++i) {
        updatePropertyEditor(properties[i]);
    }

    blockSignals(false);

    m_layout->setRowStretch(m_layout->rowCount(), 10);

    connect(UnitDatabase::getInstance(), SIGNAL(unitDatabaseChanged()),
            this, SLOT(unitDatabaseChanged()));

#ifdef DEBUG_PROPERTY_BOX
    SVDEBUG << "PropertyBox[" << this << "]::PropertyBox returning" << endl;
#endif
}

PropertyBox::~PropertyBox()
{
#ifdef DEBUG_PROPERTY_BOX
    SVDEBUG << "PropertyBox[" << this << "]::~PropertyBox" << endl;
#endif
}

void
PropertyBox::populateViewPlayFrame()
{
#ifdef DEBUG_PROPERTY_BOX
    SVDEBUG << "PropertyBox[" << this << ":" << m_container << "]::populateViewPlayFrame" << endl;
#endif

    if (m_viewPlayFrame) {
        delete m_viewPlayFrame;
        m_viewPlayFrame = nullptr;
    }

    if (!m_container) return;

    Layer *layer = dynamic_cast<Layer *>(m_container);
    if (layer) {
        disconnect(layer, SIGNAL(modelReplaced()),
                   this, SLOT(populateViewPlayFrame()));
        connect(layer, SIGNAL(modelReplaced()),
                this, SLOT(populateViewPlayFrame()));
    }

    auto params = m_container->getPlayParameters();
    if (!params && !layer) return;

    m_viewPlayFrame = new QFrame;
    m_viewPlayFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_mainBox->addWidget(m_viewPlayFrame);

    QGridLayout *layout = new QGridLayout;
    m_viewPlayFrame->setLayout(layout);

    layout->setMargin(layout->margin() / 2);

#ifdef DEBUG_PROPERTY_BOX
    SVDEBUG << "PropertyBox::populateViewPlayFrame: container " << m_container << " (name " << m_container->getPropertyContainerName() << ") params " << params << endl;
#endif

    QSize buttonSize = WidgetScale::scaleQSize(QSize(26, 26));
    int col = 0;

    if (params) {
        
        m_playButton = new NotifyingToolButton;
        m_playButton->setCheckable(true);
        m_playButton->setIcon(IconLoader().load("speaker"));
        m_playButton->setToolTip(tr("Click to toggle playback"));
        m_playButton->setChecked(!params->isPlayMuted());
        m_playButton->setFixedSize(buttonSize);
        connect(m_playButton, SIGNAL(toggled(bool)),
                this, SLOT(playAudibleButtonChanged(bool)));
        connect(m_playButton, SIGNAL(mouseEntered()),
                this, SLOT(mouseEnteredWidget()));
        connect(m_playButton, SIGNAL(mouseLeft()),
                this, SLOT(mouseLeftWidget()));
        connect(params.get(), SIGNAL(playAudibleChanged(bool)),
                this, SLOT(playAudibleChanged(bool)));

        LevelPanToolButton *levelPan = new LevelPanToolButton;
        levelPan->setFixedSize(buttonSize);
        levelPan->setImageSize((buttonSize.height() * 3) / 4);
        layout->addWidget(levelPan, 0, col++, Qt::AlignCenter);
        connect(levelPan, SIGNAL(levelChanged(float)),
                this, SLOT(playGainControlChanged(float)));
        connect(levelPan, SIGNAL(panChanged(float)),
                this, SLOT(playPanControlChanged(float)));
        connect(params.get(), SIGNAL(playGainChanged(float)),
                levelPan, SLOT(setLevel(float)));
        connect(params.get(), SIGNAL(playPanChanged(float)),
                levelPan, SLOT(setPan(float)));
        connect(levelPan, SIGNAL(mouseEntered()),
                this, SLOT(mouseEnteredWidget()));
        connect(levelPan, SIGNAL(mouseLeft()),
                this, SLOT(mouseLeftWidget()));

        layout->addWidget(m_playButton, 0, col++, Qt::AlignCenter);

        if (params->getPlayClipId() != "") {
            NotifyingToolButton *playParamButton = new NotifyingToolButton;
            playParamButton->setObjectName("playParamButton");
            playParamButton->setIcon(IconLoader().load("faders"));
            playParamButton->setFixedSize(buttonSize);
            layout->addWidget(playParamButton, 0, col++, Qt::AlignCenter);
            connect(playParamButton, SIGNAL(clicked()),
                    this, SLOT(editPlayParameters()));
            connect(playParamButton, SIGNAL(mouseEntered()),
                    this, SLOT(mouseEnteredWidget()));
            connect(playParamButton, SIGNAL(mouseLeft()),
                    this, SLOT(mouseLeftWidget()));
        }
    }

    layout->setColumnStretch(col++, 10);

    if (layer) {

        QLabel *showLabel = new QLabel(tr("Show"));
        layout->addWidget(showLabel, 0, col++, Qt::AlignVCenter | Qt::AlignRight);

        m_showButton = new LEDButton(palette().highlight().color());
        layout->addWidget(m_showButton, 0, col++, Qt::AlignVCenter | Qt::AlignLeft);
        connect(m_showButton, SIGNAL(stateChanged(bool)),
                this, SIGNAL(showLayer(bool)));
        connect(m_showButton, SIGNAL(mouseEntered()),
                this, SLOT(mouseEnteredWidget()));
        connect(m_showButton, SIGNAL(mouseLeft()),
                this, SLOT(mouseLeftWidget()));
    }
}

void
PropertyBox::updatePropertyEditor(PropertyContainer::PropertyName name,
                                  bool rangeChanged)
{
    PropertyContainer::PropertyType type = m_container->getPropertyType(name);
    int row = m_layout->rowCount();

    int min = 0, max = 0, value = 0, deflt = 0;
    value = m_container->getPropertyRangeAndValue(name, &min, &max, &deflt);

    bool have = (m_propertyControllers.find(name) !=
                 m_propertyControllers.end());

    QString groupName = m_container->getPropertyGroupName(name);
    QString propertyLabel = m_container->getPropertyLabel(name);
    QString iconName = m_container->getPropertyIconName(name);

#ifdef DEBUG_PROPERTY_BOX
    SVDEBUG << "PropertyBox[" << this
              << "(\"" << m_container->getPropertyContainerName()
              << "\")]";
    SVDEBUG << "::updatePropertyEditor(\"" << name << "\", "
         << rangeChanged << "):";
    SVDEBUG << " type " << type << ", value " << value
         << ", have " << have << ", group \"" << groupName << "\"" << endl;
#endif

    QString groupLabel = groupName;
    if (groupName == QString()) {
        groupName = "ungrouped: " + name; // not tr(), this is internal id
        groupLabel = propertyLabel;
    }
    
    if (!have) {
        if (m_groupLayouts.find(groupName) == m_groupLayouts.end()) {
            QWidget *labelWidget = new QLabel(groupLabel, m_mainWidget);
            m_layout->addWidget(labelWidget, row, 0);
            QWidget *frame = new QWidget(m_mainWidget);
            frame->setMinimumSize(WidgetScale::scaleQSize(QSize(1, 24)));
            m_groupLayouts[groupName] = new QGridLayout;
#ifdef Q_OS_MAC
            // Seems to be plenty of whitespace already
            m_groupLayouts[groupName]->setContentsMargins(0, 0, 0, 0);
#else
            // Need a bit of padding on the left
            m_groupLayouts[groupName]->setContentsMargins
                (WidgetScale::scalePixelSize(10), 0, 0, 0);
#endif
            frame->setLayout(m_groupLayouts[groupName]);
            m_layout->addWidget(frame, row, 1, 1, 2);
            m_layout->setColumnStretch(1, 10);
        }
    }

    QGridLayout *groupLayout = m_groupLayouts[groupName];

#ifdef DEBUG_PROPERTY_BOX
    SVDEBUG << "groupName becomes \"" << groupName << "\", groupLabel = \""
         << groupLabel << "\", groupLayout = " << groupLayout << endl;
#endif
    
    assert(groupLayout);

    QWidget *existing = m_propertyControllers[name];
    
    switch (type) {

    case PropertyContainer::ToggleProperty:
    {
        QAbstractButton *button;

        if (!(button = qobject_cast<QAbstractButton *>(existing))) {
#ifdef DEBUG_PROPERTY_BOX 
            SVDEBUG << "PropertyBox: creating new checkbox" << endl;
#endif
            if (iconName != "") {
#ifdef Q_OS_MAC
                button = new NotifyingToolButton();
#else
                button = new NotifyingPushButton();
#endif
                button->setCheckable(true);
                QIcon icon(IconLoader().load(iconName));
                button->setIcon(icon);
                button->setObjectName(name);
                button->setFixedSize(WidgetScale::scaleQSize(QSize(18, 18)));
            } else {
                button = new NotifyingCheckBox();
                button->setObjectName(name);
            }
            connect(button, SIGNAL(toggled(bool)),
                    this, SLOT(propertyControllerChanged(bool)));
            connect(button, SIGNAL(mouseEntered()),
                    this, SLOT(mouseEnteredWidget()));
            connect(button, SIGNAL(mouseLeft()),
                    this, SLOT(mouseLeftWidget()));
            button->setToolTip(propertyLabel);

            if (existing) {
                groupLayout->replaceWidget(existing, button);
                delete existing;
            } else {
                groupLayout->addWidget(button, 0, groupLayout->columnCount());
            }

            m_propertyControllers[name] = button;
        }

        if (button->isChecked() != (value > 0)) {
            button->blockSignals(true);
            button->setChecked(value > 0);
            button->blockSignals(false);
        }
        break;
    }

    case PropertyContainer::RangeProperty:
    {
        AudioDial *dial;

        if ((dial = qobject_cast<AudioDial *>(existing))) {
            if (rangeChanged) {
                dial->blockSignals(true);
                dial->setMinimum(min);
                dial->setMaximum(max);
                dial->setRangeMapper(m_container->getNewPropertyRangeMapper(name));
                dial->blockSignals(false);
            }
        } else {
#ifdef DEBUG_PROPERTY_BOX 
            SVDEBUG << "PropertyBox: creating new dial" << endl;
#endif
            dial = new AudioDial();
            dial->setObjectName(name);
            dial->setMinimum(min);
            dial->setMaximum(max);
            dial->setPageStep(1);
            dial->setNotchesVisible((max - min) <= 12);
            // important to set the range mapper before the default,
            // because the range mapper is used to map the default
            dial->setRangeMapper(m_container->getNewPropertyRangeMapper(name));
            dial->setDefaultValue(deflt);
            dial->setShowToolTip(true);
            connect(dial, SIGNAL(valueChanged(int)),
                    this, SLOT(propertyControllerChanged(int)));
            connect(dial, SIGNAL(mouseEntered()),
                    this, SLOT(mouseEnteredWidget()));
            connect(dial, SIGNAL(mouseLeft()),
                    this, SLOT(mouseLeftWidget()));

            dial->setFixedWidth(WidgetScale::scalePixelSize(24));
            dial->setFixedHeight(WidgetScale::scalePixelSize(24));

            if (existing) {
                groupLayout->replaceWidget(existing, dial);
                delete existing;
            } else {
                groupLayout->addWidget(dial, 0, groupLayout->columnCount());
            }

            m_propertyControllers[name] = dial;
        }

        if (dial->value() != value) {
            dial->blockSignals(true);
            dial->setValue(value);
            dial->blockSignals(false);
        }
        break;
    }

    case PropertyContainer::ColourProperty:
    {
        ColourComboBox *cb;
        
        if (!(cb = qobject_cast<ColourComboBox *>(existing))) {

#ifdef DEBUG_PROPERTY_BOX 
            SVDEBUG << "PropertyBox: creating new colour combobox" << endl;
#endif
            cb = new ColourComboBox(true);
            cb->setObjectName(name);

            connect(cb, SIGNAL(colourChanged(int)),
                    this, SLOT(propertyControllerChanged(int)));
            connect(cb, SIGNAL(mouseEntered()),
                    this, SLOT(mouseEnteredWidget()));
            connect(cb, SIGNAL(mouseLeft()),
                    this, SLOT(mouseLeftWidget()));

            cb->setToolTip(propertyLabel);

            if (existing) {
                groupLayout->replaceWidget(existing, cb);
                delete existing;
            } else {
                groupLayout->addWidget(cb, 0, groupLayout->columnCount());
            }
            
            m_propertyControllers[name] = cb;
        }

        if (cb->currentIndex() != value) {
            cb->blockSignals(true);
            cb->setCurrentIndex(value);
            cb->blockSignals(false);
        }

        break;
    }        

    case PropertyContainer::ColourMapProperty:
    {
        ColourMapComboBox *cb;

        if (!(cb = qobject_cast<ColourMapComboBox *>(existing))) {
#ifdef DEBUG_PROPERTY_BOX 
            SVDEBUG << "PropertyBox: creating new colourmap combobox" << endl;
#endif
            cb = new ColourMapComboBox(false);
            cb->setObjectName(name);

            connect(cb, SIGNAL(colourMapChanged(int)),
                    this, SLOT(propertyControllerChanged(int)));
            connect(cb, SIGNAL(mouseEntered()),
                    this, SLOT(mouseEnteredWidget()));
            connect(cb, SIGNAL(mouseLeft()),
                    this, SLOT(mouseLeftWidget()));
            
            cb->setToolTip(propertyLabel);

            if (existing) {
                groupLayout->replaceWidget(existing, cb);
                delete existing;
            } else {
                groupLayout->addWidget(cb, 0, groupLayout->columnCount());
            }
            
            m_propertyControllers[name] = cb;
        }

        if (cb->currentIndex() != value) {
            cb->blockSignals(true);
            cb->setCurrentIndex(value);
            cb->blockSignals(false);
        }

        break;
    }        

    case PropertyContainer::ValueProperty:
    case PropertyContainer::UnitsProperty:
    {
        NotifyingComboBox *cb;

        if (!(cb = qobject_cast<NotifyingComboBox *>(existing))) {
#ifdef DEBUG_PROPERTY_BOX 
            SVDEBUG << "PropertyBox: creating new combobox" << endl;
#endif
            cb = new NotifyingComboBox();
            cb->setObjectName(name);
            cb->setDuplicatesEnabled(false);
        }

        if (!have || rangeChanged) {

            cb->blockSignals(true);
            cb->clear();
            cb->setEditable(false);

            if (type == PropertyContainer::ValueProperty) {

                for (int i = min; i <= max; ++i) {

                    QString label = m_container->getPropertyValueLabel(name, i);
                    QString iname = m_container->getPropertyValueIconName(name, i);

                    if (iname != "") {
                        QIcon icon(IconLoader().load(iname));
                        cb->addItem(icon, label);
                    } else {
                        cb->addItem(label);
                    }
                }

            } else { // PropertyContainer::UnitsProperty

                QStringList units = UnitDatabase::getInstance()->getKnownUnits();
                for (int i = 0; i < units.size(); ++i) {
                    cb->addItem(units[i]);
                }

                cb->setEditable(true);
            }
        }

        if (!have) {
            connect(cb, SIGNAL(activated(int)),
                    this, SLOT(propertyControllerChanged(int)));
            connect(cb, SIGNAL(mouseEntered()),
                    this, SLOT(mouseEnteredWidget()));
            connect(cb, SIGNAL(mouseLeft()),
                    this, SLOT(mouseLeftWidget()));

            cb->setToolTip(propertyLabel);
            groupLayout->addWidget(cb, 0, groupLayout->columnCount());
            m_propertyControllers[name] = cb;
        } else if (existing != cb) {
            groupLayout->replaceWidget(existing, cb);
            delete existing;
        }

        cb->blockSignals(true);
        if (type == PropertyContainer::ValueProperty) {
            if (cb->currentIndex() != value) {
                cb->setCurrentIndex(value);
            }
        } else {
            QString unit = UnitDatabase::getInstance()->getUnitById(value);
            if (cb->currentText() != unit) {
                for (int i = 0; i < cb->count(); ++i) {
                    if (cb->itemText(i) == unit) {
                        cb->setCurrentIndex(i);
                        break;
                    }
                }
            }
        }
        cb->blockSignals(false);

        break;
    }

    case PropertyContainer::InvalidProperty:
    default:
        break;
    }
}

void
PropertyBox::propertyContainerPropertyChanged(PropertyContainer *pc)
{
    if (pc != m_container) return;
    
#ifdef DEBUG_PROPERTY_BOX
    SVDEBUG << "PropertyBox::propertyContainerPropertyChanged" << endl;
#endif

    PropertyContainer::PropertyList properties = m_container->getProperties();
    size_t i;

    blockSignals(true);

    for (i = 0; i < properties.size(); ++i) {
        updatePropertyEditor(properties[i]);
    }

    blockSignals(false);
}

void
PropertyBox::propertyContainerPropertyRangeChanged(PropertyContainer *)
{
    blockSignals(true);

    PropertyContainer::PropertyList properties = m_container->getProperties();
    for (size_t i = 0; i < properties.size(); ++i) {
        updatePropertyEditor(properties[i], true);
    }

    blockSignals(false);
}    

void
PropertyBox::unitDatabaseChanged()
{
#ifdef DEBUG_PROPERTY_BOX
    SVDEBUG << "PropertyBox[" << this << "]: unitDatabaseChanged" << endl;
#endif
    blockSignals(true);

//    SVDEBUG << "my container is " << m_container << endl;
//    SVDEBUG << "my container's name is... " << endl;
//    SVDEBUG << m_container->objectName() << endl;

    PropertyContainer::PropertyList properties = m_container->getProperties();
    for (size_t i = 0; i < properties.size(); ++i) {
        if (m_container->getPropertyType(properties[i]) ==
            PropertyContainer::UnitsProperty) {
            updatePropertyEditor(properties[i]);
        }
    }

    blockSignals(false);
}    

void
PropertyBox::propertyControllerChanged(bool on)
{
    propertyControllerChanged(on ? 1 : 0);
}

void
PropertyBox::propertyControllerChanged(int value)
{
    QObject *obj = sender();
    QString name = obj->objectName();

#ifdef DEBUG_PROPERTY_BOX
    SVDEBUG << "PropertyBox::propertyControllerChanged(" << name
            << ", " << value << ")" << endl;
#endif
    
    PropertyContainer::PropertyType type = m_container->getPropertyType(name);

    Command *c = nullptr;

    if (type == PropertyContainer::UnitsProperty) {

        NotifyingComboBox *cb = qobject_cast<NotifyingComboBox *>(obj);
        if (cb) {
            QString unit = cb->currentText();
            c = m_container->getSetPropertyCommand
                (name, UnitDatabase::getInstance()->getUnitId(unit));
        }

    } else if (type != PropertyContainer::InvalidProperty) {

        c = m_container->getSetPropertyCommand(name, value);
    }

    if (c) CommandHistory::getInstance()->addCommand(c, true, true);
    
    updateContextHelp(obj);
}

void
PropertyBox::playAudibleChanged(bool audible)
{
    m_playButton->setChecked(audible);
}

void
PropertyBox::playAudibleButtonChanged(bool audible)
{
    auto params = m_container->getPlayParameters();
    if (!params) return;

    if (params->isPlayAudible() != audible) {
        PlayParameterRepository::EditCommand *command =
            new PlayParameterRepository::EditCommand(params);
        command->setPlayAudible(audible);
        CommandHistory::getInstance()->addCommand(command, true, true);
    }
}

void
PropertyBox::playGainControlChanged(float gain)
{
    QObject *obj = sender();

    auto params = m_container->getPlayParameters();
    if (!params) return;

    if (params->getPlayGain() != gain) {
        PlayParameterRepository::EditCommand *command =
            new PlayParameterRepository::EditCommand(params);
        command->setPlayGain(gain);
        CommandHistory::getInstance()->addCommand(command, true, true);
    }

    updateContextHelp(obj);
}

void
PropertyBox::playPanControlChanged(float pan)
{
    QObject *obj = sender();

    auto params = m_container->getPlayParameters();
    if (!params) return;

    if (params->getPlayPan() != pan) {
        PlayParameterRepository::EditCommand *command =
            new PlayParameterRepository::EditCommand(params);
        command->setPlayPan(pan);
        CommandHistory::getInstance()->addCommand(command, true, true);
    }

    updateContextHelp(obj);
}

void
PropertyBox::editPlayParameters()
{
    auto params = m_container->getPlayParameters();
    if (!params) return;

    QString clip = params->getPlayClipId();

    PlayParameterRepository::EditCommand *command = 
        new PlayParameterRepository::EditCommand(params);
    
    QInputDialog *dialog = new QInputDialog(this);

    QDir dir(":/samples");
    QStringList clipFiles = dir.entryList(QStringList() << "*.wav", QDir::Files);

    QStringList clips;
    foreach (QString str, clipFiles) {
        clips.push_back(str.replace(".wav", ""));
    }
    dialog->setComboBoxItems(clips);

    dialog->setLabelText(tr("Set playback clip:"));

    QComboBox *cb = dialog->findChild<QComboBox *>();
    if (cb) {
        for (int i = 0; i < cb->count(); ++i) {
            if (cb->itemText(i) == clip) {
                cb->setCurrentIndex(i);
            }
        }
    }

    connect(dialog, SIGNAL(textValueChanged(QString)), 
            this, SLOT(playClipChanged(QString)));

    if (dialog->exec() == QDialog::Accepted) {
        QString newClip = dialog->textValue();
        command->setPlayClipId(newClip);
        CommandHistory::getInstance()->addCommand(command, true);
    } else {
        delete command;
        // restore in case we mucked about with the configuration
        // as a consequence of signals from the dialog
        params->setPlayClipId(clip);
    }

    delete dialog;
}

void
PropertyBox::playClipChanged(QString id)
{
    auto params = m_container->getPlayParameters();
    if (!params) return;

    params->setPlayClipId(id);
}    

void
PropertyBox::layerVisibilityChanged(bool visible)
{
    if (m_showButton) m_showButton->setState(visible);
}

void
PropertyBox::mouseEnteredWidget()
{
    updateContextHelp(sender());
}

void
PropertyBox::updateContextHelp(QObject *o)
{
    QWidget *w = qobject_cast<QWidget *>(o);
    if (!w) return;

    if (!m_container) return;
    QString cname = m_container->getPropertyContainerName();
    if (cname == "") return;

    QString help;
    QString mainText;
    QString extraText;
    QString editText;

    QString wname = w->objectName();
    QString propertyLabel;
    if (wname != "") {
        propertyLabel = m_container->getPropertyLabel(wname);
    }

    LevelPanToolButton *lp = qobject_cast<LevelPanToolButton *>(w);
    AudioDial *dial = qobject_cast<AudioDial *>(w);

    if (lp) {

        mainText = tr("Adjust playback level and pan of %1").arg(cname);
        editText = tr("click then drag to adjust, ctrl+click to reset");

    } else if (wname == "playParamButton") {
 
        auto params = m_container->getPlayParameters();
        if (params) {
            help = tr("Change sound used for playback (currently \"%1\")")
                .arg(params->getPlayClipId());
        }

    } else if (dial) {
        
        double mv = dial->mappedValue();
        QString unit = "";
        if (dial->rangeMapper()) unit = dial->rangeMapper()->getUnit();
        if (unit != "") {
            extraText = tr(" (current value: %1%2)").arg(mv).arg(unit);
        } else {
            extraText = tr(" (current value: %1)").arg(mv);
        }
        editText = tr("drag up/down to adjust, ctrl+click to reset");

    } else if (w == m_showButton) {
        help = tr("Toggle Visibility of %1").arg(cname);

    } else if (w == m_playButton) {
        help = tr("Toggle Playback of %1").arg(cname);

    }

    if (help == "" && wname != "") {
        
        if (qobject_cast<QAbstractButton *>(w)) {
            mainText = tr("Toggle %1 property of %2")
                .arg(propertyLabel).arg(cname);

        } else {

            // Last param empty for historical reasons, to avoid
            // changing tr() string
            mainText = tr("Adjust %1 property of %2%3")
                .arg(propertyLabel).arg(cname).arg("");
        }
    }

    if (help == "") {
        if (mainText != "") {
            if (editText != "") {
                help = tr("%1%2: %3")
                    .arg(mainText).arg(extraText).arg(editText);
            } else {
                help = tr("%1%2")
                    .arg(mainText).arg(extraText);
            }
        }
    }

    if (help != "") {
        emit contextHelpChanged(help);
    }
}

void
PropertyBox::mouseLeftWidget()
{
    if (!(QApplication::mouseButtons() & Qt::LeftButton)) {
        emit contextHelpChanged("");
    }
}


