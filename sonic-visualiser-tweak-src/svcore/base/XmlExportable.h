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

#ifndef SV_XML_EXPORTABLE_H
#define SV_XML_EXPORTABLE_H

#include <QString>

#include "Debug.h"

class QTextStream;

class XmlExportable
{
public:
    enum {
        // The value NO_ID (-1) is never allocated as an export id
        NO_ID = -1
    };

    typedef int ExportId;
    
    XmlExportable() : m_exportId(NO_ID) { }
    virtual ~XmlExportable() { }

    /**
     * Return the numerical export identifier for this object.  It's
     * allocated the first time this is called, so objects on which
     * this is never called do not get allocated one.
     */
    ExportId getExportId() const;

    /**
     * Stream this exportable object out to XML on a text stream.
     */
    virtual void toXml(QTextStream &stream,
                       QString indent = "",
                       QString extraAttributes = "") const = 0;

    /**
     * Convert this exportable object to XML in a string.  The default
     * implementation calls toXml and returns the result as a string.
     * Do not override this unless you really know what you're doing.
     */
    virtual QString toXmlString(QString indent = "",
                                QString extraAttributes = "") const;

    static QString encodeEntities(QString);

    static QString encodeColour(int r, int g, int b); 

private:
    mutable int m_exportId;
};

#endif
