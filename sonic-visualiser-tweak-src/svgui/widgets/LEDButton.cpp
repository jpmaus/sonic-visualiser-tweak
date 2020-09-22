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

/*
    This is a modified version of a source file from the KDE
    libraries.  Copyright (c) 1998-2004 Jörg Habenicht, Richard J
    Moore, Chris Cannam and others, distributed under the GNU Lesser
    General Public License.

    Ported to Qt4 by Chris Cannam.
*/


#include "LEDButton.h"
#include "WidgetScale.h"

#include <QPainter>
#include <QImage>
#include <QColor>
#include <QMouseEvent>

#include <iostream>


class LEDButton::LEDButtonPrivate
{
    friend class LEDButton;

    int dark_factor;
    QColor offcolor;
};


LEDButton::LEDButton(QWidget *parent) :
    QWidget(parent),
    led_state(true)
{
    QColor col(Qt::green);
    d = new LEDButton::LEDButtonPrivate;
    d->dark_factor = 300;
    d->offcolor = col.darker(300);
    
    setColor(col);
}


LEDButton::LEDButton(const QColor& col, QWidget *parent) :
    QWidget(parent),
    led_state(true)
{
    d = new LEDButton::LEDButtonPrivate;
    d->dark_factor = 300;
    d->offcolor = col.darker(300);

    setColor(col);
}

LEDButton::LEDButton(const QColor& col, bool state, QWidget *parent) :
    QWidget(parent),
    led_state(state)
{
    d = new LEDButton::LEDButtonPrivate;
    d->dark_factor = 300;
    d->offcolor = col.darker(300);

    setColor(col);
}

LEDButton::~LEDButton()
{
    delete d;
}

void
LEDButton::mousePressEvent(QMouseEvent *e)
{
    cerr << "LEDButton(" << this << ")::mousePressEvent" << endl;

    if (e->buttons() & Qt::LeftButton) {
        toggle();
        bool newState = state();
        SVDEBUG << "emitting new state " << newState << endl;
        emit stateChanged(newState);
    }
}

void
LEDButton::enterEvent(QEvent *)
{
    emit mouseEntered();
}

void
LEDButton::leaveEvent(QEvent *)
{
    emit mouseLeft();
}

void
LEDButton::paintEvent(QPaintEvent *)
{
    QPainter paint;
    QColor color;
    QBrush brush;
    QPen pen;
                
    // First of all we want to know what area should be updated
    // Initialize coordinates, width, and height of the LED
    int        width = this->width();

    // Make sure the LED is round!
    if (width > this->height())
        width = this->height();
    width -= 2; // leave one pixel border
    if (width < 0) 
        width = 0;

    paint.begin(this);

    paint.setRenderHint(QPainter::Antialiasing, true);

    // Set the color of the LED according to given parameters
    color = (led_state) ? led_color : d->offcolor;

    // Set the brush to SolidPattern, this fills the entire area
    // of the ellipse which is drawn first
    brush.setStyle(Qt::SolidPattern);
    brush.setColor(color);
    paint.setBrush(brush);

    // Draws a "flat" LED with the given color:
    paint.drawEllipse( 1, 1, width - 2, width - 2 );

    // Draw the bright light spot of the LED now, using modified "old"
    // painter routine taken from KDEUI´s LEDButton widget:

    // Setting the new width of the pen is essential to avoid "pixelized"
    // shadow like it can be observed with the old LED code
    pen.setWidth( 2 );

    // shrink the light on the LED to a size about 2/3 of the complete LED
    int pos = width/5 + 1;
    int light_width = width;
    light_width *= 2;
    light_width /= 3;
        
    // Calculate the LED´s "light factor":
    int light_quote = (130*2/(light_width?light_width:1))+100;

    // Now draw the bright spot on the LED:
    while (light_width) {
        color = color.lighter( light_quote );                      // make color lighter
        pen.setColor( color );                                   // set color as pen color
        paint.setPen( pen );                                     // select the pen for drawing
        paint.drawEllipse( pos, pos, light_width, light_width ); // draw the ellipse (circle)
        light_width--;
        if (!light_width)
            break;
        paint.drawEllipse( pos, pos, light_width, light_width );
        light_width--;
        if (!light_width)
            break;
        paint.drawEllipse( pos, pos, light_width, light_width );
        pos++; light_width--;
    }

    paint.drawPoint(pos, pos);

    // Drawing of bright spot finished, now draw a thin border
    // around the LED which resembles a shadow with light coming
    // from the upper left.

    pen.setWidth(2);
    brush.setStyle(Qt::NoBrush);
    paint.setBrush(brush); // This avoids filling of the ellipse

    // Set the initial color value to colorGroup().light() (bright) and start
    // drawing the shadow border at 45° (45*16 = 720).

    int angle = -720;
    color = palette().light().color();
    
    for (int arc = 120; arc < 2880; arc += 240) {
        pen.setColor(color);
        paint.setPen(pen);
        int w = width - pen.width()/2;
        paint.drawArc(pen.width()/2 + 1, pen.width()/2 + 1, w - 2, w - 2, angle + arc, 240);
        paint.drawArc(pen.width()/2 + 1, pen.width()/2 + 1, w - 2, w - 2, angle - arc, 240);
        color = color.darker(110); //FIXME: this should somehow use the contrast value
    }        // end for ( angle = 720; angle < 6480; angle += 160 )

    paint.end();
}

bool
LEDButton::state() const
{
    return led_state;
}

QColor
LEDButton::color() const
{
    return led_color;
}

void
LEDButton::setState( bool state )
{
    if (led_state != state)
    {
        led_state = state;
        update();
    }
}

void
LEDButton::toggleState()
{
    led_state = (led_state == true) ? false : true;
    // setColor(led_color);
    update();
}

void
LEDButton::setColor(const QColor& col)
{
    if(led_color!=col) {
        led_color = col;
        d->offcolor = col.darker(d->dark_factor);
        update();
    }
}

void
LEDButton::setDarkFactor(int darkfactor)
{
    if (d->dark_factor != darkfactor) {
        d->dark_factor = darkfactor;
        d->offcolor = led_color.darker(darkfactor);
        update();
    }
}

int
LEDButton::darkFactor() const
{
    return d->dark_factor;
}

void
LEDButton::toggle()
{
    toggleState();
}

void
LEDButton::on()
{
    setState(true);
}

void
LEDButton::off()
{
    setState(false);
}

QSize
LEDButton::sizeHint() const
{
    return WidgetScale::scaleQSize(QSize(17, 17));
}

QSize
LEDButton::minimumSizeHint() const
{
    return WidgetScale::scaleQSize(QSize(17, 17));
}

