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

#ifndef SV_PLUGIN_PATH_CONFIGURATOR_H
#define SV_PLUGIN_PATH_CONFIGURATOR_H

#include <QFrame>
#include <QStringList>

class QLabel;
class QWidget;
class QListWidget;
class QPushButton;
class QGridLayout;
class QComboBox;
class QCheckBox;

#include "plugin/PluginPathSetter.h"

class PluginPathConfigurator : public QFrame
{
    Q_OBJECT

public:
    PluginPathConfigurator(QWidget *parent = 0);
    ~PluginPathConfigurator();

    void setPaths(PluginPathSetter::Paths paths);
    PluginPathSetter::Paths getPaths() const { return m_paths; }

signals:
    void pathsChanged();

private slots:
    void upClicked();
    void downClicked();
    void addClicked();
    void deleteClicked();
    void resetClicked();
    void currentTypeChanged(QString);
    void currentLocationChanged(int);
    void envOverrideChanged(int);
    void seePluginsClicked();
    
private:
    QGridLayout *m_layout;
    QLabel *m_header;
    QComboBox *m_pluginTypeSelector;
    QListWidget *m_list;
    QPushButton *m_seePlugins;
    QPushButton *m_up;
    QPushButton *m_down;
    QPushButton *m_add;
    QPushButton *m_delete;
    QPushButton *m_reset;
    QCheckBox *m_envOverride;

    PluginPathSetter::Paths m_paths;
    PluginPathSetter::Paths m_defaultPaths;
    
    void populate();
    void populateFor(PluginPathSetter::TypeKey, int makeCurrent);

    QString getLabelFor(PluginPathSetter::TypeKey);
    PluginPathSetter::TypeKey getKeyForLabel(QString label);
};

#endif


