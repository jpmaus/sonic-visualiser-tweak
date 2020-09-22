/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Dataquay

    A C++/Qt library for simple RDF datastore management.
    Copyright 2009-2012 Chris Cannam.
  
    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the name of Chris Cannam
    shall not be used in advertising or otherwise to promote the sale,
    use or other dealings in this Software without prior written
    authorization.
*/

#include "Triple.h"

#include <QDataStream>
#include <QTextStream>

namespace Dataquay
{

bool
operator==(const Triple &a, const Triple &b)
{
    if (a.a == b.a &&
        a.b == b.b &&
        a.c == b.c) return true;
    return false;
}

bool
operator!=(const Triple &a, const Triple &b)
{
    return !operator==(a, b);
}

QDataStream &
operator<<(QDataStream &out, const Triple &t)
{
    return out << t.a << t.b << t.c;
}

QDataStream &
operator>>(QDataStream &in, Triple &t)
{
    return in >> t.a >> t.b >> t.c;
}

std::ostream &
operator<<(std::ostream &out, const Triple &t)
{
    return out << "( " << t.a << " " << t.b << " " << t.c << " )";
}

QTextStream &
operator<<(QTextStream &out, const Triple &t)
{
    return out << "( " << t.a << " " << t.b << " " << t.c << " )";
}

}

