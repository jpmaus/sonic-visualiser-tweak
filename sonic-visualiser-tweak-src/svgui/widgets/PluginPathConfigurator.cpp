/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "PluginPathConfigurator.h"
#include "PluginReviewDialog.h"

#include <QPushButton>
#include <QListWidget>
#include <QGridLayout>
#include <QComboBox>
#include <QLabel>
#include <QCheckBox>
#include <QFileDialog>

#include "IconLoader.h"
#include "WidgetScale.h"

#include "plugin/PluginPathSetter.h"

PluginPathConfigurator::PluginPathConfigurator(QWidget *parent) :
    QFrame(parent)
{
    m_layout = new QGridLayout;
    setLayout(m_layout);

    QHBoxLayout *buttons = new QHBoxLayout;

    m_down = new QPushButton;
    m_down->setIcon(IconLoader().load("down"));
    m_down->setToolTip(tr("Move the selected location later in the list"));
    connect(m_down, SIGNAL(clicked()), this, SLOT(downClicked()));
    buttons->addWidget(m_down);

    m_up = new QPushButton;
    m_up->setIcon(IconLoader().load("up"));
    m_up->setToolTip(tr("Move the selected location earlier in the list"));
    connect(m_up, SIGNAL(clicked()), this, SLOT(upClicked()));
    buttons->addWidget(m_up);

    m_add = new QPushButton;
    m_add->setIcon(IconLoader().load("plus"));
    m_add->setToolTip(tr("Add a new location to the list"));
    connect(m_add, SIGNAL(clicked()), this, SLOT(addClicked()));
    buttons->addWidget(m_add);
    
    m_delete = new QPushButton;
    m_delete->setIcon(IconLoader().load("datadelete"));
    m_delete->setToolTip(tr("Remove the selected location from the list"));
    connect(m_delete, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    buttons->addWidget(m_delete);

    m_reset = new QPushButton;
    m_reset->setText(tr("Reset to Default"));
    m_reset->setToolTip(tr("Reset the list for this plugin type to its default"));
    connect(m_reset, SIGNAL(clicked()), this, SLOT(resetClicked()));
    buttons->addWidget(m_reset);

    buttons->addStretch(50);

    m_seePlugins = new QPushButton;
    m_seePlugins->setText(tr("Review plugins..."));
    connect(m_seePlugins, SIGNAL(clicked()), this, SLOT(seePluginsClicked()));
    buttons->addWidget(m_seePlugins);

    int row = 0;
    
    m_header = new QLabel;
    m_header->setText(tr("Plugin locations for plugin type:"));
    m_layout->addWidget(m_header, row, 0);

    m_pluginTypeSelector = new QComboBox;
    m_layout->addWidget(m_pluginTypeSelector, row, 1, Qt::AlignLeft);
    connect(m_pluginTypeSelector, SIGNAL(currentTextChanged(QString)),
            this, SLOT(currentTypeChanged(QString)));

    m_layout->setColumnStretch(1, 10);
    ++row;
    
    m_list = new QListWidget;
    m_layout->addWidget(m_list, row, 0, 1, 3);
    m_layout->setRowStretch(row, 20);
    connect(m_list, SIGNAL(currentRowChanged(int)),
            this, SLOT(currentLocationChanged(int)));
    ++row;

    m_layout->addLayout(buttons, row, 0, 1, 3);
    
    ++row;

    m_envOverride = new QCheckBox;
    connect(m_envOverride, SIGNAL(stateChanged(int)),
            this, SLOT(envOverrideChanged(int)));
    m_layout->addWidget(m_envOverride, row, 0, 1, 3);
    ++row;
}

PluginPathConfigurator::~PluginPathConfigurator()
{
}

QString
PluginPathConfigurator::getLabelFor(PluginPathSetter::TypeKey key)
{
    if (key.second == KnownPlugins::FormatNative) {
        switch (key.first) {
        case KnownPlugins::VampPlugin:
            return tr("Vamp");
        case KnownPlugins::LADSPAPlugin:
            return tr("LADSPA");
        case KnownPlugins::DSSIPlugin:
            return tr("DSSI");
        }
    } else if (key.second == KnownPlugins::FormatNonNative32Bit) {
        switch (key.first) {
        case KnownPlugins::VampPlugin:
            return tr("Vamp (32-bit)");
        case KnownPlugins::LADSPAPlugin:
            return tr("LADSPA (32-bit)");
        case KnownPlugins::DSSIPlugin:
            return tr("DSSI (32-bit)");
        }
    }
    SVCERR << "PluginPathConfigurator::getLabelFor: WARNING: "
           << "Unknown format value " << key.second << endl;
    return "<unknown>";
}

PluginPathSetter::TypeKey
PluginPathConfigurator::getKeyForLabel(QString label)
{
    for (const auto &p: m_paths) {
        auto key = p.first;
        if (getLabelFor(key) == label) {
            return key;
        }
    }
    SVCERR << "PluginPathConfigurator::getKeyForLabel: WARNING: "
           << "Unrecognised label \"" << label << "\"" << endl;
    return { KnownPlugins::VampPlugin, KnownPlugins::FormatNative };
}

void
PluginPathConfigurator::setPaths(PluginPathSetter::Paths paths)
{
    m_paths = paths;

    m_defaultPaths = PluginPathSetter::getDefaultPaths();

    m_pluginTypeSelector->clear();
    for (const auto &p: paths) {
        m_pluginTypeSelector->addItem(getLabelFor(p.first));
    }
    
    populate();
}

void
PluginPathConfigurator::populate()
{
    m_list->clear();

    if (m_paths.empty()) return;

    populateFor(m_paths.begin()->first, -1);
}

void
PluginPathConfigurator::populateFor(PluginPathSetter::TypeKey key,
                                    int makeCurrent)
{
    QString envVariable = m_paths.at(key).envVariable;
    bool useEnvVariable = m_paths.at(key).useEnvVariable;
    QString envVarValue =
        PluginPathSetter::getOriginalEnvironmentValue(envVariable);
    QString currentValueRubric;
    if (envVarValue == QString()) {
        currentValueRubric = tr("(Variable is currently unset)");
    } else {
        if (envVarValue.length() > 100) {
            QString envVarStart = envVarValue.left(95);
            currentValueRubric = tr("(Current value begins: \"%1 ...\")")
                .arg(envVarStart);
        } else {
            currentValueRubric = tr("(Currently set to: \"%1\")")
                .arg(envVarValue);
        }
    }        
    m_envOverride->setText
        (tr("Allow the %1 environment variable to take priority over this\n%2")
         .arg(envVariable)
         .arg(currentValueRubric));
    m_envOverride->setCheckState(useEnvVariable ? Qt::Checked : Qt::Unchecked);

    m_list->clear();

    for (int i = 0; i < m_pluginTypeSelector->count(); ++i) {
        if (getLabelFor(key) == m_pluginTypeSelector->itemText(i)) {
            m_pluginTypeSelector->blockSignals(true);
            m_pluginTypeSelector->setCurrentIndex(i);
            m_pluginTypeSelector->blockSignals(false);
        }
    }
    
    QStringList path = m_paths.at(key).directories;
    
    for (int i = 0; i < path.size(); ++i) {
        m_list->addItem(path[i]);
    }

    if (makeCurrent < path.size()) {
        m_list->setCurrentRow(makeCurrent);
        currentLocationChanged(makeCurrent);
    }
}

void
PluginPathConfigurator::currentLocationChanged(int i)
{
    QString label = m_pluginTypeSelector->currentText();
    PluginPathSetter::TypeKey key = getKeyForLabel(label);
    QStringList path = m_paths.at(key).directories;
    m_up->setEnabled(i > 0);
    m_down->setEnabled(i >= 0 && i + 1 < path.size());
    m_delete->setEnabled(i >= 0 && i < path.size());
    m_reset->setEnabled(path != m_defaultPaths.at(key).directories);
}

void
PluginPathConfigurator::currentTypeChanged(QString label)
{
    populateFor(getKeyForLabel(label), -1);
}

void
PluginPathConfigurator::envOverrideChanged(int state)
{
    bool useEnvVariable = (state == Qt::Checked);
    
    QString label = m_pluginTypeSelector->currentText();
    PluginPathSetter::TypeKey key = getKeyForLabel(label);

    auto newEntry = m_paths.at(key);
    newEntry.useEnvVariable = useEnvVariable;
    m_paths[key] = newEntry;

    emit pathsChanged();
}

void
PluginPathConfigurator::upClicked()
{
    QString label = m_pluginTypeSelector->currentText();
    PluginPathSetter::TypeKey key = getKeyForLabel(label);
    QStringList path = m_paths.at(key).directories;
        
    int current = m_list->currentRow();
    if (current <= 0) return;
    
    QStringList newPath;
    for (int i = 0; i < path.size(); ++i) {
        if (i + 1 == current) {
            newPath.push_back(path[i+1]);
            newPath.push_back(path[i]);
            ++i;
        } else {
            newPath.push_back(path[i]);
        }
    }

    auto newEntry = m_paths.at(key);
    newEntry.directories = newPath;
    m_paths[key] = newEntry;
    
    populateFor(key, current - 1);

    emit pathsChanged();
}

void
PluginPathConfigurator::downClicked()
{
    QString label = m_pluginTypeSelector->currentText();
    PluginPathSetter::TypeKey key = getKeyForLabel(label);
    QStringList path = m_paths.at(key).directories;

    int current = m_list->currentRow();
    if (current < 0 || current + 1 >= path.size()) return;

    QStringList newPath;
    for (int i = 0; i < path.size(); ++i) {
        if (i == current) {
            newPath.push_back(path[i+1]);
            newPath.push_back(path[i]);
            ++i;
        } else {
            newPath.push_back(path[i]);
        }
    }

    auto newEntry = m_paths.at(key);
    newEntry.directories = newPath;
    m_paths[key] = newEntry;
    
    populateFor(key, current + 1);

    emit pathsChanged();
}

void
PluginPathConfigurator::addClicked()
{
    QString label = m_pluginTypeSelector->currentText();
    PluginPathSetter::TypeKey key = getKeyForLabel(label);

    QString newDir = QFileDialog::getExistingDirectory
        (this, tr("Choose directory to add"));

    if (newDir == QString()) return;

    auto newEntry = m_paths.at(key);
    newEntry.directories.push_back(newDir);
    m_paths[key] = newEntry;
    
    populateFor(key, newEntry.directories.size() - 1);

    emit pathsChanged();
}

void
PluginPathConfigurator::deleteClicked()
{
    QString label = m_pluginTypeSelector->currentText();
    PluginPathSetter::TypeKey key = getKeyForLabel(label);
    QStringList path = m_paths.at(key).directories;
    
    int current = m_list->currentRow();
    if (current < 0) return;

    QStringList newPath;
    for (int i = 0; i < path.size(); ++i) {
        if (i != current) {
            newPath.push_back(path[i]);
        }
    }

    auto newEntry = m_paths.at(key);
    newEntry.directories = newPath;
    m_paths[key] = newEntry;
    
    populateFor(key, current < newPath.size() ? current : current-1);

    emit pathsChanged();
}

void
PluginPathConfigurator::resetClicked()
{
    QString label = m_pluginTypeSelector->currentText();
    PluginPathSetter::TypeKey key = getKeyForLabel(label);
    m_paths[key] = m_defaultPaths[key];
    populateFor(key, -1);

    emit pathsChanged();
}

void
PluginPathConfigurator::seePluginsClicked()
{
    PluginReviewDialog dialog(this);
    dialog.populate();
    dialog.exec();
}

