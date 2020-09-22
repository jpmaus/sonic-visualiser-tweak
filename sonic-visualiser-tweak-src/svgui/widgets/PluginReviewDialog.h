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

#ifndef SV_PLUGIN_REVIEW_DIALOG_H
#define SV_PLUGIN_REVIEW_DIALOG_H

#include <QDialog>
#include <QTableWidget>

class QEvent;

class PluginReviewDialog : public QDialog
{
    Q_OBJECT
    
public:
    PluginReviewDialog(QWidget *parent = 0);
    ~PluginReviewDialog();

    void populate();
    
private:
    QTableWidget *m_table;
};

#endif
