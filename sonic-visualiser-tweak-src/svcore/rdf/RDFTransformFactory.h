/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2008 QMUL.
   
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_RDF_TRANSFORM_FACTORY_H
#define SV_RDF_TRANSFORM_FACTORY_H

#include <QObject>
#include <QString>

#include <vector>

#include "transform/Transform.h"

class RDFTransformFactoryImpl;
class ProgressReporter;

class RDFTransformFactory : public QObject
{
    Q_OBJECT

public:
    static QString getKnownExtensions();

    RDFTransformFactory(QString url);
    virtual ~RDFTransformFactory();

    /** isRDF() may be queried at any point after construction. It
        returns true if the file was parseable as RDF.
    */
    bool isRDF();

    /** isOK() may be queried at any point after getTransforms() has
        been called. It is true if the file was parseable as RDF and
        any transforms in it could be completely constructed.

        Note that even if isOK() returns true, it is still possible
        that the file did not define any transforms; in this case,
        getTransforms() would have returned an empty list.

        If isOK() is called before getTransforms() has been invoked to
        query the file, it will return true iff isRDF() is true.
    */
    bool isOK();

    /** Return any error string resulting from loading or querying the
        file. This will be non-empty if isRDF() or isOK() returns
        false.
     */
    QString getErrorString() const;

    std::vector<Transform> getTransforms(ProgressReporter *reporter);

    static QString writeTransformToRDF(const Transform &, QString uri);

protected:
    RDFTransformFactoryImpl *m_d;
};

#endif
