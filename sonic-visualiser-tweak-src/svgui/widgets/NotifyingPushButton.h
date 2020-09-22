/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_NOTIFYING_PUSH_BUTTON_H
#define SV_NOTIFYING_PUSH_BUTTON_H

#include <QPushButton>

/**
 * Very trivial enhancement to QPushButton to make it emit signals
 * when the mouse enters and leaves (for context help). See also
 * NotifyingToolButton
 */

class NotifyingPushButton : public QPushButton
{
    Q_OBJECT

public:
    NotifyingPushButton(QWidget *parent = 0) :
        QPushButton(parent) { }

    virtual ~NotifyingPushButton();

signals:
    void mouseEntered();
    void mouseLeft();

protected:
    void enterEvent(QEvent *) override;
    void leaveEvent(QEvent *) override;
};

#endif

