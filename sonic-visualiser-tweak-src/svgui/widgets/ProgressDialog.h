/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007-2008 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_PROGRESS_DIALOG_H

#include "base/ProgressReporter.h"

class QProgressDialog;
class QTimer;

class ProgressDialog : public ProgressReporter
{
    Q_OBJECT
    
public:
    ProgressDialog(QString message,
                   bool cancellable,
                   int timeBeforeShow = 0,
                   QWidget *parent = 0,
                   Qt::WindowModality modality = Qt::NonModal);
    virtual ~ProgressDialog();

    bool isDefinite() const override;
    void setDefinite(bool definite) override;

    bool wasCancelled() const override;

signals:
    void showing();
    void cancelled();

public slots:
    void setMessage(QString text) override;
    void setProgress(int percentage) override;

protected slots:
    virtual void showTimerElapsed();
    void canceled();

protected:
    QProgressDialog *m_dialog;
    QTimer *m_showTimer;
    bool m_timerElapsed;
    bool m_cancelled;
};

#endif
