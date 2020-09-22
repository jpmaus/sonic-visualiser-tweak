/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    bqaudiostream

    A small library wrapping various audio file read/write
    implementations in C++.

    Copyright 2007-2015 Particular Programs Ltd.

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

    Except as contained in this notice, the names of Chris Cannam and
    Particular Programs Ltd shall not be used in advertising or
    otherwise to promote the sale, use or other dealings in this
    Software without prior written authorization.
*/

#include "Exceptions.h"

using std::string;

namespace breakfastquay {

FileNotFound::FileNotFound(string file) throw() :
    m_file(file)
{
    m_what = "File \"" + file + "\" not found";
}

FailedToWriteFile::FailedToWriteFile(string file) throw() :
    m_file(file)
{
    m_what = "Failed to write file \"" + file + "\"";
}

FileReadFailed::FileReadFailed(string file) throw() :
    m_file(file)
{
    m_what = "Failed to read file \"" + file + "\"";
}

FileOperationFailed::FileOperationFailed(string file, string op) throw() :
    m_file(file),
    m_operation(op),
    m_explanation("")
{
    m_what = "File operation \"" + op + "\" failed for file \"" + file + "\"";
}

FileOperationFailed::FileOperationFailed(string file, string op, string exp) throw() :
    m_file(file),
    m_operation(op),
    m_explanation(exp)
{
    m_what = "File operation \"" + op + "\" failed for file \"" + file + "\"";
    if (m_explanation != "") m_what += ": " + m_explanation;
}

InvalidFileFormat::InvalidFileFormat(string file, string how) throw()
{
    m_what = "Invalid file format for file \"" + file + "\": " + how;
}

UnknownFileType::UnknownFileType(string file) throw()
{
    m_what = "Unknown file type for file \"" + file + "\"";
}

}

